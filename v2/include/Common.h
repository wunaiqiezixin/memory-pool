#pragma once
#include <cstddef>
#include <cstdlib>
#include <atomic>
#include <array>
using byte_pointer = unsigned char*;

namespace memoryPool {
constexpr size_t MAX_BYTES = 256 * 1024;// 256kb
constexpr size_t ALIGNMENT = 8;
constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT;

//内存块的头部信息
struct BlockHeader
{
    size_t size;
    bool isUse;
    BlockHeader* next;
};

class SizeClass
{
public:
    static size_t getIndex(size_t bytes) 
    {
        //确保bytes至少为ALIGNMENT
        bytes = std::max(bytes, ALIGNMENT);
        //向上取整再减去1
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    }

    static size_t roundUp(size_t bytes)
    {
        //bytes + ALIGNMENT - 1 将不是8的整数倍的bytes抬到下一个区间
        //最后三位是0的数一定是8的整数倍
        return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }
};

} // namespace memoryPool