#include "natsu_rpc.h"
#include "natsu_config.h"
#include "format.h"
#include <curl/curl.h>

namespace natsu{

std::map<uint64_t, std::shared_ptr<co_chan<MessagePtr>>> kClientRpcChannel;
std::map<std::string, std::shared_ptr<co_chan<MessagePtr>>> kServiceRpcChannel;
std::map<std::string, std::function<std::shared_ptr<Message>(std::shared_ptr<Message>)>> kRpcMethod;

enum { PACKET_SIZE_MAX = 4096, };
const int kHeaderLen = 12;
const int kMaxChannelSize = 1024;

int64_t generate()
{
	static natsu::SnowFlake kSnowFlake;
	return kSnowFlake.generate();
}

// proto format, payload was protobuf
// now checksum data is used adler32 (depen on zlib library)
/*
----------------------------------------------------------
|               pkt len(4)          	                 |
|--------------------------------------------------------|
|				request id (8)                           |
|--------------------------------------------------------|
|               playload name len(4)                     |
|               playload name  			                 |
|--------------------------------------------------------|
|               payload data (protobuf) 	             |
|--------------------------------------------------------|
|               checksum(4)        		                 |
|--------------------------------------------------------|
*/

// class RpcPacket
// unpack stream to message and pack message to stream
class RpcPacketParser
{
public:
	// WARN: Decode() & GetMessage() is not threadsafe
	// 	     caller need locked before called
	
	// unpcak tcp stream data
	// when this called, then can call GetMessage()
	void Decode(const char* data, size_t len)
	{
		buffer_.append(data, len);
		while (ParseStream()){};
	}

	// get unpacked google protobuf message poniter
	// if there's no message decoded, it will return Null poniter
	// so caller need called by loop and judge the result
	// example: while( message = decoder.GetMessage() != NULL)
	//		    {   ......   }
	MessagePtr GetMessage(int64_t& rid)
	{
		std::map<int64_t, Message*>::iterator it = message_.begin();
		if (it != message_.end())
		{
			rid = it->first;
			google::protobuf::Message* message = it->second;
			message_.erase(it);
			return MessagePtr(message);
		}

		return MessagePtr();
	}
			
public:
	// encode a google protobuf message to stream
	// this function is static because it's no dependcy
	// NOTE: stream poniter returned will not be Null, but can be empty
	static std::shared_ptr<std::string> Encode(const Message* message, int64_t rid)
	{
		std::shared_ptr<std::string> result(new std::string());
		result->resize(sizeof(int32_t));
		result->append(reinterpret_cast<char*>(&rid), sizeof rid);
		const std::string& type_name = message->GetTypeName();
		int32_t name_len = static_cast<int32_t>(type_name.size() + 1);
		result->append(reinterpret_cast<char*>(&name_len), sizeof name_len);
		result->append(type_name.c_str(), name_len);
		bool succeed = message->AppendToString(result.get());
		if (succeed)
		{
			const char* begin = result->c_str() + kHeaderLen;
			int32_t checkSum = adler32(1, reinterpret_cast<const Bytef*>(begin), result->size() - kHeaderLen);
			result->append(reinterpret_cast<char*>(&checkSum), sizeof checkSum);
			int32_t len = result->size() - kHeaderLen;
			std::copy(reinterpret_cast<char*>(&len), reinterpret_cast<char*>(&len) + sizeof len, result->begin());
		}
		else
		{
			result->clear();
		}

		return result;
	}

private:
	bool ParseStream()
	{
		if (buffer_.size() < 12)
			return false;

		unsigned int packetlen = 0;
		::memcpy(&packetlen, buffer_.c_str(), sizeof(packetlen));
		if (buffer_.size() < packetlen)
		{
			return false;
		}

		if (packetlen > PACKET_SIZE_MAX)
		{
			buffer_.clear();
			return false;
		}

		int64_t rid = 0;
		::memcpy(&rid, buffer_.c_str() + sizeof(int32_t), sizeof(rid));

		Message* message = DecodeMessage(buffer_.c_str() + kHeaderLen, packetlen);
		buffer_.erase(0, packetlen + kHeaderLen);

		if (message)
			message_[rid] = message;
		return true;
	}

	inline Message* DecodeMessage(const char* buf, size_t bufferlength)
	{
		google::protobuf::Message* result = NULL;
		int32_t len = static_cast<int32_t>(bufferlength);
		if (len >= 10)
		{
			int32_t expected_checksum = 0;
			::memcpy(&expected_checksum, buf + bufferlength - kHeaderLen, sizeof(expected_checksum));
			const char* begin = buf;
			int32_t checksum = adler32(1, reinterpret_cast<const Bytef*>(begin), len - kHeaderLen);
			if (checksum == expected_checksum)
			{
				int32_t name_len = 0;
				::memcpy(&name_len, buf, sizeof(name_len));

				if (name_len >= 2 && name_len <= len - 2 * kHeaderLen)
				{
					std::string type_name(buf + kHeaderLen, buf + kHeaderLen + name_len - 1);
					Message* message = CreateMessage(type_name);
					if (message)
					{
						const char* data = buf + kHeaderLen + name_len;
						int32_t data_len = len - name_len - 2 * kHeaderLen;
						if (message->ParseFromArray(data, data_len))
						{
							result = message;
						}
						else
						{
							// parse error 
							delete message;
						}
					}//else { // unknown message type }

				}//else { // invalid name len }

			}//else { // check sum error }
		}

		return result;
	}

	// Create a google protobuf Message by message name
	// if there is no message named like typeName, it will return NULL 
	Message* CreateMessage(const std::string& typeName)
	{
		Message* message = NULL;
		const google::protobuf::Descriptor* descriptor = google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
		if (descriptor)
		{
			const Message* prototype = google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
			if (prototype)
			{
				message = prototype->New();
			}
		}
				
		return message;
	}

private:
	std::string buffer_;
	std::map<int64_t, Message*> message_;
};


class EtcdProvider
{
public:
	//provide_service_etcd
	void provide_service_etcd(const std::string& servicename, const std::string& etcdaddr)
	{
		int sock = socket(AF_INET, SOCK_STREAM, 0);
	    int rep = 1;
	    setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &rep, sizeof(rep));

	    while(true)
	    {
	    	co_sleep(3000);
	    	int port  = 5000 + rand() % 5000;
	    	sockaddr_in addr;
		    addr.sin_family = AF_INET;
		    addr.sin_port = htons(port);
		    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
		    socklen_t len = sizeof(addr);
		    if (-1 == bind(sock, (sockaddr*)&addr, len))
		    {
		    	fprintf(stderr, "rpc bind [%d] error: %s\n", port, strerror(errno));
		    	close(sock);
		        sock = socket(AF_INET, SOCK_STREAM, 0);
	    		setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &rep, sizeof(rep));
		        continue;
		    }

		    if (-1 == ::listen(sock, 5))
		    {
		        fprintf(stderr, "rpc listen error: %s\n", strerror(errno));
		        close(sock);
		        sock = socket(AF_INET, SOCK_STREAM, 0);
	    		setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &rep, sizeof(rep));
		        continue;
		    }

		    NatsuConfig::config("rpc_instance_id", format("%lld", generate()));
		    co_timer_add(std::chrono::seconds(10), std::bind(&EtcdProvider::register_to_etcd, this, servicename, etcdaddr, port));
			break;
		}

	    while(true)
	    {
	        sockaddr_in addr;
	        socklen_t len = sizeof(addr);
	        int sockfd = accept(sock, (sockaddr*)&addr, &len);
	        if (sockfd == -1)
	        {
	            if (EAGAIN == errno || EINTR == errno)
	                continue;

	            fprintf(stderr, "accept error: %s\n", strerror(errno));
	            break ;
	        }

	        go std::bind(&EtcdProvider::handle, this, sockfd);
	    }

	    close(sock);
	}

private:
	void register_to_etcd(const std::string& sname, const std::string& addr, unsigned short port)
	{
		//curl -L http://127.0.0.1:2379/v2/keys/foo -XPUT -d value=bar -d ttl=5
		std::string request_url = format("http://%s/v2/keys/natsu/%s/provider/%s", addr.c_str(), sname.c_str(),
									NatsuConfig::config("rpc_instance_id").c_str());
		fprintf(stderr, "rpc register: %s\n", request_url.c_str());
		CURL* curl = curl_easy_init();
		if (curl) {
		    curl_easy_setopt(curl, CURLOPT_URL, request_url.c_str());  
		    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT"); /* !!! */
		    std::string data = format("value=%s:%d&ttl=15",NatsuConfig::config("local_ipv4").c_str(), port);
		    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

		    CURLcode res = curl_easy_perform(curl);
		    if(res != CURLE_OK)
	        {
	            std::string err = curl_easy_strerror(res);
	            fprintf(stderr, "request etcd error : %s\n", err.c_str());
	        }

		    curl_easy_cleanup(curl);
		}

		co_timer_add(std::chrono::seconds(10), std::bind(&EtcdProvider::register_to_etcd, this, sname, addr, port));
	}

	void handle(int sockfd)
	{
		co_chan<std::shared_ptr<std::string>> channel_write(kMaxChannelSize);

		go std::bind(&EtcdProvider::sendrpc, this, sockfd, channel_write);

		char buf[1024];
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
	        	rpc_parser_.Decode(buf, n);
	            MessagePtr message;
				while( true )
				{
					int64_t rid = 0;
					message = rpc_parser_.GetMessage(rid);
					if(message.get())
						go std::bind(&EtcdProvider::handle_parse, this, message, rid, channel_write);
					else
						break;
				}
	            
	        }//else
	    }
	}

	void handle_parse(MessagePtr message, int64_t rid, co_chan<std::shared_ptr<std::string>>& channel_write)
	{
		if(!message) return;

		if(kRpcMethod.find(message->GetTypeName()) != kRpcMethod.end())
		{
			MessagePtr resp = kRpcMethod[message->GetTypeName()](message);
			std::shared_ptr<std::string> buf = rpc_parser_.Encode(resp.get(), rid);
			channel_write << buf;
		}		
	}

	void sendrpc(int sockfd, co_chan<std::shared_ptr<std::string>>& channel_write)
	{
		while(true)
		{
			std::shared_ptr<std::string> buf;
			channel_write >> buf;
			if(!buf) continue;

            size_t pos = 0;
            size_t len = buf->size();
            while(pos < len)
            {
                ssize_t n = write(sockfd, buf->c_str() + pos, buf->size() - pos);
                if(n == -1) 
                {
                    break;
                }

                pos += n;
            }
		}
	}

private:
	RpcPacketParser rpc_parser_; 
};


void produce_service_etcd(const std::string& servicename, const std::string& etcdaddr)
{
	//curl -L http://127.0.0.1:2379/v2/keys/foo
}


void provide_service(const std::string& servicename, const std::string& etcdaddr)
{
	static std::map<std::string, std::shared_ptr<EtcdProvider>> kProvider;
	kProvider[servicename] = std::shared_ptr<EtcdProvider>(new EtcdProvider());
	go std::bind(&EtcdProvider::provide_service_etcd, kProvider[servicename], servicename, etcdaddr);
}

void produce_service(const std::string& servicename, const std::string& etcdaddr)
{
	//go produce_service_etcd(servicename, etcdaddr);
}

}