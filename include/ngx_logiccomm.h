//
// Created by awen on 23-4-6.
//

#ifndef NGINX_NGX_LOGICCOMM_H
#define NGINX_NGX_LOGICCOMM_H
#pragma pack (1) //对齐方式,1字节对齐【结构之间成员不做任何字节对齐：紧密的排列在一起】

typedef struct _STRUCT_REGISTER
{
    int           iType;          //类型
    char          username[56];   //用户名
    char          password[40];   //密码

}STRUCT_REGISTER, *LPSTRUCT_REGISTER;

typedef struct _STRUCT_LOGIN
{
    char          username[56];   //用户名
    char          password[40];   //密码

}STRUCT_LOGIN, *LPSTRUCT_LOGIN;


#pragma pack() //取消指定对齐，恢复缺省对齐

#endif //NGINX_NGX_LOGICCOMM_H
