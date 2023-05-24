/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "asyncexec.h"

#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsThread.h>

#include <atomic>
#include <cstdio>
#include <list>
#include <memory>
#include <vector>
#include <string>

template<typename T>
class TaskQueue {
    private:
        epicsMutex mutex;
        epicsEvent event;
        std::list<T> que;

    public:
        void enqueue(const T& task)
        {
            mutex.lock();
            que.emplace_back(task);
            mutex.unlock();
            event.signal();
        }

        bool dequeue(double timeout, T& task)
        {
            bool found = false;
            mutex.lock();
            if (que.empty()) {
                mutex.unlock();
                event.wait(timeout);
                mutex.lock();
            }
            if (!que.empty()) {
                task = std::move(que.front());
                que.pop_front();
                found = true;
            }
            mutex.unlock();
            return found;
        }
};
static TaskQueue<AsyncExec::Callback> g_tasks;

class WorkerThread : public epicsThreadRunable {
    public:
        epicsThread thread;
        std::atomic<bool> running{true};

        WorkerThread(const std::string& id)
        : thread(*this, id.c_str(), epicsThreadGetStackSize(epicsThreadStackMedium))
        {
            thread.start();
        }

        ~WorkerThread()
        {
            running = false;
            thread.exitWait();
        }

        void run() override
        {
            while (running) {
                AsyncExec::Callback cb;
                if (g_tasks.dequeue(1.0, cb)) {
                    cb();
                }
            }
        }

        void stop()
        {
            running = false;
        }
};
static std::vector< std::unique_ptr<WorkerThread> > g_workers;

void AsyncExec::init(unsigned numThreads)
{
    while (numThreads--) {
        std::string id = "PyDeviceExec_" + std::to_string(numThreads);
        WorkerThread* worker = new WorkerThread(id);
        if (worker) {
            g_workers.push_back(std::unique_ptr<WorkerThread>(worker));
        }
    }

    if (g_workers.empty()) {
        printf("Failed to initialize PyDevice worker threads!");
    }
}

void AsyncExec::shutdown()
{
    // Let all threads know we're going down, so that they can start
    // wrapping up in parallel and not start any new tasks.
    for (auto& worker: g_workers) {
        worker->stop();
    }
    // This is a blocking call that waits for all threads to exit
    g_workers.clear();
}

bool AsyncExec::schedule(const AsyncExec::Callback& callback)
{
    if (g_workers.empty() || !callback)
        return false;
    g_tasks.enqueue(callback);
    return true;
}
