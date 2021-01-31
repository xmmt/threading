#pragma once

#include "function_holder.hpp"

#include <utility>
#include <thread>
#include <vector>
#include <deque>
#include <mutex>
#include <future>

namespace threading {
class stop_token;

class stop_callback_base {
public:
    virtual ~stop_callback_base() = default;

protected:
    friend class stop_state;
    virtual void invoke_callback_() const noexcept = 0;
    void add_callback_(stop_token const& token) const noexcept;
    void remove_callback_(stop_token const& token) const noexcept;
};

class stop_state final {
public:
    stop_state() noexcept = default;
    stop_state(stop_state const&) = delete;
    stop_state(stop_state&&) = delete;
    stop_state& operator=(stop_state const&) = delete;
    stop_state& operator=(stop_state&&) = delete;
    ~stop_state() = default;

    [[nodiscard]] bool stop_requested() const noexcept {
        return stop_requested_;
    }
    bool request_stop() {
        bool expected{ false };
        if (!stop_requested_.compare_exchange_strong(expected, true)) {
            return false;
        }
        std::lock_guard _{ m_ };
        for (auto& c : callbacks_) {
            c->invoke_callback_();
        }
        return true;
    }

private:
    friend class stop_callback_base;
    void add_callback_(stop_callback_base const* c) {
        if (stop_requested_) {
            c->invoke_callback_();
            return;
        }
        std::lock_guard _{ m_ };
        if (stop_requested_) {
            c->invoke_callback_();
            return;
        }
        callbacks_.emplace_back(c);
    }
    void remove_callback_(stop_callback_base const* c) {
        if (stop_requested_) {
            return;
        }
        std::lock_guard _{ m_ };
        if (stop_requested_) {
            return;
        }
        std::erase_if(callbacks_, [c](stop_callback_base const* i) { return i == c; });
    }

private:
    std::atomic_bool stop_requested_{ false };
    std::mutex m_;
    std::vector<stop_callback_base const*> callbacks_;
};

struct nostopstate_t {
    explicit nostopstate_t() = default;
};
inline constexpr nostopstate_t nostopstate{};

class stop_token final {
public:
    stop_token() noexcept = default;

    stop_token(stop_token const& other) noexcept
        : state_{ other.state_ } {
    }
    stop_token(stop_token&& other) noexcept
        : state_{ std::move(other.state_) } {
        other.state_.reset();
    }
    stop_token& operator=(stop_token const& other) noexcept {
        stop_token temp{ other };
        this->swap(temp);
        return *this;
    }
    stop_token& operator=(stop_token&& other) noexcept {
        state_ = std::move(other.state_);
        other.state_.reset();
        return *this;
    }
    ~stop_token() = default;
    void swap(stop_token& other) noexcept {
        state_.swap(other.state_);
    }

    [[nodiscard]] bool stop_requested() const noexcept {
        return state_ && state_->stop_requested();
    }
    [[nodiscard]] bool stop_possible() const noexcept {
        return state_.operator bool();
    }

    friend bool operator==(stop_token const& lhs, stop_token const& rhs) noexcept {
        return lhs.state_ == rhs.state_;
    }
    friend void swap(stop_token& lhs, stop_token& rhs) noexcept {
        lhs.swap(rhs);
    }

private:
    explicit stop_token(std::shared_ptr<stop_state> const& state) noexcept
        : state_{ state } {
    }
    friend class stop_source;
    friend class stop_callback_base;

private:
    std::shared_ptr<stop_state> state_;
};

inline void stop_callback_base::add_callback_(stop_token const& token) const noexcept {
    auto state = token.state_;
    if (state) {
        state->add_callback_(this);
    } else {
        try {
            invoke_callback_();
        } catch (...) {
            std::terminate();
        }
    }
}

inline void stop_callback_base::remove_callback_(stop_token const& token) const noexcept {
    auto state = token.state_;
    if (state) {
        state->remove_callback_(this);
    }
}

class stop_source final {
public:
    stop_source() noexcept
        : state_{ std::make_shared<stop_state>() } {
    }
    explicit stop_source(nostopstate_t) noexcept
        : state_{ nullptr } {
    }
    stop_source(stop_source const& other) noexcept
        : state_{ other.state_ } {
    }
    stop_source(stop_source&& other) noexcept
        : state_{ std::move(other.state_) } {
        other.state_.reset();
    }
    stop_source& operator=(stop_source const& other) noexcept {
        stop_source temp{ other };
        this->swap(temp);
        return *this;
    }
    stop_source& operator=(stop_source&& other) noexcept {
        state_ = std::move(other.state_);
        other.state_.reset();
        return *this;
    }
    ~stop_source() = default;

    void swap(stop_source& other) noexcept {
        state_.swap(other.state_);
    }

    [[nodiscard]] stop_token get_token() const noexcept {
        return state_ ? stop_token{ state_ } : stop_token{};
    }
    [[nodiscard]] bool stop_requested() const noexcept {
        return state_ && state_->stop_requested();
    }
    [[nodiscard]] bool stop_possible() const noexcept {
        return state_.operator bool();
    }
    bool request_stop() noexcept {
        return state_ && state_->request_stop();
    }

    friend bool operator==(stop_source const& lhs, stop_source const& rhs) noexcept {
        return lhs.state_ == rhs.state_;
    }
    friend void swap(stop_source& lhs, stop_source& rhs) noexcept {
        lhs.swap(rhs);
    }

private:
    std::shared_ptr<stop_state> state_;
};

template <class Callback>
class stop_callback : public stop_callback_base {
public:
    using callback_type = Callback;

    stop_callback() = delete;
    stop_callback(stop_callback const&) = delete;
    stop_callback(stop_callback&&) = delete;
    stop_callback& operator=(stop_callback const&) = delete;
    stop_callback& operator=(stop_callback&&) = delete;

    template <class C>
    explicit stop_callback(stop_token const& st, C&& cb) noexcept(std::is_nothrow_constructible_v<Callback, C>)
        : token_{ st }
        , callback_{ std::forward<C>(cb) } {
        add_callback_(token_);
    }
    template <class C>
    explicit stop_callback(stop_token&& st, C&& cb) noexcept(std::is_nothrow_constructible_v<Callback, C>)
        : token_{ st }
        , callback_{ std::forward<C>(cb) } {
        add_callback_(token_);
    }
    ~stop_callback() override {
        remove_callback_(token_);
    }

private:
    void invoke_callback_() const noexcept override {
        try {
            std::invoke(callback_);
        } catch (...) {
            std::terminate();
        }
    }

private:
    stop_token token_;
    Callback callback_;
};

template <class Callback>
stop_callback(stop_token, Callback) -> stop_callback<Callback>;

class jthread {
public:
    using id = std::thread::id;
    using native_handle_type = std::thread::native_handle_type;

    jthread() noexcept = default;
    jthread(jthread const&) = delete;
    jthread(jthread&& other) noexcept
        : stop_source_{ std::move(other.stop_source_) }
        , thread_{ std::move(other.thread_) } {
        other.stop_source_ = stop_source{ nostopstate };
    }
    template <class Function, class... Args>
    explicit jthread(Function&& f, Args&&... args) {
        if constexpr (std::is_invocable_v<Function, stop_token, Args...>) {
            stop_source_ = stop_source{};
            thread_ = std::thread{ std::forward<Function>(f), stop_source_.get_token(), std::forward<Args>(args)... };
        } else {
            thread_ = std::thread{ std::forward<Function>(f), std::forward<Args>(args)... };
        }
    }
    jthread& operator=(jthread const& other) = delete;
    jthread& operator=(jthread&& other) noexcept {
        stop_source_ = std::move(other.stop_source_);
        thread_ = std::move(other.thread_);
        other.stop_source_ = stop_source{ nostopstate };
        return *this;
    }
    ~jthread() {
        if (joinable()) {
            request_stop();
            join();
        }
    }

    [[nodiscard]] bool joinable() const noexcept {
        return thread_.joinable();
    }
    [[nodiscard]] jthread::id get_id() const noexcept {
        return thread_.get_id();
    }
    [[nodiscard]] native_handle_type native_handle() {
        return thread_.native_handle();
    }
    [[nodiscard]] static unsigned int hardware_concurrency() noexcept {
        return std::thread::hardware_concurrency();
    }

    void join() {
        thread_.join();
    }
    void detach() {
        thread_.detach();
    }

    void swap(jthread& other) noexcept {
        std::swap(stop_source_, other.stop_source_);
        std::swap(thread_, other.thread_);
    }

    stop_source get_stop_source() const noexcept {
        return stop_source_;
    }
    stop_token get_stop_token() const noexcept {
        return stop_source_.get_token();
    }
    bool request_stop() noexcept {
        return stop_source_.request_stop();
    }

    friend void swap(jthread& lhs, jthread& rhs) noexcept {
        lhs.swap(rhs);
    }

private:
    stop_source stop_source_{ nostopstate };
    std::thread thread_;
};

template <class Lock, class Predicate>
inline bool condition_variable_wait_stop(std::condition_variable& cv, Lock& lock, threading::stop_token stoken, Predicate pred) {
    threading::stop_callback scb(stoken, [&cv] { cv.notify_all(); });
    while (!stoken.stop_requested()) {
        if (pred())
            return true;
        cv.wait(lock);
    }
    return pred();
}

template <class Lock, class Predicate>
inline bool condition_variable_any_wait_stop(std::condition_variable_any& cv, Lock& lock, threading::stop_token stoken, Predicate pred) {
    threading::stop_callback scb(stoken, [&cv] { cv.notify_all(); });
    while (!stoken.stop_requested()) {
        if (pred())
            return true;
        cv.wait(lock);
    }
    return pred();
}

class threadpool final {
public:
    threadpool() = delete;
    explicit threadpool(size_t const threads_count)
        : threads_count_{ threads_count }
        , threads_{ threads_count } {
        for (auto& thr : threads_) {
            thr = jthread{ [this](stop_token stoken) {
                while (!stoken.stop_requested()) {
                    std::unique_lock lck{ m_ };
                    condition_variable_wait_stop(cv_, lck, stoken, [this] { return !q_.empty() || no_more_tasks_; });
                    if (stoken.stop_requested()) {
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
        cv_.notify_all();
        for (auto& thr : threads_) {
            thr.get_stop_source().request_stop();
        }
    }

private:
    size_t const threads_count_;
    std::vector<jthread> threads_;
    std::deque<utils::function<void()>> q_;
    std::mutex m_;
    std::condition_variable cv_;
    std::atomic_bool no_more_tasks_{ false };
};

} // namespace threading