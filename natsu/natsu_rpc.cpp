#include "coroutine.h"
#include "natsu_rpc.h"
#include "natsu_config.h"
#include "natsu_snowflake.h"
#include "format.h"
#include "gci-json.h"
#include <curl/curl.h>

namespace natsu
{

struct RpcChannelData
{
    int64_t id;
    MessagePtr msg;
};

std::map<uint64_t, std::shared_ptr<co_chan<MessagePtr>>> kClientRpcChannel;
std::map<std::string, std::shared_ptr<co_chan<RpcChannelData>>> kServiceRpcChannel;
std::map<std::string, std::function<MessagePtr(MessagePtr)>> kRpcMethod;

enum { PACKET_SIZE_MAX = 4096, };
const int kHeaderLen = 12;
const int kMaxChannelSize = 1024;

int64_t generate()
{
    return SnowFlake::instance().generate();
}

void register_rpc_handler(const std::string& name, std::function<MessagePtr(MessagePtr)> func)
{
    kRpcMethod[name] = func;
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
        while (ParseStream()) {};
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
            ::memcpy(&expected_checksum, buf + bufferlength - sizeof(expected_checksum), sizeof(expected_checksum));
            const char* begin = buf;
            int32_t checksum = adler32(1, reinterpret_cast<const Bytef*>(begin), len - sizeof(expected_checksum));
            if (checksum == expected_checksum)
            {
                int32_t name_len = 0;
                ::memcpy(&name_len, buf, sizeof(name_len));

                if (name_len >= 2 && name_len <= len - 8)
                {
                    std::string type_name(buf + sizeof(name_len), name_len);
                    Message* message = CreateMessage(type_name);
                    if (message)
                    {
                        const char* data = buf + sizeof(name_len) + name_len;
                        int32_t data_len = len - name_len - 2 * sizeof(name_len);
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
            else
            {
                fprintf(stderr, "checksum error\n");
            }
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
                co_sleep(3000);
                sock = socket(AF_INET, SOCK_STREAM, 0);
                setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &rep, sizeof(rep));
                continue;
            }

            if (-1 == ::listen(sock, 5))
            {
                fprintf(stderr, "rpc listen error: %s\n", strerror(errno));
                close(sock);
                co_sleep(3000);
                sock = socket(AF_INET, SOCK_STREAM, 0);
                setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &rep, sizeof(rep));
                continue;
            }

            NatsuConfig::config("rpc_instance_id", format("%lld", generate()));
            fprintf(stderr, "etcd provider provide %s\n", NatsuConfig::config("rpc_instance_id").c_str());
            co_timer_add(std::chrono::seconds(1), std::bind(&EtcdProvider::register_to_etcd, this, servicename, etcdaddr, port));
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

        CURL* curl = curl_easy_init();
        if (curl)
        {
            curl_easy_setopt(curl, CURLOPT_URL, request_url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT"); /* !!! */
            std::string data = format("value=%s:%d&ttl=15",NatsuConfig::config("local_ipv4").c_str(), port);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK)
            {
                std::string err = curl_easy_strerror(res);
                fprintf(stderr, "register_to_etcd request etcd error : %s\n", err.c_str());
            }

            curl_easy_cleanup(curl);
        }

        co_timer_add(std::chrono::seconds(10), std::bind(&EtcdProvider::register_to_etcd, this, sname, addr, port));
    }

    void handle(int sockfd)
    {
        fprintf(stderr, "provider accept producer connection %d\n", sockfd);
        co_chan<std::shared_ptr<std::string>> channel_write(kMaxChannelSize);
        co_mutex locker;
        locker.lock();
        go std::bind(&EtcdProvider::sendrpc, this, sockfd, channel_write, locker);

        char buf[1024];
        while(true)
        {
            int n = read(sockfd, buf, sizeof(buf));
            if (n == -1)
            {
                fprintf(stderr, "provider read failed %d\n", errno);
                if (EAGAIN == errno || EINTR == errno)
                    continue;

                break;
            }
            else if (n == 0)
            {
                fprintf(stderr, "provider read timeout\n");
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

            }
        }

        locker.lock();
        close(sockfd);
        fprintf(stderr, "provider find producer lost %d\n", sockfd);
    }

    void handle_parse(MessagePtr message, int64_t rid, co_chan<std::shared_ptr<std::string>>& channel_write)
    {
        fprintf(stderr, "got one rpc request and handle it\n");
        if(!message) return;

        if(kRpcMethod.find(message->GetTypeName()) != kRpcMethod.end())
        {
            std::function<MessagePtr(MessagePtr)> func = kRpcMethod[message->GetTypeName()];
            MessagePtr resp = func(message);
            if(resp)
            {
                std::shared_ptr<std::string> buf = rpc_parser_.Encode(resp.get(), rid);
                channel_write << buf;
            }
            else
            {
                fprintf(stderr, "handle rpc failed\n");
            }
        }
    }

    void sendrpc(int sockfd, co_chan<std::shared_ptr<std::string>>& channel_write, co_mutex locker)
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

        locker.unlock();
    }

private:
    RpcPacketParser rpc_parser_;
};

static std::map<std::string, std::shared_ptr<EtcdProvider>> kProvider;


class EtcdProducer
{
public:
    void produce_service_etcd(const std::string& servicename, const std::string& etcdaddr)
    {
        service_name_ = servicename;
        co_timer_add(std::chrono::seconds(1), std::bind(&EtcdProducer::produce_from_etcd, this, servicename, etcdaddr));
        co_timer_add(std::chrono::seconds(10), std::bind(&EtcdProducer::handle_produce_timeout, this));
    }

    MessagePtr invoke(MessagePtr& ptr)
    {
        uint64_t id = generate();
        RpcChannelData data;
        data.id = id;
        std::shared_ptr<co_chan<MessagePtr>> newchannel(new co_chan<MessagePtr>);
        data.msg = ptr;
        natsu::kClientRpcChannel[id] = newchannel;
        (*natsu::kServiceRpcChannel[service_name_]) << data;
        MessagePtr rsp;
        (*newchannel) >> rsp;
        natsu::kClientRpcChannel.erase(id);
        return rsp;
    }

private:
    void handle_produce_timeout()
    {
        uint64_t diff = 15000;
        diff = diff << 22;
        uint64_t n = generate();
        std::map<uint64_t, std::shared_ptr<co_chan<MessagePtr>>>::iterator it = kClientRpcChannel.begin();
        for(; it != kClientRpcChannel.end(); ++it)
        {
            if((n - it->first) > diff)
            {
                fprintf(stderr, "%lu timeout and return null\n", it->first);
                MessagePtr ptr;
                (*it->second) << ptr;
            }
        }

        co_timer_add(std::chrono::seconds(10), std::bind(&EtcdProducer::handle_produce_timeout, this));
    }

    void produce_from_etcd(const std::string& servicename, const std::string& etcdaddr)
    {
        std::string request_url = format("http://%s/v2/keys/natsu/%s/provider/", etcdaddr.c_str(), servicename.c_str());
        //fprintf(stderr, "rpc get: %s\n", request_url.c_str());
        CURL* curl = curl_easy_init();
        std::string body;
        if (curl)
        {
            curl_easy_setopt(curl, CURLOPT_URL, request_url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &EtcdProducer::writer);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK)
            {
                std::string err = curl_easy_strerror(res);
                fprintf(stderr, "produce_from_etcd request etcd error : %s\n", err.c_str());
            }
            else
            {
                //parse etcd response and update ip list
                /*
                {"action":"get",
                	"node":{ "key":"/natsu/im/provider","dir":true,"nodes":
                		[{   "key":"/natsu/im/provider/6182415165179297793",
                			"value":"127.0.0.1:9383",
                			"expiration":"2016-09-16T06:20:41.169943308Z",
                			"ttl":9,
                			"modifiedIndex":5,
                			"createdIndex":5
                		}],
                		"modifiedIndex":4,"createdIndex":4
                	}
                }

                //or
                {"action":"get","node":{"key":"/natsu/im/provider","dir":true,"modifiedIndex":4,"createdIndex":4}}

                //or
                {"errorCode":100,"message":"Key not found","cause":"/natsu/ims","index":25}
                */

                try
                {
                    ggicci::Json object = ggicci::Json::Parse(body.c_str());
                    if(object.Contains("action"))
                    {
                        if(object["action"].AsString() == "get" && object.Contains("node"))
                        {
                            ggicci::Json object_node = object["node"];
                            if(object_node.Contains("nodes"))
                            {
                                std::map<std::string,std::string> result;
                                std::map<std::string,std::string> nodelist = service_node_;
                                service_node_.clear();
                                ggicci::Json object_nodes = object_node["nodes"];
                                for(int i = 0; i < object_nodes.Size(); i++)
                                {
                                    ggicci::Json object_item = object_nodes[i];
                                    result[object_item["key"].AsString()] = object_item["value"].AsString();
                                    service_node_[object_item["key"].AsString()] = object_item["value"].AsString();

                                    if(nodelist.find(object_item["key"].AsString()) == nodelist.end())
                                    {
                                        fprintf(stderr, "got one server : %s=%s\n",object_item["key"].AsString().c_str(), object_item["value"].AsString().c_str());
                                        go std::bind(&EtcdProducer::comm_one_server, this, object_item["key"].AsString());
                                    }

                                    service_node_.clear();
                                    service_node_ = result;
                                }

                                //log
                                std::map<std::string,std::string>::iterator it = nodelist.begin();
                                for (; it != nodelist.end(); ++it)
                                {
                                    if(service_node_.find(it->first) == service_node_.end())
                                    {
                                        fprintf(stderr, "del one server : %s=%s\n", it->first.c_str(), it->second.c_str());
                                    }
                                }

                            }
                        }
                    }
                }
                catch(...)
                {
                    fprintf(stderr, "etcd response exception :%s\n", body.c_str());
                }
            }

            curl_easy_cleanup(curl);
        }

        co_timer_add(std::chrono::seconds(10), std::bind(&EtcdProducer::produce_from_etcd, this, servicename, etcdaddr));
    }

    static long writer(void *data, int size, int nmemb, std::string &content)
    {
        long sizes = size * nmemb;
        std::string temp;
        temp = std::string((char*)data,sizes);
        content += temp;
        return sizes;
    }

    void comm_one_server(const std::string& node)
    {
        if(service_node_.find(node) == service_node_.end())
        {
            fprintf(stderr, "comm_one_server no this node %s\n", node.c_str());
            return ;
        }

        std::string addr = service_node_[node];
        std::string ip = addr.substr(0, addr.find(":"));
        std::string port = addr.substr(addr.find(":") + 1);

        //connect to addr
        struct sockaddr_in client_addr;
        bzero(&client_addr,sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr.s_addr = htons(INADDR_ANY);
        client_addr.sin_port = htons(0);

        int sockfd = socket(AF_INET,SOCK_STREAM,0);

        if(bind(sockfd,(struct sockaddr*)&client_addr,sizeof(client_addr)))
        {
            fprintf(stderr, "etcd produce bind failed!\n");
            co_timer_add(std::chrono::seconds(10), std::bind(&EtcdProducer::comm_one_server, this, node));
            close(sockfd);
            return ;
        }

        struct sockaddr_in server_addr;
        bzero(&server_addr,sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        if(inet_aton(ip.c_str(), &server_addr.sin_addr) == 0)
        {
            fprintf(stderr,"etcd produce inet_aton failed %s!\n", ip.c_str());
            co_timer_add(std::chrono::seconds(10), std::bind(&EtcdProducer::comm_one_server, this, node));
            close(sockfd);
            return ;
        }

        server_addr.sin_port = htons(atoi(port.c_str()));
        socklen_t server_addr_length = sizeof(server_addr);
        if(connect(sockfd,(struct sockaddr*)&server_addr, server_addr_length) < 0)
        {
            fprintf(stderr,"etcd produce connect failed %s:%s!\n", ip.c_str(),port.c_str());
            co_timer_add(std::chrono::seconds(10), std::bind(&EtcdProducer::comm_one_server, this, node));
            close(sockfd);
            return ;
        }

        co_mutex locker;
        locker.lock();
        go std::bind(&EtcdProducer::handle_parse, this, sockfd, locker);

        while(true)
        {
            RpcChannelData data;
            (*kServiceRpcChannel[service_name_]) >> data;
            fprintf(stderr, "process one rpc request\n");
            std::shared_ptr<std::string> bin = RpcPacketParser::Encode(data.msg.get(), data.id);
            if(!bin)
            {
                fprintf(stderr, "encoding rpc data failed\n");
                continue;
            }

            size_t pos = 0;
            size_t len = bin->size();
            while(pos < len)
            {
                ssize_t n = write(sockfd, bin->c_str() + pos, bin->size() - pos);
                if(n == -1)
                {
                    fprintf(stderr, "%s send failed\n", node.c_str() );
                    co_timer_add(std::chrono::seconds(10), std::bind(&EtcdProducer::comm_one_server, this, node));
                    locker.lock();
                    close(sockfd);
                    return ;
                }

                pos += n;
            }
        }
    }

    void handle_parse(int sockfd, co_mutex locker)
    {
        RpcPacketParser rpc_parser_;
        char buf[1024];
        while(true)
        {
            int n = read(sockfd, buf, sizeof(buf));
            if (n == -1)
            {
                if (EAGAIN == errno || EINTR == errno)
                    continue;

                break;
            }
            else if (n == 0)
            {
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
                    {
                        if(kClientRpcChannel.find(rid) != kClientRpcChannel.end())
                        {
                            (*kClientRpcChannel[rid]) << message;
                        }
                        else
                        {
                            fprintf(stderr, "unable find %ld message\n", rid);
                        }
                    }
                    else
                        break;
                }
            }
        }

        locker.unlock();
    }

private:
    std::map<std::string,std::string> service_node_;
    std::string service_name_;
};

static std::map<std::string, std::shared_ptr<EtcdProducer>> kProducer;

MessagePtr invoke_rpc(const std::string& service, MessagePtr ptr)
{
    MessagePtr rsp;
    if(kProducer.find(service) != kProducer.end())
    {
        rsp = kProducer[service]->invoke(ptr);
    }

    return rsp;
}


void provide_service(const std::string& servicename, const std::string& etcdaddr)
{
    kProvider[servicename] = std::shared_ptr<EtcdProvider>(new EtcdProvider());
    go std::bind(&EtcdProvider::provide_service_etcd, kProvider[servicename], servicename, etcdaddr);
}

void produce_service(const std::string& servicename, const std::string& etcdaddr)
{
    kProducer[servicename] = std::shared_ptr<EtcdProducer>(new EtcdProducer());
    kServiceRpcChannel[servicename] = std::shared_ptr<co_chan<RpcChannelData>>(new co_chan<RpcChannelData>);
    go std::bind(&EtcdProducer::produce_service_etcd, kProducer[servicename], servicename, etcdaddr);
}

}