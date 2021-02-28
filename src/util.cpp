/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "util.h"
#include <stdexcept>
namespace Util {

std::map<std::string, std::string> getReplacables(const std::string& text)
{
    auto pos1 = text.find('%');
    auto pos2 = text.find('%', pos1+1);

    std::map<std::string, std::string> fields;

    while (pos1 != text.npos && pos2 != text.npos) {
        fields[ text.substr(pos1, pos2-pos1+1) ] = "";

        pos1 = text.find('%', pos2+1);
        pos2 = text.find('%', pos1+1);
    }

    return fields;
}

std::string replace(const std::string& text, const std::map<std::string, std::string>& fields)
{
    std::string out = text;

    for (auto& field: fields) {
        auto pos = out.find(field.first);
        while (pos != out.npos) {
            out.replace(pos, pos+field.first.size(), field.second);
            pos = out.find(field.first, pos+field.second.size());
        }
    }

    return out;
}

}; // namespace Util
