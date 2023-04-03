//
// Created by awen on 23-4-2.
//
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#include "ngx_c_socket.h"
#include "ngx_global.h"
#include "ngx_c_conf.h"
#include "ngx_func.h"
//构造函数
CSocekt::CSocekt()
{
    m_ListenPortCount = 1;   //监听一个端口
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
    return;
}
//初始化函数【fork()子进程之前干这个事】
bool CSocekt::Initialize()
{
    bool reco = ngx_open_listening_sockets();
    return reco;
}

bool CSocekt::ngx_open_listening_sockets() {
    Config_Nginx& conf = Config_Nginx::get_instance();
    m_ListenPortCount = conf.GetIntDefault("ListenPortPortCount",1);
    struct sockaddr_in serv_addr;
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

    }
    return true;
}