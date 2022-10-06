
#include "thread_pool.h"

template <typename T>
ThreadPool<T>::ThreadPool(int num, int max) :
    stop(false) {
    if (max < num) {
        max = MAX_THREADS;
    }

    if (num <= 0 || num > max) {
        throw std::exception();
    }

    for (int i = 0; i < num; i++) {
        work_threads.emplace_back(worker, this);
    }
}

template <typename T>
inline ThreadPool<T>::~ThreadPool() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    stop = true;

    condition.notify_all();
    for (auto &w : work_threads) {
        w.join();
    }
}

template <typename T>
bool ThreadPool<T>::submit(T *task) {
    queue_mutex.lock();
    tasks_queue.push(task);
    queue_mutex.unlock();
    condition.notify_one();
    return true;
}

template <typename T>
void *ThreadPool<T>::worker(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void ThreadPool<T>::run() {
    while (!stop) {
        std::unique_lock<std::mutex> lk(this->queue_mutex);
        this->condition.wait(lk, [this] {
            return !this->tasks_queue.empty();
        });
        if (this->tasks_queue.empty()) {
            continue;
        }
        T *request = tasks_queue.front();
        tasks_queue.pop();
        if (request) {
            request->process();
        }
    }
}
