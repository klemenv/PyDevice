/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef PYWRAPPER_H
#define PYWRAPPER_H

#include <functional>
#include <string>
#include <vector>

class PyWrapper {
    private:
        template <typename T> static bool convert(void* in, T& out);
        template <typename T> static bool convert(void* in, std::vector<T>& out);
    public:
        using Callback = std::function<void()>;
        static bool init();
        static void shutdown();
        static void registerIoIntr(const std::string& name, const Callback& cb);
        static void exec(const std::string& line, bool debug);
        static bool exec(const std::string& line, bool debug, std::string& val);
        template <typename T> static bool exec(const std::string& line, bool debug, T* val);
        template <typename T> static bool exec(const std::string& line, bool debug, std::vector<T>& val);
};

#endif // PYWRAPPER_H
