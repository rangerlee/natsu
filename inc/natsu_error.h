#ifndef NATSU_ERROR_H_
#define NATSU_ERROR_H_
#include <string>

namespace natsu {

class NatsuError
{
public:
	NatsuError(int code, std::string reason = "");
	NatsuError();
	bool ok();
	std::string& reason();
	int& code();
	void clear();

private:
	int err_no_;
	std::string err_str_;
};


}

#endif
