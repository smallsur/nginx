//
// Created by awen on 23-2-23.
//

#ifndef NGINX_SINGLETON_H
#define NGINX_SINGLETON_H

#include <mutex>
#include <memory>

class Singleton {
private:
    Singleton()=default;
    Singleton(const Singleton& singleton);
    const Singleton &operator=(const Singleton& singleton);
    static std::shared_ptr<Singleton> instance;
    static std::mutex mutex;
public:
    static std::shared_ptr<Singleton> getInstance();
};

#endif //NGINX_SINGLETON_H
