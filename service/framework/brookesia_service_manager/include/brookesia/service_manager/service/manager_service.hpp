/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/service/base.hpp"

namespace esp_brookesia::service {

class ServiceManager;

class ManagerService final: public ServiceBase {
public:
    enum class ServiceState : uint8_t {
        Stopped,
        Starting,
        Running,
        Stopping,
    };

    struct ServiceInfo {
        std::string name;
        std::string version;
        ServiceState state = ServiceState::Stopped;
        size_t reference_count = 0;
        bool bindable = true;
        std::vector<std::string> dependencies;
    };

    struct ServiceSchemaOverview {
        std::string name;
        std::string description;
        std::vector<std::string> function_names;
        std::vector<std::string> event_names;
    };

    enum class FunctionId : uint8_t {
        GetServiceNames,
        GetServiceInfo,
        GetServiceSchema,
        GetServiceFunctionSchema,
        GetServiceEventSchema,
        Max,
    };

    enum class EventId : uint8_t {
        Max,
    };

    enum class FunctionGetServiceInfoParam : uint8_t { Name };
    enum class FunctionGetServiceSchemaParam : uint8_t { Name };
    enum class FunctionGetServiceFunctionSchemaParam : uint8_t { ServiceName, FunctionName };
    enum class FunctionGetServiceEventSchemaParam : uint8_t { ServiceName, EventName };

    explicit ManagerService(ServiceManager &manager);

    static constexpr std::string_view get_name()
    {
        return "Manager";
    }

    static std::span<const FunctionSchema> get_static_function_schemas();
    static std::span<const EventSchema> get_static_event_schemas();

private:
    std::vector<FunctionSchema> get_function_schemas() override;
    std::vector<EventSchema> get_event_schemas() override;
    FunctionHandlerMap get_function_handlers() override;

    ServiceManager &manager_;
};

BROOKESIA_DESCRIBE_ENUM(ManagerService::ServiceState, Stopped, Starting, Running, Stopping);
BROOKESIA_DESCRIBE_STRUCT(
    ManagerService::ServiceInfo, (), (name, version, state, reference_count, bindable, dependencies)
);
BROOKESIA_DESCRIBE_STRUCT(
    ManagerService::ServiceSchemaOverview, (), (name, description, function_names, event_names)
);
BROOKESIA_DESCRIBE_ENUM(
    ManagerService::FunctionId,
    GetServiceNames,
    GetServiceInfo,
    GetServiceSchema,
    GetServiceFunctionSchema,
    GetServiceEventSchema,
    Max
);
BROOKESIA_DESCRIBE_ENUM(ManagerService::EventId, Max);
BROOKESIA_DESCRIBE_ENUM(ManagerService::FunctionGetServiceInfoParam, Name);
BROOKESIA_DESCRIBE_ENUM(ManagerService::FunctionGetServiceSchemaParam, Name);
BROOKESIA_DESCRIBE_ENUM(
    ManagerService::FunctionGetServiceFunctionSchemaParam, ServiceName, FunctionName
);
BROOKESIA_DESCRIBE_ENUM(ManagerService::FunctionGetServiceEventSchemaParam, ServiceName, EventName);

} // namespace esp_brookesia::service
