#pragma once

// N 字节大小的数据，必须放在 N 的整数倍地址上
// 计算从地址p开始，内存对齐要填充的最少字节数量
template <typename T, size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::size_type
MemoryPool<T, BlockSize>::padPointer(byte_pointer_ p, size_type align)
const noexcept
{
    size_t result = reinterpret_cast<size_t>(p);

    return (align - result) % align;
    // size_t r = result % align;
    // return r == 0 ? 0 : align - r;
} 


//取地址函数
template <typename T, size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::pointer
MemoryPool<T, BlockSize>::address(reference x)
const noexcept 
{
    return &x;
}
template <typename T, size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::const_pointer
MemoryPool<T, BlockSize>::address(const_reference x) 
const noexcept 
{
    return &x;
}


//默认构造函数
template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool() noexcept
    : currentBlock_(nullptr),
      currentSlot_(nullptr),
      lastSlot_(nullptr),
      freeSlots_(nullptr)
{}
//委托构造函数
template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool(const MemoryPool& memoryPool) noexcept 
    :MemoryPool() 
{}
//委托构造函数
template <typename T, size_t BlockSize>
template <class U>
MemoryPool<T, BlockSize>::MemoryPool(const MemoryPool<U, BlockSize>& memoryPool) noexcept 
    :MemoryPool()
{}


//析构函数
template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::~MemoryPool()
noexcept
{
    slot_pointer_ p = currentBlock_;
    while (p != nullptr)
    {
        slot_pointer_ prev = p;
        p = p->next;
        //转成void*的指针，不会调用析构函数，只释放内存
        ::operator delete(reinterpret_cast<void*>(prev));
    }
    currentBlock_ = nullptr;
}

//向操作系统申请内存
template <typename T, size_t BlockSize>
void
MemoryPool<T, BlockSize>::allocateBlock()
{
    //调用 operator new ,申请BlockSize大小的内存
    byte_pointer_ newBlock = reinterpret_cast<byte_pointer_>(::operator new(BlockSize));

    //将最开始的八字节的内存用来存放一个指针(将块插入内存块链表)
    reinterpret_cast<slot_pointer_>(newBlock)->next = currentBlock_;
    currentBlock_ = reinterpret_cast<slot_pointer_>(newBlock);

    //计算用于存放数据的起始地址
    byte_pointer_ body = newBlock + sizeof(slot_pointer_);

    //计算从地址body开始，内存对齐所需的最少字节数量
    size_type bodyPadding = padPointer(body, alignof(slot_type_));
    currentSlot_ = reinterpret_cast<slot_pointer_>(body + bodyPadding);//永远指向下一个空闲内存

    //计算最后一个空闲内存
    lastSlot_ = reinterpret_cast<slot_pointer_>(newBlock + BlockSize - sizeof(slot_type_) + 1);
}


//返回一个指向对象所需内存的指针
template <typename T, size_t BlockSize>
typename MemoryPool<T, BlockSize>::pointer
MemoryPool<T, BlockSize>::allocate(size_type, const_pointer)
{
    //先从空闲链表中找
    if (freeSlots_)
    {
        slot_pointer_ ret = freeSlots_;
        freeSlots_ = freeSlots_->next;
        return reinterpret_cast<pointer>(ret);
    }
    //判断当前内存块是否已经满了
    if (currentSlot_ >= lastSlot_)
    {
        //满了就向操作系统申请新的内存块
        allocateBlock();
    }
    //返回空闲内存
    return  reinterpret_cast<pointer>(currentSlot_++); 
}


//将析构后对象所占内存放入空闲链表
template <typename T, size_t BlockSize>
inline void 
MemoryPool<T, BlockSize>::deallocate(pointer p, size_type)
noexcept 
{
    //插入链表头部即可
    if (p)
    {
        reinterpret_cast<slot_pointer_>(p)->next = freeSlots_;
        freeSlots_ = reinterpret_cast<slot_pointer_>(p);
    }
}


//在已申请的内存上构造/析构对象
template <typename T, size_t BlockSize>
inline void 
MemoryPool<T, BlockSize>::construct(pointer p, const_reference val)
{
    //调用 placement new
    if (p)
    {
        new(p) value_type(val);
    }
}
template <typename T, size_t BlockSize>
inline void 
MemoryPool<T, BlockSize>::destroy(pointer p)
{
    //placement new 构造的对象需要手动调用析构函数
    if (p)
    {
        p->~value_type();
    }
}


//自动申请内存构造
template <typename T, size_t BlockSize>
typename MemoryPool<T, BlockSize>::pointer
MemoryPool<T, BlockSize>::newElement(const_reference val)
{
    //向内存池申请内存
    pointer p = allocate();
    //在内存上构造对象
    construct(p, val);
    return p;
}

//自动析构对象并释放内存
template <typename T, size_t BlockSize>
void 
MemoryPool<T, BlockSize>::deleteElement(pointer p)
{
    //析构内存中的对象
    destroy(p);
    //将内存放到空闲链表中
    deallocate(p);
}

//得到最多可以分配的对象
template <typename T, size_t BlockSize>
typename MemoryPool<T, BlockSize>::size_type
MemoryPool<T, BlockSize>::max_size()
const noexcept 
{
    //最多可以分配的Block
    size_type maxBlocks = -1 / BlockSize; // -1 = UMax
    //减去最开始的指针所占的地址空间    
    return (BlockSize - sizeof(slot_pointer_)) / sizeof(slot_type_) * maxBlocks;
}

