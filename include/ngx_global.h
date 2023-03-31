#ifndef __NGX_GBLDEF_H__
#define __NGX_GBLDEF_H__

//一些比较通用的定义放在这里


//结构定义
typedef struct {
    char itemName[50];
    char itemValue[500];
}Config_Nginx_Item;

//外部全局量声明
extern char  **g_os_argv;
extern char  *gp_envmem; 
extern int   g_environlen; 

#endif

