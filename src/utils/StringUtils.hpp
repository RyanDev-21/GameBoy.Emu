#ifndef STRINGUTILS_HPP
#define STRINGUTILS_HPP

#include <string>

namespace StringUtils {
void ltrim(std::string& s);
void rtrim(std::string& s);
std::string trim(std::string s);
}  // namespace StringUtils

#endif
