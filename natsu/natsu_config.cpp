#include "natsu_config.h"
#include <string>
#include <map>

namespace natsu{

static std::map<std::string,std::string> kNatsuConfig;

void NatsuConfig::config(const std::string& k, const std::string& v)
{
	kNatsuConfig[k] = v;
}

std::string NatsuConfig::config(const std::string& k)
{
	if(kNatsuConfig.find(k) != kNatsuConfig.end())
		return kNatsuConfig[k];
	return "";
}

}
