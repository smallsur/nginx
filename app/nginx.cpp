//#include <iostream>
//#include <boost/type_index.hpp>
//
//template <typename T> void inference(const T& tmp){
//    using boost::typeindex::type_id_with_cvr;
//    std::cout<<type_id_with_cvr<T>().pretty_name()<<std::endl;
//    std::cout<<type_id_with_cvr<decltype(tmp)>().pretty_name()<<std::endl;
//}


#include <iostream>
#include <stdlib.h>
#include "unistd.h"
#include "ngx_c_conf.h"
#include "ngx_func.h"
#include "ngx_global.h"
//和设置标题有关的全局量
char **g_os_argv;            //原始命令行参数数组,在main中会被赋值
char *gp_envmem = nullptr;      //指向自己分配的env环境变量的内存
int  g_environlen = 0;       //环境变量所占内存大小

int main(int argc, char* argv[]){
    g_os_argv = (char **) argv;
    ngx_init_setproctitle();//env环境搬家

    Config_Nginx& config = Config_Nginx::get_instance();
    if(!config.load("/home/awen/workstation/nginx/nginx.conf")){
        std::cout<<"config load failed"<<std::endl;
        exit(1); //stdlib
    }
    ngx_setproctitle("nginx: master process");
//    for (int i = 0; ; ++i) {
//        sleep(1);
//    }
    std::cout<<config.GetIntDefault("listen_port",12)<<std::endl;
    std::cout<<config.GetString("wen")<<std::endl;
    if(gp_envmem)
    {
        delete []gp_envmem;
        gp_envmem = nullptr;
    }
    std::cout<<"program exit normally!"<<std::endl;
    return 0;
}



