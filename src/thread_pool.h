#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <stdexcept>
#include <condition_variable>
#include <memory>

const int MAX_THREADS = 200;

template <typename T>
class ThreadPool {
public:
    /**
     * @brief Create a new thread pool
     *
     * @param num default thread num
     * @param max max of thread in thread pool
     */
    ThreadPool(int num=1, int max = MAX_THREADS);

    /**
     * @brief submit a new task to thread pool
     *
     * @param t
     * @return true
     * @return false
     */
    bool submit(T *task);

    /**
     * @brief Destroy the Thread Pool object
     *
     */
    ~ThreadPool();

private:
    /**
     * @brief work thread
     *
     */
    std::vector<std::thread> work_threads;
    /**
     * @brief task queue
     *
     */
    std::queue<T *> tasks_queue;
    /**
     * @brief task lock
     *
     */
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;

private:
    /**
     * @brief worker task
     *
     * @param arg
     * @return void*
     */
    static void *worker(void *arg);
    /**
     * @brief run the threads
     *
     * @param arg th args of task
     * @return void*
     */
    void run();
};

#endif
