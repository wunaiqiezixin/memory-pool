#include <iostream>
#include <ctime>
#include "include/MemoryPool.h"
#include <memory>
using namespace std;

#define ELEMS 1000000
#define REPS 50


int main()
{
    clock_t start, end;
    //使用MemoryPool
    MemoryPool<size_t, 4096> memoryPool;
    start = clock();
    for (size_t i = 0; i < REPS; ++i)
    {
        for (size_t j = 0; j < ELEMS; ++j)
        {
            size_t* p = memoryPool.newElement(1);
            memoryPool.deleteElement(p);
        }
    }
    end = clock();
    cout << "newElement/deleteElemnet: " << ((double)end - start) / CLOCKS_PER_SEC;
    cout << "\n\n";

    //直接调用new/delete
    allocator<size_t> alloc;
    start = clock();
    for (size_t i = 0; i < REPS; ++i)
    {
        for (size_t j = 0; j < ELEMS; ++j)
        {
            size_t* p = new size_t;
            delete p;
        }
    }
    end = clock();
    cout << "new/delete" << "              : " 
    << ((double)end - start) / CLOCKS_PER_SEC;
}