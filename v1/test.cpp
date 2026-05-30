#include <iostream>
#include <chrono>
#include <thread>
#include <string>

#include "include/MemoryPool.h"
using namespace memoryPool;

//计时器
struct Timer
{
    decltype(std::chrono::high_resolution_clock::now()) start, end;
    std::chrono::duration<float> duration;
    Timer()
    {
        start = std::chrono::high_resolution_clock::now();
    }
    ~Timer()
    {
        end = std::chrono::high_resolution_clock::now();
        duration = end - start;

        float ms = duration.count() * 1000.0f;

        std::cout << ms << "ms\n";
    }
};


//测试用例
class P1
{
    int id_;
};

class P2
{
    int id_[5];
};

class P3
{
    int id_[10];
};

class P4
{
    int id_[20];
};

//测试函数：new/delete
void benchMarkNewDelete(size_t nthreads, size_t ntimes, size_t nrounds)
{
    std::thread threadPool[nthreads];
    {
    Timer timer;
    for (int k = 0; k < nthreads; ++k)
    {
        threadPool[k] = std::thread([&](){
            for (int i = 0; i < nrounds; ++i)
            {
                for (int j = 0; j < ntimes; ++j)
                {
                    P1* p1 = new P1;
                    delete p1;
                    P2* p2 = new P2;
                    delete p2;
                    P3* p3 = new P3;
                    delete p3;
                    P4* p4 = new P4;
                    delete p4;
                }
            }
        });
    }
    for (auto& t : threadPool)
    {
        t.join();
    }
    printf("%lu threads, %lu rounds, %lu new/delete               operations per round, total time: ", nthreads, nrounds, ntimes);
    }
}


//测试函数：newElement/deleteElement
void benchMarkMemoryPool(size_t nthreads, size_t ntimes, size_t nrounds)
{
    std::thread threadPool[nthreads];
    {
    Timer timer;
    for (int k = 0; k < nthreads; ++k)
    {
        threadPool[k] = std::thread([&](){
            for (int i = 0; i < nrounds; ++i)
            {
                for (int j = 0; j < ntimes; ++j)
                {
                    P1* p1 = newElement<P1>();
					deleteElement(p1);
					P2* p2 = newElement<P2>();
					deleteElement(p2);
					P3* p3 = newElement<P3>();
					deleteElement(p3);
					P4* p4 = newElement<P4>();
					deleteElement(p4);
                }
            }
        });
    }
    for (auto& t : threadPool)
    {
        t.join();
    }
    printf("%lu threads, %lu rounds, %lu newElement/deleteElement operations per round, total time: ", nthreads, nrounds, ntimes);
    }
}

int main()
{
	HashBucket::initMemoryPool();
	std::cout << "============================================================================";
	std::cout << "===================\n";
	benchMarkMemoryPool(50, 10000, 10);
	benchMarkNewDelete(50, 10000, 10);
	std::cout << "============================================================================";
	std::cout << "===================\n";
}