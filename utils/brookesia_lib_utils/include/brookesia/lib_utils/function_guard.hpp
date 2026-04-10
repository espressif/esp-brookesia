/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdio.h>
#include <tuple>
#include <utility>
#include <exception>
#include "boost/thread/thread.hpp"
#include "brookesia/lib_utils/macro_configs.h"

namespace esp_brookesia::lib_utils {

/**
 * @brief RAII helper that invokes a callable on destruction unless released.
 *
 * @tparam T Callable type stored by the guard.
 * @tparam Args Argument types forwarded to the callable when the guard is destroyed.
 *
 * @note The destructor suppresses `boost::thread_interrupted` and prints other
 *       `std::exception` failures to `stdout`, so cleanup code should still be kept lightweight.
 */
template<typename T, typename... Args>
class FunctionGuard {
public:
    /**
     * @brief Construct a guard for a deferred callable invocation.
     *
     * @param func Callable executed in the destructor unless `release()` is called.
     * @param args Arguments stored and forwarded to @p func during destruction.
     */
    FunctionGuard(T func, Args &&... args)
        : func_(std::move(func))
        , args_(std::forward<Args>(args)...)
    {}

    // Disable copy constructor and copy assignment (RAII object should not be copied)
    FunctionGuard(const FunctionGuard &) = delete;
    FunctionGuard &operator=(const FunctionGuard &) = delete;

    // Move constructor
    FunctionGuard(FunctionGuard &&other)
        : func_(std::move(other.func_))
        , args_(std::move(other.args_))
        , is_release_(other.is_release_)
    {
        // After moving, the original object should not execute cleanup function
        other.is_release_ = true;
    }

    // Move assignment operator
    FunctionGuard &operator=(FunctionGuard &&other)
    {
        if (this != &other) {
            // Move resources
            func_ = std::move(other.func_);
            args_ = std::move(other.args_);
            is_release_ = other.is_release_;

            // After moving, the original object should not execute cleanup function
            other.is_release_ = true;
        }
        return *this;
    }

    ~FunctionGuard() noexcept
    {
        if (!is_release_) {
            try {
                std::apply([this](auto &&... args) {
                    func_(std::forward<decltype(args)>(args)...);
                }, args_);
            } catch (const boost::thread_interrupted &e) {
                // Ignore
            } catch (const std::exception &e) {
                printf("Exception in destructor: %s\n", e.what());
            }
        }
    }

    /**
     * @brief Disable the deferred invocation.
     *
     * After calling this method, destroying the guard no longer executes the stored callable.
     */
    void release()
    {
        is_release_ = true;
    }

private:
    T func_;
    std::tuple<Args...> args_;
    bool is_release_ = false;
};

} // namespace esp_brookesia::lib_utils
