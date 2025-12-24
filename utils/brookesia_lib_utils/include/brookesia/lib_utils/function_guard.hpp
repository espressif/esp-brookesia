/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdio.h>
#include <tuple>
#include <utility>
#include <exception>
#include "boost/thread/thread.hpp"

namespace esp_brookesia::lib_utils {

template<typename T, typename... Args>
class FunctionGuard {
public:
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
