#include "../include/CentralCache.h"
#include "../include/PageCache.h"
#include "../include/Common.h"
#include <cassert>
#include <thread>
#include <chrono>

namespace memoryPool {

constexpr std::chrono::milliseconds DELAY_INTERVAL{1000};
static constexpr size_t SPAN_PAGES = 8;//每次从PageCache获取内存页的大小

CentralCache::CentralCache()
{
    for (auto& list : centralFreeList_)
    {
        list.store(nullptr, std::memory_order_relaxed);
    }
    for (auto& l : locks_)
    {
       l.clear(std::memory_order_relaxed); 
    }
    //初始化延迟归还相关的变量
    for (auto& count : delayCounts_)
    {
        count.store(0, std::memory_order_relaxed);
    }
    for (auto& last : lastReturnTimes_)
    {
        last = std::chrono::steady_clock::now();
    }
    spanCount_.store(0, std::memory_order_relaxed);
}


void*
CentralCache::fetchRange(size_t index)
{
    if (index >= FREE_LIST_SIZE)
    {
        return nullptr;
    }
    //自旋锁
   while (locks_[index].test_and_set(std::memory_order_acquire))
   {
        std::this_thread::yield();//添加线程让步，避免忙等待，避免浪费CPU
   }
   void* result = nullptr;
   try 
   {
        //尝试向中心缓存自由链表获取内存
        result = centralFreeList_[index].load(std::memory_order_release);
        if (!result)//中心缓存自由链表为空
        {
            size_t size = (index + 1) * ALIGNMENT;
            result = fetchFromPageCache(size);
            if (!result)
            {
                locks_[index].clear();
                return nullptr;
            }
            //将获取的内存切分成小块
            byte_pointer start = static_cast<byte_pointer>(result);
            //计算实际获取到的页数
            size_t numPages = (size <= SPAN_PAGES * PageCache::PAGE_SIZE) ? SPAN_PAGES : (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;
            //计算内存块的数量
            size_t blockNum = numPages * PageCache::PAGE_SIZE / size;
            //连成链表
            if (blockNum > 1) 
            {
                for (int i = 1; i < blockNum; ++i)
                {
                    void* current = start + (i - 1) * size;
                    void* next    = start + i * size;
                    *reinterpret_cast<void**>(current) = next;
                }
                *reinterpret_cast<void**>(start + (blockNum - 1) * size) = nullptr;
                
                //保存result的下一个节点
                void* next = *reinterpret_cast<void**>(result);
                //将result与链表断开
                *reinterpret_cast<void**>(result) = next;
                //更新中心缓存自由链表
                centralFreeList_[index].store(next, std::memory_order_release);

                //更新SpanTracker
                size_t trackerIndex = spanCount_++;
                if (trackerIndex < spanTrackers_.size())
                {
                    spanTrackers_[trackerIndex].spanAddr.store(start, std::memory_order_release);
                    spanTrackers_[trackerIndex].blockCount.store(blockNum, std::memory_order_release);
                    spanTrackers_[trackerIndex].numPages.store(numPages, std::memory_order_release);
                    spanTrackers_[trackerIndex].freeCount.store(blockNum - 1, std::memory_order_release);
                }
            }
        }
        else//中心缓存自由链表不为空 
        {            
            void* next = *reinterpret_cast<void**>(result);
            *reinterpret_cast<void**>(result) = nullptr;
            centralFreeList_[index].store(next, std::memory_order_release);

            SpanTracker* tracker = getSpanTracker(result);
            if (tracker)
            {
                //减少一个空闲块的数量
                tracker->freeCount.fetch_sub(1, std::memory_order_release);
            }
        }
   }
   catch (...)
   {
        locks_[index].clear(std::memory_order_release);
        throw;
   }
   locks_[index].clear(std::memory_order_release);
   return result;
}

void
CentralCache::returnRange(void* start, size_t size, size_t index)
{
    if (!start || index >= FREE_LIST_SIZE)
    {
        return;
    }
    size_t blockSize = (index + 1) * ALIGNMENT;
    size_t blockNum = size / blockSize;

    //自旋锁
    while (locks_[index].test_and_set(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }

    try
    {
        void* end = start;
        size_t count = 1;
        while (*reinterpret_cast<void**>(end) && count < blockNum)
        {
            end = *reinterpret_cast<void**>(end);
            ++count;
        }  
        void* current = centralFreeList_[index].load(std::memory_order_relaxed);
        *reinterpret_cast<void**>(end) = current;
        centralFreeList_[index].store(start, std::memory_order_release);

        //更新延迟计数
        size_t currentCount = delayCounts_[index] + 1;
        auto currentTime = std::chrono::steady_clock::now();

        if (shouldPerformDelayedReturn(index, currentCount, currentTime))
        {
            performDelayReturn(index);
        }
    }
    catch(...)
    {
        locks_[index].clear(std::memory_order_release);
        throw;
    }
    locks_[index].clear();
}

//检查是否需要延迟归还
bool
CentralCache::shouldPerformDelayedReturn(size_t index, size_t currentCount, 
                                         std::chrono::steady_clock::time_point currentTime)
{
    //基于计数和时间双重检查
    if (currentCount >= MAX_DELAY_COUNT)
    {
        return true;
    }
    auto lastTime = lastReturnTimes_[index];
    return currentTime - lastTime >= DELAY_INTERVAL; 
}

//执行延迟归还
void
CentralCache::performDelayReturn(size_t index)
{
    //重置延迟计数
    delayCounts_[index].store(0, std::memory_order_relaxed);
    //更新最后归还时间
    lastReturnTimes_[index] = std::chrono::steady_clock::now();
    //统计每个span的空闲块数量
    std::unordered_map<SpanTracker*, size_t> spanCounts;
    void* currentBlock = centralFreeList_[index].load(std::memory_order_relaxed);
    while (currentBlock)
    {
        SpanTracker* span = getSpanTracker(currentBlock);
        spanCounts[span]++;
        currentBlock = *reinterpret_cast<void**>(currentBlock);
    }
    //更新每个span的空闲计数，并检查是否可以归还
    for (auto& [tracker, newFreeBlocks] : spanCounts)
    {
        updateSpanFreeCount(tracker, newFreeBlocks, index);
    }
}

//将分散在 CentralCache 空闲链表中的、属于同一个 Span 的小块内存，
//重新拼接成一个连续的大内存块，归还给系统。
void
CentralCache::updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index)
{
    size_t oldFreeCount = tracker->freeCount.load(std::memory_order_relaxed);
    size_t newFreeCount = oldFreeCount + newFreeBlocks;
    tracker->freeCount.store(newFreeCount, std::memory_order_release);
    //如果span的内存全部空闲
    if (tracker->freeCount == tracker->blockCount)
    {
        //将span中的内存块从空闲链表中移除
        void* spanAddr = tracker->spanAddr;
        size_t numPages = tracker->numPages;

        void* newHead = nullptr;
        void* pre = nullptr;
        void* head = centralFreeList_[index].load(std::memory_order_relaxed);
        void* current = head;
        
        while (current)
        {
            void* next = *reinterpret_cast<void**>(current);
            //如果当前内存块属于span
            if (current >= spanAddr &&
                current < static_cast<byte_pointer>(spanAddr) + numPages * PageCache::PAGE_SIZE)
            {
                if (pre)//不是链表的头节点
                {
                    *reinterpret_cast<void**>(pre) = next;
                }
                else //是链表的头节点
                {
                    newHead = next;
                }
            }
            else//如果当前内存块不属于span
            {
                pre = current;
            }
            current = next;
        }
        centralFreeList_[index].store(newHead, std::memory_order_release);
        PageCache::getInstance().deallocateSpan(spanAddr, numPages);
    }
}

void*
CentralCache::fetchFromPageCache(size_t size)
{
    //计算实际需要分配的页数
    size_t numPages = size <= SPAN_PAGES * PageCache::PAGE_SIZE ? SPAN_PAGES : 
                      (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;
    //调用接口
    return PageCache::getInstance().allocateSpan(numPages);
}

SpanTracker*
CentralCache::getSpanTracker(void* blockAddr)
{
    //遍历SpanTrackers_数组，找到blockAddr所属的Span
    for (size_t i = 0; i < spanCount_.load(std::memory_order_relaxed); ++i)
    {
        void* spanAddr = spanTrackers_[i].spanAddr.load(std::memory_order_relaxed);
        size_t numPages = spanTrackers_[i].numPages.load(std::memory_order_relaxed);
        if (blockAddr >= spanAddr && 
            blockAddr < static_cast<byte_pointer>(spanAddr) + numPages * PageCache::PAGE_SIZE)
        {
            return &spanTrackers_[i];
        }
    }
    return nullptr;
}


} // namespace memoryPool