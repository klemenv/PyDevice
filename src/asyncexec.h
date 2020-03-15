/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <functional>

class AsyncExec {
    public:
        using Callback = std::function<void()>;
        static bool init();
        static void shutdown();
        static void schedule(const Callback& callback);
};
