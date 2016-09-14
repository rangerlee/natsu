#ifndef NATSU_SNOWFLAKE_H_
#define NATSU_SNOWFLAKE_H_

#include <sys/time.h>

namespace natsu{

class SnowFlake
{
public:
	// generate unique id
	int64_t generate()
	{
		int64_t value = 0;
		value = now() << 22;

        value |= (machine_ & 0x3FF) << 12;

        if(last_timestamp_ != value)
        {
            sequence_ = 0;
            last_timestamp_ = value;
        }

		// warn : if > 0x1000 ,must wait next second
		if(++sequence_ >= 0x1000)
        {
            while(true)
            {
                value = now() << 22;
                value |= (machine_ & 0x3FF) << 12;
                if(last_timestamp_ != value)
                    break;
            }

            sequence_ = 0;
            last_timestamp_ = value;
        }

        value |= sequence_ & 0xFFF;
        return value;
	}

	// 0 ~ 1023
	void set_machine(int machine)
	{
		machine_ = machine;
	}

private:
	uint64_t now()
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        uint64_t time = tv.tv_usec;
        time /= 1000;
        time += (tv.tv_sec * 1000);
        return time;
    }

private:
	int machine_;
	int sequence_;
	int64_t last_timestamp_;
};

}
#endif
