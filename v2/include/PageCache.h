#include <mutex>
#include <sys/mman.h>
#include <cstring>
#include <map>
using byte_pointer = unsigned char*;

namespace memoryPool {

class PageCache
{
public:
   //页的大小
   static constexpr size_t PAGE_SIZE = 4096;//4kb
   //单例模式
   static PageCache& getInstance() 
   {
       static PageCache instance;
       return instance;
   }
   //向中心缓存分配内存
   void* allocateSpan(size_t numPages);
   //回收中心缓存的内存
   void  deallocateSpan(void* ptr, size_t numPages);

private:
   //构造函数私有化
   PageCache() = default;
   //向系统申请内存(以页为单位)
   void* systemAlloc(size_t numPages);

private:
   //Span结构体，记录内存信息
   struct Span
   {
       void* pageAddr;
       size_t numPages;
       Span* next; 
   };
   //空闲链表，不同页数对应不同链表
   std::map<size_t, Span*> freeSpans_;
   //记录所有Span信息，将内存首地址与Span映射
   std::map<void*, Span*> spanMap_;
   //互斥锁
   std::mutex mutex_;
};

} // namespace memoryPool