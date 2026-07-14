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
#include <utility>
#include <vector>

#include "boost/json.hpp"

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/helper/base.hpp"
#include "brookesia/service_manager/service/manager_service.hpp"

namespace esp_brookesia::service::helper {

/**
 * @brief Type-safe helper for the built-in Manager service.
 */
class Manager: public Base<Manager> {
public:
    using ServiceState = service::ManagerService::ServiceState;
    using ServiceInfo = service::ManagerService::ServiceInfo;
    using ServiceSchemaOverview = service::ManagerService::ServiceSchemaOverview;
    using FunctionId = service::ManagerService::FunctionId;
    using EventId = service::ManagerService::EventId;

    static constexpr std::string_view get_name()
    {
        return service::ManagerService::get_name();
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        return service::ManagerService::get_static_function_schemas();
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        return service::ManagerService::get_static_event_schemas();
    }

    static std::expected<std::vector<std::string>, std::string> get_service_names(uint32_t timeout_ms = 0)
    {
        return call_and_parse<std::vector<std::string>, boost::json::array>(
                   FunctionId::GetServiceNames, timeout_ms
               );
    }

    static std::expected<ServiceInfo, std::string> get_service_info(
        const std::string &name, uint32_t timeout_ms = 0
    )
    {
        return call_and_parse_with_args<ServiceInfo, boost::json::object>(
                   FunctionId::GetServiceInfo, timeout_ms, name
               );
    }

    static std::expected<ServiceSchemaOverview, std::string> get_service_schema(
        const std::string &name, uint32_t timeout_ms = 0
    )
    {
        return call_and_parse_with_args<ServiceSchemaOverview, boost::json::object>(
                   FunctionId::GetServiceSchema, timeout_ms, name
               );
    }

    static std::expected<FunctionSchema, std::string> get_service_function_schema(
        const std::string &service_name, const std::string &function_name, uint32_t timeout_ms = 0
    )
    {
        return call_and_parse_with_args<FunctionSchema, boost::json::object>(
                   FunctionId::GetServiceFunctionSchema, timeout_ms, service_name, function_name
               );
    }

    static std::expected<EventSchema, std::string> get_service_event_schema(
        const std::string &service_name, const std::string &event_name, uint32_t timeout_ms = 0
    )
    {
        return call_and_parse_with_args<EventSchema, boost::json::object>(
                   FunctionId::GetServiceEventSchema, timeout_ms, service_name, event_name
               );
    }

private:
    template <typename Value, typename JsonValue>
    static std::expected<Value, std::string> call_and_parse(FunctionId function_id, uint32_t timeout_ms)
    {
        auto binding = ServiceManager::get_instance().bind(get_name().data());
        if (!binding.is_valid()) {
            return std::unexpected("Failed to bind Manager service");
        }

        auto result = call_function_sync<JsonValue>(function_id, Timeout(timeout_ms));
        if (!result) {
            return std::unexpected(result.error());
        }
        Value value;
        if (!BROOKESIA_DESCRIBE_FROM_JSON(*result, value)) {
            return std::unexpected("Failed to parse Manager service result");
        }
        return value;
    }

    template <typename Value, typename JsonValue, typename... Args>
    static std::expected<Value, std::string> call_and_parse_with_args(
        FunctionId function_id, uint32_t timeout_ms, Args &&... args
    )
    {
        auto binding = ServiceManager::get_instance().bind(get_name().data());
        if (!binding.is_valid()) {
            return std::unexpected("Failed to bind Manager service");
        }

        auto result = call_function_sync<JsonValue>(
                          function_id, std::forward<Args>(args)..., Timeout(timeout_ms)
                      );
        if (!result) {
            return std::unexpected(result.error());
        }
        Value value;
        if (!BROOKESIA_DESCRIBE_FROM_JSON(*result, value)) {
            return std::unexpected("Failed to parse Manager service result");
        }
        return value;
    }
};

} // namespace esp_brookesia::service::helper
