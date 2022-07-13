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
    public:
        struct MultiTypeValue {
            std::string s;
            bool b;
            double f;
            long i;
            std::vector<long> vi;
            std::vector<double> vf;
            std::vector<std::string> vs;
            enum class Type {
                NONE,
                BOOL,
                STRING,
                FLOAT,
                INTEGER,
                VECTOR_FLOAT,
                VECTOR_INTEGER,
                VECTOR_STRING,
            } type{Type::NONE};
        };
        using Callback = std::function<void()>;
    private:
        static bool convert(void* in, MultiTypeValue& out);
    public:
        static bool init();
        static void shutdown();
        static void registerIoIntr(const std::string& name, const Callback& cb);
        static MultiTypeValue exec(const std::string& line, bool debug);
        static bool exec(const std::string& line, bool debug, std::string& val);
        template <typename T> static bool exec(const std::string& line, bool debug, T* val);
        template <typename T> static bool exec(const std::string& line, bool debug, std::vector<T>& val);
};

#endif // PYWRAPPER_H
