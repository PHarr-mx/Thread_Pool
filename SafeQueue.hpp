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
