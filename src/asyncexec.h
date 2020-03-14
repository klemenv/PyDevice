#include <functional>
#include <string>

class AsyncExec {
    public:
        using Callback = std::function<void()>;
        static bool init();
        static void shutdown();
        static void schedule(const Callback& callback);
};
