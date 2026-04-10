/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/mcp_utils/macro_configs.h"
#if !BROOKESIA_MCP_UTILS_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "esp_mcp_engine.h"
#include "esp_mcp_tool.h"
#include "esp_mcp_property.h"
#include "esp_mcp_data.h"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_manager/service/manager.hpp"
#include "brookesia/mcp_utils/mcp_utils.hpp"

namespace esp_brookesia::mcp_utils {

namespace {
// ============================================================================
// MCP tool trampoline infrastructure
//
// Since esp_mcp_tool_callback_t is a plain C function pointer with no
// user-data parameter, we generate a fixed set of unique static C functions
// at compile time (one per slot index N).  The actual handler for each slot
// is stored in a global unordered_map, so memory is only used for active
// slots rather than reserving a full fixed-size array upfront.
// ============================================================================

/// Maximum number of simultaneously registered MCP tools across all instances.
/// Raising this value increases compile-time template instantiation cost.
constexpr size_t MCP_TOOL_MAX_SLOTS = 64;

using SlotHandler = std::function<esp_mcp_value_t(const esp_mcp_property_list_t *)>;

/// Dynamic storage: only allocated entries consume memory.
boost::mutex mcp_mutex;
std::unordered_map<size_t, SlotHandler> mcp_slot_to_handler;

template<size_t N>
esp_mcp_value_t mcp_tool_trampoline(const esp_mcp_property_list_t *props)
{
    SlotHandler handler = nullptr;

    {
        boost::lock_guard lock(mcp_mutex);
        auto it = mcp_slot_to_handler.find(N);
        if (it != mcp_slot_to_handler.end()) {
            handler = it->second;
        }
    }

    if (handler) {
        return handler(props);
    }

    return esp_mcp_value_create_bool(false);
}

template<size_t... Ns>
constexpr std::array<esp_mcp_tool_callback_t, sizeof...(Ns)>
make_mcp_trampolines(std::index_sequence<Ns...>)
{
    return {mcp_tool_trampoline<Ns>...};
}

const std::array<esp_mcp_tool_callback_t, MCP_TOOL_MAX_SLOTS> s_mcp_trampolines =
    make_mcp_trampolines(std::make_index_sequence<MCP_TOOL_MAX_SLOTS> {});

size_t alloc_mcp_slot(SlotHandler handler)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: handler(%1%)", handler);

    boost::lock_guard lock(mcp_mutex);

    for (size_t i = 0; i < MCP_TOOL_MAX_SLOTS; i++) {
        if (mcp_slot_to_handler.find(i) == mcp_slot_to_handler.end()) {
            mcp_slot_to_handler.emplace(i, std::move(handler));
            BROOKESIA_LOGD("Allocated MCP tool slot[%1%] for tool '%2%'", i, tool_name);
            return i;
        }
    }
    return MCP_TOOL_MAX_SLOTS; // no slot available
}

void free_mcp_slot(size_t slot)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: slot(%1%)", slot);

    boost::lock_guard lock(mcp_mutex);

    mcp_slot_to_handler.erase(slot);
}

esp_mcp_tool_callback_t get_mcp_tool_callback(size_t slot)
{
    if (slot >= MCP_TOOL_MAX_SLOTS) {
        return nullptr;
    }
    return s_mcp_trampolines[slot];
}

// ============================================================================
// Helper: build esp_mcp_property_t from a FunctionParameterSchema
// ============================================================================
esp_mcp_property_t *create_property_from_schema(const service::FunctionParameterSchema &param)
{
    const char *name = param.name.c_str();
    switch (param.type) {
    case service::FunctionValueType::Boolean:
        if (param.default_value && std::holds_alternative<bool>(*param.default_value)) {
            return esp_mcp_property_create_with_bool(name, std::get<bool>(*param.default_value));
        }
        return esp_mcp_property_create(name, ESP_MCP_PROPERTY_TYPE_BOOLEAN);
    case service::FunctionValueType::Number:
        if (param.default_value && std::holds_alternative<double>(*param.default_value)) {
            return esp_mcp_property_create_with_float(
                       name, static_cast<float>(std::get<double>(*param.default_value))
                   );
        }
        return esp_mcp_property_create(name, ESP_MCP_PROPERTY_TYPE_FLOAT);
    case service::FunctionValueType::String:
        if (param.default_value && std::holds_alternative<std::string>(*param.default_value)) {
            return esp_mcp_property_create_with_string(
                       name, std::get<std::string>(*param.default_value).c_str()
                   );
        }
        return esp_mcp_property_create(name, ESP_MCP_PROPERTY_TYPE_STRING);
    case service::FunctionValueType::Object:
        if (param.default_value && std::holds_alternative<boost::json::object>(*param.default_value)) {
            return esp_mcp_property_create_with_object(
                       name, boost::json::serialize(std::get<boost::json::object>(*param.default_value)).c_str()
                   );
        }
        return esp_mcp_property_create(name, ESP_MCP_PROPERTY_TYPE_OBJECT);
    case service::FunctionValueType::Array:
        if (param.default_value && std::holds_alternative<boost::json::array>(*param.default_value)) {
            return esp_mcp_property_create_with_array(
                       name, boost::json::serialize(std::get<boost::json::array>(*param.default_value)).c_str()
                   );
        }
        return esp_mcp_property_create(name, ESP_MCP_PROPERTY_TYPE_ARRAY);
    default:
        return nullptr;
    }
}

// ============================================================================
// Helper: convert MCP property list to FunctionParameterMap using schema
// ============================================================================
service::FunctionParameterMap mcp_props_to_param_map(
    const esp_mcp_property_list_t *props, const service::FunctionSchema &schema
)
{
    service::FunctionParameterMap params;
    for (const auto &param : schema.parameters) {
        const char *name = param.name.c_str();
        switch (param.type) {
        case service::FunctionValueType::Boolean:
            params[param.name] = service::FunctionValue(
                                     esp_mcp_property_list_get_property_bool(props, name)
                                 );
            break;
        case service::FunctionValueType::Number:
            params[param.name] = service::FunctionValue(
                                     static_cast<double>(esp_mcp_property_list_get_property_float(props, name))
                                 );
            break;
        case service::FunctionValueType::String: {
            const char *str = esp_mcp_property_list_get_property_string(props, name);
            params[param.name] = service::FunctionValue(std::string(str ? str : ""));
            break;
        }
        case service::FunctionValueType::Object: {
            const char *obj_str = esp_mcp_property_list_get_property_object(props, name);
            if (obj_str) {
                boost::system::error_code ec;
                auto val = boost::json::parse(obj_str, ec);
                if (!ec && val.is_object()) {
                    params[param.name] = service::FunctionValue(val.as_object());
                }
            }
            break;
        }
        case service::FunctionValueType::Array: {
            const char *arr_str = esp_mcp_property_list_get_property_array(props, name);
            if (arr_str) {
                boost::system::error_code ec;
                auto val = boost::json::parse(arr_str, ec);
                if (!ec && val.is_array()) {
                    params[param.name] = service::FunctionValue(val.as_array());
                }
            }
            break;
        }
        default:
            break;
        }
    }
    return params;
}

// ============================================================================
// Helper: convert FunctionResult to esp_mcp_value_t
// ============================================================================
esp_mcp_value_t function_result_to_mcp_value(const service::FunctionResult &result)
{
    if (!result.success) {
        BROOKESIA_LOGE("Function result is not successful: %1%", result.error_message);
        return esp_mcp_value_create_bool(false);
    }
    if (!result.has_data()) {
        return esp_mcp_value_create_bool(true);
    }
    const auto &data = *result.data;
    if (std::holds_alternative<bool>(data)) {
        return esp_mcp_value_create_bool(std::get<bool>(data));
    } else if (std::holds_alternative<double>(data)) {
        return esp_mcp_value_create_float(static_cast<float>(std::get<double>(data)));
    } else if (std::holds_alternative<std::string>(data)) {
        return esp_mcp_value_create_string(std::get<std::string>(data).c_str());
    } else if (std::holds_alternative<boost::json::object>(data)) {
        return esp_mcp_value_create_string(
                   boost::json::serialize(std::get<boost::json::object>(data)).c_str()
               );
    } else if (std::holds_alternative<boost::json::array>(data)) {
        return esp_mcp_value_create_string(
                   boost::json::serialize(std::get<boost::json::array>(data)).c_str()
               );
    }
    return esp_mcp_value_create_bool(true);
}
} // anonymous namespace

// ============================================================================
// ToolRegistry
// ============================================================================
constexpr std::string_view SERVICE_FUNCTION_TOOL_NAME_PREFIX = "Service.";
constexpr std::string_view CUSTOM_FUNCTION_TOOL_NAME_PREFIX = "Custom.";

ToolRegistry::~ToolRegistry()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    remove_all_tools();
}

std::vector<std::string> ToolRegistry::add_service_tools(
    const std::string &service_name, std::vector<std::string> &&function_names
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: service_name(%1%), function_names(%2%)", service_name, function_names);

    BROOKESIA_CHECK_FALSE_RETURN(!service_name.empty(), {}, "Service name is empty");

    // Get service from service manager
    auto svc = service::ServiceManager::get_instance().get_service(service_name);
    BROOKESIA_CHECK_NULL_RETURN(svc, {}, "Service '%1%' not found", service_name);

    // Get function schemas
    auto function_schemas = svc->get_function_schemas();
    // If function names are empty, use all function schemas
    if (function_names.empty()) {
        function_names.reserve(function_schemas.size());
        for (const auto &schema : function_schemas) {
            function_names.push_back(schema.name);
        }
    }

    // Create tool function
    auto create_tool = [&](const std::string & function_name) -> std::string {
        // Check if tool already exists
        auto tool_name = std::string(SERVICE_FUNCTION_TOOL_NAME_PREFIX) + service_name + "." + function_name;
        BROOKESIA_CHECK_FALSE_RETURN(service_tools_.count(tool_name) == 0, {}, "Tool '%1%' already exists", tool_name);

        // Create slot handler
        SlotHandler handler = [this, tool_name](const esp_mcp_property_list_t *props)
        {
            // Get service tool
            auto it = service_tools_.find(tool_name);
            BROOKESIA_CHECK_FALSE_RETURN(
                it != service_tools_.end(), esp_mcp_value_create_bool(false), "Tool '%1%' not found", tool_name
            );
            auto &[_, service_tool] = it->second;

            // Get service
            auto service = service::ServiceManager::get_instance().get_service(service_tool.service_name);
            BROOKESIA_CHECK_NULL_RETURN(
                service, esp_mcp_value_create_bool(false), "Service '%1%' not found", service_tool.service_name
            );
            // Get function schema
            auto schemas = service->get_function_schemas();
            auto schema_it = std::find_if(schemas.begin(), schemas.end(), [&](const auto & schema) {
                return schema.name == service_tool.function_name;
            });
            BROOKESIA_CHECK_FALSE_RETURN(
                schema_it != schemas.end(), esp_mcp_value_create_bool(false),
                "Function '%1%' not found in service '%2%'", service_tool.function_name, service_tool.service_name
            );
            auto &schema = *schema_it;

            // Convert MCP properties to function parameters
            auto params = mcp_props_to_param_map(props, schema);
            // Call function
            auto result = service->call_function_sync(schema.name, std::move(params));

            // Convert result to MCP value
            return function_result_to_mcp_value(result);
        };

        // Allocate slot
        size_t slot = alloc_mcp_slot(std::move(handler));
        BROOKESIA_CHECK_OUT_RANGE_RETURN(slot, 0, MCP_TOOL_MAX_SLOTS - 1, {}, "Invalid slot index: %1%", slot);

        // Store tool information
        service_tools_[tool_name] = { slot, {service_name, function_name}};

        BROOKESIA_LOGI("Added MCP service tool '%1%' at slot '%2%'", tool_name, slot);

        return tool_name;
    };

    std::vector<std::string> tool_names;
    tool_names.reserve(function_names.size());
    for (const auto &function_name : function_names) {
        // Check if function exists in service
        auto function_schema_it = std::find_if(function_schemas.begin(), function_schemas.end(), [&](const auto & schema) {
            return schema.name == function_name;
        });
        if (function_schema_it == function_schemas.end()) {
            BROOKESIA_LOGE("Function '%1%' not found in service '%2%'", function_name, service_name);
            continue;
        }

        // Create tool
        auto tool_name = create_tool(function_name);
        if (tool_name.empty()) {
            BROOKESIA_LOGE("Failed to create MCP service tool '%1%', skip", function_name);
            continue;
        }

        tool_names.push_back(tool_name);
    }

    return tool_names;
}

std::string ToolRegistry::add_custom_tool(CustomTool &&tool)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: tool(%1%)", BROOKESIA_DESCRIBE_TO_STR(tool));

    BROOKESIA_CHECK_FALSE_RETURN(tool.is_valid(), {}, "Tool '%1%' is not valid", tool.schema.name);

    auto tool_name = std::string(CUSTOM_FUNCTION_TOOL_NAME_PREFIX) + tool.schema.name;
    BROOKESIA_CHECK_FALSE_RETURN(custom_tools_.count(tool_name) == 0, {}, "Tool '%1%' already exists", tool_name);

    SlotHandler handler = [this, tool_name](const esp_mcp_property_list_t *props) {
        // Get custom tool
        auto it = custom_tools_.find(tool_name);
        BROOKESIA_CHECK_FALSE_RETURN(
            it != custom_tools_.end(), esp_mcp_value_create_bool(false), "Tool '%1%' not found", tool_name
        );
        auto &[_, custom_tool] = it->second;

        // Get parameters from MCP properties
        auto params = mcp_props_to_param_map(props, custom_tool.schema);
        // Add context to parameters if it exists
        if (custom_tool.context.data_ptr != nullptr) {
            params[std::string(CUSTOM_TOOL_PARAMETER_CONTEXT_NAME)] = service::FunctionValue(custom_tool.context);
        }

        // Call custom function
        service::FunctionResult result;
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            result = custom_tool.callback(std::move(params)), esp_mcp_value_create_bool(false),
            "Failed to call custom function '%1%'", tool_name
        );

        // Convert result to MCP value
        return function_result_to_mcp_value(result);
    };

    size_t slot = alloc_mcp_slot(std::move(handler));
    BROOKESIA_CHECK_FALSE_RETURN(slot < MCP_TOOL_MAX_SLOTS, {}, "No MCP trampoline slots available");

    custom_tools_[tool_name] = { slot, std::move(tool) };

    BROOKESIA_LOGI("Added MCP custom tool '%1%' at slot '%2%'", tool_name, slot);

    return tool_name;
}

void ToolRegistry::remove_tool(const std::string &tool_name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: tool_name(%1%)", tool_name);

    auto remove_tool = [&](size_t slot) {
        free_mcp_slot(slot);
        BROOKESIA_LOGI("Removed MCP tool '%1%' at slot '%2%'", tool_name, slot);
    };

    // Check if tool is a service tool
    auto service_tool_it = service_tools_.find(tool_name);
    if (service_tool_it != service_tools_.end()) {
        remove_tool(service_tool_it->second.first);
        service_tools_.erase(service_tool_it);
        return;
    }

    // Check if tool is a custom tool
    auto custom_tool_it = custom_tools_.find(tool_name);
    if (custom_tool_it != custom_tools_.end()) {
        remove_tool(custom_tool_it->second.first);
        custom_tools_.erase(custom_tool_it);
        return;
    }

    BROOKESIA_LOGD("Tool '%1%' not found, skip", tool_name);
}

void ToolRegistry::remove_all_tools()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    for (const auto &[name, _] : service_tools_) {
        remove_tool(name);
    }
    for (const auto &[name, _] : custom_tools_) {
        remove_tool(name);
    }

    BROOKESIA_LOGI("Removed all MCP tools");
}

std::vector<esp_mcp_tool_t *> ToolRegistry::generate_tools() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // To avoid pre-commit errors, use a typedef here
    using MCP_ToolHandle = esp_mcp_tool_t *;
    auto create_tool_and_get_handle =
    [this](const std::string & tool_name, const service::FunctionSchema & schema) -> MCP_ToolHandle {
        // Get slot
        auto get_slot = [&](const std::string & tool_name) -> size_t {
            auto service_tool_it = service_tools_.find(tool_name);
            if (service_tool_it != service_tools_.end())
            {
                return service_tool_it->second.first;
            }
            auto custom_tool_it = custom_tools_.find(tool_name);
            if (custom_tool_it != custom_tools_.end())
            {
                return custom_tool_it->second.first;
            }
            return MCP_TOOL_MAX_SLOTS; // not found
        };
        size_t slot = get_slot(tool_name);
        BROOKESIA_CHECK_OUT_RANGE_RETURN(slot, 0, MCP_TOOL_MAX_SLOTS - 1, nullptr, "Invalid slot index: %1%", slot);

        // Get callback
        auto callback = get_mcp_tool_callback(slot);
        BROOKESIA_CHECK_NULL_RETURN(callback, nullptr, "Failed to get MCP tool callback for '%1%'", tool_name);

        // Create MCP tool
        auto *mcp_tool = esp_mcp_tool_create(tool_name.c_str(), schema.description.c_str(), callback);
        BROOKESIA_CHECK_NULL_RETURN(mcp_tool, nullptr, "Failed to create MCP tool '%1%'", tool_name);
        lib_utils::FunctionGuard destroy_tool_guard([this, mcp_tool]()
        {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            esp_mcp_tool_destroy(mcp_tool);
        });

        // Add properties to MCP tool
        for (const auto &param : schema.parameters)
        {
            auto *prop = create_property_from_schema(param);
            BROOKESIA_CHECK_NULL_RETURN(prop, nullptr, "Failed to create property for parameter '%1%'", param.name);
            auto ret = esp_mcp_tool_add_property(mcp_tool, prop);
            BROOKESIA_CHECK_ESP_ERR_RETURN(
                ret, nullptr, "Failed to add property '%1%' to MCP tool '%2%'", param.name, tool_name
            );
        }

        destroy_tool_guard.release();

        return mcp_tool;
    };

    std::vector<esp_mcp_tool_t *> handles;
    handles.reserve(service_tools_.size() + custom_tools_.size());

    // Create MCP tools for service tools
    std::string service_name;
    std::shared_ptr<service::ServiceBase> service = nullptr;
    std::vector<service::FunctionSchema> function_schemas;
    for (const auto &[name, slot_tool_pair] : service_tools_) {
        auto &[_, tool] = slot_tool_pair;
        // Update function schemas if needed
        if (service_name != tool.service_name) {
            service = service::ServiceManager::get_instance().get_service(tool.service_name);
            if (service == nullptr) {
                BROOKESIA_LOGE("Service '%1%' not found, skip", tool.service_name);
                continue;
            }
            service_name = tool.service_name;
            function_schemas = service->get_function_schemas();
        }

        // Find function schema
        const auto &it = std::find_if(function_schemas.begin(), function_schemas.end(), [&](const auto & schema) {
            return schema.name == tool.function_name;
        });
        if (it == function_schemas.end()) {
            BROOKESIA_LOGE("Function '%1%' not found in service '%2%'", tool.function_name, service_name);
            continue;
        }

        // Create MCP tool
        auto handle = create_tool_and_get_handle(name, *it);
        if (handle == nullptr) {
            BROOKESIA_LOGE("Failed to create MCP tool '%1%'", name);
            continue;
        }

        handles.push_back(handle);
    }

    // Create MCP tools for custom tools
    for (const auto &[name, slot_tool_pair] : custom_tools_) {
        // Get tool
        auto &[_, tool] = slot_tool_pair;

        // Create MCP tool
        auto handle = create_tool_and_get_handle(name, tool.schema);
        if (handle == nullptr) {
            BROOKESIA_LOGE("Failed to create MCP tool '%1%'", name);
            continue;
        }
        handles.push_back(handle);
    }

    return handles;
}

} // namespace esp_brookesia::mcp_utils
