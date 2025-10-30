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
            private:
                std::string error;

            public:
                SyntaxError(const std::string& reason="Python code Syntax Error")
                : error(reason) {}

                virtual const char* what() {
                    return error.c_str();
                }
        };

        class EvalError : public std::exception
        {
            private:
                std::string error;

            public:
                EvalError(const std::string& reason="Failed to evaluate Python code")
                : error(reason) {}

                virtual const char* what() {
                    return error.c_str();
                }
        };

        class ArgumentError : public std::exception
        {
            private:
                std::string error;

            public:
                ArgumentError(const std::string& reason="Invalid argument")
                : error(reason) {}

                virtual const char* what() {
                    return error.c_str();
                }
        };

        struct ByteCode
        {
            private:
                void* code;
                ByteCode(void*);
                ByteCode(ByteCode &) = delete;
                ByteCode &operator=(const ByteCode &) = delete;
                friend class PyWrapper;

            public:
                ByteCode();
                ByteCode(ByteCode&&);
                ByteCode& operator=(ByteCode&&);
                ~ByteCode();
        };
        using Callback = std::function<void()>;
    private:
        static bool convert(void* in, Variant& out);
    public:
        static bool init();
        static void shutdown();
        static void registerIoIntr(const std::string& name, const Callback& cb);

        /**
         * @brief Compile Python code into bytecode
         * 
         * Compiling Python code into intermediate bytecode allows performance
         * gains when same code will be used many times. This will parse the
         * code, but not actually evaluate it.
         * This function runs under locked GIL environment.
         * 
         * @param code Python code to be compiled
         * @param debug Prints errors to the EPICS console.
         * @return ByteCode Compiled bytecode, must be destroyed after not used any more.
         */
        static ByteCode compile(const std::string &code, bool debug);

        /**
         * @brief Evaluate previously compiled bytecode and return result.
         * 
         * Evaluating previously compiled bytecode will run the code and
         * optionally return the result.
         * This function runs under locked GIL environment.
         * 
         * @param bytecode Previously compiled bytecode
         * @param args Optional arguments passed to Python code, aka functions etc.
         * @param debug Prints errors to the EPICS console.
         * @return Variant 
         */
        static Variant eval(const ByteCode& bytecode, const std::map<std::string, Variant> &args, bool debug);

        /**
         * @brief Execute (compile and eval) given Python code
         * 
         * This is a convenience function that combines compiling Python
         * code and immediately evaluating it. This is useful when the code
         * is only specified once, for example when invoked from EPICS shell.
         * 
         * @param code Python code to be executed
         * @param args Optional arguments to the Python function
         * @param debug Prints errors to the EPICS console.
         * @return Variant Value returned from Python code, if any.
         */
        static Variant exec(const std::string &code, const std::map<std::string, Variant> &args, bool debug);

        /**
         * @brief Execute (compile and eval) given Python code with no arguments.
         * 
         * This is a convenience function that combines compiling Python code
         * and immediately evaluating it, and not arguments are needed.
         * 
         * @param code Python code to be executed
         * @param debug Prints errors to the EPICS console.
         * @return Variant Value returned from Python code, if any.
         */
        static Variant exec(const std::string &code, bool debug = false)
        {
            std::map<std::string, Variant> args;
            return PyWrapper::exec(code, args, debug);
        }

        /**
         * @brief Destroy previously compiled bytecode
         * 
         * As with any Python object, the compiled bytecode needs to be 
         * dereferenced after use to free-up any memory it has been using.
         * Dereferencing requires GIL to be obtained, and this function
         * will guarantee that GIL is locked.
         * 
         * @param bytecode Previously compiled bytecode, may be empty object.
         */
        static void destroy(ByteCode&& bytecode);
};

#endif // PYWRAPPER_H
