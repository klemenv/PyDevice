/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef VARIANT_H
#define VARIANT_H

#include <epicsTypes.h>

#include <cstdint>
#include <string>
#include <vector>

class Variant
{
private:
    std::string s;
    bool b;
    double d;
    long long l;
    unsigned long long u;
    std::vector<long long int> vl;
    std::vector<unsigned long long int> vu;
    std::vector<double> vd;
    std::vector<std::string> vs;

public:
    enum class Type
    {
        NONE,
        BOOL,
        STRING,
        DOUBLE,
        LONG,
        UNSIGNED,
        VECTOR_DOUBLE,
        VECTOR_LONG,
        VECTOR_UNSIGNED,
        VECTOR_STRING,
    } type{Type::NONE};

    class ConvertError : public std::exception
    {
        private:
            std::string error;

        public:
            ConvertError(const std::string& reason="Failed to convert value")
            : error(reason) {}

            virtual const char* what() {
                return error.c_str();
            }
    };

    Variant();
    Variant(const std::string& val);
    Variant(const char *val) : Variant(std::string(val)) {};
    Variant(const bool val);
    Variant(const long long int val);
    Variant(const unsigned long long int val);
    Variant(const double val);
    Variant(const std::vector<long long int>& val);
    Variant(const std::vector<unsigned long long int>& val);
    Variant(const std::vector<double>& val);
    Variant(const std::vector<std::string>& val);

    // Helper c'tors for EPICS types
    Variant(const signed char val)        : Variant(static_cast<const long long int>(val)){};
    Variant(const signed short int val)   : Variant(static_cast<const long long int>(val)){};
    Variant(const signed int val)         : Variant(static_cast<const long long int>(val)){};
    Variant(const signed long int val)    : Variant(static_cast<const long long int>(val)){};
    Variant(const unsigned char val)      : Variant(static_cast<const unsigned long long int>(val)){};
    Variant(const unsigned short int val) : Variant(static_cast<const unsigned long long int>(val)){};
    Variant(const unsigned int val)       : Variant(static_cast<const unsigned long long int>(val)){};
    Variant(const unsigned long int val)  : Variant(static_cast<const unsigned long long int>(val)){};
    Variant(const float val)              : Variant(static_cast<const double>(val)){};

    Variant(const char* vals,                   size_t n) : Variant(std::vector<long long int>(vals, vals+n)) {};
    Variant(const signed char* vals,            size_t n) : Variant(std::vector<long long int>(vals, vals+n)) {};
    Variant(const signed short int* vals,       size_t n) : Variant(std::vector<long long int>(vals, vals+n)) {};
    Variant(const signed int* vals,             size_t n) : Variant(std::vector<long long int>(vals, vals+n)) {};
    Variant(const signed long int* vals,        size_t n) : Variant(std::vector<long long int>(vals, vals+n)) {};
    Variant(const signed long long int* vals,   size_t n) : Variant(std::vector<long long int>(vals, vals+n)) {};
    Variant(const unsigned char* vals,          size_t n) : Variant(std::vector<unsigned long long int>(vals, vals+n)) {};
    Variant(const unsigned short int* vals,     size_t n) : Variant(std::vector<unsigned long long int>(vals, vals+n)) {};
    Variant(const unsigned int* vals,           size_t n) : Variant(std::vector<unsigned long long int>(vals, vals+n)) {};
    Variant(const unsigned long int* vals,      size_t n) : Variant(std::vector<unsigned long long int>(vals, vals+n)) {};
    Variant(const unsigned long long int* vals, size_t n) : Variant(std::vector<unsigned long long int>(vals, vals+n)) {};
    Variant(const float* vals,                  size_t n) : Variant(std::vector<double>(vals, vals+n)) {};
    Variant(const double* vals,                 size_t n) : Variant(std::vector<double>(vals, vals+n)) {};

    bool get_bool() const;
    int64_t get_long() const;
    uint64_t get_unsigned() const;
    double get_double() const;
    std::string get_string() const;
    std::vector<long long int> get_long_array() const;
    std::vector<unsigned long long int> get_unsigned_array() const;
    std::vector<double> get_double_array() const;
    std::vector<std::string> get_string_array() const;
};

#endif // VARIANT_H
