/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>

#include "boost/json.hpp"
#include "brookesia/service_manager.hpp"

namespace esp_brookesia::system::core {

inline boost::json::value function_value_to_json(const service::FunctionValue &value)
{
    return std::visit(
    [](const auto & item) -> boost::json::value {
        using ItemType = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<ItemType, service::RawBuffer>)
        {
            return boost::json::object{
                {"data_ptr", reinterpret_cast<uintptr_t>(item.data_ptr)},
                {"data_size", item.data_size},
                {"is_const", item.is_const},
            };
        } else
        {
            return boost::json::value(item);
        }
    },
    value
           );
}

inline boost::json::value event_item_to_json(const service::EventItem &item)
{
    return std::visit(
    [](const auto & value) -> boost::json::value {
        using ItemType = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<ItemType, service::RawBuffer>)
        {
            return boost::json::object{
                {"data_ptr", reinterpret_cast<uintptr_t>(value.data_ptr)},
                {"data_size", value.data_size},
                {"is_const", value.is_const},
            };
        } else
        {
            return boost::json::value(value);
        }
    },
    item
           );
}

inline std::string function_result_to_json(const service::FunctionResult &result)
{
    boost::json::object root;
    root["success"] = result.success;
    if (!result.success) {
        root["error"] = result.error_message;
    }
    if (result.data) {
        root["data"] = function_value_to_json(result.data.value());
    }
    return boost::json::serialize(root);
}

inline boost::json::object function_call_result_to_json(const service::FunctionCallResult &result)
{
    boost::json::object object;
    object["name"] = result.name;
    object["success"] = result.success;
    if (!result.success) {
        object["error"] = result.error_message;
    }
    if (result.data) {
        object["data"] = function_value_to_json(result.data.value());
    }
    return object;
}

inline std::string function_batch_result_to_json(const service::FunctionBatchResult &result)
{
    boost::json::array results;
    for (const auto &item : result.results) {
        results.push_back(function_call_result_to_json(item));
    }
    boost::json::object root;
    root["success"] = result.success;
    root["results"] = std::move(results);
    return boost::json::serialize(root);
}

inline std::string event_items_to_json(const service::EventItemMap &items)
{
    boost::json::object root;
    for (const auto &[name, item] : items) {
        root[name] = event_item_to_json(item);
    }
    return boost::json::serialize(root);
}

inline std::expected<service::FunctionValue, std::string> function_value_from_json(const boost::json::value &value)
{
    if (value.is_bool()) {
        return service::FunctionValue(value.as_bool());
    }
    if (value.is_int64()) {
        return service::FunctionValue(static_cast<double>(value.as_int64()));
    }
    if (value.is_uint64()) {
        return service::FunctionValue(static_cast<double>(value.as_uint64()));
    }
    if (value.is_double()) {
        return service::FunctionValue(value.as_double());
    }
    if (value.is_string()) {
        return service::FunctionValue(std::string(value.as_string()));
    }
    if (value.is_object()) {
        return service::FunctionValue(value.as_object());
    }
    if (value.is_array()) {
        return service::FunctionValue(value.as_array());
    }
    return std::unexpected("Function parameters do not support null values");
}

inline std::expected<service::FunctionParameterMap, std::string> parse_function_params(std::string_view params_json)
{
    service::FunctionParameterMap params;
    if (params_json.empty()) {
        return params;
    }

    boost::system::error_code error_code;
    auto parsed = boost::json::parse(params_json, error_code);
    if (error_code || !parsed.is_object()) {
        return std::unexpected("Function parameters must be a JSON object");
    }
    for (const auto &[key, value] : parsed.as_object()) {
        auto function_value = function_value_from_json(value);
        if (!function_value) {
            return std::unexpected("Invalid function parameter '" + std::string(key) + "': " + function_value.error());
        }
        params.emplace(std::string(key), std::move(*function_value));
    }
    return params;
}

inline std::expected<service::FunctionParameterMap, std::string> parse_function_params_object(
    const boost::json::object &object
)
{
    service::FunctionParameterMap params;
    for (const auto &[key, value] : object) {
        auto function_value = function_value_from_json(value);
        if (!function_value) {
            return std::unexpected("Invalid function parameter '" + std::string(key) + "': " + function_value.error());
        }
        params.emplace(std::string(key), std::move(*function_value));
    }
    return params;
}

inline std::expected<std::vector<service::FunctionCall>, std::string> parse_function_calls(std::string_view calls_json)
{
    boost::system::error_code error_code;
    auto parsed = boost::json::parse(calls_json, error_code);
    if (error_code || !parsed.is_array()) {
        return std::unexpected("Function calls must be a JSON array");
    }

    std::vector<service::FunctionCall> calls;
    for (const auto &value : parsed.as_array()) {
        if (!value.is_object()) {
            return std::unexpected("Each function call must be an object");
        }
        const auto &object = value.as_object();
        auto name_it = object.find("name");
        if (name_it == object.end()) {
            name_it = object.find("Name");
        }
        if (name_it == object.end() || !name_it->value().is_string()) {
            return std::unexpected("Function call requires string field 'name'");
        }
        boost::json::object params_object;
        auto params_it = object.find("params");
        if (params_it == object.end()) {
            params_it = object.find("Params");
        }
        if (params_it != object.end()) {
            if (!params_it->value().is_object()) {
                return std::unexpected("Function call field 'params' must be an object");
            }
            params_object = params_it->value().as_object();
        }
        auto params = parse_function_params_object(params_object);
        if (!params) {
            return std::unexpected(params.error());
        }
        calls.push_back({
            .name = std::string(name_it->value().as_string()),
            .parameters = std::move(*params),
        });
    }
    return calls;
}

} // namespace esp_brookesia::system::core
