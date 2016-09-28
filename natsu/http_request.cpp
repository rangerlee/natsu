#include "http_request.h"
#include "natsu_string.h"
#include <curl/curl.h>

namespace natsu {
namespace http {

HttpRequest::HttpRequest(const std::string& doc)
: path_(doc) {}

std::string HttpRequest::header(const std::string& k)
{
    auto it = header_.find(k);
    if( it != header_.end())
    {
        return it->second;
    }

    return "";
}

void HttpRequest::header(const std::string& k, const std::string& v)
{
	header_[k] = v;
}

std::string HttpRequest::query()
{
	size_t pos_p = path_.find(';');
    size_t pos_f = path_.find('#');
    size_t pos_q = path_.find('?');
    if(pos_q == std::string::npos)
        return "";

    size_t pos = std::min(pos_f,pos_p);
    if(pos == std::string::npos)
        return path_.substr(pos_q + 1);

    if(pos > pos_q)
        return path_.substr(pos_q + 1, pos - pos_q - 1);

    return path_.substr(pos_q + 1, std::max(pos_f,pos_p) - pos_q - 1);
}

std::string HttpRequest::data(const std::string& q)
{
	if(GET == method_)
	{
        std::string str = query();
    	std::vector<std::string> tokens;
    	natsu::tokenize(str, tokens, "&");
    	for (size_t i = 0; i < tokens.size(); ++i)
    	{
    		std::string k,v;
    		size_t pos = tokens[i].find("=");
    		if(pos != std::string::npos)
    		{
    			k = tokens[i].substr(0, pos);
    			v = tokens[i].substr(pos + 1);
    			v = curl_unescape(v.c_str(), v.size());
    		}
    		else
    		{
    			k = tokens[i].substr(0, pos - 1);
    		}

    		if(k == q)
    		{
    			return v;
    		}
    	}
    }
    else if(POST == method_)
    {
        if(header("content-type").compare("application/x-www-from-urlencoded") == 0)
        {
            std::vector<std::string> tokens;
            natsu::tokenize(body_, tokens, "&");
            for (size_t i = 0; i < tokens.size(); ++i)
            {
                std::string k,v;
                size_t pos = tokens[i].find("=");
                if(pos != std::string::npos)
                {
                    k = tokens[i].substr(0, pos);
                    v = tokens[i].substr(pos + 1);
                    v = curl_unescape(v.c_str(), v.size());
                }
                else
                {
                    k = tokens[i].substr(0, pos - 1);
                }

                if(k == q)
                {
                    return v;
                }
            }
        }
        else
        {
            //@todo list
            //form-data
            //multipart/form-data
        }
    }

	return "";
}

std::string HttpRequest::parameters()
{
	size_t pos_p = path_.find(';');
    size_t pos_f = path_.find('#');
    size_t pos_q = path_.find('?');
    if(pos_p == std::string::npos)
        return "";

    size_t pos = std::min(pos_f,pos_q);
    if(pos == std::string::npos)
        return path_.substr(pos_p + 1);

    if(pos > pos_p)
        return path_.substr(pos_p + 1, pos - pos_p - 1);

    return path_.substr(pos_p + 1, std::max(pos_f,pos_q) - pos_p - 1);
}

std::string HttpRequest::fragment()
{
	size_t pos_p = path_.find(';');
    size_t pos_f = path_.find('#');
    size_t pos_q = path_.find('?');
    if(pos_f == std::string::npos)
        return "";

    size_t pos = std::min(pos_p,pos_q);
    if(pos == std::string::npos)
        return path_.substr(pos_f + 1);

    if(pos > pos_f)
        return path_.substr(pos_f + 1, pos - pos_f - 1);

    return path_.substr(pos_f + 1, std::max(pos_p,pos_q) - pos_f - 1);
}

std::string HttpRequest::document()
{
	size_t pos_p = path_.find(';');
    size_t pos_f = path_.find('#');
    size_t pos_q = path_.find('?');
    if((pos_p == std::string::npos) && (pos_f == std::string::npos) && (pos_q == std::string::npos))
        return path_;

    size_t pos = std::min(pos_p, std::min(pos_f,pos_q));
    return path_.substr(0,pos);
}



}}
