#ifndef NATSU_STRING_H_
#define NATSU_STRING_H_

#include <string>
#include <vector>

namespace natsu {

void tokenize(const std::string& str,  std::vector<std::string>& tokens,  const std::string& delimiters);

std::string replace_all(const std::string &src, std::string org_str, std::string rep_str);


}

#endif