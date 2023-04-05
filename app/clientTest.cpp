//
// Created by awen on 23-4-5.
//

#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>

#include "ngx_c_socket.h"
#include "ngx_comm.h"
int main(){

    int sockfd = socket(AF_INET,SOCK_DGRAM,0);

    struct sockaddr_in serv_addr;

    memset(&serv_addr,0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8088);

    if(inet_pton(AF_INET,"192.168.1.126",&serv_addr.sin_addr) <= 0)  //IP地址转换函数,把第二个参数对应的ip地址转换第三个参数里边去，固定写法
    {
        printf("调用inet_pton()失败，退出！\n");
        exit(1);
    }

    if(connect(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0)
    {
        printf("调用connect()失败，退出！\n");
        exit(1);
    }
    int n;
    COMM_PKG_HEADER data;
    data.pkgLen = 100;
    data.msgCode = 123;
    data.crc32 = 100;
    send(sockfd,(void*)(&data), sizeof(COMM_PKG_HEADER),0);


}
