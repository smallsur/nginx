#ifndef __NGX_SOCKET_H__
#define __NGX_SOCKET_H__

#include <vector>      //vector
#include <sys/epoll.h> //epoll
#include <sys/socket.h>
#include <list>
#include <pthread.h>

#include "ngx_comm.h"


//一些宏定义放在这里-----------------------------------------------------------
#define NGX_LISTEN_BACKLOG  511    //已完成连接队列，nginx给511，我们也先按照这个来：不懂这个数字的同学参考第五章第四节
#define NGX_MAX_EVENTS      512    //epoll_wait一次最多接收这么多个事件，nginx中缺省是512，我们这里固定给成512就行，没太大必要修改

typedef struct ngx_listening_s   ngx_listening_t, *lpngx_listening_t;
typedef struct ngx_connection_s  ngx_connection_t,*lpngx_connection_t;
typedef class  CSocekt           CSocekt;

typedef void (CSocekt::*ngx_event_handler_pt)(lpngx_connection_t c); //定义成员函数指针

//一些专用结构定义放在这里，暂时不考虑放ngx_global.h里了-------------------------
struct ngx_listening_s  //和监听端口有关的结构
{
    int                       port;        //监听的端口号
    int                       fd;          //套接字句柄socket
    lpngx_connection_t        connection;  //连接池中的一个连接，注意这是个指针
};

//以下三个结构是非常重要的三个结构，我们遵从官方nginx的写法；
//(1)该结构表示一个TCP连接【客户端主动发起的、Nginx服务器被动接受的TCP连接】
struct ngx_connection_s
{
    ///连接相关
    int                       fd;             //套接字句柄socket
    lpngx_listening_t         listening;      //如果这个链接被分配给了一个监听套接字，那么这个里边就指向监听套接字对应的那个lpngx_listening_t的内存首地址
    unsigned                  instance:1;     //【位域】失效标志位：0：有效，1：失效【这个是官方nginx提供，到底有什么用，ngx_epoll_process_events()中详解】
    uint64_t                  iCurrsequence;  //我引入的一个序号，每次分配出去时+1，此法也有可能在一定程度上检测错包废包，具体怎么用，用到了再说
    struct sockaddr           s_sockaddr;     //保存对方地址信息用的

    ///事件相关，读事件，写事件
    ngx_event_handler_pt      rhandler;       //读事件的相关处理方法
    ngx_event_handler_pt      whandler;       //写事件的相关处理方法
    uint8_t                   w_ready;        //写准备好标记

    ///和收包有关
    unsigned char             curStat;                        //当前收包的状态
    char                      dataHeadInfo[_DATA_BUFSIZE_];   //用于保存收到的数据的包头信息
    char                      *precvbuf;                      //接收数据的缓冲区的头指针，对收到不全的包非常有用，看具体应用的代码
    unsigned int              irecvlen;                     //要收到多少数据，由这个变量指定，和precvbuf配套使用，看具体应用的代码
    bool                      ifnewrecvMem;                   //如果我们成功的收到了包头，那么我们就要分配内存开始保存 包头+消息头+包体内容，这个标记用来标记是否我们new过内存，因为new过是需要释放的
    char                      *pnewMemPointer;                //new出来的用于收包的内存首地址，和ifnewrecvMem配对使用

    ///辅助链接
    lpngx_connection_t        data;
};

//消息头，引入的目的是当收到数据包时，额外记录一些内容以备将来使用
typedef struct _STRUC_MSG_HEADER
{
    lpngx_connection_t pConn;         //记录对应的链接，注意这是个指针
    uint64_t           iCurrsequence; //收到数据包时记录对应连接的序号，将来能用于比较是否连接已经作废用
    //......其他以后扩展
}STRUC_MSG_HEADER,*LPSTRUC_MSG_HEADER;

//------------------------------------
//socket相关类
class CSocekt
{
public:
    CSocekt();                                                         //构造函数
    virtual ~CSocekt();                                                //释放函数
public:
    virtual bool Initialize();                                         //初始化函数

public:
    ///处理客户端请求
    virtual void threadRecvProcFunc(char *pMsgBuf);                    //处理客户端请求，虚函数，因为将来可以考虑自己来写子类继承本类

    ///初始化epoll，处理事件
    int  ngx_epoll_init();                                             //epoll功能初始化

    int  ngx_epoll_add_event(int fd,int readevent,int writeevent,uint32_t otherflag,uint32_t eventtype,lpngx_connection_t c);

    int  ngx_epoll_process_events(int timer);                          //epoll等待接收和处理事件

private:
    ///监听，取消监听
    bool ngx_open_listening_sockets();                                 //监听必须的端口【支持多个端口】
    void ngx_close_listening_sockets();                                //关闭监听套接字

    ///handler，1处理新连接，2收取数据
    void ngx_event_accept(lpngx_connection_t oldc);                    //建立新连接
    void ngx_wait_request_handler(lpngx_connection_t c);               //设置数据来时的读处理函数

    //获取对端信息相关
    size_t ngx_sock_ntop(struct sockaddr *sa,int port,u_char *text,size_t len);  //根据参数1给定的信息，获取地址端口字符串，返回这个字符串的长度

    ///收包，处理包
    ssize_t recvproc(lpngx_connection_t c,char *buff,ssize_t buflen);  //接收从客户端来的数据专用函数
    void ngx_wait_request_handler_proc_p1(lpngx_connection_t c);       //包头收完整后的处理，我们称为包处理阶段1：写成函数，方便复用
    void ngx_wait_request_handler_proc_plast(lpngx_connection_t c);    //收到一个完整包后的处理，放到一个函数中，方便调用

    ///辅助获取，关闭连接池
    lpngx_connection_t ngx_get_connection(int isock);                  //从连接池中获取一个空闲连接
    void ngx_free_connection(lpngx_connection_t c);                    //归还参数c所代表的连接到到连接池中
    void ngx_close_connection(lpngx_connection_t c);          //用户连入，我们accept4()时，得到的socket在处理中产生失败，则资源用这个函数释放【因为这里涉及到好几个要释放的资源，所以写成函数】

    ///辅助设置非阻塞套接字
    bool setnonblocking(int sockfd);                                   //设置非阻塞套接字

    ///辅助读取配置文件
    void ReadConf();                                                   //专门用于读各种配置项
private:
    ///基本配置
    int                            m_worker_connections;               //epoll连接的最大项数
    int                            m_ListenPortCount;                  //所监听的端口数量

    ///连接池
    lpngx_connection_t             m_pconnections;                     //注意这里可是个指针，其实这是个连接池的首地址
    lpngx_connection_t             m_pfree_connections;                //空闲连接链表头，连接池中总是有某些连接被占用，为了快速在池中找到一个空闲的连接，我把空闲的连接专门用该成员记录;

    int                            m_connection_n;                     //当前进程中所有连接对象的总数【连接池大小】
    int                            m_free_connection_n;                //连接池中可用连接总数

    ///监听套接字
    std::vector<lpngx_listening_t> m_ListenSocketList;                 //监听套接字队列

    ///epoch相关
    int                            m_epollhandle;                      //epoll_create返回的句柄
    struct epoll_event             m_events[NGX_MAX_EVENTS];           //用于在epoll_wait()中承载返回的所发生的事件

protected:
    size_t                         m_iLenPkgHeader;                    //sizeof(COMM_PKG_HEADER);
    size_t                         m_iLenMsgHeader;                    //sizeof(STRUC_MSG_HEADER);

};

#endif