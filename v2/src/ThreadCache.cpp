#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"

namespace memoryPool {

void*
ThreadCache::allocate(size_t size)
{
    if (size > MAX_BYTES)
    {
        //大对象直接进行系统调用
        return malloc(size);
    }
    if (size == 0)
    {
        size = ALIGNMENT;
    }
    
    //根据分配内存的大小得到索引
    size_t index = SizeClass::getIndex(size);

    //在空闲链表中查找
    freeListSize_[index]--;
    if (void* ptr = freeList_[index])
    {
        //将其从空闲链表中移除
        freeList_[index] = *reinterpret_cast<void**>(freeList_[index]);
        return ptr;
    }
    //没有找到，向中心缓存申请一块内存
    return fetchFromCentralCache(index);
}

void
ThreadCache::deallocate(void* ptr, size_t size)
{
    if (!ptr)
    {
        return;
    }
    if (size > MAX_BYTES)
    {
        free(ptr);
        return;
    }

    //将内存回收到空闲链表
    size_t index = SizeClass::getIndex(size);
    freeListSize_[index]++;
    //插入空闲链表头部
    *reinterpret_cast<void**>(ptr) = freeList_[index];
    freeList_[index] = ptr;

    //判断是否需要将内存归还给中心缓存
    if (shouldReturnToCentralCache(index))
    {
        returnToCentralCache(ptr, freeListSize_[index]);
    }
}


bool 
ThreadCache::shouldReturnToCentralCache(size_t index)
{
    constexpr size_t threshold = 256;//阈值
    return freeListSize_[index] > threshold;
}


void*
ThreadCache::fetchFromCentralCache(size_t index)
{
    //对应空闲链表大小为空，向中心缓存申请内存
    size_t size = freeListSize_[index] * ALIGNMENT;
    void* start = CentralCache::getInstance().fetchRange(index);
    if (!start)
    {
        return nullptr;
    }

    //将首块内存返回，其余的放入空闲链表
    void* result = start;
    freeList_[index] = *reinterpret_cast<void**>(start);

    //遍历，得到内存块的数量
    size_t batchNum = 0;
    void* current = *reinterpret_cast<void**>(start);
    while (current)
    {
        ++batchNum;
        current = *reinterpret_cast<void**>(current);
    }
    freeListSize_[index] += batchNum;

    return result;
}

void 
ThreadCache::returnToCentralCache(void* ptr, size_t size)
{
    //使用对齐的内存大小
    size_t alignedSize = SizeClass::roundUp(size);

    //根据大小计算实际的索引
    size_t index = SizeClass::getIndex(size);
    auto& list = freeList_[index];
    size_t batchNum = freeListSize_[index];
    if (batchNum <= 1)
    {
        return;
    }
    //归还3/4
    size_t keepNum = std::max(size_t(1), batchNum / 4);
    size_t returnNum = batchNum - keepNum;

    void* splitNode = ptr;
    for (int i = 1; i < keepNum; ++i)
    {
        splitNode = *reinterpret_cast<void**>(splitNode);
        if (!splitNode)
        {
            return;
        }
    } 
    //分为两个部分
    void* nextNode = *reinterpret_cast<void**>(splitNode);
    *reinterpret_cast<void**>(splitNode) = nullptr;
    
    if (returnNum > 0 && nextNode)
    {
        returnToCentralCache(nextNode, returnNum * alignedSize);
    }
}

} // namespace memoryPool