#include "natsu_app.h"
#include "coroutine.h"
#include "http_parser.h"
#include "http_router.h"

namespace natsu
{

NatsuApp::NatsuApp(std::shared_ptr<natsu::Inject> inject)
{
    inject_ = inject;
}

NatsuApp::~NatsuApp()
{
    
}


void NatsuApp::listen(unsigned short port)
{
    go std::bind(&natsu::NatsuApp::wait, this, port);
    co_sched.RunUntilNoTask();
}

void NatsuApp::wait(unsigned short port)
{
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    socklen_t len = sizeof(addr);
    if (-1 == bind(sock_, (sockaddr*)&addr, len))
    {
        close(sock_);
        return ;
    }

    if (-1 == ::listen(sock_, 5))
    {
        close(sock_);
        return ;
    }

    int rep = 1;
    setsockopt( sock_, SOL_SOCKET, SO_REUSEADDR, &rep, sizeof(rep) );

    while(true)
    {
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int sockfd = accept(sock_, (sockaddr*)&addr, &len);
        if (sockfd == -1)
        {
            if (EAGAIN == errno || EINTR == errno)
                continue;

            fprintf(stderr, "accept error:%s\n", strerror(errno));
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
                puts(buf.c_str());
                size_t n = write(sockfd, buf.c_str(), buf.size());
                if(n != buf.size()) {}
                close(sockfd);
            }
            break;

            default:
                break;

            }
        }
    }
}


}
