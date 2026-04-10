/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/base.hpp"
#include "brookesia/service_manager/function/definition.hpp"
#include "manager.hpp"

namespace esp_brookesia::agent::helper {

/**
 * @brief Helper schema definitions for the XiaoZhi agent service.
 */
class XiaoZhi: public service::helper::Base<XiaoZhi> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId : uint8_t {
        AddMCP_ToolsWithServiceFunction,
        AddMCP_ToolsWithCustomFunction,
        RemoveMCP_Tools,
        ExplainImage,
        Max,
    };

    enum class EventId : uint8_t {
        ActivationCodeReceived,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionAddMCP_ToolsWithServiceFunctionParam : uint8_t {
        ServiceName,
        FunctionNames,
    };
    enum class FunctionAddMCP_ToolsWithCustomFunctionParam : uint8_t {
        Tools,
    };
    enum class FunctionRemoveMCP_ToolsParam : uint8_t {
        Tools,
    };
    enum class FunctionExplainImageParam : uint8_t {
        Image,
        Question,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class EventActivationCodeReceivedParam : uint8_t {
        Code,
    };

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static service::FunctionSchema function_schema_add_mcp_tool_with_service_function()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::AddMCP_ToolsWithServiceFunction),
            .description = (boost::format("Add MCP tools from service functions. "
                                          "Return type: JSON array<string> of added tool names. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({
                "Service.Audio.SetVolume",
                "Service.Audio.GetVolume",
            }))).str(),
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionAddMCP_ToolsWithServiceFunctionParam::ServiceName),
                    .description = "Service name.",
                    .type = service::FunctionValueType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionAddMCP_ToolsWithServiceFunctionParam::FunctionNames),
                    .description = (boost::format("Function names as JSON array<string>. "
                                                  "Empty means all functions in the service. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({
                        "SetVolume",
                        "GetVolume"
                    }))).str(),
                    .type = service::FunctionValueType::Array,
                    .default_value = boost::json::array(),
                }
            },
            .require_scheduler = false,
        };
    }
    static service::FunctionSchema function_schema_add_mcp_tool_with_custom_function()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::AddMCP_ToolsWithCustomFunction),
            .description = (boost::format("Add custom MCP tools. "
                                          "Return type: JSON array<string> of added tool names. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({
                "Custom.Display.GetBrightness",
                "Custom.Display.SetBrightness",
            }))).str(),
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionAddMCP_ToolsWithCustomFunctionParam::Tools),
                    .description = (boost::format("Tools to add as JSON array<object>. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE((std::vector<std::map<std::string, std::string>>({
                        {
                            {"name", "Display.GetBrightness"},
                            {"description", "custom tool description 1"}
                        },
                        {
                            {"name", "Display.SetBrightness"},
                            {"description", "custom tool description 2"}
                        }
                    })))).str(),
                    .type = service::FunctionValueType::Array,
                }
            },
            .require_scheduler = false,
        };
    }
    static service::FunctionSchema function_schema_remove_mcp_tools()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::RemoveMCP_Tools),
            .description = "Remove MCP tools.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionRemoveMCP_ToolsParam::Tools),
                    .description = (boost::format("Tool names to remove. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({
                        "Service.Audio.SetVolume",
                        "Custom.Display.GetBrightness",
                    }))).str(),
                    .type = service::FunctionValueType::Array,
                }
            },
            .require_scheduler = false,
        };
    }
    static service::FunctionSchema function_schema_explain_image()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ExplainImage),
            .description = "Explain an image. Return type: string. Example: \"This image contains a cup on a desk.\"",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionExplainImageParam::Image),
                    .description = "Image data.",
                    .type = service::FunctionValueType::RawBuffer,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionExplainImageParam::Question),
                    .description = "Question text.",
                    .type = service::FunctionValueType::String,
                    .default_value = "What is in the image?",
                }
            },
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static service::EventSchema event_schema_activation_code_received()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::ActivationCodeReceived),
            .description = "Emitted when an activation code is received.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventActivationCodeReceivedParam::Code),
                    .description = "Activation code.",
                    .type = service::EventItemType::String
                }
            },
        };
    }

public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the functions required by the Base class /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Name of the XiaoZhi agent service.
     *
     * @return std::string_view Stable service name.
     */
    static constexpr std::string_view get_name()
    {
        return "AgentXiaoZhi";
    }

    /**
     * @brief Get the function schemas exported by the XiaoZhi agent.
     *
     * @return std::span<const service::FunctionSchema> Static schema span.
     */
    static std::span<const service::FunctionSchema> get_function_schemas()
    {
        static const std::array < service::FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max) >
        FUNCTION_SCHEMAS = {
            {
                function_schema_add_mcp_tool_with_service_function(),
                function_schema_add_mcp_tool_with_custom_function(),
                function_schema_remove_mcp_tools(),
                function_schema_explain_image(),
            }
        };
        return std::span<const service::FunctionSchema>(FUNCTION_SCHEMAS.begin(), FUNCTION_SCHEMAS.end());
    }

    /**
     * @brief Get the event schemas exported by the XiaoZhi agent.
     *
     * @return std::span<const service::EventSchema> Static schema span.
     */
    static std::span<const service::EventSchema> get_event_schemas()
    {
        static const std::array < service::EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max) > EVENT_SCHEMAS = {
            {
                event_schema_activation_code_received(),
            }
        };
        return std::span<const service::EventSchema>(EVENT_SCHEMAS.begin(), EVENT_SCHEMAS.end());
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief  Function related
 */
BROOKESIA_DESCRIBE_ENUM(
    XiaoZhi::FunctionId, AddMCP_ToolsWithServiceFunction, AddMCP_ToolsWithCustomFunction, RemoveMCP_Tools,
    ExplainImage, Max
);
BROOKESIA_DESCRIBE_ENUM(XiaoZhi::FunctionAddMCP_ToolsWithServiceFunctionParam, ServiceName, FunctionNames);
BROOKESIA_DESCRIBE_ENUM(XiaoZhi::FunctionAddMCP_ToolsWithCustomFunctionParam, Tools);
BROOKESIA_DESCRIBE_ENUM(XiaoZhi::FunctionRemoveMCP_ToolsParam, Tools);
BROOKESIA_DESCRIBE_ENUM(XiaoZhi::FunctionExplainImageParam, Image, Question);

/**
 * @brief  Event related
 */
BROOKESIA_DESCRIBE_ENUM(XiaoZhi::EventId, ActivationCodeReceived, Max);
BROOKESIA_DESCRIBE_ENUM(XiaoZhi::EventActivationCodeReceivedParam, Code);

} // namespace esp_brookesia::agent::helper
