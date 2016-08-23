#ifndef HTTP_RESPONSE_H_
#define HTTP_RESPONSE_H_

#include <string>
#include <memory>
#include <map>

namespace natsu {
namespace http {

class HttpResponse
{
public:
    HttpResponse();

    void header(const std::string& key, const std::string& value);
    void response(const std::string& resp, const std::string& ct = "");
    void response(const std::map<std::string,std::string>& resp);
    void response(int code);

    void redirect(const std::string& url);

public:
    std::string str(); 
    bool empty();

private:
    class HttpResponseImpl;
    std::shared_ptr<HttpResponseImpl> response_;
};


}}

#endif
