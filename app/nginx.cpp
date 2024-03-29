//#include <iostream>
//#include <boost/type_index.hpp>
//
//template <typename T> void inference(const T& tmp){
//    using boost::typeindex::type_id_with_cvr;
//    std::cout<<type_id_with_cvr<T>().pretty_name()<<std::endl;
//    std::cout<<type_id_with_cvr<decltype(tmp)>().pretty_name()<<std::endl;
//}
#include <iostream>
#include <string.h>
#include <unistd.h>


#include "ngx_c_conf.h"
#include "ngx_func.h"
#include "ngx_global.h"
#include "ngx_macro.h"
#include "ngx_c_socket.h"

static void freeresource();

char **g_os_argv = nullptr;            //原始命令行参数数组,在main中会被赋值
char *gp_envmem = nullptr;      //指向自己分配的env环境变量的内存
size_t  g_environlen = 0;       //环境变量所占内存大小
size_t   g_argvneedmem = 0;
int      g_os_argc = 0;

int   g_daemonized = 0;

pid_t ngx_pid;
pid_t ngx_ppid;

int     ngx_process;            //进程类型，比如master,worker进程等
sig_atomic_t  ngx_reap;

CLogicSocket    g_socket;
CThreadPool     g_threadpool;

int     g_stopEvent;            //标志程序退出,0不退出1，退出

int main(int argc, char* argv[]){

    int exitcode = 0;


    g_stopEvent = 0 ;


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

    ngx_log.fd = -1;                  //-1：表示日志文件尚未打开；因为后边ngx_log_stderr要用所以这里先给-1
    ngx_process = NGX_PROCESS_MASTER; //先标记本进程是master进程
    ngx_reap = 0;                     //标记子进程没有发生变化

    Config_Nginx& config = Config_Nginx::get_instance();
    if(!config.load("/home/awen/workstation/nginx/nginx.conf")){
        ngx_log_init();
        ngx_log_stderr(0,"配置文件[%s]载入失败，退出!","nginx.conf");
        exitcode = 2;
        goto lblexit;
    }
    //(2.1)内存单例类可以在这里初始化，返回值不用保存
    CMemory::get_instance();
    //(2.2)crc32校验算法单例类可以在这里初始化，返回值不用保存
    CCRC32::get_instance();
    ngx_log_init();
    if(ngx_init_signals()==-1){
        exitcode = 1;
        goto lblexit;
    }
    if(!g_socket.Initialize()){
        exitcode = 1;
        goto lblexit;
    }
    ngx_init_setproctitle();//env环境搬家

    if(config.GetIntDefault("Daemon",0)==1){
        int cdaemonresult = ngx_daemon();
        if(cdaemonresult==-1){
            exitcode = 1;    //标记失败
            goto lblexit;
        } else if(cdaemonresult==1){
            freeresource();   //只有进程退出了才goto到 lblexit，用于提醒用户进程退出了
            //而我现在这个情况属于正常fork()守护进程后的正常退出，不应该跑到lblexit()去执行，因为那里有一条打印语句标记整个进程的退出，这里不该显示该条打印语句
            exitcode = 0;
            return exitcode;  //整个进程直接在这里退出
        }
        g_daemonized = 1;
    }
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



