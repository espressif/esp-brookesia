/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/service/utils.hpp"

#include <array>
#include <utility>

namespace esp_brookesia::system::core {

std::span<const service::FunctionSchema> SystemTimerHelper::get_function_schemas()
{
    static const std::array<service::FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> SCHEMAS = {{
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::StartPeriodic),
                .description = "Start an app-owned periodic timer",
                .parameters = {
                    {
                        BROOKESIA_DESCRIBE_TO_STR(StartPeriodicParam::Name),
                        "Timer name",
                        FunctionValueType::String,
                    },
                    {
                        BROOKESIA_DESCRIBE_TO_STR(StartPeriodicParam::IntervalMs),
                        "Timer interval in milliseconds",
                        FunctionValueType::Number,
                    },
                },
                .require_scheduler = false,
                .return_value = service::FunctionReturnSchema{
                    .type = FunctionValueType::Number,
                    .description = "Identifier of the started timer",
                },
            },
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::StartDelayed),
                .description = "Start an app-owned delayed timer",
                .parameters = {
                    {
                        BROOKESIA_DESCRIBE_TO_STR(StartDelayedParam::Name),
                        "Timer name",
                        FunctionValueType::String,
                    },
                    {
                        BROOKESIA_DESCRIBE_TO_STR(StartDelayedParam::DelayMs),
                        "Timer delay in milliseconds",
                        FunctionValueType::Number,
                    },
                },
                .require_scheduler = false,
                .return_value = service::FunctionReturnSchema{
                    .type = FunctionValueType::Number,
                    .description = "Identifier of the started timer",
                },
            },
            make_timer_id_schema(FunctionId::Stop, "Stop an app-owned timer"),
        }
    };
    return std::span<const service::FunctionSchema>(SCHEMAS);
}

std::span<const service::EventSchema> SystemTimerHelper::get_event_schemas()
{
    static const std::array<service::EventSchema, 0> SCHEMAS = {};
    return std::span<const service::EventSchema>(SCHEMAS);
}

TimerService::TimerService(System &system)
    : ServiceBase({
    .name = SystemTimerHelper::get_name().data(),
})
, system_(system)
{}

std::vector<service::FunctionSchema> TimerService::get_function_schemas()
{
    auto schemas = SystemTimerHelper::get_function_schemas();
    return std::vector<service::FunctionSchema>(schemas.begin(), schemas.end());
}

std::vector<service::EventSchema> TimerService::get_event_schemas()
{
    auto schemas = SystemTimerHelper::get_event_schemas();
    return std::vector<service::EventSchema>(schemas.begin(), schemas.end());
}

service::ServiceBase::FunctionHandlerMap TimerService::get_function_handlers()
{
    using FunctionId = SystemTimerHelper::FunctionId;
    return {
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StartPeriodic), [this](FunctionParameterMap &&params)
            {
                return start_periodic(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StartDelayed), [this](FunctionParameterMap &&params)
            {
                return start_delayed(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::Stop), [this](FunctionParameterMap &&params)
            {
                return stop(std::move(params));
            }
        },
    };
}

FunctionResult TimerService::start_periodic(FunctionParameterMap &&params)
{
    auto app_id = system_.get_current_runtime_app_owner();
    if (!app_id) {
        return make_error(app_id.error());
    }
    auto name = get_string_param(params, BROOKESIA_DESCRIBE_TO_STR(SystemTimerHelper::StartPeriodicParam::Name));
    auto interval_ms = get_number_param(
                           params,
                           BROOKESIA_DESCRIBE_TO_STR(SystemTimerHelper::StartPeriodicParam::IntervalMs)
                       );
    if (!name || !interval_ms) {
        return make_error(!name ? name.error() : interval_ms.error());
    }
    auto timer_id = system_.timer_start_periodic(*app_id, *name, static_cast<int>(*interval_ms));
    if (!timer_id) {
        return make_error(timer_id.error());
    }
    return make_success(static_cast<double>(*timer_id));
}

FunctionResult TimerService::start_delayed(FunctionParameterMap &&params)
{
    auto app_id = system_.get_current_runtime_app_owner();
    if (!app_id) {
        return make_error(app_id.error());
    }
    auto name = get_string_param(params, BROOKESIA_DESCRIBE_TO_STR(SystemTimerHelper::StartDelayedParam::Name));
    auto delay = get_number_param(params, BROOKESIA_DESCRIBE_TO_STR(SystemTimerHelper::StartDelayedParam::DelayMs));
    if (!name || !delay) {
        return make_error(!name ? name.error() : delay.error());
    }
    auto timer_id = system_.timer_start_delayed(*app_id, *name, static_cast<int>(*delay));
    if (!timer_id) {
        return make_error(timer_id.error());
    }
    return make_success(static_cast<double>(*timer_id));
}

FunctionResult TimerService::stop(FunctionParameterMap &&params)
{
    auto app_id = system_.get_current_runtime_app_owner();
    if (!app_id) {
        return make_error(app_id.error());
    }
    auto timer_id = get_number_param(params, BROOKESIA_DESCRIBE_TO_STR(SystemTimerHelper::TimerParam::TimerId));
    if (!timer_id) {
        return make_error(timer_id.error());
    }
    return system_.timer_stop(*app_id, static_cast<TimerId>(*timer_id)) ?
           make_success() : make_error("Failed to stop timer");
}

} // namespace esp_brookesia::system::core
