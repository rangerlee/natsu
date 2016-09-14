#ifndef NATSU_RPC_H_
#define NATSU_RPC_H_
#include "coroutine.h"
#include <map>
#include <string>
#include "channel.h"
#include "natsu_snowflake.h"

#ifdef __linux__
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/unknown_field_set.h>

#include <zlib.h>

typedef google::protobuf::Message Message;
typedef std::shared_ptr<Message> MessagePtr;

namespace natsu {

extern std::map<uint64_t, std::shared_ptr<co_chan<MessagePtr>>> kClientRpcChannel;
extern std::map<std::string, std::shared_ptr<co_chan<MessagePtr>>> kServiceRpcChannel;
extern std::map<std::string, std::function<MessagePtr(MessagePtr)>> kRpcMethod;

void provide_service(const std::string& servicename, const std::string& etcdaddr);
void produce_service(const std::string& servicename, const std::string& etcdaddr);
int64_t generate();

}

#define NATSU_RPC_PRODUCE(S, M, REQ, RSP) \
	inline std::shared_ptr<RSP> Call##M(const std::shared_ptr<REQ>& req){\
		if(natsu::kServiceRpcChannel.find(S) == natsu::kServiceRpcChannel.end()) {\
			return std::shared_ptr<RSP>();\
		}\
		(*natsu::kServiceRpcChannel[S]) >> req;\
		std::shared_ptr<co_chan<MessagePtr>> newchannel(new co_chan<MessagePtr>);\
		uint64_t id = generate();\
		natsu::kClientRpcChannel[id] = newchannel;\
		(*newchannel) << rsp;\
		natsu::kClientRpcChannel.erase(id);\
	}

#define NATSU_RPC_PROVIDE(M, REQ, RSP) \
	inline std::shared_ptr<RSP> Do##M(const std::shared_ptr<REQ>& req);\
	natsu::kRpcMethod[#REQ] = Do##M;


#endif
