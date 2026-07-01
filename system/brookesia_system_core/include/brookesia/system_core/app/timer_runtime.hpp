/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "brookesia/system_core/app/types.hpp"

namespace esp_brookesia::system::core {

class System;

class AppTimerRuntime {
public:
    AppTimerRuntime() = default;
    AppTimerRuntime(System &system, AppId app_id);

    std::expected<TimerId, std::string> start_periodic(std::string_view name, int interval_ms) const;
    std::expected<TimerId, std::string> start_delayed(std::string_view name, int delay_ms) const;
    bool stop(TimerId timer_id) const;

private:
    System *system_ = nullptr;
    AppId app_id_ = INVALID_APP_ID;
};

} // namespace esp_brookesia::system::core
