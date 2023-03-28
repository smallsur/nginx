//
// Created by awen on 23-2-23.
//

#ifndef NGINX_SINGLETON_H
#define NGINX_SINGLETON_H

#include <mutex>
#include <memory>

template<typename T>
class Singleton {
private:
    Singleton()=default;
    Singleton(const Singleton& singleton);
    const Singleton &operator=(const Singleton& singleton);
    static std::shared_ptr<T> instance;
    static std::mutex mutex;
public:
    static std::shared_ptr<T> getInstance();
};

#endif //NGINX_SINGLETON_H
