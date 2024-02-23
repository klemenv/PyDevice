/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <map>
#include <vector>

namespace Util {

std::map<std::string, std::string> getFields(const std::string& text);
std::string replaceFields(const std::string& text, const std::map<std::string, std::string>& fields);
std::string escape(const std::string& text);
std::string join(const std::vector<std::string>& tokens, const std::string& glue);
long getEnvConfig(const std::string& name, long defval);

namespace detail {
// conversion from python basis types to strings
// std::to_string has only limited accuracy for floot double etc.
std::string float_to_string(const float v);
std::string double_to_string(const double v);
std::string long_double_to_string(const long double v);
}

// defined inline to avoid definition of function in different sub-objects.
template<typename T>
inline std::string to_string(const T v) { return std::to_string(v); }
template<>
inline std::string to_string(const float v) { return detail::float_to_string(v); }
template<>
inline std::string to_string(const double v) { return detail::double_to_string(v); }
template<>
inline std::string to_string(const long double v) { return detail::long_double_to_string(v); }

template <typename T>
std::vector<std::string> to_strings(const T* array, size_t n)
{
    std::vector<std::string> values;
    for (size_t i=0; i<n; i++) {
        values.push_back(Util::to_string(array[i]));
    }
    return values;
}

template <typename T>
std::string to_pylist_string(const T* array, size_t n)
{
    auto values = to_strings(array, n);
    return "[" + join(values, ",") + "]";
}

static inline std::string to_pylist_string(const std::vector<std::string>& array)
{
    return "[" + join(array, ",") + "]";
}

template<typename T, typename... Rest>
struct is_any : std::false_type {};

template<typename T, typename First>
struct is_any<T, First> : std::is_same<T, First> {};

template<typename T, typename First, typename... Rest>
struct is_any<T, First, Rest...>
    : std::integral_constant<bool, std::is_same<T, First>::value || is_any<T, Rest...>::value>
{};

};

#endif // UTIL_H
