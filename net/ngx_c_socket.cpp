//
// Created by awen on 23-4-2.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/time.h>

#include "ngx_c_socket.h"
#include "ngx_global.h"
#include "ngx_c_conf.h"
#include "ngx_func.h"
#include "ngx_macro.h"
#include "ngx_c_lockmutex.h"

//构造函数
CSocekt::CSocekt()
{
    ///epoll
    m_epollhandle = -1;

    ///listen port
    m_ListenPortCount = 1;   //监听一个端口
    m_worker_connections = 1;

    ///connect setting
    m_RecyConnectionWaitTime = 60;

    ///网络通讯的常用变量值
    m_iLenPkgHeader = sizeof(COMM_PKG_HEADER);    //包头的sizeof值【占用的字节数】
    m_iLenMsgHeader =  sizeof(STRUC_MSG_HEADER);  //消息头的sizeof值【占用的字节数】

    ///queue
    m_iSendMsgQueueCount = 0;     //发消息队列大小
    m_totol_recyconnection_n = 0; //待释放连接队列大小
    m_cur_size_              = 0;     //当前计时队列尺寸
    m_timer_value_           = 0;     //当前计时队列头部的时间值


    ///在线用户相关
    m_onlineUserCount        = 0;     //在线用户数量统计，先给0
}

//释放函数
CSocekt::~CSocekt()
{
    //释放必须的内存
    std::vector<lpngx_listening_t>::iterator pos;
    for(pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end(); ++pos) //vector
    {
        delete (*pos); //一定要把指针指向的内存干掉，不然内存泄漏
    }//end for
    m_ListenSocketList.clear();

}

//初始化函数【fork()子进程之前干这个事】
bool CSocekt::Initialize()
{
    ReadConf();  //读配置项
    bool reco = ngx_open_listening_sockets();
    return reco;
}

//子进程中才需要执行的初始化函数
bool CSocekt::Initialize_subproc()
{
    //发消息互斥量初始化
    if(pthread_mutex_init(&m_sendMessageQueueMutex, NULL)  != 0)
    {
        ngx_log_stderr(0,"CSocekt::Initialize()中pthread_mutex_init(&m_sendMessageQueueMutex)失败.");
        return false;
    }
    //连接相关互斥量初始化
    if(pthread_mutex_init(&m_connectionMutex, NULL)  != 0)
    {
        ngx_log_stderr(0,"CSocekt::Initialize()中pthread_mutex_init(&m_connectionMutex)失败.");
        return false;
    }
    //连接回收队列相关互斥量初始化
    if(pthread_mutex_init(&m_recyconnqueueMutex, NULL)  != 0)
    {
        ngx_log_stderr(0,"CSocekt::Initialize()中pthread_mutex_init(&m_recyconnqueueMutex)失败.");
        return false;
    }

    //初始化发消息相关信号量，信号量用于进程/线程 之间的同步，虽然 互斥量[pthread_mutex_lock]和 条件变量[pthread_cond_wait]都是线程之间的同步手段，但
    //这里用信号量实现 则 更容易理解，更容易简化问题，使用书写的代码短小且清晰；
    //第二个参数=0，表示信号量在线程之间共享，确实如此 ，如果非0，表示在进程之间共享
    //第三个参数=0，表示信号量的初始值，为0时，调用sem_wait()就会卡在那里卡着
    if(sem_init(&m_semEventSendQueue,0,0) == -1)
    {
        ngx_log_stderr(0,"CSocekt::Initialize()中sem_init(&m_semEventSendQueue,0,0)失败.");
        return false;
    }

    //创建线程
    int err;
    ThreadItem *pRecyconn;
    m_threadVector.push_back(pRecyconn = new ThreadItem(this));
    err = pthread_create(&pRecyconn->_Handle, nullptr, ServerRecyConnectionThread,pRecyconn);
    if(err != 0)
    {
        return false;
    }
    ThreadItem *pSendQueue;    //专门用来发送数据的线程
    m_threadVector.push_back(pSendQueue = new ThreadItem(this));                         //创建 一个新线程对象 并入到容器中
    err = pthread_create(&pSendQueue->_Handle, nullptr, ServerSendQueueThread,pSendQueue); //创建线程，错误不返回到errno，一般返回错误码
    if(err != 0)
    {
        return false;
    }
    if(m_ifkickTimeCount == 1)  //是否开启踢人时钟，1：开启   0：不开启
    {
        ThreadItem *pTimemonitor;    //专门用来处理到期不发心跳包的用户踢出的线程
        m_threadVector.push_back(pTimemonitor = new ThreadItem(this));
        err = pthread_create(&pTimemonitor->_Handle, NULL, ServerTimerQueueMonitorThread,pTimemonitor);
        if(err != 0)
        {
            ngx_log_stderr(0,"CSocekt::Initialize_subproc()中pthread_create(ServerTimerQueueMonitorThread)失败.");
            return false;
        }
    }
    return true;
}

//关闭退出函数[子进程中执行]
void CSocekt::Shutdown_subproc()
{
    //(1)把干活的线程停止掉，注意 系统应该尝试通过设置 g_stopEvent = 1来 开始让整个项目停止
    std::vector<ThreadItem*>::iterator iter;
    for(iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++)
    {
        pthread_join((*iter)->_Handle, NULL); //等待一个线程终止
    }
    //(2)释放一下new出来的ThreadItem【线程池中的线程】
    for(iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++)
    {
        if(*iter)
            delete *iter;
    }
    m_threadVector.clear();

    //(3)队列相关
    clearMsgSendQueue();
    clearconnection();
    clearAllFromTimerQueue();

    //(4)多线程相关
    pthread_mutex_destroy(&m_connectionMutex);          //连接相关互斥量释放
    pthread_mutex_destroy(&m_sendMessageQueueMutex);    //发消息互斥量释放
    pthread_mutex_destroy(&m_recyconnqueueMutex);       //连接回收队列相关的互斥量释放
    pthread_mutex_destroy(&m_timequeueMutex);           //时间处理队列相关的互斥量释放
    sem_destroy(&m_semEventSendQueue);                  //发消息相关线程信号量释放
}

void CSocekt::clearMsgSendQueue()
{
    char * sTmpMempoint;
    CMemory &p_memory = CMemory::get_instance();

    while(!m_MsgSendQueue.empty())
    {
        sTmpMempoint = m_MsgSendQueue.front();
        m_MsgSendQueue.pop_front();
        p_memory.FreeMemory(sTmpMempoint);
    }
}

void CSocekt::ReadConf() {
    Config_Nginx &p_config = Config_Nginx::get_instance();
    m_worker_connections = p_config.GetIntDefault("worker_connections", m_worker_connections); //epoll连接的最大项数
    m_ListenPortCount = p_config.GetIntDefault("ListenPortCount", m_ListenPortCount);       //取得要监听的端口数量
    m_RecyConnectionWaitTime = p_config.GetIntDefault("Sock_RecyConnectionWaitTime",
                                                      m_RecyConnectionWaitTime); //等待这么些秒后才回收连接
    m_ifkickTimeCount         = p_config.GetIntDefault("Sock_WaitTimeEnable",0);                                //是否开启踢人时钟，1：开启   0：不开启
    m_iWaitTime               = p_config.GetIntDefault("Sock_MaxWaitTime",m_iWaitTime);                         //多少秒检测一次是否 心跳超时，只有当Sock_WaitTimeEnable = 1时，本项才有用
    m_iWaitTime               = (m_iWaitTime > 5)?m_iWaitTime:5;                                                 //不建议低于5秒钟，因为无需太频繁
    m_ifTimeOutKick           = p_config.GetIntDefault("Sock_TimeOutKick",0);                                   //当时间到达Sock_MaxWaitTime指定的时间时，直接把客户端踢出去，只有当Sock_WaitTimeEnable = 1时，本项才有用

    m_floodAkEnable          = p_config.GetIntDefault("Sock_FloodAttackKickEnable",0);                          //Flood攻击检测是否开启,1：开启   0：不开启
    m_floodTimeInterval      = p_config.GetIntDefault("Sock_FloodTimeInterval",100);                            //表示每次收到数据包的时间间隔是100(毫秒)
    m_floodKickCount         = p_config.GetIntDefault("Sock_FloodKickCounter",10);                              //累积多少次踢出此人
}

////子进程初始化监听
bool CSocekt::ngx_open_listening_sockets()
{
    Config_Nginx& conf = Config_Nginx::get_instance();

    struct sockaddr_in serv_addr;
    char               strinfo[100];

    memset(&serv_addr,0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;                //选择协议族为IPV4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //监听本地所有的IP地址；INADDR_ANY表示的是一个服务器上所有的网卡（服务器可能不止一个网卡）多个本地ip地址都进行绑定端口号，进行侦听。

    for (int i = 0; i < m_ListenPortCount; ++i) {
        int isock = socket(AF_INET,SOCK_STREAM,0);
        if(isock == -1)
        {
            ngx_log_stderr(errno,"CSocekt::Initialize()中socket()失败,i=%d.",i);
            //其实这里直接退出，那如果以往有成功创建的socket呢？就没得到释放吧，当然走到这里表示程序不正常，应该整个退出，也没必要释放了
            return false;
        }
        int reuseaddr = 1;  //1:打开对应的设置项
        if(setsockopt(isock,SOL_SOCKET, SO_REUSEADDR,(const void *) &reuseaddr, sizeof(reuseaddr)) == -1)
        {
            ngx_log_stderr(errno,"CSocekt::Initialize()中setsockopt(SO_REUSEADDR)失败,i=%d.",i);
            close(isock); //无需理会是否正常执行了
            return false;
        }
        if(!setnonblocking(isock))
        {
            ngx_log_stderr(errno,"CSocekt::Initialize()中setnonblocking()失败,i=%d.",i);
            close(isock);
            return false;
        }
        strinfo[0] = 0;
        sprintf(strinfo,"ListenPort%d",i);
        int iport = conf.GetIntDefault(strinfo,10000);
        serv_addr.sin_port = htons((in_port_t)iport);   //in_port_t其实就是uint16_t
        if(bind(isock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        {
            ngx_log_stderr(errno,"CSocekt::Initialize()中bind()失败,i=%d.",i);
            close(isock);
            return false;
        }
        //开始监听
        if(listen(isock,NGX_LISTEN_BACKLOG) == -1)
        {
            ngx_log_stderr(errno,"CSocekt::Initialize()中listen()失败,i=%d.",i);
            close(isock);
            return false;
        }
        //可以，放到列表里来
        lpngx_listening_t p_listensocketitem = new ngx_listening_t; //千万不要写错，注意前边类型是指针，后边类型是一个结构体
        memset(p_listensocketitem,0,sizeof(ngx_listening_t));      //注意后边用的是 ngx_listening_t而不是lpngx_listening_t
        p_listensocketitem->port = iport;                          //记录下所监听的端口号
        p_listensocketitem->fd   = isock;                          //套接字木柄保存下来
        ngx_log_error_core(NGX_LOG_INFO,0,"监听%d端口成功!",iport); //显示一些信息到日志中
        m_ListenSocketList.push_back(p_listensocketitem);
    }
    if(m_ListenSocketList.size()<=0){
        return false;
    }
    return true;
}
////子进程关闭监听
void CSocekt::ngx_close_listening_sockets()
{
    for(int i = 0; i < m_ListenPortCount; i++) //要关闭这么多个监听端口
    {
        close(m_ListenSocketList[i]->fd);
        ngx_log_error_core(NGX_LOG_INFO,0,"关闭监听端口%d!",m_ListenSocketList[i]->port); //显示一些信息到日志中
    }
    return;
}

////设置socket非阻塞
bool CSocekt::setnonblocking(int sockfd)
{
    int nb=1; //0：清除，1：设置
    if(ioctl(sockfd, FIONBIO, &nb) == -1) //FIONBIO：设置/清除非阻塞I/O标记：0：清除，1：设置
    {
        return false;
    }
    return true;

}

//将一个待发送消息入到发消息队列中
void CSocekt::msgSend(char *psendbuf)
{
    CLock lock(&m_sendMessageQueueMutex);
    m_MsgSendQueue.push_back(psendbuf);
    ++m_iSendMsgQueueCount;   //原子操作

    //将信号量的值+1,这样其他卡在sem_wait的就可以走下去
    if(sem_post(&m_semEventSendQueue)==-1)  //让ServerSendQueueThread()流程走下来干活
    {
        ngx_log_stderr(0,"CSocekt::msgSend()sem_post(&m_semEventSendQueue)失败.");
    }
}

//主动关闭一个连接时的要做些善后的处理函数
void CSocekt::zdClosesocketProc(lpngx_connection_t p_Conn)
{
    if(m_ifkickTimeCount == 1)
    {
        DeleteFromTimerQueue(p_Conn); //从时间队列中把连接干掉
    }
    if(p_Conn->fd != -1)
    {
        close(p_Conn->fd); //这个socket关闭，关闭后epoll就会被从红黑树中删除，所以这之后无法收到任何epoll事件
        p_Conn->fd = -1;
    }

    if(p_Conn->iThrowsendCount > 0)
        --p_Conn->iThrowsendCount;   //归0

    inRecyConnectQueue(p_Conn);
}

////初始化epoll，并将监听的socket添加进链接，同时互相关注
int CSocekt::ngx_epoll_init() {
    ///初始化epoll
    m_epollhandle = epoll_create(m_worker_connections);
    if (m_epollhandle == -1)
    {
        ngx_log_stderr(errno,"CSocekt::ngx_epoll_init()中epoll_create()失败.");
        exit(2); //这是致命问题了，直接退，资源由系统释放吧，这里不刻意释放了，比较麻烦
    }
    ///初始化连接池
    initconnection();
    ///监听的socket添加进链接，同时互相关注
    std::vector<lpngx_listening_t>::iterator pos;
    for(pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end(); ++pos)
    {
        lpngx_connection_t p_Conn = ngx_get_connection((*pos)->fd); //从连接池中获取一个空闲连接对象
        if (p_Conn == NULL)
        {
            //这是致命问题，刚开始怎么可能连接池就为空呢？
            ngx_log_stderr(errno,"CSocekt::ngx_epoll_init()中ngx_get_connection()失败.");
            exit(2); //这是致命问题了，直接退，资源由系统释放吧，这里不刻意释放了，比较麻烦
        }
        p_Conn->listening = (*pos);   //连接对象 和监听对象关联，方便通过连接对象找监听对象
        (*pos)->connection = p_Conn;  //监听对象 和连接对象关联，方便通过监听对象找连接对象

        p_Conn->rhandler = &CSocekt::ngx_event_accept;
//        p_Conn->whandler = &CSocekt::ngx_write_request_handler;

        if(ngx_epoll_oper_event(
                (*pos)->fd,         //socekt句柄
                EPOLL_CTL_ADD,      //事件类型，这里是增加
                EPOLLIN|EPOLLRDHUP, //标志，这里代表要增加的标志,EPOLLIN：可读，EPOLLRDHUP：TCP连接的远端关闭或者半关闭
                0,                  //对于事件类型为增加的，不需要这个参数
                p_Conn              //连接池中的连接
        ) == -1)
        {
            exit(2); //有问题，直接退出，日志 已经写过了
        }
    }
    return 1;
}
//对epoll事件的具体操作
//返回值：成功返回1，失败返回-1；
int CSocekt::ngx_epoll_oper_event(
        int                fd,               //句柄，一个socket
        uint32_t           eventtype,        //事件类型，一般是EPOLL_CTL_ADD，EPOLL_CTL_MOD，EPOLL_CTL_DEL ，说白了就是操作epoll红黑树的节点(增加，修改，删除)
        uint32_t           flag,             //标志，具体含义取决于eventtype
        int                bcaction,         //补充动作，用于补充flag标记的不足  :  0：增加   1：去掉 2：完全覆盖 ,eventtype是EPOLL_CTL_MOD时这个参数就有用
        lpngx_connection_t pConn             //pConn：一个指针【其实是一个连接】，EPOLL_CTL_ADD时增加到红黑树中去，将来epoll_wait时能取出来用
)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));

    if(eventtype == EPOLL_CTL_ADD) //往红黑树中增加节点；
    {
        //红黑树从无到有增加节点
        //ev.data.ptr = (void *)pConn;
        ev.events = flag;      //既然是增加节点，则不管原来是啥标记
        pConn->events = flag;  //这个连接本身也记录这个标记
    }
    else if(eventtype == EPOLL_CTL_MOD)
    {
        //节点已经在红黑树中，修改节点的事件信息
        ev.events = pConn->events;  //先把标记恢复回来
        if(bcaction == 0)
        {
            //增加某个标记
            ev.events |= flag;
        }
        else if(bcaction == 1)
        {
            //去掉某个标记
            ev.events &= ~flag;
        }
        else
        {
            //完全覆盖某个标记
            ev.events = flag;      //完全覆盖
        }
        pConn->events = ev.events; //记录该标记
    }
    else
    {
        //删除红黑树中节点，目前没这个需求【socket关闭这项会自动从红黑树移除】，所以将来再扩展
        return  1;  //先直接返回1表示成功
    }

    //原来的理解中，绑定ptr这个事，只在EPOLL_CTL_ADD的时候做一次即可，但是发现EPOLL_CTL_MOD似乎会破坏掉.data.ptr，因此不管是EPOLL_CTL_ADD，还是EPOLL_CTL_MOD，都给进去
    //找了下内核源码SYSCALL_DEFINE4(epoll_ctl, int, epfd, int, op, int, fd,		struct epoll_event __user *, event)，感觉真的会覆盖掉：
    //copy_from_user(&epds, event, sizeof(struct epoll_event)))，感觉这个内核处理这个事情太粗暴了
    ev.data.ptr = (void *)pConn;

    if(epoll_ctl(m_epollhandle,eventtype,fd,&ev) == -1)
    {
        ngx_log_stderr(errno,"CSocekt::ngx_epoll_oper_event()中epoll_ctl(%d,%ud,%ud,%d)失败.",fd,eventtype,flag,bcaction);
        return -1;
    }
    return 1;
}

int CSocekt::ngx_epoll_process_events(int timer) {
    int events = epoll_wait(m_epollhandle,m_events,NGX_MAX_EVENTS, timer);

    if(events == -1){
        //#define EINTR  4，EINTR错误的产生：当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误。
        //例如：在socket服务器端，设置了信号捕获机制，有子进程，当在父进程阻塞于慢系统调用时由父进程捕获到了一个有效信号时，内核会致使accept返回一个EINTR错误(被中断的系统调用)。
        if(errno == EINTR)///中断信号
        {
            //信号所致，直接返回，一般认为这不是毛病，但还是打印下日志记录一下，因为一般也不会人为给worker进程发送消息
            ngx_log_error_core(NGX_LOG_INFO,errno,"CSocekt::ngx_epoll_process_events()中epoll_wait()失败!");
            return 1;  //正常返回
        }
        else
        {
            //这被认为应该是有问题，记录日志
            ngx_log_error_core(NGX_LOG_ALERT,errno,"CSocekt::ngx_epoll_process_events()中epoll_wait()失败!");
            return 0;  //非正常返回
        }
    }
    if(events == 0) //超时，但没事件来
    {
        if(timer != -1)
        {
            //要求epoll_wait阻塞一定的时间而不是一直阻塞，这属于阻塞到时间了，则正常返回
            return 1;
        }
        //无限等待【所以不存在超时】，但却没返回任何事件，这应该不正常有问题
        ngx_log_error_core(NGX_LOG_ALERT,0,"CSocekt::ngx_epoll_process_events()中epoll_wait()没超时却没返回任何事件!");
        return 0; //非正常返回
    }
    lpngx_connection_t c;
//    uintptr_t          instance;
    uint32_t           revents;
    for (int i = 0; i < events; ++i) {
        c = (lpngx_connection_t)(m_events[i].data.ptr);//ngx_epoll_add_event()给进去的，
//        instance = (uintptr_t) c & 1;                             //将地址的最后一位取出来，用instance变量标识, 见ngx_epoll_add_event，该值是当时随着连接池中的连接一起给进来的
        c = (lpngx_connection_t) ((uintptr_t)c & (uintptr_t) ~1); //最后1位干掉，得到真正的c地址

        if(c->fd == -1)  //一个套接字，当关联一个 连接池中的连接【对象】时，这个套接字值是要给到c->fd的，
            //那什么时候这个c->fd会变成-1呢？关闭连接时这个fd会被设置为-1，哪行代码设置的-1再研究，但应该不是ngx_free_connection()函数设置的-1
        {
            //比如我们用epoll_wait取得三个事件，处理第一个事件时，因为业务需要，我们把这个连接关闭，那我们应该会把c->fd设置为-1；
            //第二个事件照常处理
            //第三个事件，假如这第三个事件，也跟第一个事件对应的是同一个连接，那这个条件就会成立；那么这种事件，属于过期事件，不该处理
            ngx_log_error_core(NGX_LOG_DEBUG,0,"CSocekt::ngx_epoll_process_events()中遇到了fd=-1的过期事件:%p.",c);
            continue; //这种事件就不处理即可
        }

        revents = m_events[i].events;//取出事件类型

        if(revents & EPOLLIN)  //如果是读事件
        {
            //一个客户端新连入，这个会成立
            //c->r_ready = 1;               //标记可以读；【从连接池拿出一个连接时这个连接的所有成员都是0】
            (this->* (c->rhandler) )(c);    //注意括号的运用来正确设置优先级，防止编译出错；【如果是个新客户连入
        }

        if(revents & EPOLLOUT) //如果是写事件【对方关闭连接也触发这个，再研究。。。。。。】，注意上边的 if(revents & (EPOLLERR|EPOLLHUP))  revents |= EPOLLIN|EPOLLOUT; 读写标记都给加上了
        {
            if(revents & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) //客户端关闭，如果服务器端挂着一个写通知事件，则这里个条件是可能成立的
            {
                --c->iThrowsendCount;
            }
            else
            {
                (this->* (c->whandler) )(c);   //如果有数据没有发送完毕，由系统驱动来发送，则这里执行的应该是 CSocekt::ngx_write_request_handler()
            }

        }
    }
    return 1;
}

//--------------------------------------------------------------------
//处理发送消息队列的线程
void* CSocekt::ServerSendQueueThread(void* threadData)
{
    ThreadItem *pThread = static_cast<ThreadItem*>(threadData);
    CSocekt *pSocketObj = pThread->_pThis;
    int err;
    std::list <char *>::iterator pos,pos2,posend;

    char *pMsgBuf;
    LPSTRUC_MSG_HEADER	pMsgHeader;
    LPCOMM_PKG_HEADER   pPkgHeader;
    lpngx_connection_t  p_Conn;
    unsigned short      itmp;
    ssize_t             sendsize;

    CMemory &p_memory = CMemory::get_instance();

    while(g_stopEvent == 0) //不退出
    {
        //如果信号量值>0，则 -1(减1) 并走下去，否则卡这里卡着【为了让信号量值+1，可以在其他线程调用sem_post达到，实际上在CSocekt::msgSend()调用sem_post就达到了让这里sem_wait走下去的目的】
        //******如果被某个信号中断，sem_wait也可能过早的返回，错误为EINTR；
        //整个程序退出之前，也要sem_post()一下，确保如果本线程卡在sem_wait()，也能走下去从而让本线程成功返回
        if(sem_wait(&pSocketObj->m_semEventSendQueue) == -1)
        {
            //失败？及时报告，其他的也不好干啥
            if(errno != EINTR) //这个我就不算个错误了【当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误。】
                ngx_log_stderr(errno,"CSocekt::ServerSendQueueThread()中sem_wait(&pSocketObj->m_semEventSendQueue)失败.");
        }

        //一般走到这里都表示需要处理数据收发了
        if(g_stopEvent != 0)  //要求整个进程退出
            break;

        if(pSocketObj->m_iSendMsgQueueCount > 0) //原子的
        {
            err = pthread_mutex_lock(&pSocketObj->m_sendMessageQueueMutex); //因为我们要操作发送消息对列m_MsgSendQueue，所以这里要临界
            if(err != 0) ngx_log_stderr(err,"CSocekt::ServerSendQueueThread()中pthread_mutex_lock()失败，返回的错误码为%d!",err);

            pos    = pSocketObj->m_MsgSendQueue.begin();
            posend = pSocketObj->m_MsgSendQueue.end();

            while(pos != posend)
            {
                pMsgBuf = (*pos);                          //拿到的每个消息都是 消息头+包头+包体【但要注意，我们是不发送消息头给客户端的】
                pMsgHeader = (LPSTRUC_MSG_HEADER)pMsgBuf;  //指向消息头
                pPkgHeader = (LPCOMM_PKG_HEADER)(pMsgBuf+pSocketObj->m_iLenMsgHeader);	//指向包头
                p_Conn = pMsgHeader->pConn;

                //包过期，因为如果 这个连接被回收，比如在ngx_close_connection(),inRecyConnectQueue()中都会自增iCurrsequence
                //而且这里有没必要针对 本连接 来用m_connectionMutex临界 ,只要下面条件成立，肯定是客户端连接已断，要发送的数据肯定不需要发送了
                if(p_Conn->iCurrsequence != pMsgHeader->iCurrsequence)
                {
                    //本包中保存的序列号与p_Conn【连接池中连接】中实际的序列号已经不同，丢弃此消息，小心处理该消息的删除
                    pos2=pos;
                    pos++;
                    pSocketObj->m_MsgSendQueue.erase(pos2);
                    --pSocketObj->m_iSendMsgQueueCount; //发送消息队列容量少1
                    p_memory.FreeMemory(pMsgBuf);
                    continue;
                } //end if

                if(p_Conn->iThrowsendCount > 0)
                {
                    //靠系统驱动来发送消息，所以这里不能再发送
                    pos++;
                    continue;
                }

                //走到这里，可以发送消息，一些必须的信息记录，要发送的东西也要从发送队列里干掉
                p_Conn->psendMemPointer = pMsgBuf;      //发送后释放用的，因为这段内存是new出来的
                pos2=pos;
                pos++;
                pSocketObj->m_MsgSendQueue.erase(pos2);
                --pSocketObj->m_iSendMsgQueueCount;      //发送消息队列容量少1
                p_Conn->psendbuf = (char *)pPkgHeader;   //要发送的数据的缓冲区指针，因为发送数据不一定全部都能发送出去，我们要记录数据发送到了哪里，需要知道下次数据从哪里开始发送
                itmp = ntohs(pPkgHeader->pkgLen);        //包头+包体 长度 ，打包时用了htons【本机序转网络序】，所以这里为了得到该数值，用了个ntohs【网络序转本机序】；
                p_Conn->isendlen = itmp;                 //要发送多少数据，因为发送数据不一定全部都能发送出去，我们需要知道剩余有多少数据还没发送

                //这里是重点，我们采用 epoll水平触发的策略，能走到这里的，都应该是还没有投递 写事件 到epoll中
                //epoll水平触发发送数据的改进方案：
                //开始不把socket写事件通知加入到epoll,当我需要写数据的时候，直接调用write/send发送数据；
                //如果返回了EAGIN【发送缓冲区满了，需要等待可写事件才能继续往缓冲区里写数据】，此时，我再把写事件通知加入到epoll，
                //此时，就变成了在epoll驱动下写数据，全部数据发送完毕后，再把写事件通知从epoll中干掉；
                //优点：数据不多的时候，可以避免epoll的写事件的增加/删除，提高了程序的执行效率；
                //(1)直接调用write或者send发送数据
                ngx_log_stderr(errno,"即将发送数据%ud。",p_Conn->isendlen);

                sendsize = pSocketObj->sendproc(p_Conn,p_Conn->psendbuf,p_Conn->isendlen); //注意参数
                if(sendsize > 0)
                {
                    if(sendsize == p_Conn->isendlen) //成功发送出去了数据，一下就发送出去这很顺利
                    {
                        //成功发送的和要求发送的数据相等，说明全部发送成功了 发送缓冲区去了【数据全部发完】
                        p_memory.FreeMemory(p_Conn->psendMemPointer);  //释放内存
                        p_Conn->psendMemPointer = nullptr;
                        p_Conn->iThrowsendCount = 0;  //这行其实可以没有，因此此时此刻这东西就是=0的
                        ngx_log_stderr(0,"CSocekt::ServerSendQueueThread()中数据发送完毕，很好。"); //做个提示吧，商用时可以干掉
                    }
                    else  //没有全部发送完毕(EAGAIN)，数据只发出去了一部分，但肯定是因为 发送缓冲区满了,那么
                    {
                        //发送到了哪里，剩余多少，记录下来，方便下次sendproc()时使用
                        p_Conn->psendbuf = p_Conn->psendbuf + sendsize;
                        p_Conn->isendlen = p_Conn->isendlen - sendsize;
                        //因为发送缓冲区慢了，所以 现在我要依赖系统通知来发送数据了
                        ++p_Conn->iThrowsendCount;             //标记发送缓冲区满了，需要通过epoll事件来驱动消息的继续发送【原子+1，且不可写成p_Conn->iThrowsendCount = p_Conn->iThrowsendCount +1 ，这种写法不是原子+1】
                        if(pSocketObj->ngx_epoll_oper_event(
                                p_Conn->fd,         //socket句柄
                                EPOLL_CTL_MOD,      //事件类型，这里是增加【因为我们准备增加个写通知】
                                EPOLLOUT,           //标志，这里代表要增加的标志,EPOLLOUT：可写【可写的时候通知我】
                                0,                  //对于事件类型为增加的，EPOLL_CTL_MOD需要这个参数, 0：增加   1：去掉 2：完全覆盖
                                p_Conn              //连接池中的连接
                        ) == -1)
                        {
                            //有这情况发生？这可比较麻烦，不过先do nothing
                            ngx_log_stderr(errno,"CSocekt::ServerSendQueueThread()ngx_epoll_oper_event()失败.");
                        }

                        ngx_log_stderr(errno,"CSocekt::ServerSendQueueThread()中数据没发送完毕【发送缓冲区满】，整个要发送%d，实际发送了%d。",p_Conn->isendlen,sendsize);

                    } //end if(sendsize > 0)
                    continue;  //继续处理其他消息
                }  //end if(sendsize > 0)

                    //能走到这里，应该是有点问题的
                else if(sendsize == 0)
                {
                    //发送0个字节，首先因为我发送的内容不是0个字节的；
                    //然后如果发送 缓冲区满则返回的应该是-1，而错误码应该是EAGAIN，所以我综合认为，这种情况我就把这个发送的包丢弃了【按对端关闭了socket处理】
                    //这个打印下日志，我还真想观察观察是否真有这种现象发生
                    //ngx_log_stderr(errno,"CSocekt::ServerSendQueueThread()中sendproc()居然返回0？"); //如果对方关闭连接出现send=0，那么这个日志可能会常出现，商用时就 应该干掉
                    //然后这个包干掉，不发送了
                    p_memory.FreeMemory(p_Conn->psendMemPointer);  //释放内存
                    p_Conn->psendMemPointer = NULL;
                    p_Conn->iThrowsendCount = 0;  //这行其实可以没有，因此此时此刻这东西就是=0的
                    continue;
                }

                    //能走到这里，继续处理问题
                else if(sendsize == -1)
                {
                    //发送缓冲区已经满了【一个字节都没发出去，说明发送 缓冲区当前正好是满的】
                    ++p_Conn->iThrowsendCount; //标记发送缓冲区满了，需要通过epoll事件来驱动消息的继续发送
                    if(pSocketObj->ngx_epoll_oper_event(
                            p_Conn->fd,         //socket句柄
                            EPOLL_CTL_MOD,      //事件类型，这里是增加【因为我们准备增加个写通知】
                            EPOLLOUT,           //标志，这里代表要增加的标志,EPOLLOUT：可写【可写的时候通知我】
                            0,                  //对于事件类型为增加的，EPOLL_CTL_MOD需要这个参数, 0：增加   1：去掉 2：完全覆盖
                            p_Conn              //连接池中的连接
                    ) == -1)
                    {
                        //有这情况发生？这可比较麻烦，不过先do nothing
                        ngx_log_stderr(errno,"CSocekt::ServerSendQueueThread()中ngx_epoll_add_event()_2失败.");
                    }
                    continue;
                }

                else
                {
                    //能走到这里的，应该就是返回值-2了，一般就认为对端断开了，等待recv()来做断开socket以及回收资源
                    p_memory.FreeMemory(p_Conn->psendMemPointer);  //释放内存
                    p_Conn->psendMemPointer = NULL;
                    p_Conn->iThrowsendCount = 0;  //这行其实可以没有，因此此时此刻这东西就是=0的
                    continue;
                }

            } //end while(pos != posend)

            err = pthread_mutex_unlock(&pSocketObj->m_sendMessageQueueMutex);
            if(err != 0)  ngx_log_stderr(err,"CSocekt::ServerSendQueueThread()pthread_mutex_unlock()失败，返回的错误码为%d!",err);

        } //if(pSocketObj->m_iSendMsgQueueCount > 0)
    } //end while

    return (void*)0;
}


//测试是否flood攻击成立，成立则返回true，否则返回false
bool CSocekt::TestFlood(lpngx_connection_t pConn)
{
    struct  timeval sCurrTime;   //当前时间结构
    uint64_t        iCurrTime;   //当前时间（单位：毫秒）
    bool  reco      = false;

    gettimeofday(&sCurrTime, NULL); //取得当前时间
    iCurrTime =  (sCurrTime.tv_sec * 1000 + sCurrTime.tv_usec / 1000);  //毫秒
    if((iCurrTime - pConn->FloodkickLastTime) < m_floodTimeInterval)   //两次收到包的时间 < 100毫秒
    {
        //发包太频繁记录
        pConn->FloodAttackCount++;
        pConn->FloodkickLastTime = iCurrTime;
    }
    else
    {
        //既然发包不这么频繁，则恢复计数值
        pConn->FloodAttackCount = 0;
        pConn->FloodkickLastTime = iCurrTime;
    }

    //ngx_log_stderr(0,"pConn->FloodAttackCount=%d,m_floodKickCount=%d.",pConn->FloodAttackCount,m_floodKickCount);

    if(pConn->FloodAttackCount >= m_floodKickCount)
    {
        //可以踢此人的标志
        reco = true;
    }
    return reco;
}





