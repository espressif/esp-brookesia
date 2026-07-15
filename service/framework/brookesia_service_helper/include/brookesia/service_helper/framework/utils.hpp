/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <span>
#include <string>
#include <string_view>

#include "boost/json.hpp"

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/helper/base.hpp"
#include "brookesia/service_manager/service/utils_service.hpp"

namespace esp_brookesia::service::helper {

/**
 * @brief Type-safe helper for the built-in Utils service.
 */
class Utils: public Base<Utils> {
public:
    using DebugFeatureState = service::UtilsService::DebugFeatureState;
    using DebugConfig = service::UtilsService::DebugConfig;
    using DebugCapabilities = service::UtilsService::DebugCapabilities;
    using DebugRuntimeState = service::UtilsService::DebugRuntimeState;
    using DebugSnapshot = service::UtilsService::DebugSnapshot;
    using MemoryDebugSnapshot = service::UtilsService::MemoryDebugSnapshot;
    using ThreadDebugSnapshot = service::UtilsService::ThreadDebugSnapshot;
    using FunctionId = service::UtilsService::FunctionId;
    using EventId = service::UtilsService::EventId;

    static constexpr std::string_view get_name()
    {
        return service::UtilsService::get_name();
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        return service::UtilsService::get_static_function_schemas();
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        return service::UtilsService::get_static_event_schemas();
    }

    static std::expected<DebugCapabilities, std::string> get_debug_capabilities(uint32_t timeout_ms = 0)
    {
        return call_and_parse<DebugCapabilities>(FunctionId::GetDebugCapabilities, timeout_ms);
    }

    static std::expected<DebugConfig, std::string> get_debug_config(uint32_t timeout_ms = 0)
    {
        return call_and_parse<DebugConfig>(FunctionId::GetDebugConfig, timeout_ms);
    }

    static std::expected<void, std::string> set_debug_config(const DebugConfig &config, uint32_t timeout_ms = 0)
    {
        auto binding = bind_service();
        if (!binding.is_valid()) {
            return std::unexpected("Failed to bind Utils service");
        }
        auto config_json = BROOKESIA_DESCRIBE_TO_JSON(config);
        if (!config_json.is_object()) {
            return std::unexpected("Failed to serialize Utils debug configuration");
        }
        return call_function_sync<void>(FunctionId::SetDebugConfig, config_json.as_object(), Timeout(timeout_ms));
    }

    static std::expected<void, std::string> start_memory_debug(uint32_t timeout_ms = 0)
    {
        return call_void(FunctionId::StartMemoryDebug, timeout_ms);
    }

    static std::expected<void, std::string> stop_memory_debug(uint32_t timeout_ms = 0)
    {
        return call_void(FunctionId::StopMemoryDebug, timeout_ms);
    }

    static std::expected<void, std::string> start_thread_debug(uint32_t timeout_ms = 0)
    {
        return call_void(FunctionId::StartThreadDebug, timeout_ms);
    }

    static std::expected<void, std::string> stop_thread_debug(uint32_t timeout_ms = 0)
    {
        return call_void(FunctionId::StopThreadDebug, timeout_ms);
    }

    static std::expected<DebugRuntimeState, std::string> get_debug_state(uint32_t timeout_ms = 0)
    {
        return call_and_parse<DebugRuntimeState>(FunctionId::GetDebugState, timeout_ms);
    }

    static std::expected<DebugSnapshot, std::string> get_debug_snapshot(uint32_t timeout_ms = 0)
    {
        return call_and_parse<DebugSnapshot>(FunctionId::GetDebugSnapshot, timeout_ms);
    }

    static std::expected<MemoryDebugSnapshot, std::string> get_memory_snapshot(uint32_t timeout_ms = 0)
    {
        return call_and_parse<MemoryDebugSnapshot>(FunctionId::GetMemorySnapshot, timeout_ms);
    }

private:
    static ServiceBinding bind_service()
    {
        return ServiceManager::get_instance().bind(get_name().data());
    }

    static std::expected<void, std::string> call_void(FunctionId function_id, uint32_t timeout_ms)
    {
        auto binding = bind_service();
        if (!binding.is_valid()) {
            return std::unexpected("Failed to bind Utils service");
        }
        return call_function_sync<void>(function_id, Timeout(timeout_ms));
    }

    template <typename Value>
    static std::expected<Value, std::string> call_and_parse(FunctionId function_id, uint32_t timeout_ms)
    {
        auto binding = bind_service();
        if (!binding.is_valid()) {
            return std::unexpected("Failed to bind Utils service");
        }
        auto result = call_function_sync<boost::json::object>(function_id, Timeout(timeout_ms));
        if (!result) {
            return std::unexpected(result.error());
        }
        Value value;
        if (!BROOKESIA_DESCRIBE_FROM_JSON(*result, value)) {
            return std::unexpected("Failed to parse Utils service result");
        }
        return value;
    }
};

} // namespace esp_brookesia::service::helper
