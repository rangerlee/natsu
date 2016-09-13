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

std::string replace_all(const std::string &src, std::string org_str, std::string rep_str)  
{  
    std::string result;
    std::string::size_type lastpos = 0;
    while(true)
    {
        std::string::size_type pos = src.find_first_of(org_str, lastpos);
        if(pos == std::string::npos)
        {
            result.append(src.substr(lastpos));
            break;
        }

        result.append(src.substr(lastpos, pos - lastpos));
        result.append(rep_str);
        lastpos = pos + 1;
    }

    return result;
} 


}
