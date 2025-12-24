/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_FUNCTION_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/function/registry.hpp"

namespace esp_brookesia::service {

bool FunctionRegistry::add(FunctionSchema &&func_schema, FunctionHandler &&func_handler)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: func_schema(%1%), func_handler(%2%)", BROOKESIA_DESCRIBE_TO_STR(func_schema),
        BROOKESIA_DESCRIBE_TO_STR(func_handler)
    );

    BROOKESIA_CHECK_FALSE_RETURN(!func_schema.name.empty(), false, "Function name is empty");
    BROOKESIA_CHECK_FALSE_RETURN(func_handler != nullptr, false, "Function handler is null");

    boost::lock_guard lock(functions_mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(
        functions_.find(func_schema.name) == functions_.end(), false,
        "Function `%1%` already registered", func_schema.name
    );

    // Copy the function name because func_schema.name will be moved afterwards
    auto func_name = func_schema.name;
    functions_[func_name] = std::make_pair(std::move(func_schema), std::move(func_handler));

    BROOKESIA_LOGD("Register function `%1%`", func_name);

    return true;
}

bool FunctionRegistry::remove(const std::string &func_name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: func_name(%1%)", func_name);

    boost::lock_guard lock(functions_mutex_);

    auto it = functions_.find(func_name);
    BROOKESIA_CHECK_FALSE_RETURN(
        it != functions_.end(), false, "Function `%1%` not found", func_name
    );

    functions_.erase(it);

    BROOKESIA_LOGD("Unregister function `%1%`", func_name);

    return true;
}

bool FunctionRegistry::remove_all()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(functions_mutex_);
    functions_.clear();

    return true;
}

FunctionResult FunctionRegistry::call(const std::string &func_name, FunctionParameterMap &&parameters)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: func_name(%1%), parameters(%2%)", func_name, BROOKESIA_DESCRIBE_TO_STR(parameters));

    boost::unique_lock lock(functions_mutex_);

    FunctionResult error_result{
        .success = false,
    };
    auto &error_message = error_result.error_message;

    auto func_it = functions_.find(func_name);
    if (func_it == functions_.end()) {
        error_message = std::string("Function not found: ") + func_name;
        BROOKESIA_CHECK_FALSE_RETURN(false, error_result, "%1%", error_message);
    }

    auto &func_schema = func_it->second.first;
    auto func_handler = func_it->second.second;

    // Validate parameters and fill default values
    FunctionParameterMap validated_parameters = std::move(parameters);
    BROOKESIA_CHECK_FALSE_RETURN(
        validate_parameters(func_schema, validated_parameters, error_message), error_result, "%1%", error_message
    );

    // Unlock the mutex to avoid deadlock
    lock.unlock();

    // Call the function
    return func_handler(std::move(validated_parameters));
}

std::vector<FunctionSchema> FunctionRegistry::get_schemas()
{
    std::vector<FunctionSchema> definitions;
    boost::lock_guard lock(functions_mutex_);
    for (const auto& [func_name, func_schema_handler] : functions_) {
        definitions.push_back(func_schema_handler.first);
    }

    return definitions;
}

boost::json::array FunctionRegistry::get_schemas_json()
{
    boost::json::array schema;
    boost::lock_guard lock(functions_mutex_);
    for (const auto& [_, func_schema_handler] : functions_) {
        const auto &[func_schema, handler] = func_schema_handler;
        schema.push_back(std::move(BROOKESIA_DESCRIBE_TO_JSON(func_schema)));
    }

    return schema;
}

bool FunctionRegistry::validate_parameters(
    const FunctionSchema &func_schema, FunctionParameterMap &parameters, std::string &error_msg
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Check required parameters and fill default values
    for (const auto &param : func_schema.parameters) {
        auto it = parameters.find(param.name);

        if (it == parameters.end()) {
            if (param.is_required()) {
                error_msg = "Missing required parameter: `" + param.name + "`";
                break;
            } else {
                // Fill default value for optional parameters
                parameters[param.name] = param.default_value.value();
            }
        } else {
            // Validate the type of the provided parameter
            if (!param.is_compatible_value(it->second)) {
                // Get the actual type of the provided value
                FunctionValueType actual_type = FunctionValueType::String;  // Default fallback
                if (std::holds_alternative<bool>(it->second)) {
                    actual_type = FunctionValueType::Boolean;
                } else if (std::holds_alternative<double>(it->second)) {
                    actual_type = FunctionValueType::Number;
                } else if (std::holds_alternative<std::string>(it->second)) {
                    actual_type = FunctionValueType::String;
                } else if (std::holds_alternative<boost::json::object>(it->second)) {
                    actual_type = FunctionValueType::Object;
                } else if (std::holds_alternative<boost::json::array>(it->second)) {
                    actual_type = FunctionValueType::Array;
                }
                error_msg = "Invalid type for parameter `" + param.name +
                            "`: expected `" + BROOKESIA_DESCRIBE_TO_STR(param.type) +
                            "`, but got `" + BROOKESIA_DESCRIBE_TO_STR(actual_type) + "`";
                break;
            }
        }
    }

    if (!error_msg.empty()) {
#if BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG
        BROOKESIA_LOGE("%1%", error_msg);
#endif
        return false;
    }

    // Check extra parameters
    for (const auto& [arg_name, arg_value] : parameters) {
        bool found = false;
        for (const auto &param : func_schema.parameters) {
            if (param.name == arg_name) {
                found = true;
                break;
            }
        }
        if (!found) {
            error_msg = "Unknown parameter: `" + arg_name + "`";
            break;
        }
    }

    if (!error_msg.empty()) {
#if BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG
        BROOKESIA_LOGE("%1%", error_msg);
#endif
        return false;
    }

    return true;
}

} // namespace esp_brookesia::service
