//
// Created by awen on 23-4-3.
//
#include <string.h>
#include <errno.h>

#include "ngx_c_conf.h"
#include "ngx_macro.h"
#include "ngx_global.h"
#include "ngx_func.h"
#include "ngx_c_socket.h"
#include "ngx_comm.h"

lpngx_connection_t CSocekt::ngx_get_connection(int isock)
{
    lpngx_connection_t  c = m_pfree_connections; //空闲连接链表头

    if(c == nullptr)
    {
        //系统应该控制连接数量，防止空闲连接被耗尽，能走到这里，都不正常
        ngx_log_stderr(0,"CSocekt::ngx_get_connection()中空闲链表为空,这不应该!");
        return nullptr;
    }

    m_pfree_connections = c->data;                       //指向连接池中下一个未用的节点
    m_free_connection_n--;                               //空闲连接少1

    //(1)注意这里的操作,先把c指向的对象中有用的东西搞出来保存成变量，因为这些数据可能有用
    uintptr_t  instance = c->instance;   //常规c->instance在刚构造连接池时这里是1【失效】
    uint64_t iCurrsequence = c->iCurrsequence;
    //....其他内容再增加


    //(2)把以往有用的数据搞出来后，清空并给适当值
    memset(c,0,sizeof(ngx_connection_t));                //注意，类型不要用成lpngx_connection_t，否则就出错了
    c->fd = isock;//套接字要保存起来，这东西具有唯一性

    c->curStat = _PKG_HD_INIT;                           //收包状态处于 初始状态，准备接收数据包头【状态机】
    c->precvbuf = c->dataHeadInfo;                       //收包我要先收到这里来，因为我要先收包头，所以收数据的buff直接就是dataHeadInfo
    c->irecvlen = sizeof(COMM_PKG_HEADER);               //这里指定收数据的长度，这里先要求收包头这么长字节的数据
    c->ifnewrecvMem = false;                             //标记我们并没有new内存，所以不用释放
    c->pnewMemPointer = nullptr;                            //既然没new内存，那自然指向的内存地址先给NULL
    //....其他内容再增加
    //(3)这个值有用，所以在上边(1)中被保留，没有被清空，这里又把这个值赋回来
    c->instance = !instance;                            //抄自官方nginx，到底有啥用，以后再说【分配内存时候，连接池里每个连接对象这个变量给的值都为1，所以这里取反应该是0【有效】；】
    c->iCurrsequence=iCurrsequence;
    ++c->iCurrsequence;  //每次取用该值都增加1

    //wev->write = 1;  这个标记有没有 意义加，后续再研究
    return c;
}

void CSocekt::ngx_free_connection(lpngx_connection_t c)
{
    if(c->ifnewrecvMem == true)
    {
        //我们曾经给这个连接分配过内存，则要释放内存
        CMemory::get_instance().FreeMemory(c->pnewMemPointer);
        c->pnewMemPointer = nullptr;
        c->ifnewrecvMem = false;  //这行有用？
    }

    c->data = m_pfree_connections;                       //回收的节点指向原来串起来的空闲链的链头

    //节点本身也要干一些事
    ++c->iCurrsequence;                                  //回收后，该值就增加1,以用于判断某些网络事件是否过期【一被释放就立即+1也是有必要的】

    m_pfree_connections = c;                             //修改 原来的链头使链头指向新节点
    ++m_free_connection_n;                               //空闲连接多1
    return;
}

void CSocekt::ngx_close_connection(lpngx_connection_t c) {
    int fd = c->fd;
    if(close(fd) == -1)
    {
        ngx_log_error_core(NGX_LOG_ALERT,errno,"CSocekt::ngx_close_accepted_connection()中close(%d)失败!",fd);
    }
    c->fd = -1;
    ngx_free_connection(c);
    return;
}
