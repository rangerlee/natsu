#ifndef NATSU_PCREREGEX_H_
#define NATSU_PCREREGEX_H_

#include <pcre.h>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace natsu{

/** \class  PcreRegex
 */
class PcreRegex
{
public:
    enum {
        VECSIZE = 64
    };

    struct MatchResult {
        std::string name;
        std::vector<std::string> value;
    };

    inline bool add_rule(const std::string &name, const std::string &patten)
    {
        pcre *re = pcre_compile(patten.c_str(), PCRE_UTF8|PCRE_NO_AUTO_CAPTURE, &error_, &error_offset_, NULL);
        if(re == NULL) {
            return false;
        } else {
            pcre_array_.push_back(re);
            patten_array_.push_back(name);
            return true;
        }
    }

    inline void clear()
    {
        for(size_t i=0; i<pcre_array_.size(); i++) {
            pcre_free(pcre_array_[i]);
        }
        pcre_array_.clear();
        patten_array_.clear();
    }

    std::vector<MatchResult> match(const char* content)
    {
        std::vector<MatchResult> result_arr;
        int length = strlen(content);
        char buf[512];
        for(size_t i=0; i<pcre_array_.size(); i++) {
            MatchResult result;
            result.name = patten_array_[i];
            int cur = 0;
            int rc;
            while(cur<length && (rc = pcre_exec(pcre_array_[i], NULL, content, length, cur, PCRE_NOTEMPTY, vector_, VECSIZE)) >= 0) {
                for(int j=0; j<rc; j++) {
                    memset(buf, 0, sizeof(buf));
                    strncpy(buf, content+vector_[2*j], vector_[2*j+1]-vector_[2*j]);
                    result.value.push_back(buf);
                }
                cur = vector_[1];
            }
            if(result.value.size() > 0)
                result_arr.push_back(result);
        }

        std::sort(result_arr.begin(),result_arr.end(),MatchResultCompare());
        return result_arr;
    }

private:
    class MatchResultCompare
    {
    public:
        bool operator()(const MatchResult& firstResult, const MatchResult& secondResult)
        {
            return firstResult.value[0].size() > secondResult.value[0].size();
        }
    };

private:
    const char *error_;
    int error_offset_;
    int vector_[VECSIZE];
    std::vector<pcre*> pcre_array_;
    std::vector<std::string> patten_array_;
};

}
#endif
