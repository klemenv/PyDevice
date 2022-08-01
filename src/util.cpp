/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "util.h"
#include <envDefs.h>
#include <cctype>
#include <stdexcept>

namespace Util {

std::map<std::string, std::string> getFields(const std::string& text)
{
    const long MAX_FIELD_LEN = 4;
    std::map<std::string, std::string> fields;

    std::string token;
    char prevChar = '\0';
    for (size_t i = 0; i < text.length(); i++) {
        if (isupper(text.at(i))) {
            if (!isalpha(prevChar) || !token.empty()) {
                token += text.at(i);
            }
        } else if (islower(text.at(i))) {
            token.clear();
        } else {
            if (!token.empty()) {
                if (token.length() <= MAX_FIELD_LEN) {
                    fields[token] = token;
                }
                token.clear();
            }
        }
        prevChar = text.at(i);
    }
    if (!token.empty()) {
        if (token.length() <= MAX_FIELD_LEN) {
            fields[token] = token;
        }
    }

    return fields;
}

std::string replaceFields(const std::string& text, const std::map<std::string, std::string>& fields)
{
    const std::string delimiter = "%";
    std::string out = text;

    bool replaced = false;
    for (size_t pos = 0; pos < out.length(); pos++) {
        for (auto& field: fields) {
            std::string token;

            // Try exact match first, ie. VAL, but needs to be surrounded by non-alnum characters
            token = field.first;
            if (out.compare(pos, token.length(), token) == 0) {
                char charBefore = (pos == 0 || replaced ? '.' : out.at(pos-1));
                char charAfter  = (pos+token.length() >= out.length() ? '.' : out.at(pos+token.length()));
                if (!isalnum(charBefore) && !isalnum(charAfter)) {
                    out.replace(pos, token.length(), field.second);
                    pos += field.second.length() - 1;
                    replaced = true;
                    break;
                }
            }

            // Put % sign around the field name, ie. %VAL%
            token = delimiter + field.first + delimiter;
            if (out.compare(pos, token.length(), token) == 0) {
                out.replace(pos, token.length(), field.second);
                pos += field.second.length() - 1;
                replaced = true;
                break;
            }

            replaced = false;
        }
    }

    return out;
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

std::string join(const std::vector<std::string>& tokens, const std::string& glue)
{
    std::string out;
    for (auto& token: tokens) {
        out += token + glue;
    }
    return out.substr(0, out.length() - glue.length());
}

long getEnvConfig(const std::string& name, long defval)
{
    long value = defval;
    std::string sdefval = std::to_string(defval);
    ENV_PARAM param{const_cast<char*>(name.c_str()), const_cast<char*>(sdefval.c_str())};
    envGetLongConfigParam(&param, &value);
    if (value < 1) {
        value = defval;
    }

    return value;
}

}; // namespace Util
