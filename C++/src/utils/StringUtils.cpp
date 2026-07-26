#include "StringUtils.hpp"

#include <algorithm>
#include <cctype>

void StringUtils::ltrim(std::string& s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
          }));
}

void StringUtils::rtrim(std::string& s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](unsigned char ch) { return !std::isspace(ch); })
              .base(),
          s.end());
}

std::string StringUtils::trim(std::string s) {
  rtrim(s);
  ltrim(s);
  return s;
}
