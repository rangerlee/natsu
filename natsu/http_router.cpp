#include "http_router.h"

namespace natsu {
namespace http {

HttpRouter::HttpRouter()
{
}

HttpRouter& HttpRouter::instance()
{
    static HttpRouter instance;
    return instance;
}

void HttpRouter::register_handler(const std::string& pattern, Handler h, Method method)
{
    switch (method)
    {
        case GET:
            handle_get_[pattern] = h;
            break;

        case POST:
            handle_post_[pattern] = h;
            break;

        default:
            break;
    }
}

void HttpRouter::handle(std::shared_ptr<HttpRequest> req,std::shared_ptr<HttpResponse> resp)
{
    //@todo regex
    switch(req->method)
    {
        case GET:
            handle(handle_get_, req, resp);
            break;

        case POST:
            handle(handle_post_, req, resp);
            break;

        default:
            break;
    }
}

void HttpRouter::handle(std::map<std::string, Handler>& h,
                std::shared_ptr<HttpRequest> req,std::shared_ptr<HttpResponse> resp)
{
    if(h.find(req->path) != h.end())
    {
        (h[req->path])(req, resp);
    }
    else
    {
        resp->response(404);
    }
}


}}
