/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_mcp_engine.h"
#include "brookesia/service_manager/function/definition.hpp"
#include "brookesia/mcp_utils/macro_configs.h"

namespace esp_brookesia::mcp_utils {

/**
 * @brief Tool descriptor that forwards MCP calls to a registered service function.
 */
struct ServiceTool {
    /**
     * @brief Check whether the tool descriptor contains the required fields.
     *
     * @return `true` when both service and function names are non-empty.
     */
    bool is_valid() const
    {
        return !service_name.empty() && !function_name.empty();
    }

    std::string service_name;  ///< Target service name.
    std::string function_name; ///< Target function name inside the service.
};
BROOKESIA_DESCRIBE_STRUCT(ServiceTool, (), (service_name, function_name));

/**
 * @brief Callback signature for MCP tools implemented by custom code.
 */
using CustomToolCallback = service::FunctionResult(*)(service::FunctionParameterMap && params);

/**
 * @brief Tool descriptor backed by a custom callback implementation.
 */
struct CustomTool {
    /**
     * @brief Check whether the custom tool descriptor contains the required fields.
     *
     * @return `true` when the schema name is non-empty and the callback is valid.
     */
    bool is_valid() const
    {
        return !schema.name.empty() && (callback != nullptr);
    }

    service::FunctionSchema schema; ///< Tool schema exposed to the MCP engine.
    CustomToolCallback callback;    ///< Callback invoked when the tool is executed.
    service::RawBuffer context{};   ///< Opaque caller-defined context forwarded through the tool schema.
};
BROOKESIA_DESCRIBE_STRUCT(CustomTool, (), (schema, callback, context));

/**
 * @brief Reserved parameter name used for tool context forwarding.
 */
constexpr const char *CUSTOM_TOOL_PARAMETER_CONTEXT_NAME = "Context";

/**
 * @brief Registry that materializes MCP tools from services and custom callbacks.
 */
class ToolRegistry {
public:
    /**
     * @brief Construct an empty tool registry.
     */
    ToolRegistry() = default;
    /**
     * @brief Destroy the registry and release generated tool resources.
     */
    ~ToolRegistry();

    /**
     * @brief Add a tool that forward calls to an existing registered service functions.
     *
     * @param service_name  Service name.
     * @param function_names  Function names to add. If empty, all functions of the service will be added.
     * @return      The tool names (name example: "Service.<service_name>.<function_name>") on success, or
     *              empty vector on failure.
     */
    std::vector<std::string> add_service_tools(
        const std::string &service_name, std::vector<std::string> &&function_names = {}
    );

    /**
     * @brief Add a tool that is backed by a custom function.
     *
     * @param tool  Custom tool to add.
     * @return      The tool name (name example: "Custom.<tool.schema.name>") on success, or empty string on failure.
     */
    std::string add_custom_tool(CustomTool &&tool);

    /**
     * @brief Remove a previously added tool by name.
     *
     * @param tool_name  Name returned by add_service_tool() / add_custom_tool().
     */
    void remove_tool(const std::string &tool_name);

    /**
     * @brief Remove all tools added through this registry.
     */
    void remove_all_tools();

    /**
     * @brief Generate MCP tool descriptors for every registered service and custom tool.
     *
     * @return Vector of tool pointers owned by the registry-managed backing storage.
     */
    std::vector<esp_mcp_tool_t *> generate_tools() const;

private:
    std::map<std::string, std::pair<size_t /* slot */, ServiceTool>> service_tools_;
    std::map<std::string, std::pair<size_t /* slot */, CustomTool>> custom_tools_;
};

} // namespace esp_brookesia::mcp_utils
