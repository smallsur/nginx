//
// Created by awen on 23-3-29.
//
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include "ngx_func.h"
#include "ngx_global.h"

//size_t   g_environlen;
//size_t   g_argvneedmem;
//char*  gp_envmem;

void ngx_init_setproctitle() {
    int i;
    gp_envmem = new char[g_environlen];
    memset(gp_envmem,0,g_environlen);
    char *p = gp_envmem;
    for ( i = 0; environ[i]; ++i) {
        size_t size = strlen(environ[i]) + 1;
        strcpy(p,environ[i]);
        environ[i] = p;
        p = p + size;
    }
    return;
}

void ngx_setproctitle(const char *title){
    size_t ititlelen = strlen(title);

    size_t esy = g_argvneedmem + g_environlen;
    if( esy <= ititlelen)
    {
        return;
    }
    g_os_argv[1] = nullptr;

    char *ptmp = g_os_argv[0]; //让ptmp指向g_os_argv所指向的内存
    strcpy(ptmp,title);
    ptmp += ititlelen; //跳过标题

    size_t cha = esy - ititlelen;
    memset(ptmp,0,cha);
    return;
}