/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "asyncexec.h"

#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsThread.h>

#include <atomic>
#include <list>
#include <memory>

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
//            printf("empty/found: %d/%d\n", que.empty(), found);
            mutex.unlock();
            return found;
        }
};

class WorkerThread : public epicsThreadRunable {
    public:
        epicsThread thread;
        TaskQueue<AsyncExec::Callback> tasks;
        std::atomic<bool> running{true};

        WorkerThread()
        : thread(*this, "WorkerThread", epicsThreadGetStackSize(epicsThreadStackSmall))
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
                if (tasks.dequeue(1.0, cb)) {
                    cb();
                }
            }
        }
};

static std::unique_ptr<WorkerThread> worker;

bool AsyncExec::init()
{
    worker.reset(new WorkerThread());
    return !!worker;
}

void AsyncExec::shutdown()
{
    worker.reset();
}

void AsyncExec::schedule(const AsyncExec::Callback& callback)
{
    assert(!!worker);
    assert(!!callback);
    worker->tasks.enqueue(callback);
}