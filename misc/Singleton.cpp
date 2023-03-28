//
// Created by awen on 23-2-23.
//
#include <iostream>
#include "Singleton.h"


template<typename T> std::shared_ptr<T> Singleton<T>::instance = nullptr;
template<typename T> std::mutex Singleton<T>::mutex;

template<typename T> std::shared_ptr<T> Singleton<T>::getInstance() {
    if(instance== nullptr) {
        mutex.lock();
        if (instance == nullptr) {
            instance = std::shared_ptr<T>(new Singleton());
        }
        mutex.unlock();
    }
    return instance;
}


