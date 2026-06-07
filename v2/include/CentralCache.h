#pragma once
#include "Common.h"
#include <atomic>
#include <chrono>
#include <array>
#include <unordered_map>


namespace memoryPool {

struct SpanTracker
{
    std::atomic<void*>  spanAddr{nullptr};  //span对应的连续内存页的起始地址
    std::atomic<size_t> numPages{0};        //span的内存页数量
    std::atomic<size_t> blockCount{0};      //内存页被切分的内存块数
    std::atomic<size_t> freeCount{0};       //空闲的内存块数 
};

class CentralCache
{
public:
    static CentralCache& getInstance()
    {
        static CentralCache instance;
        return instance;
    }
    //向ThreadCache提供的接口
    void* fetchRange(size_t index);
    void  returnRange(void* start, size_t size, size_t index);

private:
    //相互是还所有原子指针为nullptr
    CentralCache();
    //从页缓存获取内存
    void* fetchFromPageCache(size_t size);
    //获取Span信息
    SpanTracker* getSpanTracker(void* blockAddr);
    //更新Span的空闲计数，并检查是否可以归还
    void updateSpanFreeCount(SpanTracker* traker, size_t newBlocks, size_t index);

private:
    //中心缓存的自由链表
    std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_;
    //用于同步的自旋锁
    std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;

    //使用数组存储Span信息，避免map的开销
    std::array<SpanTracker, 1024> spanTrackers_;
    std::atomic<size_t> spanCount_{0};
    //延迟归还相关的成员变量
    static const size_t MAX_DELAY_COUNT = 48; //最大延迟计数
    std::array<std::atomic<size_t>, FREE_LIST_SIZE> delayCounts_;//每个大小类的延迟计数

    std::array<std::chrono::steady_clock::time_point, FREE_LIST_SIZE> lastReturnTimes_;//上次归还时间
    static const std::chrono::milliseconds DELAY_INTERVAL;//延迟间隔

    bool shouldPerformDelayedReturn(size_t index, size_t currentCount, 
                                    std::chrono::steady_clock::time_point currentTime);
    void performDelayReturn(size_t index);

};

} // namespace memoryPool