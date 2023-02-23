//
// Created by awen on 23-2-23.
//

#include "Singleton.h"

Singleton* Singleton::instance = nullptr;

Singleton* Singleton::getInstance() {

    if(instance==nullptr){
        instance = new Singleton();
    }
    return instance;
}

Singleton::resource_release::~resource_release() {
    if(instance!= nullptr){
        free(instance);
    }
}