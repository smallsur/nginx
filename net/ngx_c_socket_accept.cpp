//
// Created by awen on 23-4-3.
//
#include <errno.h>
#include <sys/ioctl.h> //ioctl
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>


#include "ngx_c_socket.h"
#include "ngx_global.h"
#include "ngx_macro.h"
#include "ngx_func.h"

void CSocekt::ngx_event_accept(lpngx_connection_t oldc) {

    //因为listen套接字上用的不是ET【边缘触发】，而是LT【水平触发】，意味着客户端连入如果我要不处理，这个函数会被多次调用，所以，我这里这里可以不必多次accept()，可以只执行一次accept()
    //这也可以避免本函数被卡太久，注意，本函数应该尽快返回，以免阻塞程序运行；
    struct sockaddr mysockaddr;        //远端服务器的socket地址
    socklen_t socklen;
    int err;
    int level;
    int s;
    static int use_accept4 = 1;   //我们先认为能够使用accept4()函数
    lpngx_connection_t newc;              //代表连接池中的一个连接【注意这是指针】

    //ngx_log_s
    socklen = sizeof(mysockaddr);
    do   //用do，跳到while后边去方便
    {
        if (use_accept4) {
            s = accept4(oldc->fd, &mysockaddr, &socklen,
                        SOCK_NONBLOCK); //从内核获取一个用户端连接，最后一个参数SOCK_NONBLOCK表示返回一个非阻塞的socket，节省一次ioctl【设置为非阻塞】调用
        } else {
            s = accept(oldc->fd, &mysockaddr, &socklen);
        }
        if(s == -1)
        {
            err = errno;

            //对accept、send和recv而言，事件未发生时errno通常被设置成EAGAIN（意为“再来一次”）或者EWOULDBLOCK（意为“期待阻塞”）
            if(err == EAGAIN) //accept()没准备好，这个EAGAIN错误EWOULDBLOCK是一样的
            {
                //除非你用一个循环不断的accept()取走所有的连接，不然一般不会有这个错误【我们这里只取一个连接】
                return ;
            }
            level = NGX_LOG_ALERT;
            if (err == ECONNABORTED)  //ECONNRESET错误则发生在对方意外关闭套接字后【您的主机中的软件放弃了一个已建立的连接--由于超时或者其它失败而中止接连(用户插拔网线就可能有这个错误出现)】
            {
                //该错误被描述为“software caused connection abort”，即“软件引起的连接中止”。原因在于当服务和客户进程在完成用于 TCP 连接的“三次握手”后，
                //客户 TCP 却发送了一个 RST （复位）分节，在服务进程看来，就在该连接已由 TCP 排队，等着服务进程调用 accept 的时候 RST 却到达了。
                //POSIX 规定此时的 errno 值必须 ECONNABORTED。源自 Berkeley 的实现完全在内核中处理中止的连接，服务进程将永远不知道该中止的发生。
                //服务器进程一般可以忽略该错误，直接再次调用accept。
                level = NGX_LOG_ERR;
            }
            else if (err == EMFILE || err == ENFILE) //EMFILE:进程的fd已用尽【已达到系统所允许单一进程所能打开的文件/套接字总数】。可参考：https://blog.csdn.net/sdn_prc/article/details/28661661   以及 https://bbs.csdn.net/topics/390592927
                //ulimit -n ,看看文件描述符限制,如果是1024的话，需要改大;  打开的文件句柄数过多 ,把系统的fd软限制和硬限制都抬高.
                //ENFILE这个errno的存在，表明一定存在system-wide的resource limits，而不仅仅有process-specific的resource limits。按照常识，process-specific的resource limits，一定受限于system-wide的resource limits。
            {
                level = NGX_LOG_CRIT;
            }
            ngx_log_error_core(level,errno,"CSocekt::ngx_event_accept()中accept4()失败!");

            if(use_accept4 && err == ENOSYS) //accept4()函数没实现，坑爹？
            {
                use_accept4 = 0;  //标记不使用accept4()函数，改用accept()函数
                continue;         //回去重新用accept()函数搞
            }

            if (err == ECONNABORTED)  //对方关闭套接字
            {
                //这个错误因为可以忽略，所以不用干啥
                //do nothing
            }

            if (err == EMFILE || err == ENFILE)
            {
                //do nothing，这个官方做法是先把读事件从listen socket上移除，然后再弄个定时器，定时器到了则继续执行该函数，但是定时器到了有个标记，会把读事件增加到listen socket上去；
                //我这里目前先不处理吧【因为上边已经写这个日志了】；
            }
            return;
        }  //end if(s == -1)
        //走到这里的，表示accept4()/accept()成功了
        if(m_onlineUserCount >= m_worker_connections)  //用户连接数过多，要关闭该用户socket，因为现在也没分配连接，所以直接关闭即可
        {
            ngx_log_stderr(0,"超出系统允许的最大连入用户数(最大允许连入数%d)，关闭连入请求(%d)。",m_worker_connections,s);
            close(s);
            return ;
        }
        newc = ngx_get_connection(s);
        if(newc == nullptr)
        {
            //连接池中连接不够用，那么就得把这个socekt直接关闭并返回了，因为在ngx_get_connection()中已经写日志了，所以这里不需要写日志了
            if(close(s) == -1)
            {
                ngx_log_error_core(NGX_LOG_ALERT,errno,"CSocekt::ngx_event_accept()中close(%d)失败!",s);
            }
            return;
        }
        memcpy(&newc->s_sockaddr,&mysockaddr,socklen);
        if(!use_accept4)
        {
            //如果不是用accept4()取得的socket，那么就要设置为非阻塞【因为用accept4()的已经被accept4()设置为非阻塞了】
            if(setnonblocking(s) == false)
            {
                //设置非阻塞居然失败
                ngx_close_connection(newc);
                return; //直接返回
            }
        }
        newc->listening = oldc->listening;                    //连接对象 和监听对象关联，方便通过连接对象找监听对象【关联到监听端口】
//        newc->w_ready = 1;                                    //标记可以写，新连接写事件肯定是ready的；【从连接池拿出一个连接时这个连接的所有成员都是0】
        newc->rhandler = &CSocekt::ngx_wait_request_handler;
        newc->whandler = &CSocekt::ngx_write_request_handler;  //设置数据发送时的写处理函数。
        //客户端应该主动发送第一次的数据，这里将读事件加入epoll监控
//        if(ngx_epoll_add_event(s,                 //socket句柄
//                               1,0,              //读，写
//                               0,          //其他补充标记【EPOLLET(高速模式，边缘触发ET)】
//                               EPOLL_CTL_ADD,    //事件类型【增加，还有删除/修改】
//                               newc              //连接池中的连接
//        ) == -1)
//        {
//            //增加事件失败，失败日志在ngx_epoll_add_event中写过了，因此这里不多写啥；
//            ngx_close_connection(newc);
//            return; //直接返回
//        }
        if(ngx_epoll_oper_event(
                s,                  //socekt句柄
                EPOLL_CTL_ADD,      //事件类型，这里是增加
                EPOLLIN|EPOLLRDHUP, //标志，这里代表要增加的标志,EPOLLIN：可读，EPOLLRDHUP：TCP连接的远端关闭或者半关闭
                0,                  //对于事件类型为增加的，不需要这个参数
                newc                //连接池中的连接
        ) == -1)
        {
            //增加事件失败，失败日志在ngx_epoll_add_event中写过了，因此这里不多写啥；
            ngx_close_connection(newc);//关闭socket,这种可以立即回收这个连接，无需延迟，因为其上还没有数据收发，谈不到业务逻辑因此无需延迟；
            return; //直接返回
        }
        if(m_ifkickTimeCount == 1)
        {
            AddToTimerQueue(newc);
        }
        ++m_onlineUserCount;  //连入用户数量+1
        break;  //一般就是循环一次就跳出去
    } while (true);

}
