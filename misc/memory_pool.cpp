//
// Created by awen on 23-2-22.
//
//#include <memory>
#include <cstdlib>
#include <iostream>
#include "memory_pool.h"


int Memory_Pool::n_Count = 0;
int Memory_Pool::n_MallocCount = 0;
int Memory_Pool::m_sChunkCount = 10;
Memory_Pool* Memory_Pool::pos = nullptr;


void *Memory_Pool::operator new(size_t size) {
#ifdef OPEN_MEMORY_POOL
    Memory_Pool *tmpLink = reinterpret_cast<Memory_Pool*>(malloc(size));
    return tmpLink;
#else
    Memory_Pool *tmpLink = nullptr;
    if(pos== nullptr){
        Memory_Pool* newBlock = reinterpret_cast<Memory_Pool*>(std::malloc(size * m_sChunkCount));
        tmpLink = newBlock;
        for (; tmpLink != &newBlock[m_sChunkCount-1]; ++tmpLink) {
             tmpLink->next = tmpLink + 1;
        }
        tmpLink->next = nullptr;
        pos = newBlock;
        ++n_MallocCount;
    }
    tmpLink = pos;
    pos = pos->next;
    ++n_Count;
//    std::cout<<tmpLink<<std::endl;
    return reinterpret_cast<void *>(tmpLink);
#endif
}

void Memory_Pool::operator delete(void *p)  {
#ifdef OPEN_MEMORY_POOL
    free(p);
    return;
#else
    reinterpret_cast<Memory_Pool*>(p)->next = pos;
    pos = reinterpret_cast<Memory_Pool*>(p);
#endif
}