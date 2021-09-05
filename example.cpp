#include <iostream>
#include <vector>
#include <chrono>
#include <type_traits>
#include <limits.h>
#include <unistd.h>
#include <atomic>

#include "ThreadPool.h"

auto func = [](int k){for(int j =2; j<k;++j)\
                            if( k%j ==0)return false;
                            return true;};
// auto increse = [&inc](){}
int main()
{
    char buf[1024];
    ::read(STDIN_FILENO, buf, 1024);
    int cpuNumber = sysconf(_SC_NPROCESSORS_CONF);
    ThreadPool pool(cpuNumber);
    //保存异步任务结果的vector
    std::vector<std::future<void> > results;
    //ThreadPool.enqueue( [](){;} )    typename std::result_of<Fn(Args...)>::type
    std::atomic<int> inc(0);
    for(int i = 0; i < 100000; ++i) {
        // results.emplace_back( pool.enqueue([&inc](){ ++inc;}) );
        pool.enqueue([&inc](){ ++inc;});
    }
    // int ret=0;
    // for(auto && result: results)
    //     if(result.get())ret++;
    std::cout << static_cast<int>(inc.load())<< std::endl;
    
    return 0;
}
