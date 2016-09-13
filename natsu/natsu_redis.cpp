#include "natsu_redis.h"
#include "coroutine.h"
#include <stdarg.h>
#include <exception>
#include "singleton.h"
#include "hiredis/hiredis.h"
#include "gci-json.h"
#include "format.h"
 

namespace natsu {

static const char* _NATSU_REDIS_SERVER_ADDR_ = "server_addr";
static const char* _NATSU_REDIS_SERVER_PORT_ = "server_port";
static const char* _NATSU_REDIS_CONNECT_TIMEOUT_ = "connect_timeout";
static const char* _NATSU_REDIS_ERROR_MISMATCH_ = "unexcepted reply";
static const int   _NATSU_REDIS_CONNECT_TIMEOUT_DEFAULT_ = 5;

class RedisManager;
/* *
 * RedisClientImpl
*/
class RedisClientImpl : public RedisClient
{
public:
	friend class RedisManager;

	struct RedisConfig
	{
		std::string addr;
		unsigned short port;
		unsigned int conntimeout;
	};

	RedisClientImpl(const std::string& name, const std::string& config)
	: redis_name_(name)
	{
		redis_config_.reset(new RedisConfig);
		ggicci::Json object = ggicci::Json::Parse(config.c_str());
		if(object.IsNull())
		{
			throw std::logic_error("json parse failed");
		}

		redis_config_->addr = object[_NATSU_REDIS_SERVER_ADDR_].AsString();
		redis_config_->port = object[_NATSU_REDIS_SERVER_PORT_].AsInt();

		try
		{
			redis_config_->conntimeout = object[_NATSU_REDIS_CONNECT_TIMEOUT_].AsInt();
			if(redis_config_->conntimeout < 1)
				redis_config_->conntimeout = _NATSU_REDIS_CONNECT_TIMEOUT_DEFAULT_;
		}
		catch(...)
		{
			redis_config_->conntimeout = _NATSU_REDIS_CONNECT_TIMEOUT_DEFAULT_;
		}

		connect_timeout_.tv_sec = redis_config_->conntimeout;
		connect_timeout_.tv_usec = 0;
	}

	virtual ~RedisClientImpl()
	{

	}

public:
	virtual int del(const std::string& key, RedisError& err)
	{
		if(do_redis_command_args("del %s", key.c_str()))
		{
			if(REDIS_REPLY_INTEGER != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return 0;
			}

			return static_cast<int>(redis_reply_->integer);
		}
		
		err = redis_error_;
		return 0;
	}

    virtual int del(const std::vector<std::string> keys, RedisError& err)
    {
    	//cant't construct va_list on GCC, so we format redis command
    	if(do_redis_command_format("del", keys))
		{
			if(REDIS_REPLY_INTEGER != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return 0;
			}

			return static_cast<int>(redis_reply_->integer);
		}

		err = redis_error_;
		return 0;
    }

    virtual bool exists(const std::string& key, RedisError& err)
    {
    	if(do_redis_command_args("exists %b", key.c_str(), key.size()))
		{
			if(REDIS_REPLY_INTEGER != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return false;
			}

			return static_cast<int>(redis_reply_->integer) == 1;
		}

		err = redis_error_;
		return false;
    }

    //string
    virtual std::string get(const std::string& key, RedisError& err)
    {
    	if(do_redis_command_args("get %s", key.c_str()))
		{
			if(REDIS_REPLY_STRING != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return 0;
			}

			return redis_reply_->len > 0 ? std::string(redis_reply_->str, redis_reply_->len) : "";
		}

		err = redis_error_;
		return "";
    }

    virtual bool set(const std::string& key, const std::string& value, RedisError& err)
    {
    	std::vector<std::string> v;
    	v.push_back(key);
    	v.push_back(value);
    	if(do_redis_command_format("set", v))
    	{
    		if(REDIS_REPLY_STATUS != redis_reply_->type && REDIS_REPLY_ERROR != redis_reply_->type)
    		{
    			err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return false;
    		}

    		return redis_reply_->len > 0 ? strcasecmp(redis_reply_->str, "ok") == 0 : false;
    	}

    	err = redis_error_;
		return "";
    }

    virtual bool set(const std::string& key, const std::string& value, bool set_when_exist, RedisError& err)
    {
     	std::vector<std::string> v;
    	v.push_back(key);
    	v.push_back(value);
    	v.push_back(set_when_exist ? "XX" : "NX");
    	if(do_redis_command_format("set", v))
    	{
    		if(REDIS_REPLY_STATUS != redis_reply_->type && REDIS_REPLY_ERROR != redis_reply_->type)
    		{
    			err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return false;
    		}

    		return redis_reply_->len > 0 ? strcasecmp(redis_reply_->str, "ok") == 0 : false;
    	}

    	err = redis_error_;
		return "";
    }

    virtual bool set(const std::string& key, const std::string& value, int millisecond, bool set_when_exist, RedisError& err)
    {
     	std::vector<std::string> v;
    	v.push_back(key);
    	v.push_back(value);
    	v.push_back("PX");
    	v.push_back(format("%d", millisecond));
    	v.push_back(set_when_exist ? "XX" : "NX");
    	if(do_redis_command_format("set", v))
    	{
    		if(REDIS_REPLY_STATUS != redis_reply_->type && REDIS_REPLY_ERROR != redis_reply_->type)
    		{
    			err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return false;
    		}

    		return redis_reply_->len > 0 ? strcasecmp(redis_reply_->str, "ok") == 0 : false;
    	}

    	err = redis_error_;
		return "";
    }

    //hashmap
    virtual int hset(const std::string& key, const std::string& filed, const std::string& value, RedisError& err)
    {
     	if(do_redis_command_args("hset %b %b %b", key.c_str(), key.size(), filed.c_str(), filed.size(), value.c_str(), value.size()))
     	{
     		if(REDIS_REPLY_INTEGER != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return 0;
			}

			return static_cast<int>(redis_reply_->integer);
     	}

     	err = redis_error_;
		return 0;
    }

    virtual std::string hget(const std::string& key, const std::string& filed, RedisError& err)
    {
     	if(do_redis_command_args("hget %b %b", key.c_str(), key.size(), filed.c_str(), filed.size()))
     	{
     		if(REDIS_REPLY_STRING != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return 0;
			}

			return redis_reply_->len > 0 ? std::string(redis_reply_->str, redis_reply_->len) : "";
     	}

     	err = redis_error_;
		return "";
    }

    virtual int hdel(const std::string& key, const std::string& filed, RedisError& err)
    {
     	if(do_redis_command_args("hdel %b %b", key.c_str(), key.size(), filed.c_str(), filed.size()))
     	{
     		if(REDIS_REPLY_INTEGER != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return 0;
			}

			return static_cast<int>(redis_reply_->integer);
     	}

     	err = redis_error_;
		return 0;
    }

    virtual int hdel(const std::string& key, const std::vector<std::string>& filed, RedisError& err)
    {
     	std::vector<std::string> fileds = filed;
     	fileds.insert(fileds.begin(), key);
     	if(do_redis_command_format("hdel", fileds))
     	{
     		if(REDIS_REPLY_INTEGER != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return 0;
			}

			return static_cast<int>(redis_reply_->integer);
     	}

     	err = redis_error_;
		return 0;
    }

    //set
    virtual int sadd(const std::string& key, const std::string& value, RedisError& err)
    {
    	if(do_redis_command_args("sadd %b %b", key.c_str(), key.size(), value.c_str(), value.size()))
     	{
     		if(REDIS_REPLY_INTEGER != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return 0;
			}

			return static_cast<int>(redis_reply_->integer);
     	}

     	err = redis_error_;
		return 0;
    }

    virtual int sadd(const std::string& key, const std::vector<std::string>& value, RedisError& err)
    {
		if(do_redis_command_format("sadd", value))
     	{
     		if(REDIS_REPLY_INTEGER != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return 0;
			}

			return static_cast<int>(redis_reply_->integer);
     	}

     	err = redis_error_;
		return 0;
    }

    virtual std::set<std::string> smembers(const std::string& key, RedisError& err)
    {
    	std::set<std::string> result;
    	if(do_redis_command_args("smembers %b", key.c_str(), key.size()))
     	{
     		if(REDIS_REPLY_ARRAY != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : _NATSU_REDIS_ERROR_MISMATCH_;
				return result;
			}

			for (size_t i = 0; i < redis_reply_->elements; i ++)
	        {
	            redisReply* member_reply = redis_reply_->element[i];
	            result.insert(std::string(member_reply->str, member_reply->len));
	        }

	        return result;
     	}

     	err = redis_error_;
		return result;
    }

private:
	void *redis_block_for_reply() {
	    void *reply;

	    if (redis_context_->flags & REDIS_BLOCK) {
	        if (redisGetReply(redis_context_.get(),&reply) != REDIS_OK)
	            return NULL;
	        return reply;
	    }
	    return NULL;
	}

	bool do_redis_command_format(const std::string& cmd, std::vector<std::string> v)
	{
		redis_reply_.reset();
		cmd_cache_.clear();
		cmd_cache_ = format("*%d\r\n$%d\r\n%s\r\n", v.size() + 1, cmd.size(), cmd.c_str());
		for (size_t i = 0; i < v.size(); ++i)
		{
			cmd_cache_.append(format("$%d\r\n", v[i].size()));
			cmd_cache_.append(v[1]);
			cmd_cache_.append("\r\n");
		}

		redis_error_.clear();
		if(NULL == redis_context_.get())
		{
			redis_context_.reset(redisConnectWithTimeout(redis_config_->addr.c_str(), redis_config_->port, connect_timeout_), redisFree);
			if(NULL == redis_context_.get() || redis_context_->err)
			{
				redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
				return false;
			}
		}

		if(redis_context_ && redis_context_->err)
		{
			redis_context_.reset(redisConnectWithTimeout(redis_config_->addr.c_str(), redis_config_->port, connect_timeout_), redisFree);
			if(NULL == redis_context_.get() || redis_context_->err)
			{
				redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
				return false;
			}
		}
		
		if(REDIS_OK == redisAppendFormattedCommand(redis_context_.get(), cmd.c_str(), cmd.size()))
		{
			redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
			redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
			return false;
		}

		redisReply* reply = (redisReply*)redis_block_for_reply();
		if(NULL == reply)
		{
			redis_context_.reset(redisConnectWithTimeout(redis_config_->addr.c_str(), redis_config_->port, connect_timeout_), redisFree);
			if(NULL == redis_context_.get() || redis_context_->err)
			{
				redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
				return false;
			}

			if(REDIS_OK == redisAppendFormattedCommand(redis_context_.get(), cmd.c_str(), cmd.size()))
			{
				redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
				return false;
			}

			reply = (redisReply*)redis_block_for_reply();
		}

		if(reply)
		{
			redis_reply_.reset(reply, freeReplyObject);
		}
		else
		{
			redis_error_.code() = redis_context_->err ? redis_context_->err : -1;
			redis_error_.reason() = redis_context_->err ? redis_context_->errstr : "";
		}

		return redis_reply_.get() != NULL;
	}

	bool do_redis_command_args(const char* fmt, ...)
	{
		redis_reply_.reset();
		va_list va;
		va_start(va, fmt);
		bool ret = do_redis_command(fmt, va);
		va_end(va);
		return ret;
	}

	bool do_redis_command(const char* fmt, va_list va)
	{
		redis_reply_.reset();
		redis_error_.clear();
		if(NULL == redis_context_.get())
		{
			redis_context_.reset(redisConnectWithTimeout(redis_config_->addr.c_str(), redis_config_->port, connect_timeout_), redisFree);
			if(NULL == redis_context_.get() || redis_context_->err)
			{
				redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
				return false;
			}
		}

		if(redis_context_ && redis_context_->err)
		{
			redis_context_.reset(redisConnectWithTimeout(redis_config_->addr.c_str(), redis_config_->port, connect_timeout_), redisFree);
			if(NULL == redis_context_.get() || redis_context_->err)
			{
				redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
				return false;
			}
		}
		
		redisReply* reply = (redisReply*)redisvCommand(redis_context_.get(), fmt, va);
		if(NULL == reply)
		{
			redis_context_.reset(redisConnectWithTimeout(redis_config_->addr.c_str(), redis_config_->port, connect_timeout_), redisFree);
			if(NULL == redis_context_.get() || redis_context_->err)
			{
				redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
				return false;
			}

			reply = (redisReply*)redisvCommand(redis_context_.get(), fmt, va);
		}

		if(reply)
		{
			redis_reply_.reset(reply, freeReplyObject);
		}
		else
		{
			redis_error_.code() = redis_context_->err ? redis_context_->err : -1;
			redis_error_.reason() = redis_context_->err ? redis_context_->errstr : "";
		}

		return redis_reply_.get() != NULL;
	}

private:
	std::shared_ptr<redisContext> redis_context_;
	std::shared_ptr<redisReply> redis_reply_;
	RedisError redis_error_;
	std::string redis_name_;
	std::shared_ptr<RedisConfig> redis_config_;
	std::string cmd_cache_;
	struct timeval connect_timeout_;
};

/* *
 * RedisManager
*/
class RedisManager : public singleton<RedisManager>
{
public:
	void add_redis_server(const std::string& redisname, const std::string& config, size_t link)
	{	
		if(redis_.find(redisname) == redis_.end())
		{
			std::shared_ptr<co_chan<RedisClientImpl*>> p(new co_chan<RedisClientImpl*>(link));
			redis_[redisname] = p;
		}

		for(size_t i = 0; i < link; i++)
		{
			RedisClientImpl* data = new RedisClientImpl(redisname, config);
			(*redis_[redisname]) << data; 
		}
	}

	RedisClientImpl* get_redis_server(const std::string& redisname)
	{
		RedisClientImpl* client = NULL;
		std::map<std::string,std::shared_ptr<co_chan<RedisClientImpl*>>>::iterator iter = redis_.find(redisname);
		if(iter != redis_.end())
		{
			(*iter->second) >> client;
		}

		return client;
	}

	void release_redis_client(RedisClientImpl* data)
	{
		if(redis_.find(data->redis_name_) != redis_.end())
		{
			(*redis_[data->redis_name_]) << data;
		}
	}

private:
	//ignore memory leak, beause it's global instance
	std::map<std::string, std::shared_ptr<co_chan<RedisClientImpl*>>> redis_;
};



void redisInit(const std::string& redisname, const std::string& config, size_t link)
{
	RedisManager::instance().add_redis_server(redisname, config, link);
}

void delRedisInstance(RedisClientImpl* client)
{
	RedisManager::instance().release_redis_client(client);
}

std::shared_ptr<RedisClient> newRedisInstance(const std::string& redisname)
{
	std::shared_ptr<RedisClient> client(RedisManager::instance().get_redis_server(redisname), delRedisInstance);
	return client;
}



} //namespace
