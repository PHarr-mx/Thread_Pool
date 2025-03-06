# Thread_Pool
简易线程池（C++17）

[项目地址](https://github.com/PHarr-mx/Thread_Pool/)

什么是线程池? 

"管理一个任务队列，一个线程队列，然后每次把一个任务分配给一个线程执行，循环往复。"

## 线程安全的队列
我们理所应当的希望任务尽可能的按照提交顺序执行。因此我们可以用队列来管理任务。但是`std::queue`并不是线程安全的，为了保证任务能执行且仅执行一次。我们需要实现一个线程安全的队列。

这一步其实很简单，我们使用`std::mutex`和`std::unique_lock`配合，实现对常用的操作加锁即可。

```cpp
#ifndef _SAFEQUEUE_HPP
#define _SAFEQUEUE_HPP

#include <queue>
#include <mutex>

template<typename T>
class SageQueue {
public:
    SageQueue() = default;

    ~SageQueue() = default;

    size_t size() {
        std::unique_lock<std::mutex> lock(_mtx);
        return _q.size();
    }

    bool empty() {
        std::unique_lock<std::mutex> lock(_mtx);
        return _q.empty();
    }

    void push(const T &x) {
        std::unique_lock<std::mutex> lock(_mtx);
        _q.push(x);
    }

    void push(const T &&x) {
        std::unique_lock<std::mutex> lock(_mtx);
        _q.push(std::move(x));
    }

    void pop() {
        std::unique_lock<std::mutex> lock(_mtx);
        _q.pop();
    }

    T top() {
        std::unique_lock<std::mutex> lock(_mtx);
        return _q.front();
    }

private:
    std::queue<T> _q;
    std::mutex _mtx;
};

#endif _SAFEQUEUE_HPP

```

## 线程池所需变量

```cpp
using TaskType = std::function<void()>; // 任务类型
std::atomic<bool> _shutdown; // 线程池是否关闭
SageQueue<TaskType> _tasks; // 任务队列
std::vector<std::thread> _threads; // 线程队列
std::mutex _mtx;
std::condition_variable _tasks_empty; // 任务队列为空的条件变量，防止死锁
```

1. `TaskType`是什么？

    我们线程池可以接受各种任务，比如函数、仿函数、Lambda、bind表达式等，以及这些函数的形参可能各不相同。但是我们队列只能接受同一种类型的元素，因此我们对任务进行了二次封装，以确保可以接受各种任务。具体的封装过程，可以看提交函数部分。 

2. 为什么`_shutdown`使用原子操作？

    确保当线程池结束时，所有的线程可以及时的结束。

## 线程工作类

`std::thread`并不支持给线程分配一个任务，然后等待执行完成后再分配一个任务。我们如何实现线程复用？

我们的策略就是给线程分配的任务就是：“不断从任务队列获取任务并执行”，这样就实现线程复用。

我这里使用了内部类+仿函数的形式实现这一需求。
```cpp
class ThreadWorker {
    public:
        ThreadWorker(ThreadPool *_pool, const int _id) : _pool(_pool), _id(_id) {};

        // 重载函数调用运算符
        void operator()() {
            TaskType func; // 任务基类
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
```
1. 为什么要用内部类实现？ 

    因为这里需要访问线程池的私有成员变量。

2. 为什么`while`循环的条件是`not _pool->_shutdown or not _pool->_tasks.empty()`?

    因为如果只判断任务队列为空，可能我们还没有来得及添加任务，线程就会结束。如果只判断`_shutdown` 可能会导致线程池关闭时还没有执行的任务被丢弃。

3. 为什么等待的条件是`_pool->_tasks.empty() and not _pool->_shutdown`？

    因为等待的条件应该是**线程池没有结束**且**当前没有需要执行的任务**。

## 提交函数

```cpp
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
```

1. 为什么函数要这样定义`auto submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))>`

    为了可以接受任意形式的任务，我们这里采用了可变参数模板。其中`f`是任务，`args`是实参。关于返回类型，首先我们要知道，我们提交任务并不能立即执行，任务只有等待有空闲的线程才能执行。因此我们的任务都是异步执行的，所以我们需要用`std::future`来获得任务运行的结果，所以我们用`deltype(f(args...))`反推出任务返回值的类型。

2. 为什么要用`bind`表达式封装，而不是直接用`packaged_task`？

    `packaged_task`封装时不会直接获得实参，而是需要再执行时再传入实参。这里是为了方便后续的调用。

3. 为什么要用`shared_ptr`?

    基于RAII原则，保证内存会在任务结束后自动析构。

4. 为什么不直接把`shared_ptr`直接放入任务队列，而是需要用Lambda再次封装？

    经过了`bind`表达式和`packaged_task`封装后，任务的形参已经是`void`，但是返回值是`std::future<RT>`。而`RT`不同的任务可能是各不相同，因此直接放入到队列中依旧会产生类型不相同的情况。

    ## 线程池启动

    ```cpp
    	// 构造函数
        ThreadPool(const int _n_threads) : _threads(_n_threads), _shutdown(false) {
            for (int i = 0; i < _threads.size(); i++) {
                _threads[i] = std::thread(ThreadWorker(this, i));
            }
        };
    ```

    这里是在构造是完成了线程池的启动。

    ## 线程池关闭

    ```cpp
    	void shutdown() {
            _shutdown = true;
            _tasks_empty.notify_all(); // 唤醒所以因任务队列为空而等待的线程。
            for (size_t i = 0; i < _threads.size(); i++) {
                if (_threads[i].joinable()) _threads[i].join();
            }
        }
    ```

    唤醒所有的线程，并保证线程全部结束。为什么这里不把线程池的关闭在析构函数中实现。这里还是考虑到RAII原则，对象的析构应该在声明周期结束后自动完成析构。但是我们可能会需要获得任务的运行结果。因此这里通过显示的调用，由用户自己决定什么时候获取运行结果。

    ## 禁用默认函数

```cpp
ThreadPool(const ThreadPool &) = delete; // 禁用拷贝构造
ThreadPool(ThreadPool &&) = delete; // 禁用移动构造
ThreadPool &operator=(const ThreadPool &) = delete; // 禁用拷贝赋值
ThreadPool &operator=(ThreadPool &&) = delete; // 禁用移动赋值
```

禁用这些函数，主要是为了保证资源独占和状态安全。
