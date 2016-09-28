#ifndef HTTP_REQUEST_H_
#define HTTP_REQUEST_H_

#include <string>
#include <map>

namespace natsu {
namespace http {

enum Method
{
    PUT,
    GET,
    POST,
	DELETE,
};

class HttpRequest
{
public:
	HttpRequest(const std::string& doc);
    std::string header(const std::string& k);
    void header(const std::string& k, const std::string& v);
    std::string query();
	
	//data() for GET is query
	//data() for POST is form-data
    std::string data(const std::string& k);
	
    std::string parameters();
    std::string fragment();
    std::string document();
    Method& method() { return method_; }
    std::string& body() { return body_; }

private:
    Method method_;
    std::string path_;
    std::map<std::string,std::string> header_;
    std::string body_;
};


}}

#endif
