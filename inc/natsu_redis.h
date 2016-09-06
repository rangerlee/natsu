#ifndef NATSU_REDIS_H_
#define NATSU_REDIS_H_

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <set>
#include <list>

#include "natsu_error.h"
 

namespace natsu{

typedef NatsuError RedisError;

struct RedisClient
{
    /* *
     * key
    */
    virtual int del(const std::string& key, RedisError& err) = 0;
    virtual int del(const std::vector<std::string> keys, RedisError& err) = 0;
    virtual bool exists(const std::string& key, RedisError& err) = 0;

    /* *
     * string
    */
     virtual std::string get(const std::string& key, RedisError& err) = 0;
     virtual bool set(const std::string& key, const std::string& value, RedisError& err) = 0;
     virtual bool set(const std::string& key, const std::string& value, bool set_when_exist, RedisError& err) = 0;
     virtual bool set(const std::string& key, const std::string& value, int millisecond, bool set_when_exist, RedisError& err) = 0;

     /* *
     * hashmap
    */
     virtual int hset(const std::string& key, const std::string& filed, const std::string& value, RedisError& err) = 0;
     virtual std::string hget(const std::string& key, const std::string& filed, RedisError& err) = 0;
     virtual int hdel(const std::string& key, const std::string& filed, RedisError& err) = 0;
     virtual int hdel(const std::string& key, const std::vector<std::string>& filed, RedisError& err) = 0;

    /* *
    * set
    */
    virtual int sadd(const std::string& key, const std::string& value, RedisError& err) = 0;
    virtual int sadd(const std::string& key, const std::vector<std::string>& value, RedisError& err) = 0;
    virtual std::set<std::string>  smembers(const std::string& key, RedisError& err) = 0;
};



/* *
 * redisInit 
 * @param redisname : redis name, must unique
 * @param config : redis config, like
 *                { 
 *                  "server_addr" : "127.0.0.1", //redis server ip
 *                  "server_port" : 6379         //redis server port
 *                }
 * @param link : connection count
*/
void redisInit(const std::string& redisname, const std::string& config, size_t link);


/* *
 * redisCommand
 * @param redisname : redis name        
 * @return std::shared_ptr<RedisClient> : hiredis client
*/
std::shared_ptr<RedisClient> newRedisInstance(const std::string& redisname);



}
#endif
