#include <iostream>
#include "natsu.h"
#include "natsu_redis.h"
using namespace std;

void read_from_redis(std::shared_ptr<natsu::http::HttpRequest>,std::shared_ptr<natsu::http::HttpResponse> rsp)
{
	natsu::RedisError e;
	std::shared_ptr<natsu::RedisClient> ins = natsu::newRedisInstance("im");
	std::string s = ins->get("test", e);
	if(e.ok())
		rsp->response(s, "text/plain");
	else
		rsp->response(404);
}


int main()
{
	std::string config = "{\"server_addr\": \"192.168.85.217\", \"server_port\":6379}";
	std::cout << "test program" << std::endl;
    natsu::redisInit("im",config,5);
  	std::cout << "init redis poll" << std::endl;
	natsu::NatsuApp app;
	app.register_handler("/", read_from_redis);
	app.listen(9000);
    return 0;
}
