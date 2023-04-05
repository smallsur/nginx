//
// Created by awen on 23-2-23.
//

#ifndef NGINX_NGX_C_CONF_H
#define NGINX_NGX_C_CONF_H
#include <vector>
#include <memory>

#include "ngx_global.h"

template<typename T>
class Singleton{
public:
    static T& get_instance()noexcept(std::is_nothrow_constructible<T>::value){
        static T instance{Token()};
        return instance;
    }
    virtual ~Singleton()=default;
    Singleton(const Singleton&)=delete;
    Singleton& operator =(const Singleton&)=delete;
protected:
    Singleton()=default;
    struct Token{};
};


class Config_Nginx: public Singleton<Config_Nginx>{
public:
    explicit Config_Nginx(Token){
    };
    Config_Nginx(const Config_Nginx &) =delete;
    Config_Nginx& operator=(const Config_Nginx &) =delete;
    bool load(const char * file_path);
    const char *GetString(const char *p_itemname);
    int  GetIntDefault(const char *p_itemname,const int def);
private:
    std::vector<std::shared_ptr<Config_Nginx_Item>> configItemList;
};


class CMemory:public Singleton<CMemory>{
public:
    explicit CMemory(Token){
    };
    void *AllocMemory(int memCount,bool ifmemset);
    void FreeMemory(void *point);
};

#endif //NGINX_NGX_C_CONF_H
