/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <functional>
#include <string>

class PyWorker {
    private:
        static bool convert(void* in, int32_t& out);
        static bool convert(void* in, double& out);
        static bool convert(void* in, uint16_t& out);
        static bool convert(void* in, std::string& out);
    public:
        using Callback = std::function<void()>;
        static bool init();
        static void shutdown();
        static void registerIoIntr(const std::string& name, const Callback& cb);
        static void exec(const std::string& line, bool debug);
        static bool exec(const std::string& line, bool debug, std::string& val);
        template <typename T> static bool exec(const std::string& line, bool debug, T* val);
};
