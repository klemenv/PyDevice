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

std::vector<std::string> getMacros(const std::string& text);
std::string replaceMacro(const std::string& text, const std::string& macro, const std::string& replacement);
std::string escape(const std::string& text);
std::string join(const std::vector<std::string>& tokens, const std::string& glue);
long getEnvConfig(const std::string& name, long defval);

};

#endif // UTIL_H
