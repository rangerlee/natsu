#ifndef NATSU_CONFIG_H_
#define NATSU_CONFIG_H_

#include <string>

namespace natsu{

class NatsuConfig
{
public:
	static void config(const std::string& k, const std::string& v);
	static std::string config(const std::string& k);
};

}

#endif