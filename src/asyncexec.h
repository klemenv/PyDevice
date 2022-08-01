/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef ASYNCEXEC_H
#define ASYNCEXEC_H

#include <functional>

class AsyncExec {
    public:
        using Callback = std::function<void()>;
        static void init(unsigned numThreads);
        static void shutdown();
        static bool schedule(const Callback& callback);
};

#endif // ASYNCEXEC_H
