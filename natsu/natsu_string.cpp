#include "natsu_string.h"

namespace natsu {

void tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters)  
{  
    std::string::size_type lastpos = str.find_first_not_of(delimiters, 0);  
    std::string::size_type pos = str.find_first_of(delimiters, lastpos);  
    while (std::string::npos != pos || std::string::npos != lastpos)  
    {  
        tokens.push_back(str.substr(lastpos, pos - lastpos));  
        lastpos = str.find_first_not_of(delimiters, pos);  
        pos = str.find_first_of(delimiters, lastpos);  
    }  
}


}
