#include "natsu_app.h"
#include "coroutine.h"
#include "http_parser.h"
#include "http_router.h"
#include "natsu_config.h"
#include "natsu_rpc.h"

natsu::NatsuConfig kNatsuConfig;

namespace natsu
{

NatsuApp::NatsuApp(std::shared_ptr<natsu::Inject> inject)
{
    inject_ = inject;
}

NatsuApp::~NatsuApp()
{
    
}

void NatsuApp::listen(const std::string& ip, unsigned short port)
{
    NatsuConfig::config("local_ipv4", ip);
    go std::bind(&natsu::NatsuApp::wait, this, port);
    co_sched.RunUntilNoTask();
}

void NatsuApp::wait(unsigned short port)
{
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    int rep = 1;
    setsockopt( sock_, SOL_SOCKET, SO_REUSEADDR, &rep, sizeof(rep) );

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    socklen_t len = sizeof(addr);
    if (-1 == bind(sock_, (sockaddr*)&addr, len))
    {
        fprintf(stderr, "bind error: %s\n", strerror(errno));
        close(sock_);
        return ;
    }

    if (-1 == ::listen(sock_, 5))
    {
        fprintf(stderr, "listen error: %s\n", strerror(errno));
        close(sock_);
        return ;
    }

    while(true)
    {
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int sockfd = accept(sock_, (sockaddr*)&addr, &len);
        if (sockfd == -1)
        {
            if (EAGAIN == errno || EINTR == errno)
                continue;

            fprintf(stderr, "accept error: %s\n", strerror(errno));
            break ;
        }

        go std::bind(&natsu::NatsuApp::handle, this, sockfd);
    }

    close(sock_);
}

void NatsuApp::handle(int sockfd)
{
    char buf[1024];
    natsu::http::HttpParser parser;
    while(true)
    {
        int n = read(sockfd, buf, sizeof(buf));
        if (n == -1)
        {
            if (EAGAIN == errno || EINTR == errno)
                continue;

            close(sockfd);
            break;
        }
        else if (n == 0)
        {
            close(sockfd);
            break;
        }
        else
        {
            natsu::tribool ret = parser.parse(buf, n);
            switch(ret)
            {
            case natsu::failure:
            {
                close(sockfd);
            }
            break;

            case natsu::success:
            {
                std::shared_ptr<natsu::http::HttpResponse> resp(new natsu::http::HttpResponse());
                try
                {
                    std::shared_ptr<natsu::http::HttpRequest> req = parser.request();
                    if(inject_) inject_->before(req, resp);
                    if(resp->empty())
                    {
                        natsu::http::HttpRouter::instance().handle(req, resp);
                        if(inject_) inject_->after(req, resp);
                    }
                }
                catch(...)
                {
                    if(inject_)
                        inject_->fail(parser.request(), resp);
                    else
                        resp->response(500);
                }

                std::string buf = resp->str();
                size_t pos = 0;
                size_t len = buf.size();
                while(pos < len)
                {
                    ssize_t n = write(sockfd, buf.c_str() + pos, buf.size() - pos);
                    if(n == -1) 
                    {
                        break;
                    }

                    pos += n;
                }
                
                close(sockfd);
            }
            break;

            default:
                break;

            }
        }
    }
}

void NatsuApp::register_handler(const std::string& pattern, 
		std::function<void(std::shared_ptr<natsu::http::HttpRequest>,std::shared_ptr<natsu::http::HttpResponse>)> h, natsu::http::Method m)
{
	natsu::http::HttpRouter::instance().register_handler(pattern, h, m);
}

void NatsuApp::provide_service(const std::string& servicename, const std::string& etcdaddr)
{
    natsu::provide_service(servicename, etcdaddr);
}

void NatsuApp::produce_service(const std::string& servicename, const std::string& etcdaddr)
{
    natsu::produce_service(servicename, etcdaddr);
}


}
