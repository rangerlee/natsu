#include <iostream>
#include "natsu.h"
#include "natsu_redis.h"
#include "natsu_rpc.h"
using namespace std;

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/unknown_field_set.h>

#include "rpc.pb.h"

void read_from_redis(std::shared_ptr<natsu::http::HttpRequest>,std::shared_ptr<natsu::http::HttpResponse> rsp)
{
	//natsu::RedisError e;
	//std::shared_ptr<natsu::RedisClient> ins = natsu::newRedisInstance("im");
	//std::string s = ins->get("test", e);
	//if(e.ok())
	//	rsp->response(s, "text/plain");
	//else
	//	rsp->response(404);
	std::shared_ptr<rpc::req> r(new rpc::req);
	r->set_data("hello"); 
	std::shared_ptr<rpc::rsp> rs = natsu::invoke<rpc::req, rpc::rsp>("im", r);
	if(rs)
		rsp->response(rs->data());
	else 
		rsp->response(500);
}	



std::shared_ptr<rpc::rsp> DoRPC(const std::shared_ptr<rpc::req> r)
{
	std::shared_ptr<rpc::rsp> q(new rpc::rsp);
	q->set_data("world"); 
	printf("DoRPC\n");
	return q;
}


int main()
{
	NATSU_RPC_PROVIDE(DoRPC, rpc::req, rpc::rsp)

	std::string config = "{\"server_addr\": \"192.168.85.217\", \"server_port\":6379}";
	std::cout << "test program" << std::endl;
	natsu::redisInit("im",config,5);
  	std::cout << "init redis poll" << std::endl;
	natsu::NatsuApp app;
	//app.register_rpc(DoRPC);
	app.provide_service("im", "127.0.0.1:2379");
	app.produce_service("im", "127.0.0.1:2379");
	app.register_handler("/", read_from_redis);
	app.listen("127.0.0.1", 9000);
    return 0;
}
