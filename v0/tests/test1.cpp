#include <iostream>
#include <chrono>
#include "../include/MemoryPool.h"
#include <memory>

class Timer
{
public:
    Timer()
    {
        m_StartTimepoint = std::chrono::high_resolution_clock::now();
    }
    ~Timer()
    {
        auto endTimepoint = std::chrono::high_resolution_clock::now();
        
        auto start = std::chrono::time_point_cast<std::chrono::microseconds>(m_StartTimepoint);
        auto end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint);

        auto duration = end - start;

        std::cout << duration.count() << "us" << std::endl;
    }
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTimepoint;    
};
