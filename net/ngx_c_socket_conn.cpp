//
// Created by awen on 23-4-3.
//
#include <string.h>
#include <errno.h>
#include <time.h>

#include "ngx_c_conf.h"
#include "ngx_macro.h"
#include "ngx_global.h"
#include "ngx_func.h"
#include "ngx_c_socket.h"
#include "ngx_comm.h"
#include "ngx_c_lockmutex.h"

ngx_connection_s::ngx_connection_s()//构造函数
{
    iCurrsequence = 0;
    pthread_mutex_init(&logicPorcMutex, NULL); //互斥量初始化
}

ngx_connection_s::~ngx_connection_s()//析构函数
{
    pthread_mutex_destroy(&logicPorcMutex);    //互斥量释放
}

void ngx_connection_s::GetOneToUse()
{
    events = 0;
    ++iCurrsequence;
    fd = -1;
    ///receive option reset
    curStat = _PKG_HD_INIT;                           //收包状态处于 初始状态，准备接收数据包头【状态机】
    precvbuf = dataHeadInfo;                          //收包我要先收到这里来，因为我要先收包头，所以收数据的buff直接就是dataHeadInfo
    irecvlen = sizeof(COMM_PKG_HEADER); //这里指定收数据的长度，这里先要求收包头这么长字节的数据
    precvMemPointer = nullptr;                           //既然没new内存，那自然指向的内存地址先给NULL

    ///send option reset
    iThrowsendCount = 0;                              //原子的
    psendbuf = nullptr;
    isendlen = 0;
    psendMemPointer = nullptr;                           //发送数据头指针记录
    lastPingTime = time(nullptr);

    FloodkickLastTime = 0;                            //Flood攻击上次收到包的时间
    FloodAttackCount  = 0;	                          //Flood攻击在该时间内收到包的次数统计
}

void ngx_connection_s::PutOneToFree()
{
    ++iCurrsequence;
    if(precvMemPointer != nullptr)//我们曾经给这个连接分配过接收数据的内存，则要释放内存
    {
        CMemory::get_instance().FreeMemory(precvMemPointer);
        precvMemPointer = nullptr;
    }
    if(psendMemPointer != nullptr) //如果发送数据的缓冲区里有内容，则要释放内存
    {
        CMemory::get_instance().FreeMemory(psendMemPointer);
        psendMemPointer = nullptr;
    }
    iThrowsendCount = 0;                              //设置不设置感觉都行
}

void CSocekt::initconnection()
{
    lpngx_connection_t p_Conn;
    CMemory &p_memory = CMemory::get_instance();

    int ilenconnpool = sizeof(ngx_connection_t);
    for(int i = 0; i < m_worker_connections; ++i) //先创建这么多个连接，后续不够再增加
    {
        p_Conn = (lpngx_connection_t)p_memory.AllocMemory(ilenconnpool,true); //清理内存 , 因为这里分配内存new char，无法执行构造函数，所以如下：
        //手工调用构造函数，因为AllocMemory里无法调用构造函数
        p_Conn = new(p_Conn) ngx_connection_t();  //定位new【不懂请百度】，释放则显式调用p_Conn->~ngx_connection_t();
        p_Conn->GetOneToUse();
        m_connectionList.push_back(p_Conn);     //所有链接【不管是否空闲】都放在这个list
        m_freeconnectionList.push_back(p_Conn); //空闲连接会放在这个list
    } //end for
    m_free_connection_n = m_total_connection_n = m_connectionList.size(); //开始这两个列表一样大
}
//最终回收连接池，释放内存
void CSocekt::clearconnection()
{
    lpngx_connection_t p_Conn;
    CMemory &p_memory = CMemory::get_instance();

    while(!m_connectionList.empty())
    {
        p_Conn = m_connectionList.front();
        m_connectionList.pop_front();
        p_Conn->~ngx_connection_t();     //手工调用析构函数
        p_memory.FreeMemory(p_Conn);
    }
}

lpngx_connection_t CSocekt::ngx_get_connection(int isock)
{
    CLock lock(&m_connectionMutex);

    if(!m_freeconnectionList.empty())
    {
        //有空闲的，自然是从空闲的中摘取
        lpngx_connection_t p_Conn = m_freeconnectionList.front(); //返回第一个元素但不检查元素存在与否
        m_freeconnectionList.pop_front();                         //移除第一个元素但不返回
        p_Conn->GetOneToUse();
        --m_free_connection_n;
        p_Conn->fd = isock;
        return p_Conn;
    }
    CMemory &p_memory = CMemory::get_instance();
    lpngx_connection_t p_Conn = (lpngx_connection_t)p_memory.AllocMemory(sizeof(ngx_connection_t),true);
    p_Conn = new(p_Conn) ngx_connection_t();
    p_Conn->GetOneToUse();
    m_connectionList.push_back(p_Conn); //入到总表中来，但不能入到空闲表中来，因为凡是调这个函数的，肯定是要用这个连接的
    ++m_total_connection_n;
    p_Conn->fd = isock;
    return p_Conn;
}

void CSocekt::ngx_free_connection(lpngx_connection_t pConn)
{
    //因为有线程可能要动连接池中连接，所以在合理互斥也是必要的
    CLock lock(&m_connectionMutex);

    //首先明确一点，连接，所有连接全部都在m_connectionList里；
    pConn->PutOneToFree();

    //扔到空闲连接列表里
    m_freeconnectionList.push_back(pConn);

    //空闲连接数+1
    ++m_free_connection_n;
}

//将要回收的连接放到一个队列中来，后续有专门的线程会处理这个队列中的连接的回收
//有些连接，我们不希望马上释放，要隔一段时间后再释放以确保服务器的稳定，所以，我们把这种隔一段时间才释放的连接先放到一个队列中来
void CSocekt::inRecyConnectQueue(lpngx_connection_t pConn)
{
    ngx_log_stderr(0,"CSocekt::inRecyConnectQueue()执行，连接入到回收队列中.");
    std::list<lpngx_connection_t>::iterator pos;
    bool iffind = false;

    CLock lock(&m_recyconnqueueMutex); //针对连接回收列表的互斥量，因为线程ServerRecyConnectionThread()也有要用到这个回收列表；

    //如下判断防止连接被多次扔到回收站中来
    for(pos = m_recyconnectionList.begin(); pos != m_recyconnectionList.end(); ++pos)
    {
        if((*pos) == pConn)
        {
            iffind = true;
            break;
        }
    }
    if(iffind == true) //找到了，不必再入了
    {
        //我有义务保证这个只入一次嘛
        return;
    }

    pConn->inRecyTime = time(nullptr);        //记录回收时间
    ++pConn->iCurrsequence;
    m_recyconnectionList.push_back(pConn); //等待ServerRecyConnectionThread线程自会处理
    ++m_totol_recyconnection_n;            //待释放连接队列大小+1
    --m_onlineUserCount;                   //连入用户数量-1
}


//处理连接回收的线程
void* CSocekt::ServerRecyConnectionThread(void* threadData)
{
    ThreadItem *pThread = static_cast<ThreadItem*>(threadData);
    CSocekt *pSocketObj = pThread->_pThis;

    time_t currtime;
    int err;
    std::list<lpngx_connection_t>::iterator pos,posend;
    lpngx_connection_t p_Conn;

    while(1)
    {
        //为简化问题，我们直接每次休息200毫秒
        usleep(200 * 1000);  //单位是微妙,又因为1毫秒=1000微妙，所以 200 *1000 = 200毫秒

        //不管啥情况，先把这个条件成立时该做的动作做了
        if(pSocketObj->m_totol_recyconnection_n > 0)
        {
            currtime = time(nullptr);
            err = pthread_mutex_lock(&pSocketObj->m_recyconnqueueMutex);
            if(err != 0) ngx_log_stderr(err,"CSocekt::ServerRecyConnectionThread()中pthread_mutex_lock()失败，返回的错误码为%d!",err);

            lblRRTD:
            pos    = pSocketObj->m_recyconnectionList.begin();
            posend = pSocketObj->m_recyconnectionList.end();
            for(; pos != posend; ++pos)
            {
                p_Conn = (*pos);
                if(
                        ( (p_Conn->inRecyTime + pSocketObj->m_RecyConnectionWaitTime) > currtime)  && (g_stopEvent == 0) //如果不是要整个系统退出，你可以continue，否则就得要强制释放
                        )
                {
                    continue; //没到释放的时间
                }
                //到释放的时间了:
                //......这将来可能还要做一些是否能释放的判断[在我们写完发送数据代码之后吧]，先预留位置
                //....


                //流程走到这里，表示可以释放，那我们就开始释放
                --pSocketObj->m_totol_recyconnection_n;        //待释放连接队列大小-1
                pSocketObj->m_recyconnectionList.erase(pos);   //迭代器已经失效，但pos所指内容在p_Conn里保存着呢

                ngx_log_stderr(0,"CSocekt::ServerRecyConnectionThread()执行，连接%d被归还.",p_Conn->fd);

                pSocketObj->ngx_free_connection(p_Conn);	   //归还参数pConn所代表的连接到到连接池中
                goto lblRRTD;
            }
            err = pthread_mutex_unlock(&pSocketObj->m_recyconnqueueMutex);
            if(err != 0)  ngx_log_stderr(err,"CSocekt::ServerRecyConnectionThread()pthread_mutex_unlock()失败，返回的错误码为%d!",err);
        } //end if

        if(g_stopEvent == 1) //要退出整个程序，那么肯定要先退出这个循环
        {
            if(pSocketObj->m_totol_recyconnection_n > 0)
            {
                //因为要退出，所以就得硬释放了【不管到没到时间，不管有没有其他不 允许释放的需求，都得硬释放】
                err = pthread_mutex_lock(&pSocketObj->m_recyconnqueueMutex);
                if(err != 0) ngx_log_stderr(err,"CSocekt::ServerRecyConnectionThread()中pthread_mutex_lock2()失败，返回的错误码为%d!",err);

                lblRRTD2:
                pos    = pSocketObj->m_recyconnectionList.begin();
                posend = pSocketObj->m_recyconnectionList.end();
                for(; pos != posend; ++pos)
                {
                    p_Conn = (*pos);
                    --pSocketObj->m_totol_recyconnection_n;        //待释放连接队列大小-1
                    pSocketObj->m_recyconnectionList.erase(pos);   //迭代器已经失效，但pos所指内容在p_Conn里保存着呢
                    pSocketObj->ngx_free_connection(p_Conn);	   //归还参数pConn所代表的连接到到连接池中
                    goto lblRRTD2;
                } //end for
                err = pthread_mutex_unlock(&pSocketObj->m_recyconnqueueMutex);
                if(err != 0)  ngx_log_stderr(err,"CSocekt::ServerRecyConnectionThread()pthread_mutex_unlock2()失败，返回的错误码为%d!",err);
            } //end if
            break; //整个程序要退出了，所以break;
        }  //end if
    } //end while

    return (void*)0;
}


void CSocekt::ngx_close_connection(lpngx_connection_t c) {
    ngx_free_connection(c);
    int fd = c->fd;
    if(close(fd) == -1)
    {
        ngx_log_error_core(NGX_LOG_ALERT,errno,"CSocekt::ngx_close_accepted_connection()中close(%d)失败!",fd);
    }
    c->fd = -1;
}
