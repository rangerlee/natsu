#ifndef HTTP_REQUEST_H_
#define HTTP_REQUEST_H_

#include <string>
#include <map>

namespace natsu {
namespace http {

enum Method
{
    HEAD,
    GET,
    POST
};

class HttpRequest
{
public:
    //std::string header(const std::string& k);
    //std::map<std::string,std::string> query();

public:
    Method method;
    std::string path;
    std::map<std::string,std::string> header;
    std::string body;
};


}}

#endif
