//
// Created by awen on 23-2-22.
//

#ifndef NGINX_MEMORY_POOL_H
#define NGINX_MEMORY_POOL_H

#include <cstddef>

#define OPEN_MEMORY_POOL 1

class Memory_Pool{
public:
    static void *operator new(size_t size);
    static void operator delete(void *p);
    static int n_Count;
    static int n_MallocCount;
private:
    static int m_sChunkCount;
    static Memory_Pool *pos; //position
    Memory_Pool* next;
};

#endif //NGINX_MEMORY_POOL_H
