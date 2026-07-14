/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include <optional>
#include <utility>

#include "boost/format.hpp"

#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/service/manager.hpp"
#include "brookesia/service_manager/service/manager_service.hpp"

namespace esp_brookesia::service {
namespace {

using FunctionId = ManagerService::FunctionId;

FunctionSchema make_function_schema(
    FunctionId id, std::string description, std::optional<FunctionValueType> return_type = std::nullopt,
    std::vector<FunctionParameterSchema> parameters = {}
)
{
    FunctionSchema schema{
        .name = BROOKESIA_DESCRIBE_TO_STR(id),
        .description = std::move(description),
        .parameters = std::move(parameters),
        .require_scheduler = false,
    };
    if (return_type) {
        schema.return_value = FunctionReturnSchema{
            .type = *return_type,
            .description = "Function result.",
        };
    }
    return schema;
}

FunctionParameterSchema make_parameter(std::string name, std::string description, FunctionValueType type)
{
    return {
        .name = std::move(name),
        .description = std::move(description),
        .type = type,
    };
}

} // namespace

ManagerService::ManagerService(ServiceManager &manager)
    : ServiceBase({
    .name = get_name().data(),
    .description = "Inspect registered services and their schemas.",
    .version = make_version(
        BROOKESIA_SERVICE_MANAGER_VER_MAJOR,
        BROOKESIA_SERVICE_MANAGER_VER_MINOR,
        BROOKESIA_SERVICE_MANAGER_VER_PATCH
    ),
    .bindable = true,
})
, manager_(manager)
{
}

std::span<const FunctionSchema> ManagerService::get_static_function_schemas()
{
    static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> SCHEMAS = {{
            make_function_schema(
                FunctionId::GetServiceNames, "Get registered service names.", FunctionValueType::Array
            ),
            make_function_schema(
                FunctionId::GetServiceInfo, "Get runtime information for one service.", FunctionValueType::Object,
            {
                make_parameter(
                    BROOKESIA_DESCRIBE_TO_STR(FunctionGetServiceInfoParam::Name), "Service name.",
                    FunctionValueType::String
                )
            }
            ),
            make_function_schema(
                FunctionId::GetServiceSchema, "Get one service schema overview.", FunctionValueType::Object,
            {
                make_parameter(
                    BROOKESIA_DESCRIBE_TO_STR(FunctionGetServiceSchemaParam::Name), "Service name.",
                    FunctionValueType::String
                )
            }
            ),
            make_function_schema(
                FunctionId::GetServiceFunctionSchema, "Get one service function schema.", FunctionValueType::Object,
            {
                make_parameter(
                    BROOKESIA_DESCRIBE_TO_STR(FunctionGetServiceFunctionSchemaParam::ServiceName),
                    "Service name.", FunctionValueType::String
                ),
                make_parameter(
                    BROOKESIA_DESCRIBE_TO_STR(FunctionGetServiceFunctionSchemaParam::FunctionName),
                    "Function name.", FunctionValueType::String
                ),
            }
            ),
            make_function_schema(
                FunctionId::GetServiceEventSchema, "Get one service event schema.", FunctionValueType::Object,
            {
                make_parameter(
                    BROOKESIA_DESCRIBE_TO_STR(FunctionGetServiceEventSchemaParam::ServiceName),
                    "Service name.", FunctionValueType::String
                ),
                make_parameter(
                    BROOKESIA_DESCRIBE_TO_STR(FunctionGetServiceEventSchemaParam::EventName),
                    "Event name.", FunctionValueType::String
                ),
            }
            ),
        }
    };
    return SCHEMAS;
}

std::span<const EventSchema> ManagerService::get_static_event_schemas()
{
    static constexpr std::array<EventSchema, 0> SCHEMAS{};
    return SCHEMAS;
}

std::vector<FunctionSchema> ManagerService::get_function_schemas()
{
    auto schemas = get_static_function_schemas();
    return {schemas.begin(), schemas.end()};
}

std::vector<EventSchema> ManagerService::get_event_schemas()
{
    return {};
}

ServiceBase::FunctionHandlerMap ManagerService::get_function_handlers()
{
    return {
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetServiceNames),
            [this](FunctionParameterMap &&)
            {
                return to_function_result(std::expected<boost::json::array, std::string>(
                                              BROOKESIA_DESCRIBE_TO_JSON(manager_.get_service_names()).as_array()
                                          ));
            },
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetServiceInfo),
            [this](FunctionParameterMap &&args)
            {
                const auto &name = std::get<std::string>(
                    args.at(BROOKESIA_DESCRIBE_TO_STR(FunctionGetServiceInfoParam::Name))
                );
                auto result = manager_.get_service_info(name);
                if (!result) {
                    return to_function_result(std::expected<boost::json::object, std::string>(
                                                  std::unexpected((boost::format("Service not found: %1%") % name).str())
                                              ));
                }
                return to_function_result(std::expected<boost::json::object, std::string>(
                                              BROOKESIA_DESCRIBE_TO_JSON(*result).as_object()
                                          ));
            },
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetServiceSchema),
            [this](FunctionParameterMap &&args)
            {
                const auto &name = std::get<std::string>(
                    args.at(BROOKESIA_DESCRIBE_TO_STR(FunctionGetServiceSchemaParam::Name))
                );
                auto result = manager_.get_service_schema(name);
                if (!result) {
                    return to_function_result(std::expected<boost::json::object, std::string>(
                                                  std::unexpected((boost::format("Service not found: %1%") % name).str())
                                              ));
                }
                return to_function_result(std::expected<boost::json::object, std::string>(
                                              BROOKESIA_DESCRIBE_TO_JSON(*result).as_object()
                                          ));
            },
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetServiceFunctionSchema),
            [this](FunctionParameterMap &&args)
            {
                const auto &service_name = std::get<std::string>(args.at(
                            BROOKESIA_DESCRIBE_TO_STR(FunctionGetServiceFunctionSchemaParam::ServiceName)
                        ));
                const auto &function_name = std::get<std::string>(args.at(
                            BROOKESIA_DESCRIBE_TO_STR(FunctionGetServiceFunctionSchemaParam::FunctionName)
                        ));
                auto result = manager_.get_service_function_schema(service_name, function_name);
                if (!result) {
                    auto kind = manager_.get_service_info(service_name) ? "Function" : "Service";
                    auto name = (kind == std::string_view("Function"))
                    ? service_name + "." + function_name : service_name;
                    return to_function_result(std::expected<boost::json::object, std::string>(
                                                  std::unexpected((boost::format("%1% not found: %2%") % kind % name).str())
                                              ));
                }
                return to_function_result(std::expected<boost::json::object, std::string>(
                                              BROOKESIA_DESCRIBE_TO_JSON(*result).as_object()
                                          ));
            },
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetServiceEventSchema),
            [this](FunctionParameterMap &&args)
            {
                const auto &service_name = std::get<std::string>(args.at(
                            BROOKESIA_DESCRIBE_TO_STR(FunctionGetServiceEventSchemaParam::ServiceName)
                        ));
                const auto &event_name = std::get<std::string>(args.at(
                            BROOKESIA_DESCRIBE_TO_STR(FunctionGetServiceEventSchemaParam::EventName)
                        ));
                auto result = manager_.get_service_event_schema(service_name, event_name);
                if (!result) {
                    auto kind = manager_.get_service_info(service_name) ? "Event" : "Service";
                    auto name = (kind == std::string_view("Event"))
                    ? service_name + "." + event_name : service_name;
                    return to_function_result(std::expected<boost::json::object, std::string>(
                                                  std::unexpected((boost::format("%1% not found: %2%") % kind % name).str())
                                              ));
                }
                return to_function_result(std::expected<boost::json::object, std::string>(
                                              BROOKESIA_DESCRIBE_TO_JSON(*result).as_object()
                                          ));
            },
        },
    };
}

} // namespace esp_brookesia::service
