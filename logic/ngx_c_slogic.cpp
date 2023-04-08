//
// Created by awen on 23-4-6.
//
#include <arpa/inet.h>
#include <string.h>

#include "ngx_c_slogic.h"
#include "ngx_logiccomm.h"
#include "ngx_func.h"
#include "ngx_c_conf.h"

//定义成员函数指针
typedef bool (CLogicSocket::*handler)(  lpngx_connection_t pConn,      //连接池中连接的指针
                                        LPSTRUC_MSG_HEADER pMsgHeader,  //消息头指针
                                        char *pPkgBody,                 //包体指针
                                        unsigned short iBodyLength);    //包体长度

//用来保存 成员函数指针 的这么个数组
static const handler statusHandler[] =
        {
                //数组前5个元素，保留，以备将来增加一些基本服务器功能
                nullptr,                                                   //【0】：下标从0开始
                nullptr,                                                   //【1】：下标从0开始
                nullptr,                                                   //【2】：下标从0开始
                nullptr,                                                   //【3】：下标从0开始
                nullptr,                                                   //【4】：下标从0开始

                //开始处理具体的业务逻辑
                &CLogicSocket::_HandleRegister,                         //【5】：实现具体的注册功能
                &CLogicSocket::_HandleLogIn,                            //【6】：实现具体的登录功能
                //......其他待扩展，比如实现攻击功能，实现加血功能等等；

        };

#define AUTH_TOTAL_COMMANDS sizeof(statusHandler)/sizeof(handler) //整个命令有多少个，编译时即可知道

//构造函数
CLogicSocket::CLogicSocket():CSocekt()
{

}
//析构函数
CLogicSocket::~CLogicSocket()
{

}

//初始化函数【fork()子进程之前干这个事】
//成功返回true，失败返回false
bool CLogicSocket::Initialize()
{
    //做一些和本类相关的初始化工作
    //....日后根据需要扩展

    bool bParentInit = CSocekt::Initialize();  //调用父类的同名函数
    return bParentInit;
}

//处理收到的数据包
//pMsgBuf：消息头 + 包头 + 包体 ：自解释；
void CLogicSocket::threadRecvProcFunc(char *pMsgBuf)
{
    LPSTRUC_MSG_HEADER pMsgHeader = (LPSTRUC_MSG_HEADER)pMsgBuf;                  //消息头
    LPCOMM_PKG_HEADER  pPkgHeader = (LPCOMM_PKG_HEADER)(pMsgBuf+m_iLenMsgHeader); //包头
    void  *pPkgBody = nullptr;                                                       //指向包体的指针
    unsigned short pkglen = ntohs(pPkgHeader->pkgLen);                            //客户端指明的包宽度【包头+包体】

    if(m_iLenPkgHeader == pkglen)
    {
        //没有包体，只有包头
        if(pPkgHeader->crc32 != 0) //只有包头的crc值给0
        {
            return; //crc错，直接丢弃
        }
        pPkgBody = nullptr;
    }
    else
    {
        //有包体，走到这里
        pPkgHeader->crc32 = ntohl(pPkgHeader->crc32);		          //针对4字节的数据，网络序转主机序
        pPkgBody = (void *)(pMsgBuf+m_iLenMsgHeader+m_iLenPkgHeader); //跳过消息头 以及 包头 ，指向包体

        //计算crc值判断包的完整性
        int calccrc = CCRC32::get_instance().Get_CRC((unsigned char *)pPkgBody,pkglen-m_iLenPkgHeader); //计算纯包体的crc值
        if(calccrc != pPkgHeader->crc32) //服务器端根据包体计算crc值，和客户端传递过来的包头中的crc32信息比较
        {
            ngx_log_stderr(0,"CLogicSocket::threadRecvProcFunc()中CRC错误，丢弃数据!");    //正式代码中可以干掉这个信息
            return; //crc错，直接丢弃
        }
    }

    //包crc校验OK才能走到这里
    unsigned short imsgCode = ntohs(pPkgHeader->msgCode); //消息代码拿出来
    lpngx_connection_t p_Conn = pMsgHeader->pConn;        //消息头中藏着连接池中连接的指针

    //我们要做一些判断
    //(1)如果从收到客户端发送来的包，到服务器释放一个线程池中的线程处理该包的过程中，客户端断开了，那显然，这种收到的包我们就不必处理了；
    if(p_Conn->iCurrsequence != pMsgHeader->iCurrsequence)   //该连接池中连接以被其他tcp连接【其他socket】占用，这说明原来的 客户端和本服务器的连接断了，这种包直接丢弃不理
    {
        return; //丢弃不理这种包了【客户端断开了】
    }

    //(2)判断消息码是正确的，防止客户端恶意侵害我们服务器，发送一个不在我们服务器处理范围内的消息码
    if(imsgCode >= AUTH_TOTAL_COMMANDS) //无符号数不可能<0
    {
        ngx_log_stderr(0,"CLogicSocket::threadRecvProcFunc()中imsgCode=%d消息码不对!",imsgCode); //这种有恶意倾向或者错误倾向的包，希望打印出来看看是谁干的
        return; //丢弃不理这种包【恶意包或者错误包】
    }

    //能走到这里的，包没过期，不恶意，那好继续判断是否有相应的处理函数
    //(3)有对应的消息处理函数吗
    if(statusHandler[imsgCode] == nullptr) //这种用imsgCode的方式可以使查找要执行的成员函数效率特别高
    {
        ngx_log_stderr(0,"CLogicSocket::threadRecvProcFunc()中imsgCode=%d消息码找不到对应的处理函数!",imsgCode); //这种有恶意倾向或者错误倾向的包，希望打印出来看看是谁干的
        return;  //没有相关的处理函数
    }

    //一切正确，可以放心大胆的处理了
    //(4)调用消息码对应的成员函数来处理
    (this->*statusHandler[imsgCode])(p_Conn,pMsgHeader,(char *)pPkgBody,pkglen-m_iLenPkgHeader);
}

//----------------------------------------------------------------------------------------------------------
//处理各种业务逻辑
bool CLogicSocket::_HandleRegister(lpngx_connection_t pConn,LPSTRUC_MSG_HEADER pMsgHeader,char *pPkgBody,unsigned short iBodyLength)
{
    //(1)首先判断包体的合法性
    if(pPkgBody == NULL) //具体看客户端服务器约定，如果约定这个命令[msgCode]必须带包体，那么如果不带包体，就认为是恶意包，直接不处理
    {
        return false;
    }

    int iRecvLen = sizeof(STRUCT_REGISTER);
    if(iRecvLen != iBodyLength) //发送过来的结构大小不对，认为是恶意包，直接不处理
    {
        return false;
    }

    //(2)对于同一个用户，可能同时发送来多个请求过来，造成多个线程同时为该 用户服务，比如以网游为例，用户要在商店中买A物品，又买B物品，而用户的钱 只够买A或者B，不够同时买A和B呢？
    //那如果用户发送购买命令过来买了一次A，又买了一次B，如果是两个线程来执行同一个用户的这两次不同的购买命令，很可能造成这个用户购买成功了 A，又购买成功了 B
    //所以，为了稳妥起见，针对某个用户的命令，我们一般都要互斥,我们需要增加临界的变量于ngx_connection_s结构中
    CLock lock(&pConn->logicPorcMutex); //凡是和本用户有关的访问都互斥

    //(3)取得了整个发送过来的数据
    LPSTRUCT_REGISTER p_RecvInfo = (LPSTRUCT_REGISTER)pPkgBody;

    //(4)这里可能要考虑 根据业务逻辑，进一步判断收到的数据的合法性，
    //当前该玩家的状态是否适合收到这个数据等等【比如如果用户没登陆，它就不适合购买物品等等】
    //这里大家自己发挥，自己根据业务需要来扩充代码，老师就不带着大家扩充了。。。。。。。。。。。。
    //。。。。。。。。

    //(5)给客户端返回数据时，一般也是返回一个结构，这个结构内容具体由客户端/服务器协商，这里我们就以给客户端也返回同样的 STRUCT_REGISTER 结构来举例
    //LPSTRUCT_REGISTER pFromPkgHeader =  (LPSTRUCT_REGISTER)(((char *)pMsgHeader)+m_iLenMsgHeader);	//指向收到的包的包头，其中数据后续可能要用到
    LPCOMM_PKG_HEADER pPkgHeader;
    CMemory  &p_memory = CMemory::get_instance();
    CCRC32   &p_crc32 = CCRC32::get_instance();
    int iSendLen = sizeof(STRUCT_REGISTER);
    //a)分配要发送出去的包的内存
    char *p_sendbuf = (char *)p_memory.AllocMemory(m_iLenMsgHeader+m_iLenPkgHeader+iSendLen,false);//准备发送的格式，这里是 消息头+包头+包体
    //b)填充消息头
    memcpy(p_sendbuf,pMsgHeader,m_iLenMsgHeader);                   //消息头直接拷贝到这里来
    //c)填充包头
    pPkgHeader = (LPCOMM_PKG_HEADER)(p_sendbuf+m_iLenMsgHeader);    //指向包头
    pPkgHeader->msgCode = _CMD_REGISTER;	                        //消息代码，可以统一在ngx_logiccomm.h中定义
    pPkgHeader->msgCode = htons(pPkgHeader->msgCode);	            //htons主机序转网络序
    pPkgHeader->pkgLen  = htons(m_iLenPkgHeader + iSendLen);        //整个包的尺寸【包头+包体尺寸】
    //d)填充包体
    LPSTRUCT_REGISTER p_sendInfo = (LPSTRUCT_REGISTER)(p_sendbuf+m_iLenMsgHeader+m_iLenPkgHeader);	//跳过消息头，跳过包头，就是包体了
    //。。。。。这里根据需要，填充要发回给客户端的内容,int类型要使用htonl()转，short类型要使用htons()转；

    //e)包体内容全部确定好后，计算包体的crc32值
    pPkgHeader->crc32   = p_crc32.Get_CRC((unsigned char *)p_sendInfo,iSendLen);
    pPkgHeader->crc32   = htonl(pPkgHeader->crc32);

    //f)发送数据包
    /*msgSend(p_sendbuf);
    //如果时机OK才add_event
    if(ngx_epoll_add_event(pConn->fd,                 //socket句柄
                                0,1,              //读，写 ,这里读为1，表示客户端应该主动给我服务器发送消息，我服务器需要首先收到客户端的消息；
                                0,//EPOLLET,      //其他补充标记【EPOLLET(高速模式，边缘触发ET)】
                                                      //后续因为实际项目需要，我们采用LT模式【水平触发模式/低速模式】
                                EPOLL_CTL_MOD,    //事件类型【增加，还有删除/修改】
                                pConn              //连接池中的连接
                                ) == -1)
                                {
                                    //ngx_log_stderr(0,"111111111111!");
                                }
    */

    /*
     sleep(100);  //休息这么长时间
     //如果连接回收了，则肯定是iCurrsequence不等了
     if(pMsgHeader->iCurrsequence != pConn->iCurrsequence)
     {
         //应该是不等，因为这个插座已经被回收了
         ngx_log_stderr(0,"插座不等,%L--%L",pMsgHeader->iCurrsequence,pConn->iCurrsequence);
     }
     else
     {
         ngx_log_stderr(0,"插座相等哦,%L--%L",pMsgHeader->iCurrsequence,pConn->iCurrsequence);
     }*/


    ngx_log_stderr(0,"执行了CLogicSocket::_HandleRegister()!");
    return true;
}
bool CLogicSocket::_HandleLogIn(lpngx_connection_t pConn,LPSTRUC_MSG_HEADER pMsgHeader,char *pPkgBody,unsigned short iBodyLength)
{
    ngx_log_stderr(0,"执行了CLogicSocket::_HandleLogIn()!");
    return true;
}