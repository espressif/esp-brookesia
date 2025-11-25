/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <cstdint>

namespace esp_brookesia::lib_utils {

/**
 * @brief Base class for state machine states
 *
 * Provides lifecycle hooks (on_enter, on_exit, on_update) and configuration
 * for timeout and periodic update intervals.
 */
class StateBase {
public:
    StateBase() = default;
    virtual ~StateBase() = default;

    /**
     * @brief Called when entering this state
     *
     * @param from_state Name of the previous state (empty for initial state)
     * @return true if entry is allowed, false to deny entry
     */
    virtual bool on_enter(const std::string &from_state = "")
    {
        (void)from_state;
        return true;
    }

    /**
     * @brief Called when exiting this state
     *
     * @param to_state Name of the next state
     * @return true if exit is allowed, false to deny exit
     */
    virtual bool on_exit(const std::string &to_state = "")
    {
        (void)to_state;
        return true;
    }

    /**
     * @brief Called periodically if update interval is set
     */
    virtual void on_update() {}

    /**
     * @brief Set timeout for this state
     *
     * @param ms Timeout in milliseconds
     * @param event Event to trigger when timeout occurs
     */
    void set_timeout(uint32_t ms, const std::string &event)
    {
        timeout_ms_ = ms;
        timeout_event_ = event;
    }

    /**
     * @brief Set periodic update interval for this state
     *
     * @param interval_ms Update interval in milliseconds
     */
    void set_update_interval(uint32_t interval_ms)
    {
        update_interval_ms_ = interval_ms;
    }

    uint32_t get_timeout_ms() const
    {
        return timeout_ms_;
    }

    const std::string &get_timeout_event() const
    {
        return timeout_event_;
    }

    uint32_t get_update_interval() const
    {
        return update_interval_ms_;
    }

private:
    uint32_t timeout_ms_ = 0;
    std::string timeout_event_;
    uint32_t update_interval_ms_ = 0;
};

} // namespace esp_brookesia::lib_utils
