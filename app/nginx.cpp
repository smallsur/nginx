//#include <iostream>
//#include <boost/type_index.hpp>
//
//template <typename T> void inference(const T& tmp){
//    using boost::typeindex::type_id_with_cvr;
//    std::cout<<type_id_with_cvr<T>().pretty_name()<<std::endl;
//    std::cout<<type_id_with_cvr<decltype(tmp)>().pretty_name()<<std::endl;
//}


#include <iostream>
#include <unistd.h>

#include "ngx_c_conf.h"

int main(int argc, char* argv[]){
//    char buf[1024];
//    getcwd(buf,sizeof(buf));
//    std::cout<<buf<<std::endl;
    Config_Nginx& config = Config_Nginx::get_instance();
    if(!config.load("/home/awen/workstation/nginx/nginx.conf")){
        std::cout<<"config load failed"<<std::endl;
    }


    return 0;
}



