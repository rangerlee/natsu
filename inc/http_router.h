#ifndef HTTP_ROUTER_H_
#define HTTP_ROUTER_H_

#include <string>
#include <memory>
#include <functional>
#include "http_request.h"
#include "http_response.h"

namespace natsu {
namespace http {
    

class HttpRouter
{
    typedef	std::function<void(std::shared_ptr<HttpRequest>,std::shared_ptr<HttpResponse>)> Handler;
public:
    HttpRouter();
    static HttpRouter& instance();
    void register_handler(const std::string& pattern, Handler h, Method m);
    void handle(std::shared_ptr<HttpRequest>,std::shared_ptr<HttpResponse>);

private:
    class HttpRouterImpl;
    std::shared_ptr<HttpRouterImpl> router_;
};

}}

#endif
