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

std::map<std::string, std::string> getReplacables(const std::string& text);
std::string replace(const std::string& text, const std::map<std::string, std::string>& fields);
std::string escape(const std::string& text);

template <typename T>
std::string arrayToStr(const std::vector<T>& val)
{
    std::string value = "[";
    for (const auto v: val) {
        value += std::to_string(v) + ",";
    }
    if (value.back() == ',') {
        value.back() = ']';
    } else {
        value += "]";
    }
    return value;
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
