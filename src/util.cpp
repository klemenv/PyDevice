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
    auto pos1 = out.find('%');
    auto pos2 = out.find('%', pos1+1);

    while (pos1 != out.npos && pos2 != out.npos) {
        std::string field = out.substr(pos1, pos2-pos1+1);
        std::string val;
        try {
            val = fields.at(field);
        } catch (std::out_of_range) {
            val = "";
        }

        out.replace(pos1, pos2-pos1+1, val);

        pos1 = out.find('%', pos1);
        pos2 = out.find('%', pos1+1);
    }

    return out;
}

}; // namespace Util
