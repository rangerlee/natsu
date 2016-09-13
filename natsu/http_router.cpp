#include "http_router.h"
#include "natsu_string.h"

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
            {
                handle_get_[pattern] = h;
                std::string regex_str = "^" + pattern;
                std::string r = natsu::replace_all(regex_str, "*", "\\w*");
                r = natsu::replace_all(r, "+", "\\w+");
                r = natsu::replace_all(r, "{", "\\w{");
                regex_get_.add_rule(pattern, r);
            }
            break;

        case POST:
            {
                handle_post_[pattern] = h;
                std::string regex_str = "^" + pattern;
                std::string r = natsu::replace_all(regex_str, "*", "\\w*");
                r = natsu::replace_all(r, "+", "\\w+");
                r = natsu::replace_all(r, "{", "\\w{");
                regex_post_.add_rule(pattern, r);
            }
            break;

        default:
            break;
    }
}

void HttpRouter::handle(std::shared_ptr<HttpRequest> req,std::shared_ptr<HttpResponse> resp)
{
    switch(req->method())
    {
        case GET:
            handle(handle_get_, regex_get_, req, resp);
            break;

        case POST:
            handle(handle_post_, regex_post_, req, resp);
            break;

        default:
            break;
    }
}

void HttpRouter::handle(std::map<std::string, Handler>& h, PcreRegex& regex,
                std::shared_ptr<HttpRequest> req,std::shared_ptr<HttpResponse> resp)
{
    std::vector<natsu::PcreRegex::MatchResult> result = regex.match(req->document().c_str());
    if(result.size() && result[0].name.size())
    {
        (h[result[0].name])(req, resp);
    }
    else
    {
        resp->response(404);
    }
}


}}
