/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <chrono>
#include <thread>
#include <utility>
#include <vector>
#include "boost/thread/exceptions.hpp"

namespace boost {

using thread = std::thread;

class thread_group {
public:
    thread_group() = default;
    thread_group(const thread_group &) = delete;
    thread_group &operator=(const thread_group &) = delete;

    template <typename Function>
    thread *create_thread(Function &&function)
    {
        threads_.push_back(std::thread(std::forward<Function>(function)));
        return &threads_.back();
    }

    void interrupt_all()
    {
    }

    void join_all()
    {
        for (auto &thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        threads_.clear();
    }

    size_t size() const
    {
        return threads_.size();
    }

private:
    std::vector<std::thread> threads_;
};

namespace this_thread {

template <typename Rep, typename Period>
void sleep_for(const std::chrono::duration<Rep, Period> &duration)
{
    std::this_thread::sleep_for(duration);
}

inline bool interruption_requested()
{
    return false;
}

} // namespace this_thread

} // namespace boost
