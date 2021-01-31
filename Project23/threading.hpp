#pragma once

#include "function_holder.hpp"

#include <utility>
#include <thread>
#include <vector>
#include <deque>
#include <mutex>
#include <future>

namespace threading {

class threadpool final {
public:
    threadpool() = delete;
    explicit threadpool(size_t const threads_count)
        : threads_count_{ threads_count }
        , threads_{ threads_count } {
        for (auto& thr : threads_) {
            thr = std::thread{ [this] {
                while (!stop_requested_) {
                    std::unique_lock lck{ m_ };
                    cv_.wait(lck, [this] { return !q_.empty() || no_more_tasks_ || stop_requested_; });
                    if (stop_requested_) {
                        break;
                    }
                    if (q_.empty() && no_more_tasks_) {
                        break;
                    }
                    auto f = std::move(q_.front());
                    q_.pop_front();
                    lck.unlock();
                    try {
                        std::invoke(f);
                    } catch (...) {
#if _DEBUG
                        throw;
#endif
                    }
                }
            } };
        }
    }
    threadpool(threadpool const&) = delete;
    threadpool(threadpool&&) = delete;
    threadpool& operator=(threadpool const&) = delete;
    threadpool& operator=(threadpool&&) = delete;
    ~threadpool() {
        join();
    }

    void submit(utils::function<void()>&& f) {
        if (no_more_tasks_) {
            return;
        }
        std::lock_guard _{ m_ };
        q_.emplace_back(std::move(f));
        cv_.notify_one();
    }
    void join() {
        no_more_tasks_ = true;
        cv_.notify_all();
        for (auto& thr : threads_) {
            if (thr.joinable()) {
                thr.join();
            }
        }
    }
    void no_more_tasks() {
        no_more_tasks_ = true;
        cv_.notify_all();
    }
    void stop() {
        no_more_tasks_ = true;
        stop_requested_ = true;
        cv_.notify_all();
    }

private:
    size_t const threads_count_;
    std::vector<std::thread> threads_;
    std::deque<utils::function<void()>> q_;
    std::mutex m_;
    std::condition_variable cv_;
    std::atomic_bool no_more_tasks_{ false };
    std::atomic_bool stop_requested_{ false };
};

} // namespace threading