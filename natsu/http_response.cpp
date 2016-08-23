#include "http_response.h"
#include <sstream>

namespace natsu {
namespace http {


class HttpResponse::HttpResponseImpl
{
public:
    HttpResponseImpl()
    {
        code_ = 200;
    }

    std::string make()
    {
        std::stringstream stream;
        switch(code_)
        {
        case 200:
            stream << "HTTP/1.1 " << code_ << " OK\r\n";
            break;

        case 302:
            stream << "HTTP/1.1 " << code_ << " Temporarily Moved\r\n";
            break;

        case 404:
            stream << "HTTP/1.1 " << code_ << " Not Found\r\n";
            break;

        case 500:
            stream << "HTTP/1.1 " << code_ << " Internal Server Error\r\n";
            break;
        }

        header_["Connection"] = "close";
        auto it = header_.find("Content-Length");
        if(it != header_.end()) header_.erase(it);
        for(auto it = header_.begin(); it != header_.end(); ++it)
        {
            stream << it->first << ": " << it->second << "\r\n";
        }

        stream << "Content-Length: " << body_.size() << "\r\n";
        stream << "\r\n";
        stream << body_;

        return stream.str();
    }

    int code_;
    std::map<std::string,std::string> header_;
    std::string body_;
};


HttpResponse::HttpResponse()
{
    response_.reset(new HttpResponseImpl);
}

void HttpResponse::header(const std::string& key, const std::string& value)
{
    response_->header_[key] = value;
}

void HttpResponse::response(const std::string& resp, const std::string& ct)
{
    response_->header_["Content-Type"] = ct;
    response_->body_ = resp;
}

void HttpResponse::response(const std::map<std::string,std::string>& resp)
{
    std::string r;
    auto it = resp.begin();
    while(true)
    {
        r.append(it->first);
        r.append("=");
        r.append(it->second);
        if(++it == resp.end())
        {
            r.append("&");
        }
        else
        {
            break;
        }
    }

    response(r);
}

void HttpResponse::response(int code)
{
    response_->code_ = code;
    response_->body_.clear();
}

void HttpResponse::redirect(const std::string& u)
{
    response_->code_ = 302;
    response_->header_["Location"] = u;
    response_->body_.clear();
}

std::string HttpResponse::str()
{
    return response_->make();
}

bool HttpResponse::empty()
{
    return response_->code_ == 200 &&
         response_->header_.size() == 0 &&
         response_->body_.size() == 0 ;
}

}}
