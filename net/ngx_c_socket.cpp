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
//构造函数
CSocekt::CSocekt()
{
    m_epollhandle = -1;

    m_ListenPortCount = 1;   //监听一个端口
    m_worker_connections = 1;

    m_free_connection_n = 0;
    m_connection_n = 0;

    m_pconnections = nullptr;
    m_pfree_connections = nullptr;

    //一些和网络通讯有关的常用变量值，供后续频繁使用时提高效率
    m_iLenPkgHeader = sizeof(COMM_PKG_HEADER);    //包头的sizeof值【占用的字节数】
    m_iLenMsgHeader =  sizeof(STRUC_MSG_HEADER);  //消息头的sizeof值【占用的字节数】

    m_iRecvMsgQueueCount = 0;
    pthread_mutex_init(&m_recvMessageQueueMutex, nullptr);
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

    if(m_pconnections != nullptr)//释放连接池
        delete [] m_pconnections;
    clearMsgRecvQueue();
    pthread_mutex_destroy(&m_recvMessageQueueMutex);
    return;
}

void CSocekt::clearMsgRecvQueue() {
    char * sTmpMempoint;
    CMemory& p_memory = CMemory::get_instance();

    //临界与否，日后再考虑，当前先不考虑。。。。。。如果将来有线程池再考虑临界问题
    while(!m_MsgRecvQueue.empty())
    {
        sTmpMempoint = m_MsgRecvQueue.front();
        m_MsgRecvQueue.pop_front();
        p_memory.FreeMemory(sTmpMempoint);
    }
}

//初始化函数【fork()子进程之前干这个事】
bool CSocekt::Initialize()
{
    ReadConf();  //读配置项
    bool reco = ngx_open_listening_sockets();
    return reco;
}

void CSocekt::ReadConf()
{
    Config_Nginx &p_config = Config_Nginx::get_instance();
    m_worker_connections = p_config.GetIntDefault("worker_connections",m_worker_connections); //epoll连接的最大项数
    m_ListenPortCount    = p_config.GetIntDefault("ListenPortCount",m_ListenPortCount);       //取得要监听的端口数量
    return;
}

bool CSocekt::ngx_open_listening_sockets() {
    Config_Nginx& conf = Config_Nginx::get_instance();
//    m_ListenPortCount = conf.GetIntDefault("ListenPortCount",1);


    struct sockaddr_in serv_addr;
    char               strinfo[100];

    memset(&serv_addr,0, sizeof(serv_addr));
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

//设置socket连接为非阻塞模式【这种函数的写法很固定】：非阻塞，概念在五章四节讲解的非常清楚【不断调用，不断调用这种：拷贝数据的时候是阻塞的】
bool CSocekt::setnonblocking(int sockfd)
{
    int nb=1; //0：清除，1：设置
    if(ioctl(sockfd, FIONBIO, &nb) == -1) //FIONBIO：设置/清除非阻塞I/O标记：0：清除，1：设置
    {
        return false;
    }
    return true;

    //如下也是一种写法，跟上边这种写法其实是一样的，但上边的写法更简单
    /*
    //fcntl:file control【文件控制】相关函数，执行各种描述符控制操作
    //参数1：所要设置的描述符，这里是套接字【也是描述符的一种】
    int opts = fcntl(sockfd, F_GETFL);  //用F_GETFL先获取描述符的一些标志信息
    if(opts < 0)
    {
        ngx_log_stderr(errno,"CSocekt::setnonblocking()中fcntl(F_GETFL)失败.");
        return false;
    }
    opts |= O_NONBLOCK; //把非阻塞标记加到原来的标记上，标记这是个非阻塞套接字【如何关闭非阻塞呢？opts &= ~O_NONBLOCK,然后再F_SETFL一下即可】
    if(fcntl(sockfd, F_SETFL, opts) < 0)
    {
        ngx_log_stderr(errno,"CSocekt::setnonblocking()中fcntl(F_SETFL)失败.");
        return false;
    }
    return true;
    */
}

void CSocekt::ngx_close_listening_sockets()
{
    for(int i = 0; i < m_ListenPortCount; i++) //要关闭这么多个监听端口
    {
        //ngx_log_stderr(0,"端口是%d,socketid是%d.",m_ListenSocketList[i]->port,m_ListenSocketList[i]->fd);
        close(m_ListenSocketList[i]->fd);
        ngx_log_error_core(NGX_LOG_INFO,0,"关闭监听端口%d!",m_ListenSocketList[i]->port); //显示一些信息到日志中
    }//end for(int i = 0; i < m_ListenPortCount; i++)
    return;
}

int CSocekt::ngx_epoll_init() {
    m_epollhandle = epoll_create(m_worker_connections);
    if (m_epollhandle == -1)
    {
        ngx_log_stderr(errno,"CSocekt::ngx_epoll_init()中epoll_create()失败.");
        exit(2); //这是致命问题了，直接退，资源由系统释放吧，这里不刻意释放了，比较麻烦
    }
    m_connection_n = m_worker_connections;

    m_pconnections = new ngx_connection_t[m_connection_n];


    int i = m_connection_n;
    lpngx_connection_t next = nullptr;
    lpngx_connection_t c = m_pconnections;
    do {
        i--;
        c[i].data = next;
        c[i].fd = -1;
        c[i].instance = 1;
        c[i].iCurrsequence = 0;
        next = &c[i];
    } while (i>0);
    m_free_connection_n = m_connection_n;
    m_pfree_connections = next;
    std::vector<lpngx_listening_t>::iterator pos;
    for(pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end(); ++pos)
    {
        c = ngx_get_connection((*pos)->fd); //从连接池中获取一个空闲连接对象
        if (c == nullptr)
        {
            //这是致命问题，刚开始怎么可能连接池就为空呢？
            ngx_log_stderr(errno,"CSocekt::ngx_epoll_init()中ngx_get_connection()失败.");
            exit(2); //这是致命问题了，直接退，资源由系统释放吧，这里不刻意释放了，比较麻烦
        }
        c->listening = (*pos);   //连接对象 和监听对象关联，方便通过连接对象找监听对象
        (*pos)->connection = c;  //监听对象 和连接对象关联，方便通过监听对象找连接对象

        //rev->accept = 1; //监听端口必须设置accept标志为1  ，这个是否有必要，再研究

        //对监听端口的读事件设置处理方法，因为监听端口是用来等对方连接的发送三路握手的，所以监听端口关心的就是读事件
        c->rhandler = &CSocekt::ngx_event_accept;

        //往监听socket上增加监听事件，从而开始让监听端口履行其职责【如果不加这行，虽然端口能连上，但不会触发ngx_epoll_process_events()里边的epoll_wait()往下走】
        if(ngx_epoll_add_event((*pos)->fd,       //socekt句柄
                               1,0,             //读，写【只关心读事件，所以参数2：readevent=1,而参数3：writeevent=0】
                               0,               //其他补充标记
                               EPOLL_CTL_ADD,   //事件类型【增加，还有删除/修改】
                               c                //连接池中的连接
        ) == -1)
        {
            exit(2); //有问题，直接退出，日志 已经写过了
        }
    } //end for
    return 1;
}


int CSocekt::ngx_epoll_add_event(int fd, int readevent, int writeevent, uint32_t otherflag, uint32_t eventtype,
                                 lpngx_connection_t c) {
    struct epoll_event ev;
    //int op;
    memset(&ev, 0, sizeof(ev));

    if(readevent==1){
        ev.events = EPOLLIN|EPOLLRDHUP;
    } else{

    }
    if(otherflag!=0){
        ev.events |= otherflag;
    }
    //以下这段代码抄自nginx官方,因为指针的最后一位【二进制位】肯定不是1，所以 和 c->instance做 |运算；到时候通过一些编码，既可以取得c的真实地址，又可以把此时此刻的c->instance值取到
    //比如c是个地址，可能的值是 0x00af0578，对应的二进制是‭101011110000010101111000‬，而 | 1后是0x00af0579
    ev.data.ptr = (void *)((uintptr_t)c | c->instance);
    if (epoll_ctl(m_epollhandle,eventtype,fd,&ev)==-1){
        ngx_log_stderr(errno,"CSocekt::ngx_epoll_add_event()中epoll_ctl(%d,%d,%d,%u,%u)失败.",fd,readevent,writeevent,otherflag,eventtype);
        return -1;
    }
    return 1;
}

int CSocekt::ngx_epoll_process_events(int timer) {
    int events = epoll_wait(m_epollhandle,m_events,NGX_MAX_EVENTS, timer);
    if(events == -1){
        //#define EINTR  4，EINTR错误的产生：当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误。
        //例如：在socket服务器端，设置了信号捕获机制，有子进程，当在父进程阻塞于慢系统调用时由父进程捕获到了一个有效信号时，内核会致使accept返回一个EINTR错误(被中断的系统调用)。
        if(errno == EINTR)
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

            //这里可以增加个日志，也可以不增加日志
            ngx_log_error_core(NGX_LOG_DEBUG,0,"CSocekt::ngx_epoll_process_events()中遇到了fd=-1的过期事件:%p.",c);
            continue; //这种事件就不处理即可
        }
        if(c->instance != instance)
        {
            //--------------------以下这些说法来自于资料--------------------------------------
            //什么时候这个条件成立呢？【换种问法：instance标志为什么可以判断事件是否过期呢？】
            //比如我们用epoll_wait取得三个事件，处理第一个事件时，因为业务需要，我们把这个连接关闭【麻烦就麻烦在这个连接被服务器关闭上了】，但是恰好第三个事件也跟这个连接有关；
            //因为第一个事件就把socket连接关闭了，显然第三个事件我们是不应该处理的【因为这是个过期事件】，若处理肯定会导致错误；
            //那我们上述把c->fd设置为-1，可以解决这个问题吗？ 能解决一部分问题，但另外一部分不能解决，不能解决的问题描述如下【这么离奇的情况应该极少遇到】：
            //a)处理第一个事件时，因为业务需要，我们把这个连接【假设套接字为50】关闭，同时设置c->fd = -1;并且调用ngx_free_connection将该连接归还给连接池；
            //b)处理第二个事件，恰好第二个事件是建立新连接事件，调用ngx_get_connection从连接池中取出的连接非常可能就是刚刚释放的第一个事件对应的连接池中的连接；
            //c)又因为a中套接字50被释放了，所以会被操作系统拿来复用，复用给了b)【一般这么快就被复用也是醉了】；
            //d)当处理第三个事件时，第三个事件其实是已经过期的，应该不处理，那怎么判断这第三个事件是过期的呢？ 【假设现在处理的是第三个事件，此时这个 连接池中的该连接 实际上已经被用作第二个事件对应的socket上了】；
            //依靠instance标志位能够解决这个问题，当调用ngx_get_connection从连接池中获取一个新连接时，我们把instance标志位置反，所以这个条件如果不成立，说明这个连接已经被挪作他用了；

            //--------------------我的个人思考--------------------------------------
            //如果收到了若干个事件，其中连接关闭也搞了多次，导致这个instance标志位被取反2次，那么，造成的结果就是：还是有可能遇到某些过期事件没有被发现【这里也就没有被continue】，照旧被当做没过期事件处理了；
            //如果是这样，那就只能被照旧处理了。可能会造成偶尔某个连接被误关闭？但是整体服务器程序运行应该是平稳，问题不大的，这种漏网而被当成没过期来处理的的过期事件应该是极少发生的

            ngx_log_error_core(NGX_LOG_DEBUG,0,"CSocekt::ngx_epoll_process_events()中遇到了instance值改变的过期事件:%p.",c);
            continue; //这种事件就不处理即可
        }

        revents = m_events[i].events;//取出事件类型
        if(revents & (EPOLLERR|EPOLLHUP)) //例如对方close掉套接字，这里会感应到【换句话说：如果发生了错误或者客户端断连】
        {
            //这加上读写标记，方便后续代码处理
            revents |= EPOLLIN|EPOLLOUT;   //EPOLLIN：表示对应的链接上有数据可以读出（TCP链接的远端主动关闭连接，也相当于可读事件，因为本服务器小处理发送来的FIN包）
            //EPOLLOUT：表示对应的连接上可以写入数据发送【写准备好】
            //ngx_log_stderr(errno,"2222222222222222222222222.");
        }
        if(revents & EPOLLIN)  //如果是读事件
        {
            //一个客户端新连入，这个会成立
            //c->r_ready = 1;               //标记可以读；【从连接池拿出一个连接时这个连接的所有成员都是0】
            (this->* (c->rhandler) )(c);    //注意括号的运用来正确设置优先级，防止编译出错；【如果是个新客户连入
            //如果新连接进入，这里执行的应该是CSocekt::ngx_event_accept(c)】
            //如果是已经连入，发送数据到这里，则这里执行的应该是 CSocekt::ngx_wait_request_handler
        }
        if(revents & EPOLLOUT) //如果是写事件
        {
            //....待扩展

            ngx_log_stderr(errno,"111111111111111111111111111111.");

        }
    }
    return 1;
}