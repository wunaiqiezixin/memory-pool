#pragma once

#include <cstddef>
#include <mutex>
#include <memory>
#include <atomic>

namespace memoryPool {
#define MEMORYPOOL_NUM  64
#define SLOT_BASE_SIZE   8
#define SLOT_MAX_SIZE  512
using byte_pointer = unsigned char*;
struct Slot
{
    std::atomic<Slot*> next;//原子指针，保证操作的原子性
};

class MemoryPool
{
public:
    MemoryPool(size_t BlockSize);
    ~MemoryPool();
    void init(size_t SlotSize);

    void* allocate(size_t size);
    void deallocate(Slot* p);

private:
    void allocateNewBlock();
    size_t padPointer(byte_pointer p, size_t align);

    //无锁入队/出队操作
    bool pushFreeList(Slot* slot);
    Slot* popFreeList();

private:
    size_t              BlockSize_;        //一次性分配内存块的大小
    size_t              SlotSize_;         //槽的大小(由哈希映射确定) 
    Slot*               currentBlock_;     //内存块链表的头指针
    Slot*               currentSlot_;      //下一个空闲的内存槽
    Slot*               lastSlot_;         //当前内存块不够用的标记
    std::mutex          mutexForBlock_;    //避免多线程下内存块的多次分配
    std::atomic<Slot*>  freeList_;         //空闲链表的头指针(原子指针)
};

class HashBucket
{
public:
    static void initMemoryPool();
    static void* useMemory(size_t size);
    static void freeMemory(void* p);
    static MemoryPool& getMemoryPool(size_t index);

    template <typename T, typename... Args>
    T* newElement(Args&&... args);
    
    template <typename T> 
    void deleteElement();
};



}