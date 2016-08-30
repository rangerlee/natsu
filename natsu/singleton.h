#ifndef SINGLETON_H_
#define SINGLETON_H_

namespace natsu {

template<typename T>  
class singleton  
{  
public:  
    inline static T& instance()  
    {  
        static T static_instance;  
        return static_instance;  
    }  
  
private:  
    singleton(){};  
    singleton(singleton const&){};  
    singleton& operator= (singleton const&){ return *this;};  
    virtual ~singleton(){};
    friend T;  
};


}
#endif
