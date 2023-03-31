//
// Created by awen on 23-3-29.
//
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include "ngx_func.h"
#include "ngx_global.h"


void ngx_init_setproctitle() {
    int i;
    for (int i = 0; environ[i]; ++i) {
        g_environlen += strlen(environ[i]) + 1;
    }
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


    size_t e_environlen = 0;
    for (int i = 0; g_os_argv[i]; i++)
    {
        e_environlen += strlen(g_os_argv[i]) + 1;
    }

    size_t esy = e_environlen + g_environlen;
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