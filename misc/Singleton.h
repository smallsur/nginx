//
// Created by awen on 23-2-23.
//

#ifndef NGINX_SINGLETON_H
#define NGINX_SINGLETON_H

#include <cstdlib>
#include <thread>
class Singleton {
private:
    Singleton()=default;
    static Singleton* instance;
    class resource_release {
    public:
        resource_release() = default;
        ~resource_release();
    };
    static resource_release tmp;
public:
    static Singleton* getInstance();
};


#endif //NGINX_SINGLETON_H
