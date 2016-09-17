#ifndef NATSU_RPC_H_
#define NATSU_RPC_H_

#include <map>
#include <string>
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

void provide_service(const std::string& servicename, const std::string& etcdaddr);
void produce_service(const std::string& servicename, const std::string& etcdaddr);
MessagePtr invoke_rpc(const std::string& servicename, MessagePtr);
void register_rpc_handler(const std::string& name, std::function<MessagePtr(MessagePtr)> func);

}

#define NATSU_RPC_PROVIDE(NAME, REQ, RSP)\
	natsu::register_rpc_handler(REQ().GetTypeName(), [](MessagePtr _req_) -> MessagePtr{\
	MessagePtr _rsp_;\
	std::shared_ptr<REQ> req = std::dynamic_pointer_cast<REQ>(_req_);\
	std::shared_ptr<RSP> rsp = NAME(req);\
	return std::dynamic_pointer_cast<Message>(rsp);\
	});

#endif
