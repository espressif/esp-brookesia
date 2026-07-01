/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/system_core/service/gui.hpp"
#include "brookesia/system_core/service/system.hpp"
#include "brookesia/system_core/service/timer.hpp"
#include "brookesia/system_core/system/system.hpp"

namespace esp_brookesia::system::core {

using service::FunctionParameterMap;
using service::FunctionResult;
using service::FunctionSchema;
using service::FunctionValue;
using service::FunctionValueType;

FunctionResult make_error(std::string error);
FunctionResult make_success();

template <typename T>
FunctionResult make_success(T value)
{
    return FunctionResult{
        .success = true,
        .data = FunctionValue(std::move(value)),
    };
}

std::expected<std::string, std::string> get_string_param(const FunctionParameterMap &params, std::string_view name);
std::expected<double, std::string> get_number_param(const FunctionParameterMap &params, std::string_view name);
std::expected<bool, std::string> get_bool_param(const FunctionParameterMap &params, std::string_view name);
std::expected<boost::json::object, std::string> get_object_param(
    const FunctionParameterMap &params,
    std::string_view name
);
std::expected<boost::json::array, std::string> get_array_param(
    const FunctionParameterMap &params,
    std::string_view name
);
std::expected<std::string, std::string> get_object_string(
    const boost::json::object &object,
    std::string_view name,
    std::string default_value = {}
);
std::expected<int32_t, std::string> get_object_int(
    const boost::json::object &object,
    std::string_view name,
    int32_t default_value = 0
);
std::expected<bool, std::string> get_object_bool(
    const boost::json::object &object,
    std::string_view name,
    bool default_value = false
);
std::expected<gui::Animation, std::string> animation_from_json(const boost::json::object &object);
std::expected<std::string, std::string> get_flexible_object_string(
    const boost::json::object &object,
    std::string_view primary,
    std::string_view fallback
);
std::expected<boost::json::object, std::string> get_flexible_object_object(
    const boost::json::object &object,
    std::string_view primary,
    std::string_view fallback,
    boost::json::object default_value = {}
);
std::expected<std::vector<gui::BindingValueUpdate>, std::string> parse_binding_updates(
    const boost::json::array &array
);
std::expected<std::vector<GuiBatchCommand>, std::string> parse_gui_batch_commands(
    const boost::json::array &array
);
boost::json::object gui_batch_result_to_json(const GuiBatchResult &batch_result);
AppId to_app_id(double value);
bool is_native_system_app(const AppInfo &app);
std::expected<std::optional<AppInfo>, std::string> get_runtime_caller(System &system);
boost::json::object app_info_to_json(const AppInfo &info, std::string_view language);
FunctionSchema make_no_param_schema(SystemCoreHelper::FunctionId id, std::string description);
FunctionSchema make_app_id_schema(SystemCoreHelper::FunctionId id, std::string description);
FunctionSchema make_timer_id_schema(SystemTimerHelper::FunctionId id, std::string description);

} // namespace esp_brookesia::system::core
