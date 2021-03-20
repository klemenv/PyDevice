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

    for (size_t pos = 0; pos < out.length(); pos++) {
        for (auto& field: fields) {
            if (out.compare(pos, field.first.length(), field.first) == 0) {
                out.replace(pos, field.first.length(), field.second);
                pos += field.second.length() - 1;
                break;
            }
        }
    }

    return out;
}

std::string escape(const std::string& text)
{
    const static std::map<std::string, std::string> escapables = {
        { "\n", "\\n" },
        { "\r", "\\r" },
        { "'",  "\\'" },
    };
    return Util::replace(text, escapables);
}

}; // namespace Util
