/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>
#include "boost/json.hpp"
#include "boost/thread.hpp"
#include "brookesia/service_manager/function/definition.hpp"

namespace esp_brookesia::service {

using FunctionHandler = std::function < FunctionResult(FunctionParameterMap &&) >;

class FunctionRegistry {
public:
    FunctionRegistry() = default;
    ~FunctionRegistry() = default;

    bool add(FunctionSchema &&func_schema, FunctionHandler &&func_handler);
    bool remove(const std::string &func_name);
    bool remove_all();

    FunctionResult call(const std::string &func_name, FunctionParameterMap &&parameters);

    const FunctionSchema *get_schema(const std::string &func_name)
    {
        boost::lock_guard lock(functions_mutex_);
        auto it = functions_.find(func_name);
        if (it == functions_.end()) {
            return nullptr;
        }
        return &it->second.first;
    }
    std::vector<FunctionSchema> get_schemas();
    boost::json::array get_schemas_json();
    bool has(const std::string &func_name)
    {
        boost::lock_guard lock(functions_mutex_);
        return functions_.find(func_name) != functions_.end();
    }
    size_t get_count()
    {
        boost::lock_guard lock(functions_mutex_);
        return functions_.size();
    }

private:
    bool validate_parameters(
        const FunctionSchema &func_schema, FunctionParameterMap &parameters, std::string &error_msg
    );

    boost::mutex functions_mutex_;
    std::map<std::string, std::pair<FunctionSchema, FunctionHandler>> functions_;
};

} // namespace esp_brookesia::service
