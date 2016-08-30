#ifndef NATSU_REDIS_H_
#define NATSU_REDIS_H_

#include <string>
#include <map>
#include <memory>
#include "hiredis.h"

namespace natsu{


struct RedisClient
{
    virtual std::shared_ptr<redisReply> redisCommand(const std::string& command) = 0;
};



/* *
 * redisInit 
 * @param redisname : redis name, must unique
 * @param ip : redis address 
 * @param port : redis listen port 6379
 * @param link : connection count
 * @param timeout : connect timeout
*/
void redisInit(const std::string& redisname, const std::string& ip, unsigned short port, size_t link, size_t timeout);


/* *
 * redisCommand
 * @param redisname : redis name 
 * @return std::shared_ptr<RedisClient> : hiredis client
*/
std::shared_ptr<RedisClient> redisInstance(const std::string& redisname);



}
#endif
