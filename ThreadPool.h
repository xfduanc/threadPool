#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
public:
    ThreadPool(size_t);
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    ~ThreadPool();
private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;
    
    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};
 
// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
    :   stop(false)
{
    for(size_t i = 0;i<threads;++i)
        workers.emplace_back(
            [this]()
            {
                //这个task就是一个壳子，啥也没有，里面调用了packaged_task()调用了operator()
                std::function<void()> task;
                while(true)
                {
                    {
                    /*
                        template <class Predicate>
                        void wait (unique_lock<mutex>& lck, Predicate pred);
                        if pred false , block, true can through
                    */
                   //只有条件为true才能穿过
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,\
                                 [this]{ return this->stop || !this->tasks.empty(); });
                        if(this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                    //取到了任务，执行任务，不要return ，继续监听这个任务队列
                }
            }//end of thread body
        );//end of emplace_back
}
/*
template <typename  Fn, typename... Args>
struct result_of<Fn(Args...)>;
*/
// add new work_task into the queue of the ThreadPool
// retT is std::future< std::result_of<Fn(Args)...>>
template<class Fn, class... Args>
auto ThreadPool::enqueue( Fn&& f, Args&&... args) 
    -> std::future< typename std::result_of<Fn(Args...)>::type >
{
    using return_type = typename std::result_of<Fn(Args...)>::type;

    //return_type为future的模板类型，也就是返回值future的模板类型
    //通过接收端的bind来绑定参数，在运行端去设置具体的参数
    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<Fn>(f), std::forward<Args>(args)...)
        );

    std::future<return_type> res = task->get_future();
    {
        //在独占式锁这个地方去插入线程的入口，注意这个线程的入口是通过packaged_task的operator()来实现的
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task](){ (*task)(); });
    }
    //结束一次插入，条件变量+1
    condition.notify_one();
    //返回这个future，这个future是通过packaged_task的get_future来实现的
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(std::thread &worker: workers)
        worker.join();
}

#endif
