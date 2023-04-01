#ifndef __NGX_GBLDEF_H__
#define __NGX_GBLDEF_H__

//一些比较通用的定义放在这里

#include <signal.h>

//结构定义
typedef struct {
    char itemName[50];
    char itemValue[500];
}Config_Nginx_Item;

typedef struct
{
    int    log_level;   //日志级别 或者日志类型，ngx_macro.h里分0-8共9个级别
    int    fd;          //日志文件描述符
}ngx_log_t;

typedef struct
{
    int           signo;       //信号对应的数字编号 ，每个信号都有对应的#define ，大家已经学过了
    const  char   *signame;    //信号对应的中文名字 ，比如SIGHUP

    //信号处理函数,这个函数由我们自己来提供，但是它的参数和返回值是固定的【操作系统就这样要求】,大家写的时候就先这么写，也不用思考这么多；
    void  (*handler)(int signo, siginfo_t *siginfo, void *ucontext); //函数指针,   siginfo_t:系统定义的结构
} ngx_signal_t;

//外部全局量声明
extern char  **g_os_argv;
extern char  *gp_envmem; 
extern size_t   g_environlen;
extern size_t   g_argvneedmem;
extern int      g_os_argc;

extern pid_t       ngx_pid;
extern pid_t        ngx_ppid;
extern ngx_log_t   ngx_log;


#endif

