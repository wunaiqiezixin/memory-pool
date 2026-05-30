#include "../include/MemoryPool.h"

namespace memoryPool {

MemoryPool::MemoryPool(size_t BlockSize)
    : BlockSize_(BlockSize)
{}

MemoryPool::~MemoryPool()
{
    Slot* curr = currentBlock_;
    while (curr)
    {
        Slot* prev = curr;
        curr = curr->next;
        //转换成void*的指针，不会调用对象的析构函数
        ::operator delete(reinterpret_cast<void*>(prev));
    }

    //将空闲链表的头指针设为空
    freeList_.store(nullptr, std::memory_order_relaxed);
}

void 
MemoryPool::init(size_t SlotSize)
{
    SlotSize_ = SlotSize;
    currentBlock_ = nullptr;
    currentSlot_ = nullptr;
    lastSlot_ = nullptr;
    freeList_.store(nullptr, std::memory_order_relaxed);
}

bool
MemoryPool::pushFreeList(Slot* slot)
{
    while (true)
    {
        // 1.原子读取旧的头节点
        Slot* oldHead = freeList_.load(std::memory_order_relaxed);
        // 2.让新节点指向当前头
        slot->next.store(oldHead, std::memory_order_relaxed);
        // 3.尝试更新头节点
        if (freeList_.compare_exchange_weak(oldHead,
                                            slot,
                                            std::memory_order_release,
                                            std::memory_order_relaxed
                                        ))
        {
            return true;
        }
        // CAS失败则重试
    }
}

Slot* 
MemoryPool::popFreeList()
{
    while (true)
    {
        // 1.原子读取旧的头节点
        Slot* oldHead = freeList_.load(std::memory_order_relaxed);
        if (oldHead == nullptr)
        {
            return nullptr;
        }
        // 2.原子读取新的头节点
        Slot* newHead = oldHead->next.load(std::memory_order_relaxed);
        // 3.尝试更新头节点
        if (freeList_.compare_exchange_weak(oldHead,
                                            newHead,
                                            std::memory_order_release,
                                            std::memory_order_relaxed))
        {
            return oldHead;
        }
        // CAS失败则重试
    }
}

void 
MemoryPool::allocateNewBlock()
{
    void* newBlock = ::operator new(BlockSize_);

    reinterpret_cast<Slot*>(newBlock)->next = currentBlock_;
    currentBlock_ = reinterpret_cast<Slot*>(newBlock);

    byte_pointer body = reinterpret_cast<byte_pointer>(newBlock) + sizeof(Slot*);
    size_t bodyPadding = padPointer(body, SlotSize_);

    currentSlot_ = reinterpret_cast<Slot*>(body + bodyPadding);
    lastSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<byte_pointer>(newBlock) + BlockSize_ - SlotSize_ + 1);

    freeList_.store(nullptr, std::memory_order_relaxed);
}

size_t
MemoryPool::padPointer(byte_pointer p, size_t align)
{
    size_t result = reinterpret_cast<uintptr_t>(p);
    return static_cast<size_t>((align - result) % align);
}

void* 
MemoryPool::allocate(size_t size)
{
    if (size <= 0)
    {
        return nullptr;
    }
    //优先检查空闲链表是否为空
    Slot* slot = popFreeList();
    if (slot)
    {
        return slot;
    }
    //检查内存块是否够用
    std::lock_guard<std::mutex> lock(mutexForBlock_);
    if (currentSlot_ >= lastSlot_)
    {
        allocateNewBlock();
    }
    //返回下一个空闲内存槽
    void* ret = reinterpret_cast<void*>(currentSlot_);
    currentSlot_ += SlotSize_ / sizeof(Slot);
    return ret;
}

void 
MemoryPool::deallocate(void* p)
{
    if (p == nullptr)
    {
        return;
    }
    pushFreeList(static_cast<Slot*>(p));
}


void 
HashBucket::initMemoryPool()
{
    for (int i = 0; i < MEMORYPOOL_NUM; ++i)
    {
        getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
    }
}

//单例模式
MemoryPool&
HashBucket::getMemoryPool(size_t index)
{
    static MemoryPool memoryPool[MEMORYPOOL_NUM];
    return memoryPool[index];
}

void*
HashBucket::useMemory(size_t size)
{
    if (size <= 0)
    {
        return nullptr;
    }
    if (size > SLOT_MAX_SIZE)
    {
        return ::operator new(size);
    }
    return getMemoryPool((size + SLOT_BASE_SIZE - 1) / SLOT_BASE_SIZE - 1).allocate(size);
}

void 
HashBucket::freeMemory(void* p, size_t size)
{
    if (size < 0 || p == nullptr)
    {
        return;
    }
    if (size > SLOT_MAX_SIZE)
    {
        ::operator delete(p);
    }
    getMemoryPool(
        (size + SLOT_BASE_SIZE - 1) / SLOT_BASE_SIZE - 1
    ).deallocate(p);
}



} // namespace memoryPool