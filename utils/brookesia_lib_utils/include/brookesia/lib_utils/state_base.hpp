/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <cstdint>
#include "brookesia/lib_utils/macro_configs.h"

namespace esp_brookesia::lib_utils {

/**
 * @brief Base class for state machine states
 *
 * Provides lifecycle hooks (on_enter, on_exit, on_update) and configuration
 * for timeout and periodic update intervals.
 */
class StateBase {
public:
    /**
     * @brief Constructor.
     *
     * @param name State name.
     */
    explicit StateBase(const std::string &name)
        : name_(name)
    {}
    /**
     * @brief Virtual destructor.
     */
    virtual ~StateBase() = default;

    /**
     * @brief Hook invoked before the state becomes active.
     *
     * @param from_state Name of the previous state, or an empty string for the initial state.
     * @param action Transition action name, or an empty string when not specified.
     * @return `true` to allow the transition, or `false` to reject entry.
     */
    virtual bool on_enter(const std::string &from_state = "", const std::string &action = "")
    {
        (void)from_state;
        (void)action;
        return true;
    }

    /**
     * @brief Hook invoked before the state is left.
     *
     * @param to_state Name of the next state, or an empty string when unspecified.
     * @param action Transition action name, or an empty string when not specified.
     * @return `true` to allow the transition, or `false` to reject exit.
     */
    virtual bool on_exit(const std::string &to_state = "", const std::string &action = "")
    {
        (void)to_state;
        (void)action;
        return true;
    }

    /**
     * @brief Hook invoked periodically while the state remains active.
     *
     * @note This callback is scheduled only when `set_update_interval()` configures a non-zero interval.
     */
    virtual void on_update() {}

    /**
     * @brief Configure an automatic timeout transition for this state.
     *
     * @param ms Timeout duration in milliseconds. A value of `0` disables the timeout.
     * @param action Action name triggered when the timeout expires.
     */
    void set_timeout(uint32_t ms, const std::string &action)
    {
        timeout_ms_ = ms;
        timeout_action_ = action;
    }

    /**
     * @brief Configure the periodic update interval for this state.
     *
     * @param interval_ms Update period in milliseconds. A value of `0` disables `on_update()` scheduling.
     */
    void set_update_interval(uint32_t interval_ms)
    {
        update_interval_ms_ = interval_ms;
    }

    /**
     * @brief Get the state name.
     *
     * @return State name.
     */
    const std::string &get_name() const
    {
        return name_;
    }

    /**
     * @brief Get the configured timeout duration.
     *
     * @return Timeout duration in milliseconds, or `0` when no timeout is configured.
     */
    uint32_t get_timeout_ms() const
    {
        return timeout_ms_;
    }

    /**
     * @brief Get the action triggered when the timeout expires.
     *
     * @return Configured timeout action name.
     */
    const std::string &get_timeout_action() const
    {
        return timeout_action_;
    }

    /**
     * @brief Get the configured periodic update interval.
     *
     * @return Update interval in milliseconds, or `0` when periodic updates are disabled.
     */
    uint32_t get_update_interval() const
    {
        return update_interval_ms_;
    }

private:
    std::string name_;
    uint32_t timeout_ms_ = 0;
    std::string timeout_action_;
    uint32_t update_interval_ms_ = 0;
};

} // namespace esp_brookesia::lib_utils
