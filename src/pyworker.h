#include <string>

class PyWorker {
    private:
    public:
        static bool init();
        static void shutdown();
        static void exec(const std::string& line, bool debug);
        static bool exec(const std::string& line, bool debug, int32_t* val);
};
