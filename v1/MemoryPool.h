#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <utility>

namespace memoryPool {

using byte_pointer = unsigned char*;

struct Slot
{
    std::atomic<Slot*> next;
};

class MemoryPool
{
public:
    MemoryPool(size_t BlockSize);
    ~MemoryPool();
    void init(size_t SlotSize);

    void* allocate();
    void deallocate();

private:
    void allocateNewBlock();
    size_t padPointer(byte_pointer p, size_t align);

private:
    size_t              BlockSize_;
    size_t              SlotSize_;
    Slot*               currentBlock_;
    Slot*               currentSlot_;
    Slot*               lastSlot_;
    std::atomic<Slot*>  freeList_;
    std::mutex          mutexForBlock_;
};

class HashBucket
{
public:
    static void initMemoryPool();
    static void* useMemory(size_t size);
    static void freeMemory();
    static MemoryPool& getMemoryPool(int index);
    
    template <typename T, typename... Args>
    friend T* newElement(Args&&... args);

    template <typename T>
    friend void deleteElement(T* p);

};


}// namespace memoryPool