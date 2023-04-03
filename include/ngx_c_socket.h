//
// Created by awen on 23-4-2.
//

#ifndef NGINX_NGX_C_SOCKET_H
#define NGINX_NGX_C_SOCKET_H
#include <vector>

#define NGX_LISTEN_BACKLOG 511

typedef struct ngx_listening_s
{
    int port;
    int fd;
}ngx_listening_t, *lpngx_listening_t;

class CSocekt
{
public:
    CSocekt();                                            //构造函数
    virtual ~CSocekt();                                   //释放函数

public:
    virtual bool Initialize();                            //初始化函数

private:
    bool ngx_open_listening_sockets();                    //监听必须的端口【支持多个端口】
    void ngx_close_listening_sockets();                   //关闭监听套接字
    bool setnonblocking(int sockfd);                      //设置非阻塞套接字

private:
    int                            m_ListenPortCount;     //所监听的端口数量
    std::vector<lpngx_listening_t> m_ListenSocketList;    //监听套接字队列
};


#endif //NGINX_NGX_C_SOCKET_H
