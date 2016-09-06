#include "natsu_error.h"

namespace natsu {

/* *
 * NatsuError
*/
NatsuError::NatsuError(int code, std::string reason)
: err_no_(code), err_str_(reason) {}

NatsuError::NatsuError()
: err_no_(0) {}

bool NatsuError::ok()
{
	return err_no_ == 0;
}

std::string& NatsuError::reason()
{
	return err_str_;
}

int& NatsuError::code()
{
	return err_no_;
}

void NatsuError::clear()
{
	err_no_ = 0;
	err_str_.clear();
}


}
