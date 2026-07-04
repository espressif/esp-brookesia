/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "brookesia/service_manager.hpp"
#include "brookesia/system_core/app/types.hpp"

namespace esp_brookesia::system::core {

class System;

class SystemTimerHelper {
public:
    enum class FunctionId : uint8_t {
        StartPeriodic,
        StartDelayed,
        Stop,
        Max,
    };
    enum class TimerParam : uint8_t {
        TimerId,
    };
    enum class StartPeriodicParam : uint8_t {
        Name,
        IntervalMs,
    };
    enum class StartDelayedParam : uint8_t {
        Name,
        DelayMs,
    };

    static constexpr std::string_view get_name()
    {
        return BROOKESIA_SYSTEM_CORE_TIMER_SERVICE_NAME;
    }
    static std::span<const service::FunctionSchema> get_function_schemas();
    static std::span<const service::EventSchema> get_event_schemas();
};
BROOKESIA_DESCRIBE_ENUM(SystemTimerHelper::FunctionId, StartPeriodic, StartDelayed, Stop, Max)
BROOKESIA_DESCRIBE_ENUM(SystemTimerHelper::TimerParam, TimerId)
BROOKESIA_DESCRIBE_ENUM(SystemTimerHelper::StartPeriodicParam, Name, IntervalMs)
BROOKESIA_DESCRIBE_ENUM(SystemTimerHelper::StartDelayedParam, Name, DelayMs)

class TimerService final: public service::ServiceBase {
public:
    explicit TimerService(System &system);

private:
    std::vector<service::FunctionSchema> get_function_schemas() override;
    std::vector<service::EventSchema> get_event_schemas() override;
    FunctionHandlerMap get_function_handlers() override;

    service::FunctionResult start_periodic(service::FunctionParameterMap &&params);
    service::FunctionResult start_delayed(service::FunctionParameterMap &&params);
    service::FunctionResult stop(service::FunctionParameterMap &&params);

    System &system_;
};

} // namespace esp_brookesia::system::core
