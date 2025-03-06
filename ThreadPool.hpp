#ifndef _THREADPOOL_HPP
#define _THREADPOOL_HPP

#include "SafeQueue.hpp"
#include <functional>
#include <thread>
#include <condition_variable>
#include <future>

class ThreadPool {
public:
    // 构造函数
    ThreadPool(const int _n_threads) : _threads(_n_threads), _shutdown(false) {
        for (int i = 0; i < _threads.size(); i++) {
            _threads[i] = std::thread(ThreadWorker(this, i));
        }
    };

    ThreadPool(const ThreadPool &) = delete; // 禁用拷贝构造

    ThreadPool(ThreadPool &&) = delete; // 禁用移动构造

    ThreadPool &operator=(const ThreadPool &) = delete; // 禁用拷贝赋值

    ThreadPool &operator=(ThreadPool &&) = delete; // 禁用移动赋值

    template<typename F, typename...Args>
    auto submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))> { // f是函数, args是实参
        using RT = decltype(f(args...)); // return type 返回类型的别名
        // 用 bind 表示式把 函数 和 实参 封装到 func 中
        std::function<RT()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        // 先用 packaged_task 把 func 变成一个可以异步执行的对象，并且用共享指针来访问他
        auto task_ptr = std::make_shared<std::packaged_task<RT()>>(func);
        _tasks.push([task_ptr]() {
            (*task_ptr)();
        });
        _tasks_empty.notify_one(); // 唤醒任意一个被阻塞的进程
        return task_ptr->get_future(); // 返回一个 future 用于异步的获得函数运行的结果
    }

    void shutdown() {
        _shutdown = true;
        _tasks_empty.notify_all(); // 唤醒所以因任务队列为空而等待的线程。
        for (size_t i = 0; i < _threads.size(); i++) {
            if (_threads[i].joinable()) _threads[i].join();
        }
    }

private:
    using TaskType = std::function<void()>; // 任务类型
    std::atomic<bool> _shutdown; // 线程池是否关闭
    SageQueue<TaskType> _tasks; // 任务队列
    std::vector<std::thread> _threads; // 线程队列
    std::mutex _mtx;
    std::condition_variable _tasks_empty; // 任务队列为空的条件变量，防止死锁

    // 线程工作类
    class ThreadWorker {
    public:
        ThreadWorker(ThreadPool *_pool, const int _id) : _pool(_pool), _id(_id) {};

        // 重载函数调用运算符
        void operator()() {
            TaskType func;
            bool gotten = false; // 是否已经取出任务
            while (not _pool->_shutdown or not _pool->_tasks.empty()) {
                std::unique_lock<std::mutex> lock(_pool->_mtx);
                if (_pool->_tasks.empty() and not _pool->_shutdown) _pool->_tasks_empty.wait(lock);
                if (_pool->_tasks.empty()) gotten = false;
                else { // 成功取出任务
                    gotten = true;
                    func = std::move(_pool->_tasks.top());
                    _pool->_tasks.pop();
                }
                if (gotten) func(); // 如果已经取出任务就执行
            }
        }
    private:
        int _id; // 线程 id
        ThreadPool *_pool; // 所属线程池
    };
};

#endif _THREADPOOL_HPP
