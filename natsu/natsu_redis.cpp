#include "natsu_redis.h"
#include "hiredis.h"
#include "coroutine.h"
#include <stdarg.h>
#include <exception>
#include "singleton.h"
#include "hiredis.h"
#include "gci-json.h"
#include "format.h"
 

namespace natsu {

class RedisManager;
/* *
 * RedisClientImpl
*/
class RedisClientImpl : public RedisClient
{
public:
	friend class RedisManager;

	RedisClientImpl(const std::string& name, const std::string& config)
	: redis_name_(name)
	{
		json_object_ = ggicci::Json::Parse(config.c_str());
		if(json_object_.IsNull())
			throw std::logic_error("json parse failed");
	}

	virtual ~RedisClientImpl()
	{

	}

public:
	virtual int del(const std::string& key, RedisError& err)
	{
		if(do_redis_command_args("del %s", key.c_str()))
		{
			//parse redisReply
			if(REDIS_REPLY_INTEGER != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : "type dismatch";
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
			//parse redisReply
			if(REDIS_REPLY_INTEGER != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : "type dismatch";
				return 0;
			}

			return static_cast<int>(redis_reply_->integer);
		}

		err = redis_error_;
		return 0;
    }

    virtual bool exists(const std::string& key, RedisError& err)
    {
    	//todo
    	return true;
    }

    //string
    virtual std::string get(const std::string& key, RedisError& err)
    {
    	if(do_redis_command_args("get %s", key.c_str()))
		{
			//parse redisReply
			if(REDIS_REPLY_STRING != redis_reply_->type)
			{
				err.code() = -1;
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : "type dismatch";
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
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : "type dismatch";
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
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : "type dismatch";
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
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : "type dismatch";
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
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : "type dismatch";
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
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : "type dismatch";
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
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : "type dismatch";
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
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : "type dismatch";
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
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : "type dismatch";
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
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : "type dismatch";
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
				err.reason() = REDIS_REPLY_ERROR == redis_reply_->type ? redis_reply_->str : "type dismatch";
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
			redis_context_.reset(redisConnect(json_object_["server_addr"].AsString().c_str(), json_object_["server_port"].AsInt()), redisFree);
			if(NULL == redis_context_.get() || redis_context_->err)
			{
				redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
				return false;
			}
		}

		if(redis_context_ && redis_context_->err)
		{
			#if NATSU_HIREDIS_SUPPORT_RECONNECT
			if(REDIS_OK != redisReconnect(redis_context_.get()))
			{
				redis_error_.code() = redis_context_->err ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_->err ? redis_context_->errstr : "";
				return false;
			}
			#else
			redis_context_.reset(redisConnect(json_object_["server_addr"].AsString().c_str(), json_object_["server_port"].AsInt()), redisFree);
			if(NULL == redis_context_.get() || redis_context_->err)
			{
				redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
				return false;
			}
			#endif
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
			#if NATSU_HIREDIS_SUPPORT_RECONNECT
			if(REDIS_OK != redisReconnect(redis_context_.get()))
			{
				redis_error_.code() = redis_context_->err ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_->err ? redis_context_->errstr : "";
				return false;
			}
			#else
			redis_context_.reset(redisConnect(json_object_["server_addr"].AsString().c_str(), json_object_["server_port"].AsInt()), redisFree);
			if(NULL == redis_context_.get() || redis_context_->err)
			{
				redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
				return false;
			}
			#endif

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
			redis_context_.reset(redisConnect(json_object_["server_addr"].AsString().c_str(), json_object_["server_port"].AsInt()), redisFree);
			if(NULL == redis_context_.get() || redis_context_->err)
			{
				redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
				return false;
			}
		}

		if(redis_context_ && redis_context_->err)
		{
			#if NATSU_HIREDIS_SUPPORT_RECONNECT
			if(REDIS_OK != redisReconnect(redis_context_.get()))
			{
				redis_error_.code() = redis_context_->err ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_->err ? redis_context_->errstr : "";
				return false;
			}
			#else
			redis_context_.reset(redisConnect(json_object_["server_addr"].AsString().c_str(), json_object_["server_port"].AsInt()), redisFree);
			if(NULL == redis_context_.get() || redis_context_->err)
			{
				redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
				return false;
			}
			#endif
		}
		
		redisReply* reply = (redisReply*)redisvCommand(redis_context_.get(), fmt, va);
		if(NULL == reply)
		{
			#if NATSU_HIREDIS_SUPPORT_RECONNECT
			if(REDIS_OK != redisReconnect(redis_context_.get()))
			{
				redis_error_.code() = redis_context_->err ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_->err ? redis_context_->errstr : "";
				return false;
			}
			#else
			redis_context_.reset(redisConnect(json_object_["server_addr"].AsString().c_str(), json_object_["server_port"].AsInt()), redisFree);
			if(NULL == redis_context_.get() || redis_context_->err)
			{
				redis_error_.code() = redis_context_.get() ? redis_context_->err : -1;
				redis_error_.reason() = redis_context_.get() ? redis_context_->errstr : "";
				return false;
			}
			#endif

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
	ggicci::Json json_object_;
	std::string cmd_cache_;
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