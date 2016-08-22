#ifndef NATSU_APP_H_
#define NATSU_APP_H_

namespace natsu {

class Inject
{
public:
    void before(std::shared_ptr<HttpRequest>,std::shared_ptr<HttpResponse>) {}
    void after(std::shared_ptr<HttpRequest>,std::shared_ptr<HttpResponse>) {}
    void fail(std::shared_ptr<HttpRequest>,std::shared_ptr<HttpResponse>) {}
};

template<Inject* I = NULL>
class NatsuApp
{
public:
    NatsuApp();
    virtual ~NatsuApp();

    bool listen(unsigned short port);
    void run();

private:
    void handle(int sockfd);
    void wait();

private:
    Inject* inject_ = I;
    int sock_;
};

}
#endif
