#pragma once
#include "Common.h"

namespace memoryPool {

//线程本地缓存
class ThreadCache
{
public:
    //单例模式
    ThreadCache* getInstance()
    {
        static thread_local ThreadCache instance;
        return &instance;
    }
    //内存的分配/回收
    void* allocate(size_t size);
    void  deallocate(void* ptr, size_t size);

private:
    //构造函数私有化
    ThreadCache() 
    {
        freeList_.fill(nullptr);
        freeListSize_.fill(0);
    }

    //向中心缓存申请内存
    void* fetchFromCentralCache(size_t index);
    //将内存归还到中心缓存
    void  returnToCentralCache(void* start, size_t size);

    bool shouldReturnToCentralCache(size_t index);

private:
    //线程本地缓存的自由链表
    std::array<void*, FREE_LIST_SIZE> freeList_;
    //自由链表的大小统计
    std::array<size_t, FREE_LIST_SIZE> freeListSize_;
};

} // namespace memoryPool
