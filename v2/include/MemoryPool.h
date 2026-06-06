#pragma once
#include "ThreadCache.h"

namespace memoryPool {

class MemoryPool
{
public:
    void* allocate(size_t size)
    {
        ThreadCache::getInstance()->allocate(size);
    }
    void deallocate(void* ptr, size_t size)
    {
        ThreadCache::getInstance()->deallocate(ptr, size);
    }
};
    
} // namespace memoryPool
