#include "include/MemoryPool.h"
#include <iostream>
#include <vector>
#include <thread>
#include <ctime>
using namespace memoryPool;

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

//单轮次申请释放数、线程总数、总轮数
void benchMarkMemoryPool(size_t ntimes, size_t nworks, size_t rounds)
{
    std::vector<std::thread> threadPool(nworks); //线程池
    size_t total_timecost = 0;
    for (int k = 0; k < nworks; ++k)
    {
        threadPool[k] = std::thread([&]()
    {
        for (int i = 0; i < rounds; ++i)
        {
            size_t start = clock();

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

            size_t end = clock();

            total_timecost += end - start;
        }
    });
    for (int k = 0; k < nworks; ++k)
    {
        threadPool[k].join();
    }
    }
    printf("%lu个线程并发执行%lu轮次，每次newElement/deleteElement%lu次，"
           "总计花费 %lu ms\n");
}

//单轮次申请释放数、线程总数、总轮数
void benchMarkNewDelete(size_t ntimes, size_t nworks, size_t rounds)
{
    std::vector<std::thread> threadPool(nworks); //线程池
    size_t total_timecost = 0;
    for (int k = 0; k < nworks; ++k)
    {
        threadPool[k] = std::thread([&]()
    {
        for (int i = 0; i < rounds; ++i)
        {
            size_t start = clock();

            for (int j = 0; j < ntimes; ++j)
            {
                P1* p1 = new P1;
                delete p1;
                P2* p2 = new P2;
                delete(p2);
                P3* p3 = new P3;
                delete(p3);
                P4* p4 = new P4;
                delete(p4);
            }

            size_t end = clock();

            total_timecost += end - start;
        }
    });
    for (int k = 0; k < nworks; ++k)
    {
        threadPool[k].join();
    }
    }
    printf("%lu个线程并发执行%lu轮次，每次new/delete%lu次，"
           "总计花费 %lu ms\n");
}

int main()
{
    std::cout << "\n\n";
    HashBucket::initMemoryPool();
    std::cout << "=================================================\n";
    benchMarkMemoryPool(100, 1, 10);
    std::cout << "=================================================\n";
    benchMarkNewDelete(100, 1, 10);
    std::cout << "=================================================\n";
    std::cout << "\n\n";
}