/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
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
#include "brookesia/service_manager/function/definition.hpp"

namespace esp_brookesia::service {

/**
 * @brief Signature of a service function implementation.
 */
using FunctionHandler = std::function < FunctionResult(FunctionParameterMap &&) >;

/**
 * @brief Registry of callable service functions and their schemas.
 */
class FunctionRegistry {
public:
    FunctionRegistry() = default;
    ~FunctionRegistry() = default;

    /**
     * @brief Register a function schema and its handler.
     *
     * @param[in] func_schema Function metadata.
     * @param[in] func_handler Handler that executes the function.
     * @return true on success, false if the function name already exists.
     */
    bool add(FunctionSchema func_schema, FunctionHandler func_handler);
    /**
     * @brief Remove a registered function.
     *
     * @param[in] func_name Function name to remove.
     * @return true if the function was removed.
     */
    bool remove(const std::string &func_name);
    /**
     * @brief Remove every registered function.
     *
     * @return true if the registry was cleared successfully.
     */
    bool remove_all();

    /**
     * @brief Validate parameters and invoke a registered function.
     *
     * @param[in] func_name Function name to call.
     * @param[in] parameters Parameter map passed to the handler.
     * @return FunctionResult Execution result or validation failure information.
     */
    FunctionResult call(const std::string &func_name, FunctionParameterMap parameters);

    /**
     * @brief Look up the schema for a registered function.
     *
     * @param[in] func_name Function name to query.
     * @return const FunctionSchema* Pointer to the schema, or `nullptr` if not found.
     */
    const FunctionSchema *get_schema(const std::string &func_name)
    {
        boost::lock_guard lock(functions_mutex_);
        auto it = functions_.find(func_name);
        if (it == functions_.end()) {
            return nullptr;
        }
        return &it->second.first;
    }
    /**
     * @brief Get a snapshot of all registered function schemas.
     *
     * @return std::vector<FunctionSchema> Copy of the registered schemas.
     */
    std::vector<FunctionSchema> get_schemas() const;
    /**
     * @brief Export all registered function schemas as JSON.
     *
     * @return boost::json::array JSON representation of every schema.
     */
    boost::json::array get_schemas_json();
    /**
     * @brief Check whether a function is registered.
     *
     * @param[in] func_name Function name to check.
     * @return true if the function exists.
     */
    bool has(const std::string &func_name)
    {
        boost::lock_guard lock(functions_mutex_);
        return functions_.find(func_name) != functions_.end();
    }
    /**
     * @brief Get the number of registered functions.
     *
     * @return size_t Count of registered functions.
     */
    size_t get_count()
    {
        boost::lock_guard lock(functions_mutex_);
        return functions_.size();
    }

private:
    bool validate_parameters(
        const FunctionSchema &func_schema, FunctionParameterMap &parameters, std::string &error_msg
    );

    mutable boost::mutex functions_mutex_;
    std::map<std::string, std::pair<FunctionSchema, FunctionHandler>> functions_;
};

} // namespace esp_brookesia::service
