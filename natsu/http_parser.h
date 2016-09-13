#ifndef HTTPPARSER_HPP
#define HTTPPARSER_HPP

#include <iostream>
#include <string>
#include <map>
#include <string.h>
#include <algorithm>
#include <memory>

#include "tribool.h"
#include "http_request.h"

#ifdef WIN32
#define strcasecmp _stricmp
#endif

namespace natsu {
namespace http {

class HttpParser
{
public:
    HttpParser()
    {
        reset();
    }

    void reset()
    {
        parse_func_ = &HttpParser::parse_first_line;
        req_.reset();

        cache_.clear();
        content_length_ = 0;
    }

    tribool parse()
    {
        std::string sBuf;
        sBuf.swap(cache_);
        reset();
        cache_.swap(sBuf);

        if(parse_func_) return (this->*parse_func_) ();

        return failure;
    }

    tribool parse(const char* pData, size_t len)
    {
        cache_.append(pData,len);

        if(parse_func_) return (this->*parse_func_) ();

        return failure;
    }

    std::shared_ptr<HttpRequest>& request()
    {
        return req_;
    }


private:
    tribool parse_body_with_length()
    {
        if(cache_.size() >= content_length_)
        {
            req_->body().clear();
            req_->body() = cache_.substr(0,content_length_);
            cache_.erase(0,content_length_);

            return success;
        }

        return indeterminate;
    }

    tribool parse_body_with_chunk()
    {
        while(true)
        {
            static const size_t MaxChunkNum = 10;
            size_t pos = cache_.find("\r\n");
            if(pos == std::string::npos)
            {
                if(cache_.size() > MaxChunkNum)
                {
                    return failure;
                }

                return indeterminate;
            }

            //CHUNKED
            std::string line = cache_.substr(0, pos);

            size_t len = stringtoint(line.c_str());
            if(len == 0)
            {
                return success;
            }
            else if((cache_.size() - pos) < (len + 4))
            {
                break;
            }
            else
            {
                cache_.erase(0, pos + 2);
                req_->body().append(cache_.c_str(), len);
                cache_.erase(0, len + 2);
            }
        }

        return indeterminate;
    }

    tribool parse_headers()
    {
        static const size_t MaxHeader = 1024;
        size_t pos = cache_.find("\r\n");
        if(pos != std::string::npos)
        {
            std::string line = cache_.substr(0, pos);
            cache_.erase(0, pos + 2);
            if(line == "")
            {
                ///HEAD END
                parse_func_ = &HttpParser::parse_body_with_length;

                ///Transfer-Encoding
                if(req_->header("transfer-encoding").size())
                {
                    std::string encode = req_->header("transfer-encoding");
                    if(strcasecmp("CHUNKED",encode.c_str()) == 0)
                        parse_func_ = &HttpParser::parse_body_with_chunk;
                }
                else
                {
                    if(req_->header("content-length").size())
                    {
                        std::string len = req_->header("content-length");
                        content_length_ = atoi(len.c_str());                        
                    }
                    else
                        content_length_ = 0;

                    if( content_length_ == 0)
                    {
                        return success;
                    }
                }                

                return (this->*parse_func_) ();
            }

            size_t mid = line.find(":");
            if(mid == std::string::npos)
            {
                return failure;
            }

            std::string key = line.substr(0, mid);
            std::string value = line.substr(mid + 1);

            if(key == "")
            {
                return failure;
            }
            key.erase(trimright(key) + 1);
            value.erase(0,trimleft(value));
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            req_->header(key, value);

            return parse_headers();
        }
        else
        {
            if(cache_.size() > MaxHeader)
            {
                return failure;
            }
        }

        return indeterminate;
    }

    tribool parse_first_line()
    {
        static const size_t MaxUrl = 1024;
        static const size_t MinLine = 14;

        size_t pos = cache_.find("\r\n");
        if(pos != std::string::npos)
        {
            std::string sLine = cache_.substr(0, pos);
            cache_.erase(0, pos + 2);
            ///MIN FIRST LINE
            if(MinLine > pos)
            {
                std::cout << "POS " << pos << std::endl;
                return failure;
            }

            ///REQ  METHOD PATH HTTP/1.1
            ///RSP  HTTP/1.1 CODE DESC
            if(strcasecmp("HTTP/1.", sLine.substr(0,7).c_str()) == 0)
            {
                ///RSP
                std::cout << "RESPONSE: " << sLine << std::endl;
                return failure;
            }
            else
            {
                size_t first = sLine.find(" ");
                if( first == std::string::npos)
                {
                    return failure;
                }

                std::string sMethod = sLine.substr(0, first);
                size_t second = sLine.find(" ", first + 1);
                if( second == std::string::npos)
                {
                    return failure;
                }

                std::string path = sLine.substr(first + 1, second - first - 1);
                req_.reset(new HttpRequest(path));
                if(strcasecmp("GET",sMethod.c_str()) == 0)
                    req_->method() = GET;
                else if(strcasecmp("POST",sMethod.c_str()) == 0)
                    req_->method() = POST;
                else if(strcasecmp("HEAD",sMethod.c_str()) == 0)
                    req_->method() = HEAD;
                else
                {
                    return failure;
                }
            }

            parse_func_ = &HttpParser::parse_headers;
            return (this->*parse_func_) ();
        }
        else
        {
            if(cache_.size() > MaxUrl)
                return failure;
        }

        return indeterminate;
    }

    static size_t trimleft(const std::string& s)
    {
        size_t i = 0;
        while(i < s.size())
        {
            if(s.at(i) == ' ')
                i++;
            else
                return i;
        }

        return i;
    }

    static size_t trimright(const std::string& s)
    {
        size_t i = s.size();
        while(--i)
        {
            if(s.at(i) != ' ')
                return i;
        }

        return i;
    }

    static int stringtoint(const char* str)
    {
        char* end = NULL;
        return strtol(str, &end, 16);
    }

private:
    typedef tribool (HttpParser::*ParseFunction)();
    ParseFunction   parse_func_;
    size_t          content_length_;

    std::shared_ptr<HttpRequest>      req_;
    std::string     cache_;
};

} // namespace http
} // namespace natsu
#endif // HTTPPARSER_HPP_INCLUDED
