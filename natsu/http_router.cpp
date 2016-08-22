#include "http_router.h"

namespace natsu {
namespace http {

class HttpRouter::HttpRouterImpl
{
public:
    void register_handler(const std::string& pattern, Handler& handle, Method method)
    {
        switch (method)
        {
        case GET:
            handle_get_[pattern] = handle;
            break;

        case POST:
            handle_post_[pattern] = handle;
            break;

        default:
            break;
        }
    }

    void handle(std::shared_ptr<HttpRequest> req,std::shared_ptr<HttpResponse> resp)
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

    void handle(std::map<std::string, Handler>& h,
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

private:
    std::map<std::string, Handler> handle_get_;
    std::map<std::string, Handler> handle_post_;
};


HttpRouter::HttpRouter()
{
    router_.reset(new HttpRouterImpl);
}

HttpRouter& HttpRouter::instance()
{
    static HttpRouter instance;
    return instance;
}

void HttpRouter::register_handler(const std::string& pattern, Handler h, Method method)
{
    router_->register_handler(pattern, h, method);
}

void HttpRouter::handle(std::shared_ptr<HttpRequest> req,std::shared_ptr<HttpResponse> resp)
{
    router_->handle(req, resp);
}


}}
