#ifndef HTTP_ROUTER_H_
#define HTTP_ROUTER_H_

#include <string>
#include <memory>
#include <functional>
#include "http_request.h"
#include "http_response.h"

namespace natsu {
namespace http {
 
typedef	std::function<void(std::shared_ptr<HttpRequest>,std::shared_ptr<HttpResponse>)> Handler; 

class HttpRouter
{
public:
    HttpRouter();
    static HttpRouter& instance();
    void register_handler(const std::string& pattern, Handler h, Method m);
    void handle(std::shared_ptr<HttpRequest>,std::shared_ptr<HttpResponse>);

private:
	void handle(std::map<std::string, Handler>&,
                std::shared_ptr<HttpRequest>,std::shared_ptr<HttpResponse>);

private:
    std::map<std::string, Handler> handle_get_;
    std::map<std::string, Handler> handle_post_;
};

}}

#endif
