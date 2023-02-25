//#include <iostream>
//#include <boost/type_index.hpp>
//
//template <typename T> void inference(const T& tmp){
//    using boost::typeindex::type_id_with_cvr;
//    std::cout<<type_id_with_cvr<T>().pretty_name()<<std::endl;
//    std::cout<<type_id_with_cvr<decltype(tmp)>().pretty_name()<<std::endl;
//}
#include <iostream>
#include <memory>
#include "ngx_c_conf.h"
int main(){
    std::shared_ptr<Config_Nginx> config = Config_Nginx::getInstance();
    return 0;
}


