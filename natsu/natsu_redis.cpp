#include "natsu_redis.h"
#include "hiredis.h"
#include "coroutine.h"
#include <string>
#include "singleton.h"


namespace natsu {

enum { MaxRedisCommandPoolSize = 1000, };

struct RedisCommand
{
	/* data */
	std::string command;

	RedisCommand(const std::string& c) : command(c){}
};

struct ChannelData
{
	/* data */
	std::string name;
	co_chan<std::shared_ptr<RedisCommand>> command_channel;
	co_chan<std::shared_ptr<redisReply>> reply_channel;
};


/* *
 * RedisClientImpl
*/
class RedisClientImpl : public RedisClient
{
public:
	RedisClientImpl(std::shared_ptr<ChannelData>& data);
	virtual ~RedisClientImpl();
	virtual std::shared_ptr<redisReply> redisCommand(const std::string& command);
private:
	std::shared_ptr<ChannelData> channel_data_;
};


/* *
 * RedisManager
*/
class RedisManager : public singleton<RedisManager>
{
public:
	void add_redis_server(const std::string& redisname, const std::string& ip, unsigned short port, size_t link, size_t timeout)
	{	
		if(redis_.find(redisname) == redis_.end())
		{
			std::shared_ptr<co_chan<std::shared_ptr<ChannelData>>> p(new co_chan<std::shared_ptr<ChannelData>>(link));
			redis_[redisname] = p;
		}

		for(size_t i = 0; i < link; i++)
		{
			std::shared_ptr<ChannelData> data(new ChannelData());
			data->name = redisname;
			go std::bind(&RedisManager::do_redis_task, this, ip, port, timeout, data);
			(*redis_[redisname]) << data; 
		}
	}

	std::shared_ptr<RedisClient> get_redis_server(const std::string& redisname)
	{
		std::shared_ptr<RedisClient> client;
		std::map<std::string,std::shared_ptr<co_chan<std::shared_ptr<ChannelData>>>>::iterator iter = redis_.find(redisname);
		if(iter != redis_.end())
		{
			std::shared_ptr<ChannelData> data;
			(*iter->second) >> data;
			client.reset(new RedisClientImpl(data));
		}

		return client;
	}


private:
	void do_redis_task(const std::string& ip, unsigned short port, size_t timeout, std::shared_ptr<ChannelData> ch)
	{
		std::shared_ptr<redisContext> context;
		std::shared_ptr<redisReply> reply;

		while(true)
		{
			std::shared_ptr<RedisCommand> cmd;
			ch->command_channel >> cmd;

			//@todo timeout

			if(!context)
			{
				context.reset(redisConnect(ip.c_str(), port));
				//update time
			}

			if(!context)
			{
				reply.reset();
				ch->reply_channel << reply;
				continue;
			}

			redisReply* result = (redisReply*)redisCommand(context.get(),cmd->command.c_str());
			std::shared_ptr<redisReply> r(result, freeReplyObject);
			ch->reply_channel << r;
		}
	}

public:
	void ReleaseChannelData(std::shared_ptr<ChannelData>& data)
	{
		if(redis_.find(data->name) != redis_.end())
		{
			(*redis_[data->name]) << data;
		}
	}

private:
	std::map<std::string, std::shared_ptr<co_chan<std::shared_ptr<ChannelData>>>> redis_;
};


//** RedisClientImpl start **//
RedisClientImpl::RedisClientImpl(std::shared_ptr<ChannelData>& data)
: channel_data_(data)
{}

RedisClientImpl::~RedisClientImpl()
{
	RedisManager::instance().ReleaseChannelData(channel_data_);
}

std::shared_ptr<redisReply> RedisClientImpl::redisCommand(const std::string& command)
{
	std::shared_ptr<RedisCommand> cmd = std::make_shared<RedisCommand>(command);
	channel_data_->command_channel << cmd;

	std::shared_ptr<redisReply> reply;
	channel_data_->reply_channel >> reply;
	return reply;
}
//** RedisClientImpl end **//


void redisInit(const std::string& redisname, const std::string& ip, unsigned short port, size_t link, size_t timeout)
{
	RedisManager::instance().add_redis_server(redisname, ip, port, link, timeout);
}

std::shared_ptr<RedisClient> redisInstance(const std::string& redisname)
{
	return RedisManager::instance().get_redis_server(redisname);
}



} //namespace
