#pragma once

#include <thread>
#include <mutex>

namespace ligero::thread_model {

struct single_thread {
    using lock_t = int;
    using lock_guard_t = int;
    const unsigned int available_threads = 1;
    
    constexpr inline auto& get_lock() noexcept { return mutex_; }
protected:
    int mutex_;
};

struct max_hardware_threads {
    using lock_t = std::mutex;
    using lock_guard_t = std::scoped_lock<std::mutex>;
    const unsigned int available_threads = std::thread::hardware_concurrency();
    
    inline auto& get_lock() noexcept { return mutex_; }
protected:
    std::mutex mutex_;
};

}  // namespace ligero::thread_model
