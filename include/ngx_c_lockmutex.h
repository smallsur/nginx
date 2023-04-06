//
// Created by awen on 23-4-6.
//

#ifndef NGINX_NGX_C_LOCKMUTEX_H
#define NGINX_NGX_C_LOCKMUTEX_H

//#include <thread>
#include <pthread.h>

class CLock
{
public:
    CLock(pthread_mutex_t *pMutex)
    {
        m_pMutex = pMutex;
        pthread_mutex_lock(m_pMutex); //加锁互斥量
    }
    ~CLock()
    {
        pthread_mutex_unlock(m_pMutex); //解锁互斥量
    }
private:
    pthread_mutex_t *m_pMutex;

};

#endif //NGINX_NGX_C_LOCKMUTEX_H
