//
// Created by awen on 23-2-23.
//
#include <fstream>
#include <string.h>
#include <iostream>
#include <memory>
#include "ngx_c_conf.h"
#include "ngx_func.h"

void Rtrim(char *string){
    if(string== nullptr) {
        return;
    }
    size_t len = 0;
    len = strlen(string);
    while(len>0&&string[len-1]==' '){
        string[--len]=0;
    }
}
void Ltrim(char *string){
    size_t len = 0;
    len = strlen(string);
    char *p_tmp = string;
    if( (*p_tmp) != ' ') //不是以空格开头
        return;
    //找第一个不为空格的
    while((*p_tmp) != '\0')
    {
        if( (*p_tmp) == ' ')
            p_tmp++;
        else
            break;
    }
    if((*p_tmp) == '\0') //全是空格
    {
        *string = '\0';
        return;
    }
    char *p_tmp2 = string;
    while((*p_tmp) != '\0')
    {
        (*p_tmp2) = (*p_tmp);
        p_tmp++;
        p_tmp2++;
    }
    (*p_tmp2) = '\0';
}


/*
 * read setting:
 * max length = 1024;
 *
 */
bool Config_Nginx::load(const char *file_path) {
    if(file_path== nullptr|| strlen(file_path)==0){
        return false;
    }
    std::ifstream fin(file_path,std::ios::in);
//    fin.open(file_path,std::ios::in);
    if(!fin.is_open()){

        return false;
    }
    char buf[1024]={0};
    while (fin.getline(buf, sizeof(buf))){
        if(buf[0]=='\0'){
            continue;
        }
        Ltrim(buf);
        if(*buf==';' || *buf==' ' || *buf=='#' || *buf=='\t'|| *buf=='\n')
            continue;
        size_t len = strlen(buf);
        while (len >0){
            if(buf[len-1]==' '||buf[len-1]==10 ||buf[len-1]==13){//换行和回车
                buf[len-1]='\0';
            }
            len--;
        }
        if(buf[0]=='\0'){
            continue;
        }
        if(*buf=='[') //[开头的也不处理
            continue;
        char *ptmp = strchr(buf,61);
        if(ptmp!= nullptr){
            auto item = std::make_shared<Config_Nginx_Item>();
            memset((void *)item->itemName,0,sizeof (*item->itemName));
            memset((void *)item->itemValue,0,sizeof (*item->itemValue));
            strcpy(item->itemValue,ptmp+1);
            strncpy(item->itemName,buf,ptmp-buf);

            Rtrim(item->itemValue);
            Rtrim(item->itemName);
            Ltrim(item->itemValue);
            Ltrim(item->itemName);
            configItemList.push_back(item);
        }

    }
    fin.close();
    return true;
}

//const char *Config_Nginx::getString(const char *name) {
//    auto begin= configItemList.begin();
//    for (; begin != configItemList.end(); ++begin) {
//        if (strcasecmp(name,(*begin)->itemName)){
//            return (*begin)->itemValue;
//        }
//    }
//    return nullptr;
//}

const char *Config_Nginx::GetString(const char *p_itemname){
    auto begin= configItemList.begin();
    for (; begin != configItemList.end(); ++begin) {
        if (strcasecmp(p_itemname,(*begin)->itemName)==0){
            return (*begin)->itemValue;
        }
    }
    return nullptr;
}

int  Config_Nginx::GetIntDefault(const char *p_itemname, const int def) {
    auto begin= configItemList.begin();
    for (; begin != configItemList.end(); ++begin) {
        if (strcasecmp(p_itemname,(*begin)->itemName)==0){
            return atoi((*begin)->itemValue);
        }
    }
    return def;
}

