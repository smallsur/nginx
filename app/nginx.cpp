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
#include <string.h>
#include <errno.h>
#include "unistd.h"
#include "ngx_c_conf.h"
#include "ngx_func.h"
#include "ngx_global.h"
#include "ngx_macro.h"

static void freeresource();
//和设置标题有关的全局量
char **g_os_argv = nullptr;            //原始命令行参数数组,在main中会被赋值
char *gp_envmem = nullptr;      //指向自己分配的env环境变量的内存
size_t  g_environlen = 0;       //环境变量所占内存大小
size_t   g_argvneedmem = 0;
int      g_os_argc = 0;


pid_t ngx_pid;
pid_t ngx_ppid;
int main(int argc, char* argv[]){

    int exitcode = 0;

    ngx_pid = getpid();
    ngx_ppid = getppid();

    g_os_argv = (char **) argv;
    g_os_argc = argc;
    g_argvneedmem = 0;
    for (int i = 0; argv[i] ; ++i) {
        g_argvneedmem += strlen(argv[i]) + 1;
    }
    g_environlen = 0;
    for (int i = 0; environ[i] ; ++i) {
        g_environlen += strlen(environ[i]) + 1;
    }

    ngx_init_setproctitle();//env环境搬家

    Config_Nginx& config = Config_Nginx::get_instance();
    if(!config.load("/home/awen/workstation/nginx/nginx.conf")){
        ngx_log_stderr(0,"配置文件[%s]载入失败，退出!","nginx.conf");
        exitcode = 2;
        goto lblexit;
    }

    ngx_log_init();
    if(ngx_init_signals()==-1){
        exitcode = 1;
        goto lblexit;
    }
//    ngx_setproctitle("nginx: master process");
//    for (int i = 0; ; ++i) {
//        sleep(1);
//    }
//    std::cout<<config.GetIntDefault("listen_port",12)<<std::endl;
//    std::cout<<config.GetString("wen")<<std::endl;
//    //ngx_log_stderr调用演示代码：
//    ngx_log_stderr(0, "invalid option: \"%s\"", argv[0]);  //nginx: invalid option: "./nginx"
//    ngx_log_stderr(0, "invalid option: %10d", 21);         //nginx: invalid option:         21  ---21前面有8个空格
//    ngx_log_stderr(0, "invalid option: %.6f", 21.378);     //nginx: invalid option: 21.378000   ---%.这种只跟f配合有效，往末尾填充0
//    ngx_log_stderr(0, "invalid option: %.6f", 12.999);     //nginx: invalid option: 12.999000
//    ngx_log_stderr(0, "invalid option: %.2f", 12.999);     //nginx: invalid option: 13.00
//    ngx_log_stderr(0, "invalid option: %xd", 1678);        //nginx: invalid option: 68e
//    ngx_log_stderr(0, "invalid option: %Xd", 1678);        //nginx: invalid option: 68E
//    ngx_log_stderr(15, "invalid option: %s , %d", "testInfo",326);        //nginx: invalid option: testInfo , 326 (15: Block device required)
//    ngx_log_stderr(0, "invalid option: %d", 1678);         //nginx: invalid option: 1678
//
//
//    //测试ngx_log_error_core函数的调用
//    ngx_log_error_core(5,8,"这个XXX工作的有问题，显示的结果=%s","YYYY");
    ngx_master_process_cycle();
    lblexit:
    //(5)该释放的资源要释放掉
        freeresource();  //一系列的main返回前的释放动作函数
        std::cout<<"program exit !"<<std::endl;
        return exitcode;
}

void freeresource()
{
    //(1)对于因为设置可执行程序标题导致的环境变量分配的内存，我们应该释放
    if(gp_envmem)
    {
        delete [] gp_envmem;
        gp_envmem = nullptr;
    }

    //(2)关闭日志文件
    if(ngx_log.fd != STDERR_FILENO && ngx_log.fd != -1)
    {
        close(ngx_log.fd); //不用判断结果了
        ngx_log.fd = -1; //标记下，防止被再次close吧
    }
}



