#ifndef _TEST_HPP
#define _TEST_HPP

#include <iostream>
#include <random>
#include <chrono>
#include "ThreadPool.hpp"

namespace TEST1 {
    using std::cout;
    using std::endl;

    std::mt19937 mt{std::random_device()()};

    std::uniform_int_distribution<int> dist(100, 200);

    auto rnd = std::bind(dist, mt);

    void simulate_hard_computation() { // 复杂计算模拟
        // 随机睡眠 [1000,2000] 毫秒
        std::this_thread::sleep_for(std::chrono::milliseconds(rnd()));
    }

    // 计算两数之和并输出
    void addition(int a, int b) {
        simulate_hard_computation();
        cout << a << " + " << b << " = " << a + b << endl;
    }

    // 计算两数之和并存储到 ans
    void addition_store(int a, int b, int &ans) {
        simulate_hard_computation();
        ans = a + b;
    }

    // 计算两数之和并返回
    int addition_return(int a, int b) {
        simulate_hard_computation();
        return a + b;
    }

    void multiply(int a, int b) {
        simulate_hard_computation();
        cout << a << " * " << b << " " << a * b << endl;
    }


    void test() {
        ThreadPool pool(5); // 创建五个线程的线程池

        for (int a = 0; a < 5; a++)
            for (int b = 0; b < 5; b++)
                pool.submit(addition, a, b);

        int a1 = rnd(), b1 = rnd(), c1;
        auto f1 = pool.submit(addition_store, a1, b1, c1);

        int a2 = rnd(), b2 = rnd();
        auto c2 = pool.submit(addition_return, a2, b2);

        for (int a = 0; a < 5; a++)
            for (int b = 0; b < 5; b++)
                pool.submit(multiply, a, b);

        f1.get();
        cout << a1 << " + " << b1 << " = " << c1 << endl;
        cout << a2 << " + " << b2 << " = " << c2.get() << endl;

        pool.shutdown();
    }
}

#endif _TEST_HPP
