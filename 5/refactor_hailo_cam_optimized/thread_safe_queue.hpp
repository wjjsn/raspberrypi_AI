#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class thread_safe_queue {
    mutable std::mutex mutex_;
    std::condition_variable condition_variable_;

public:
    std::queue<T> queue_;
    thread_safe_queue() = default;
    void push(T item)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(item);
        }
        condition_variable_.notify_one();
    }
    T front() const
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_variable_.wait(lock, [this] { return !queue_.empty(); });
        return queue_.front();
    }
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    bool pop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        queue_.pop();
        return true;
    }
    bool try_pop(T &item)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = queue_.front();
        queue_.pop();
        return true;
    }
    void front_pop(T &item)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_variable_.wait(lock, [this] { return !queue_.empty(); });
        item = queue_.front();
        queue_.pop();
    }
};