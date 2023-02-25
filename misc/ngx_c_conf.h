//
// Created by awen on 23-2-23.
//

#ifndef NGINX_NGX_C_CONF_H
#define NGINX_NGX_C_CONF_H
#include <vector>
#include <memory>
#include <string>
#include "Singleton.h"

typedef struct {
    char itemName[50];
    char itemValue[500];
}Config_Nginx_Item;

class Config_Nginx: public Singleton{
public:
    bool load(const char * file_path);
    const char *getString(const char *name);
    int getInt(const char *name, const int def);
private:
    std::vector<std::shared_ptr<Config_Nginx_Item>> configItemList;
};


#endif //NGINX_NGX_C_CONF_H
