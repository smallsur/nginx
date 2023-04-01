//
// Created by awen on 23-4-1.
//
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "ngx_c_conf.h"
#include "ngx_global.h"
#include "ngx_func.h"
#include "ngx_macro.h"

static void ngx_start_worker_processes(int threadnums);
static int ngx_spawn_process(int threadnums,const char *pprocname);
static void ngx_worker_process_cycle(int inum,const char *pprocname);
static void ngx_worker_process_init(int inum);

static u_char  master_process[] = "master process";


void  ngx_master_process_cycle(){
    sigset_t set;        //信号集

    sigemptyset(&set);   //清空信号集

    //下列这些信号在执行本函数期间不希望收到【考虑到官方nginx中有这些信号，老师就都搬过来了】（保护不希望由信号中断的代码临界区）
    //建议fork()子进程时学习这种写法，防止信号的干扰；
    sigaddset(&set, SIGCHLD);     //子进程状态改变
    sigaddset(&set, SIGALRM);     //定时器超时
    sigaddset(&set, SIGIO);       //异步I/O
    sigaddset(&set, SIGINT);      //终端中断符
    sigaddset(&set, SIGHUP);      //连接断开
    sigaddset(&set, SIGUSR1);     //用户定义信号
    sigaddset(&set, SIGUSR2);     //用户定义信号
    sigaddset(&set, SIGWINCH);    //终端窗口大小改变
    sigaddset(&set, SIGTERM);     //终止
    sigaddset(&set, SIGQUIT);     //终端退出符

    if (sigprocmask(SIG_BLOCK, &set, nullptr) == -1) //第一个参数用了SIG_BLOCK表明设置 进程 新的信号屏蔽字 为 “当前信号屏蔽字 和 第二个参数指向的信号集的并集
    {
        ngx_log_error_core(NGX_LOG_ALERT,errno,"ngx_master_process_cycle()中sigprocmask()失败!");
    }
    //即便sigprocmask失败，程序流程 也继续往下走

    size_t size;
    int    i;
    size = sizeof(master_process);  //注意我这里用的是sizeof，所以字符串末尾的\0是被计算进来了的
    size += g_argvneedmem;          //argv参数长度加进来
    if(size < 1000) //长度小于这个，我才设置标题
    {
        char title[1000] = {0};
        strcpy(title,(const char *)master_process); //"master process"
        strcat(title," ");  //跟一个空格分开一些，清晰    //"master process "
        for (i = 0; i < g_os_argc; i++)         //"master process ./nginx"
        {
            strcat(title,g_os_argv[i]);
        }//end for
        ngx_setproctitle(title); //设置标题
    }
    Config_Nginx& p_config = Config_Nginx::get_instance(); //单例类
    int workprocess = p_config.GetIntDefault("WorkerProcesses",1); //从配置文件中得到要创建的worker进程数量
    ngx_start_worker_processes(workprocess);  //这里要创建worker子进程

    sigemptyset(&set);
    for ( ;; )
    {

//        usleep(100000);
//        ngx_log_error_core(0,0,"haha--这是父进程，pid为%P",ngx_pid);

        //a)根据给定的参数设置新的mask 并 阻塞当前进程【因为是个空集，所以不阻塞任何信号】
        //b)此时，一旦收到信号，便恢复原先的信号屏蔽【我们原来的mask在上边设置的，阻塞了多达10个信号，从而保证我下边的执行流程不会再次被其他信号截断】
        //c)调用该信号对应的信号处理函数
        //d)信号处理函数返回后，sigsuspend返回，使程序流程继续往下走
        //printf("for进来了！\n"); //发现，如果print不加\n，无法及时显示到屏幕上，是行缓存问题，以往没注意；可参考https://blog.csdn.net/qq_26093511/article/details/53255970

            sigsuspend(&set); //阻塞在这里，等待一个信号，此时进程是挂起的，不占用cpu时间，只有收到信号才会被唤醒（返回）；
        //此时master进程完全靠信号驱动干活

        //    printf("执行到sigsuspend()下边来了\n");

///        printf("master进程休息1秒\n");
        //sleep(1); //休息1秒
        //以后扩充.......

    }
    return;
};
static void ngx_start_worker_processes(int threadnums)
{
    int i;
    for (i = 0; i < threadnums; i++)  //master进程在走这个循环，来创建若干个子进程
    {
        ngx_spawn_process(i,"worker process");
    } //end for
    return;
}

static int ngx_spawn_process(int inum,const char *pprocname)
{
    pid_t  pid;

    pid = fork(); //fork()系统调用产生子进程
    printf("%d",g_os_argc);

    switch (pid)  //pid判断父子进程，分支处理
    {
        case -1: //产生子进程失败
            ngx_log_error_core(NGX_LOG_ALERT,errno,"ngx_spawn_process()fork()产生子进程num=%d,procname=\"%s\"失败!",inum,pprocname);
            return -1;

        case 0:  //子进程分支
            ngx_ppid = ngx_pid;              //因为是子进程了，所有原来的pid变成了父pid
            ngx_pid = getpid();                //重新获取pid,即本子进程的pid
            ngx_worker_process_cycle(inum,pprocname);    //我希望所有worker子进程，在这个函数里不断循环着不出来，也就是说，子进程流程不往下边走;
            break;

        default: //这个应该是父进程分支，直接break;，流程往switch之后走
            break;
    }//end switch

    //父进程分支会走到这里，子进程流程不往下边走-------------------------
    //若有需要，以后再扩展增加其他代码......
    return pid;
}

static void ngx_worker_process_cycle(int inum,const char *pprocname)
{
    //重新为子进程设置进程名，不要与父进程重复------
    ngx_worker_process_init(inum);
    ngx_setproctitle(pprocname); //设置标题

    //暂时先放个死循环，我们在这个循环里一直不出来
    //setvbuf(stdout,NULL,_IONBF,0); //这个函数. 直接将printf缓冲区禁止， printf就直接输出了。
    for(;;)
    {

        //先sleep一下 以后扩充.......
        //printf("worker进程休息1秒");
        //fflush(stdout); //刷新标准输出缓冲区，把输出缓冲区里的东西打印到标准输出设备上，则printf里的东西会立即输出；
        //sleep(1); //休息1秒
        //usleep(100000);
//        ngx_log_error_core(0,0,"good--这是子进程，编号为%d,pid为%P！",inum,ngx_pid);

        //printf("1212");
        //if(inum == 1)
        //{
        //ngx_log_stderr(0,"good--这是子进程，编号为%d,pid为%P",inum,ngx_pid);
        //printf("good--这是子进程，编号为%d,pid为%d\r\n",inum,ngx_pid);
        //ngx_log_error_core(0,0,"good--这是子进程，编号为%d",inum,ngx_pid);
        //printf("我的测试哈inum=%d",inum++);
        //fflush(stdout);
        //}

        //ngx_log_stderr(0,"good--这是子进程，pid为%P",ngx_pid);
        //ngx_log_error_core(0,0,"good--这是子进程，编号为%d,pid为%P",inum,ngx_pid);

    } //end for(;;)
    return;
}
static void ngx_worker_process_init(int inum)
{
    sigset_t  set;      //信号集

    sigemptyset(&set);  //清空信号集
    if (sigprocmask(SIG_SETMASK, &set, nullptr) == -1)  //原来是屏蔽那10个信号【防止fork()期间收到信号导致混乱】，现在不再屏蔽任何信号【接收任何信号】
    {
        ngx_log_error_core(NGX_LOG_ALERT,errno,"ngx_worker_process_init()中sigprocmask()失败!");
    }

    //....将来再扩充代码
    //....
    return;
}