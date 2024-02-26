/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef PYWRAPPER_H
#define PYWRAPPER_H

#include "variant.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

class PyWrapper {
    public:
        class SyntaxError : public std::exception
        {
            public:
                virtual const char* what() {
                    return "Python Syntax Error";
                }
        };

        class EvalError : public std::exception
        {
            public:
                virtual const char* what() {
                    return "Code Eval Error";
                }
        };

        class ArgumentError : public std::exception
        {
            public:
                virtual const char* what() {
                    return "Argument Error";
                }
        };

        struct ByteCode
        {
            private:
                void* code;
                ByteCode(void*);
            public:
                ByteCode();
                ByteCode(ByteCode &);
                ByteCode(ByteCode&&);
                ByteCode& operator=(const ByteCode&);
                ~ByteCode();
                friend class PyWrapper;
        };
        struct MultiTypeValue
        {
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
        static bool convert(void* in, Variant& out);
    public:
        static bool init();
        static void shutdown();
        static void registerIoIntr(const std::string& name, const Callback& cb);

        static ByteCode compile(const std::string &code, bool debug);
        static Variant eval(const ByteCode& bytecode, const std::map<std::string, Variant> &args, bool debug);
        static Variant exec(const std::string& code, const std::map<std::string, Variant> &args, bool debug)
        {
            auto bytecode = PyWrapper::compile(code, true);
            return PyWrapper::eval(bytecode, args, true);
        }
        static Variant exec(const std::string& code, bool debug)
        {
            std::map<std::string, Variant> args;
            return PyWrapper::exec(code, args, debug);
        }
};

#endif // PYWRAPPER_H
