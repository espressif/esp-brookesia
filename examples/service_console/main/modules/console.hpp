/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <cstddef>

class Console {
public:
    struct Config {
        size_t task_stack_size = 7 * 1024;
        size_t task_priority = 20;
        size_t max_cmdline_length = 1024;
    };

    static Console &get_instance()
    {
        static Console instance;
        return instance;
    }

    /**
     * @brief Start console with default configuration
     * @return true if started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Start console with the given configuration
     * @param config Configuration for the console
     * @return true if started successfully, false otherwise
     */
    bool start(const Config &config);

    /**
     * @brief Check if console is started
     * @return true if console is running, false otherwise
     */
    bool is_running() const
    {
        return is_running_.load();
    }

private:
    Console() = default;
    ~Console() = default;
    Console(const Console &) = delete;
    Console &operator=(const Console &) = delete;

    void print_banner();
    void register_commands();

    std::atomic<bool> is_running_{false};
    Config config_{};
};
