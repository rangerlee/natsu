#ifndef NATSU_APP_H_
#define NATSU_APP_H_

#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <memory>

#include "http_request.h"
#include "http_response.h"

namespace natsu {

class Inject
{
public:
    virtual void before(std::shared_ptr<natsu::http::HttpRequest>&,std::shared_ptr<natsu::http::HttpResponse>&) {}
    virtual void after(std::shared_ptr<natsu::http::HttpRequest>&,std::shared_ptr<natsu::http::HttpResponse>&) {}
    virtual void fail(std::shared_ptr<natsu::http::HttpRequest>&,std::shared_ptr<natsu::http::HttpResponse>&) {}
};


class NatsuApp
{
public:
    NatsuApp(std::shared_ptr<natsu::Inject> I = std::make_shared<Inject>());
    virtual ~NatsuApp();

    void listen(unsigned short port);
	void register_handler(const std::string& pattern, 
		std::function<void(std::shared_ptr<natsu::http::HttpRequest>,std::shared_ptr<natsu::http::HttpResponse>)> h, natsu::http::Method m = natsu::http::GET);

private:
    void handle(int sockfd);
    void wait(unsigned short port);

private:
    std::shared_ptr<natsu::Inject> inject_;
    int sock_;
};

}
#endif
