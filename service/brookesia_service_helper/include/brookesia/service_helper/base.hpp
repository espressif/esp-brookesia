/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <string_view>
#include <expected>
#include <type_traits>
#include <future>
#include <memory>
#include "boost/format.hpp"
#include "brookesia/service_manager/function/definition.hpp"
#include "brookesia/service_manager/event/definition.hpp"
#include "brookesia/service_manager/service/manager.hpp"

namespace esp_brookesia::service::helper {

// Forward declaration helper to allow CRTP pattern
template <typename T>
concept DerivedMeta = requires {
    typename T::FunctionId;
    std::is_enum_v<typename T::FunctionId>;
    typename T::EventId;
    std::is_enum_v<typename T::EventId>;
    T::get_name();
    T::get_function_schemas();
    T::get_event_schemas();
};

/**
 * @brief Base class for all service helpers (CRTP)
 *
 * @tparam Derived The derived class (must be a subclass of Base)
 *
 * Note: Concept check is removed from template parameter to allow CRTP pattern
 * where Derived is incomplete. Type and method checks are performed when methods
 * are actually used via static_assert or compiler errors.
 */
template <typename Derived>
class Base {
public:
    static std::string_view get_name()
    {
        static_assert(DerivedMeta<Derived>, "Derived must satisfy DerivedMeta concept");
        return Derived::get_name();
    }

    template <typename FunctionIdType>
    static const FunctionSchema *get_function_schema(FunctionIdType function_id)
    {
        static_assert(DerivedMeta<Derived>, "Derived must satisfy DerivedMeta concept");
        static_assert(
            std::is_same_v<FunctionIdType, typename Derived::FunctionId>, "FunctionIdType must be Derived::FunctionId"
        );

        auto function_schemas = Derived::get_function_schemas();
        for (const auto &function_schema : function_schemas) {
            if (function_schema.name == BROOKESIA_DESCRIBE_ENUM_TO_STR(function_id)) {
                return &function_schema;
            }
        }
        auto error_msg = (boost::format("Service [%1%] function schema not found for function_id: %2%") %
                          Derived::get_name() % BROOKESIA_DESCRIBE_ENUM_TO_STR(function_id)).str();
        printf("%s\n", error_msg.c_str());
        return nullptr;
    }

    template <typename EventIdType>
    static const EventSchema *get_event_schema(EventIdType event_id)
    {
        static_assert(DerivedMeta<Derived>, "Derived must satisfy DerivedMeta concept");
        static_assert(
            std::is_same_v<EventIdType, typename Derived::EventId>, "EventIdType must be Derived::EventId"
        );

        auto event_schemas = Derived::get_event_schemas();
        if (BROOKESIA_DESCRIBE_ENUM_TO_NUM(event_id) >= event_schemas.size()) {
            auto error_msg = (boost::format("Service [%1%] event schema index out of range: %2%") %
                              Derived::get_name() % BROOKESIA_DESCRIBE_ENUM_TO_NUM(event_id)).str();
            printf("%s\n", error_msg.c_str());
            return nullptr;
        }
        for (const auto &event_schema : event_schemas) {
            if (event_schema.name == BROOKESIA_DESCRIBE_ENUM_TO_STR(event_id)) {
                return &event_schema;
            }
        }
        auto error_msg = (boost::format("Service [%1%] event schema not found for event_id: %2%") %
                          Derived::get_name() % BROOKESIA_DESCRIBE_ENUM_TO_STR(event_id)).str();
        printf("%s\n", error_msg.c_str());
        return nullptr;
    }

    static bool is_available()
    {
        static_assert(DerivedMeta<Derived>, "Derived must satisfy DerivedMeta concept");
        return (ServiceManager::get_instance().get_service(Derived::get_name().data()) != nullptr);
    }

private:
    using ServiceAndSchema = std::pair<std::shared_ptr<ServiceBase>, const FunctionSchema *>;

    /**
     * @brief Helper function to validate and get service and function schema
     *
     * @param[in] function_id Function ID to get schema for
     * @return std::expected containing pair of (service, function_schema) or error message
     */
    template <typename FunctionIdType>
    static std::expected<ServiceAndSchema, std::string> get_service_and_schema(
        FunctionIdType function_id
    )
    {
        static_assert(DerivedMeta<Derived>, "Derived must satisfy DerivedMeta concept");
        static_assert(
            std::is_same_v<FunctionIdType, typename Derived::FunctionId>,
            "FunctionIdType must be Derived::FunctionId"
        );

        auto service = ServiceManager::get_instance().get_service(Derived::get_name().data());
        if (!service) {
            return std::unexpected("Service not found");
        }
        auto function_schema = get_function_schema(function_id);
        if (!function_schema) {
            return std::unexpected("Function schema not found");
        }
        return ServiceAndSchema{service, function_schema};
    }

    /**
     * @brief Helper function to create a future with error result
     *
     * @param[in] error_msg Error message
     * @return std::future<FunctionResult> Future containing error result
     */
    static std::future<FunctionResult> make_error_future(const std::string &error_msg)
    {
        auto result_promise = std::make_shared<std::promise<FunctionResult>>();
        auto result_future = result_promise->get_future();
        FunctionResult result{
            .success = false,
            .error_message = error_msg,
        };
        result_promise->set_value(std::move(result));
        return result_future;
    }

public:
    static constexpr uint32_t DEFAULT_CALL_TIMEOUT_MS = 100;

    /**
     * @brief Helper function to process function result and convert to expected return type
     *
     * @param[in] result Function result from service call
     * @return std::expected containing ReturnType or error message
     */
    template <typename ReturnType>
    static std::expected<ReturnType, std::string> process_function_result(const FunctionResult &result)
    {
        if (!result.success) {
            return std::unexpected(result.error_message);
        }
        if constexpr (std::is_same_v<ReturnType, void>) {
            return {};
        } else {
            if (!result.has_data()) {
                return std::unexpected("Function result has no data");
            }
            auto data = std::get_if<ReturnType>(&result.data.value());
            if (!data) {
                return std::unexpected("Invalid function result type");
            }
            return *data;
        }
    }

    template <typename ReturnType = void, typename FunctionIdType>
    static std::expected<ReturnType, std::string> call_function_sync(
        FunctionIdType function_id, FunctionParameterMap && parameters_map = {},
        uint32_t timeout_ms = DEFAULT_CALL_TIMEOUT_MS
    )
    {
        auto service_and_schema = get_service_and_schema(function_id);
        if (!service_and_schema) {
            return std::unexpected(service_and_schema.error());
        }
        auto &[service, function_schema] = *service_and_schema;
        auto result = service->call_function_sync(function_schema->name, std::move(parameters_map), timeout_ms);
        return process_function_result<ReturnType>(result);
    }

    template <typename FunctionIdType>
    static std::future<FunctionResult> call_function_async(
        FunctionIdType function_id, FunctionParameterMap &&parameters_map = {}
    )
    {
        auto service_and_schema = get_service_and_schema(function_id);
        if (!service_and_schema) {
            return make_error_future(service_and_schema.error());
        }
        auto &[service, function_schema] = *service_and_schema;
        return service->call_function_async(function_schema->name, std::move(parameters_map));
    }

    template <typename EventIdType>
    static EventRegistry::SignalConnection subscribe_event(
        EventIdType event_id, const EventRegistry::SignalSlot &slot
    )
    {
        static_assert(DerivedMeta<Derived>, "Derived must satisfy DerivedMeta concept");
        static_assert(
            std::is_same_v<EventIdType, typename Derived::EventId>, "EventIdType must be Derived::EventId"
        );

        auto service = ServiceManager::get_instance().get_service(Derived::get_name().data());
        if (!service) {
            return EventRegistry::SignalConnection();
        }
        return service->subscribe_event(BROOKESIA_DESCRIBE_ENUM_TO_STR(event_id), slot);
    }

    /**
     * @brief Helper function to convert std::expected to FunctionResult
     *
     * @tparam T Return value type, can be void or any type convertible to FunctionValue
     * @param[in] result std::expected object
     * @return FunctionResult Converted result
     */
    template<typename T>
    static FunctionResult to_function_result(std::expected<T, std::string> &&result)
    {
        if (result) {
            if constexpr (std::is_void_v<T>) {
                return FunctionResult{.success = true};
            } else {
                return FunctionResult{
                    .success = true,
                    .data = FunctionValue(std::move(result.value()))
                };
            }
        } else {
            return FunctionResult{
                .success = false,
                .error_message = result.error()
            };
        }
    }
};

} // namespace esp_brookesia::service::helper

// ============================================================================
// Helper macros: Simplify FunctionHandlerMap writing
// ============================================================================

/**
 * @brief Create a zero-parameter function handler based on Derived and FunctionId
 *
 * @param Derived The derived class type
 * @param function_id FunctionId enum value
 * @param func_call Actual function call (e.g., function_get_volume())
 *
 * Example:
 * BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(MyService, MyService::FunctionId::GetVolume, function_get_volume())
 */
#define BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Derived, function_id, func_call) \
    [&]() { \
        auto schema = esp_brookesia::service::helper::Base<Derived>::get_function_schema(function_id); \
        std::string func_name = (schema && !schema->name.empty()) ? schema->name : ""; \
        return std::make_pair( \
            std::move(func_name), \
            [this](esp_brookesia::service::FunctionParameterMap &&) -> esp_brookesia::service::FunctionResult { \
                return esp_brookesia::service::helper::Base<Derived>::to_function_result(func_call); \
            } \
        ); \
    }()

/**
 * @brief Create a single-parameter function handler based on Derived and FunctionId
 *
 * @param Derived The derived class type
 * @param function_id FunctionId enum value
 * @param param_type Parameter C++ type (e.g., std::string, double)
 * @param func_call Function call, use PARAM as parameter placeholder
 *
 * Example:
 * BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(MyService, MyService::FunctionId::PlayUrl, std::string, function_play_url(PARAM))
 */
#define BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(Derived, function_id, param_type, func_call) \
    [&]() { \
        auto schema = esp_brookesia::service::helper::Base<Derived>::get_function_schema(function_id); \
        std::string func_name = (schema && !schema->name.empty()) ? schema->name : ""; \
        std::string param_name = (schema && !schema->parameters.empty() && !schema->parameters[0].name.empty()) ? schema->parameters[0].name : ""; \
        return std::make_pair( \
            std::move(func_name), \
            [this, param_name = std::move(param_name)](esp_brookesia::service::FunctionParameterMap &&args) -> esp_brookesia::service::FunctionResult { \
                if (param_name.empty()) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter name is empty" \
                    }; \
                } \
                auto it = args.find(param_name); \
                if (it == args.end()) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter not found: " + param_name \
                    }; \
                } \
                auto *param_ptr = std::get_if<param_type>(&it->second); \
                if (!param_ptr) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter type mismatch for: " + param_name \
                    }; \
                } \
                auto &PARAM = *param_ptr; \
                return esp_brookesia::service::helper::Base<Derived>::to_function_result(func_call); \
            } \
        ); \
    }()

/**
 * @brief Create a two-parameter function handler based on Derived and FunctionId
 *
 * @param Derived The derived class type
 * @param function_id FunctionId enum value
 * @param param1_type First parameter C++ type
 * @param param2_type Second parameter C++ type
 * @param func_call Function call, use PARAM1, PARAM2 as parameter placeholders
 *
 * Example:
 * BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(MyService, MyService::FunctionId::Add, double, double,
 *                     function_add(PARAM1, PARAM2))
 */
#define BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(Derived, function_id, param1_type, param2_type, func_call) \
    [&]() { \
        auto schema = esp_brookesia::service::helper::Base<Derived>::get_function_schema(function_id); \
        std::string func_name = (schema && !schema->name.empty()) ? schema->name : ""; \
        std::string param1_name = (schema && schema->parameters.size() >= 1 && !schema->parameters[0].name.empty()) ? schema->parameters[0].name : ""; \
        std::string param2_name = (schema && schema->parameters.size() >= 2 && !schema->parameters[1].name.empty()) ? schema->parameters[1].name : ""; \
        return std::make_pair( \
            std::move(func_name), \
            [this, param1_name = std::move(param1_name), param2_name = std::move(param2_name)](esp_brookesia::service::FunctionParameterMap &&args) -> esp_brookesia::service::FunctionResult { \
                if (param1_name.empty() || param2_name.empty()) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter name is empty" \
                    }; \
                } \
                auto it1 = args.find(param1_name); \
                if (it1 == args.end()) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter not found: " + param1_name \
                    }; \
                } \
                auto it2 = args.find(param2_name); \
                if (it2 == args.end()) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter not found: " + param2_name \
                    }; \
                } \
                auto *param1_ptr = std::get_if<param1_type>(&it1->second); \
                if (!param1_ptr) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter type mismatch for: " + param1_name \
                    }; \
                } \
                auto *param2_ptr = std::get_if<param2_type>(&it2->second); \
                if (!param2_ptr) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter type mismatch for: " + param2_name \
                    }; \
                } \
                auto &PARAM1 = *param1_ptr; \
                auto &PARAM2 = *param2_ptr; \
                return esp_brookesia::service::helper::Base<Derived>::to_function_result(func_call); \
            } \
        ); \
    }()

/**
 * @brief Create a three-parameter function handler based on Derived and FunctionId
 *
 * @param Derived The derived class type
 * @param function_id FunctionId enum value
 * @param param1_type First parameter C++ type
 * @param param2_type Second parameter C++ type
 * @param param3_type Third parameter C++ type
 * @param func_call Function call, use PARAM1, PARAM2, PARAM3 as parameter placeholders
 *
 * Example:
 * BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_3(MyService, MyService::FunctionId::SetConfig, std::string, int, bool,
 *                     function_set_config(PARAM1, PARAM2, PARAM3))
 */
#define BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_3(Derived, function_id, param1_type, param2_type, param3_type, func_call) \
    [&]() { \
        auto schema = esp_brookesia::service::helper::Base<Derived>::get_function_schema(function_id); \
        std::string func_name = (schema && !schema->name.empty()) ? schema->name : ""; \
        std::string param1_name = (schema && schema->parameters.size() >= 1 && !schema->parameters[0].name.empty()) ? schema->parameters[0].name : ""; \
        std::string param2_name = (schema && schema->parameters.size() >= 2 && !schema->parameters[1].name.empty()) ? schema->parameters[1].name : ""; \
        std::string param3_name = (schema && schema->parameters.size() >= 3 && !schema->parameters[2].name.empty()) ? schema->parameters[2].name : ""; \
        return std::make_pair( \
            std::move(func_name), \
            [this, param1_name = std::move(param1_name), param2_name = std::move(param2_name), param3_name = std::move(param3_name)](esp_brookesia::service::FunctionParameterMap &&args) -> esp_brookesia::service::FunctionResult { \
                if (param1_name.empty() || param2_name.empty() || param3_name.empty()) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter name is empty" \
                    }; \
                } \
                auto it1 = args.find(param1_name); \
                if (it1 == args.end()) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter not found: " + param1_name \
                    }; \
                } \
                auto it2 = args.find(param2_name); \
                if (it2 == args.end()) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter not found: " + param2_name \
                    }; \
                } \
                auto it3 = args.find(param3_name); \
                if (it3 == args.end()) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter not found: " + param3_name \
                    }; \
                } \
                auto *param1_ptr = std::get_if<param1_type>(&it1->second); \
                if (!param1_ptr) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter type mismatch for: " + param1_name \
                    }; \
                } \
                auto *param2_ptr = std::get_if<param2_type>(&it2->second); \
                if (!param2_ptr) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter type mismatch for: " + param2_name \
                    }; \
                } \
                auto *param3_ptr = std::get_if<param3_type>(&it3->second); \
                if (!param3_ptr) { \
                    return esp_brookesia::service::FunctionResult{ \
                        .success = false, \
                        .error_message = "Parameter type mismatch for: " + param3_name \
                    }; \
                } \
                auto &PARAM1 = *param1_ptr; \
                auto &PARAM2 = *param2_ptr; \
                auto &PARAM3 = *param3_ptr; \
                return esp_brookesia::service::helper::Base<Derived>::to_function_result(func_call); \
            } \
        ); \
    }()
