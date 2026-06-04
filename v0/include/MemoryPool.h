#pragma once

#include <cstddef>   // size_t
#include <new>       // operator new, operator delete, placement new
#include <stdexcept> // std::bad_alloc

template <typename T, size_t BlockSize = 4096>
class MemoryPool
{
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

    /*
    rebind是一个类型转换器：
    它让分配类型T的分配器能够分配类型U的对象
    */
    template <typename U>
    struct rebind
    {
        using other = MemoryPool<U, BlockSize>;
    };

private:
    union Slot_
    {
        T element;
        Slot_* next;
    };

    using byte_pointer_ = unsigned char*;/* 操作一个字节的内存 */
    using slot_type_ = Slot_;
    using slot_pointer_ = Slot_*;

    /* 私有成员变量 */
    slot_pointer_ currentBlock_;    /*指向内存块链表的头指针*/
    slot_pointer_ currentSlot_;     /*指向下一个空闲的内存*/
    slot_pointer_ lastSlot_;        /*当前内存块不够用的标记*/
    slot_pointer_ freeSlots_;       /*指向空闲链表的头指针*/

    /* 私有成员函数 */
    //计算内存对齐所需填充的最少字节数
    size_type padPointer(byte_pointer_ p, size_type align) const noexcept;
    //向操作系统申请一块内存放入内存池
    void allocateBlock();

public:

    //取地址函数：C++ 标准分配器接口的强制要求
    pointer address(reference x) const noexcept;
    const_pointer address(const_reference x) const noexcept;

    //构造函数
    MemoryPool() noexcept;
    MemoryPool(const MemoryPool& memoryPool) noexcept;
    template <typename U>
    MemoryPool(const MemoryPool<U, BlockSize>& memoryPool) noexcept;
    //析构函数
    ~MemoryPool() noexcept;

    //向内存池申请/释放一个对象的内存
    pointer allocate(size_type n = 1, const_pointer hint = nullptr);//hint为了提高缓存命中率
    void deallocate(pointer p, size_type n = 1) noexcept;//将析构对象的内存放到空闲链表

    //在空闲内存上构造/析构一个对象
    void construct(pointer p, const_reference val);
    void destroy(pointer p);

    //自动申请内存构造对象/析构对象释放内存的函数
    pointer newElement(const_reference val);
    void deleteElement(pointer p);

    //得到最大分配数量
    size_type max_size() const noexcept;

};

#include "../src/MemoryPool.tcc"
