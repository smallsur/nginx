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
    return;
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

//    if(m_pconnections != nullptr)//释放连接池
//        delete [] m_pconnections;

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
    /*ThreadItem *pSendQueue;
    m_threadVector.push_back(pSendQueue = new ThreadItem(this));                         //创建 一个新线程对象 并入到容器中
    err = pthread_create(&pSendQueue->_Handle, NULL, ServerSendQueueThread,pSendQueue); //创建线程，错误不返回到errno，一般返回错误码
    if(err != 0)
    {
        return false;
    }*/
    //---
    ThreadItem *pRecyconn;
    m_threadVector.push_back(pRecyconn = new ThreadItem(this));
    err = pthread_create(&pRecyconn->_Handle, nullptr, ServerRecyConnectionThread,pRecyconn);
    if(err != 0)
    {
        return false;
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

    //(4)多线程相关
    pthread_mutex_destroy(&m_connectionMutex);          //连接相关互斥量释放
    pthread_mutex_destroy(&m_sendMessageQueueMutex);    //发消息互斥量释放
    pthread_mutex_destroy(&m_recyconnqueueMutex);       //连接回收队列相关的互斥量释放
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

void CSocekt::ReadConf()
{
    Config_Nginx &p_config = Config_Nginx::get_instance();
    m_worker_connections = p_config.GetIntDefault("worker_connections",m_worker_connections); //epoll连接的最大项数
    m_ListenPortCount    = p_config.GetIntDefault("ListenPortCount",m_ListenPortCount);       //取得要监听的端口数量
    m_RecyConnectionWaitTime  = p_config.GetIntDefault("Sock_RecyConnectionWaitTime",m_RecyConnectionWaitTime); //等待这么些秒后才回收连接

    return;
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
        int                bcaction,         //补充动作，用于补充flag标记的不足  :  0：增加   1：去掉
        lpngx_connection_t pConn             //pConn：一个指针【其实是一个连接】，EPOLL_CTL_ADD时增加到红黑树中去，将来epoll_wait时能取出来用
)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));

    if(eventtype == EPOLL_CTL_ADD) //往红黑树中增加节点；
    {
        //红黑树从无到有增加节点
        ev.data.ptr = (void *)pConn;
        ev.events = flag;      //既然是增加节点，则不管原来是啥标记
        pConn->events = flag;  //这个连接本身也记录这个标记
    }
    else if(eventtype == EPOLL_CTL_MOD)
    {
        //节点已经在红黑树中，修改节点的事件信息
    }
    else
    {
        //删除红黑树中节点，目前没这个需求，所以将来再扩展
        return  1;  //先直接返回1表示成功
    }

    if(epoll_ctl(m_epollhandle,eventtype,fd,&ev) == -1)
    {
        ngx_log_stderr(errno,"CSocekt::ngx_epoll_oper_event()中epoll_ctl(%d,%ud,%ud,%d)失败.",fd,eventtype,flag,bcaction);
        return -1;
    }
    return 1;
}

//int CSocekt::ngx_epoll_add_event(int fd, int readevent, int writeevent, uint32_t otherflag, uint32_t eventtype,
//                                 lpngx_connection_t c) {
//    struct epoll_event ev;
//    //int op;
//    memset(&ev, 0, sizeof(ev));
//
//    if(readevent==1){
//        ev.events = EPOLLIN|EPOLLRDHUP;
//    } else{
//
//    }
//    if(otherflag!=0){
//        ev.events |= otherflag;
//    }
//    //以下这段代码抄自nginx官方,因为指针的最后一位【二进制位】肯定不是1，所以 和 c->instance做 |运算；到时候通过一些编码，既可以取得c的真实地址，又可以把此时此刻的c->instance值取到
//    //比如c是个地址，可能的值是 0x00af0578，对应的二进制是‭101011110000010101111000‬，而 | 1后是0x00af0579
//    ev.data.ptr = (void *)((uintptr_t)c | c->instance);
//    if (epoll_ctl(m_epollhandle,eventtype,fd,&ev)==-1){
//        ngx_log_stderr(errno,"CSocekt::ngx_epoll_add_event()中epoll_ctl(%d,%d,%d,%u,%u)失败.",fd,readevent,writeevent,otherflag,eventtype);
//        return -1;
//    }
//    return 1;
//}
//


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
    uintptr_t          instance;
    uint32_t           revents;
    for (int i = 0; i < events; ++i) {
        c = (lpngx_connection_t)(m_events[i].data.ptr);//ngx_epoll_add_event()给进去的，
        instance = (uintptr_t) c & 1;                             //将地址的最后一位取出来，用instance变量标识, 见ngx_epoll_add_event，该值是当时随着连接池中的连接一起给进来的
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
        if(c->instance != instance)
        {
            ngx_log_error_core(NGX_LOG_DEBUG,0,"CSocekt::ngx_epoll_process_events()中遇到了instance值改变的过期事件:%p.",c);
            continue; //这种事件就不处理即可
        }

        revents = m_events[i].events;//取出事件类型

        if(revents & (EPOLLERR|EPOLLHUP)) //例如对方close掉套接字，这里会感应到【换句话说：如果发生了错误或者客户端断连】
        {
            revents |= EPOLLIN|EPOLLOUT;   //EPOLLIN：表示对应的链接上有数据可以读出（TCP链接的远端主动关闭连接，也相当于可读事件，因为本服务器小处理发送来的FIN包）
        }

        if(revents & EPOLLIN)  //如果是读事件
        {
            //一个客户端新连入，这个会成立
            //c->r_ready = 1;               //标记可以读；【从连接池拿出一个连接时这个连接的所有成员都是0】
            (this->* (c->rhandler) )(c);    //注意括号的运用来正确设置优先级，防止编译出错；【如果是个新客户连入
        }
        if(revents & EPOLLOUT) //如果是写事件
        {
            ngx_log_stderr(errno,"111111111111111111111111111111.");
        }
    }
    return 1;
}