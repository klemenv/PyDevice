#include <functional>
#include <string>

class PyWorker {
    public:
        using Callback = std::function<void()>;
        static bool init();
        static void shutdown();
        static void registerIoIntr(const std::string& name, const Callback& cb);
        static void exec(const std::string& line, bool debug);
        static bool exec(const std::string& line, bool debug, int32_t* val);
};
