//
// Created by awen on 23-2-23.
//
#include <iostream>
#include "Singleton.h"


std::shared_ptr<Singleton> Singleton::instance = nullptr;
std::mutex Singleton::mutex;

std::shared_ptr<Singleton> Singleton::getInstance() {
    if(instance== nullptr) {
        mutex.lock();
        if (instance == nullptr) {
            instance = std::shared_ptr<Singleton>(new Singleton());
        }
        mutex.unlock();
    }
    return instance;
}


