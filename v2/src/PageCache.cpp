#include "../include/PageCache.h"

namespace memoryPool {

void*
PageCache::systemAlloc(size_t numPages)
{
    size_t size = numPages * PAGE_SIZE;

    // 使用mmap分配内存
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return nullptr;

    // 清零内存
    memset(ptr, 0, size);
    return ptr;
}

void* 
PageCache::allocateSpan(size_t numPages)
{
    //互斥锁，保证线程安全
    std::lock_guard<std::mutex> lock(mutex_);
    //在空闲链表中查找
    auto it = freeSpans_.lower_bound(numPages);
    if (it != freeSpans_.end())
    {
        //将其从空闲链表中删除
        Span* span = it->second;
        if (span->next)
        {
            freeSpans_[numPages] = freeSpans_[numPages]->next;
        }
        else 
        {
            freeSpans_.erase(it);
        }
        //判断找到的内存是否大于需要的内存
        if (span->numPages > numPages)
        {
            //将多余部分放回空闲链表
            Span* newSpan{new Span};
            newSpan->pageAddr = static_cast<byte_pointer>(span->pageAddr) + numPages * PAGE_SIZE;
            newSpan->numPages = span->numPages - numPages;
            //将newSpan插入空闲链表头部
            auto& list = freeSpans_[newSpan->numPages];
            newSpan->next = list;
            list = newSpan;
            //更新span的内存页数量
            span->numPages = numPages;
        }
        //将span的内存首地址记录
        spanMap_[span->pageAddr] = span;
        //返回内存
        return span->pageAddr;
    }
    //如果空闲链表找不到合适的内存，向系统申请
    void* memory = systemAlloc(numPages);
    //创建Span
    Span* span{new Span};
    span->pageAddr = memory;
    span->numPages = numPages;
    span->next = nullptr;
    //将地址记录
    spanMap_[memory] = span;
    return memory;
}

//将中心缓存的内存回收
void 
PageCache::deallocateSpan(void* ptr, size_t numPages)
{
    //互斥锁，保证线程安全
    std::lock_guard<std::mutex> lock(mutex_);
    //检查回收的内存是否在spanMap_中
    auto it = spanMap_.find(ptr);
    if (it == spanMap_.end())
    {
        return;//不在，直接返回
    }
    Span* span = it->second;
    //尝试合并下一个Span
    void* nextAddr = static_cast<byte_pointer>(span->pageAddr) + numPages * PAGE_SIZE;
    auto nextIt = spanMap_.find(nextAddr);
    Span* nextSpan = nextIt->second;
    //判断nextSpan是否在空闲链表中，并删除
    bool found = false;
    auto& nextList = freeSpans_[nextSpan->numPages];
    //如果为头部
    if (nextList == nextSpan)
    {
        found = true;
        nextList = nextList->next;
    }
    else 
    {
        Span* prev = nextList;
        while (prev->next)
        {
            if (prev->next == nextSpan)
            {
                found = true;
                prev->next = prev->next->next;
                break;
            }
            prev = prev->next;
        }
    }
    //找到了才进行合并
    if (found)
    {
        span->numPages += nextSpan->numPages;
        delete nextSpan;
    }
    //将span放入空闲链表
    auto& list = freeSpans_[span->numPages];
    span->next = list;
    list = span;

}

} // namespace memoryPool