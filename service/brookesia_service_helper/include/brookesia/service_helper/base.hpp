/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <string_view>
#include <expected>
#include <optional>
#include <type_traits>
#include <future>
#include <memory>
#include <tuple>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <chrono>
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

// Tag type for specifying timeout at the end of parameter list
struct Timeout {
    uint32_t value;
    constexpr explicit Timeout(uint32_t v) : value(v) {}
};

// Concept to check if a type is Timeout
template<typename T>
concept IsTimeout = std::is_same_v<std::decay_t<T>, Timeout>;

// Concept to check if a type can be used as FunctionResultHandler (e.g. std::function or lambda)
template<typename T>
concept IsFunctionResultHandler =
    std::is_constructible_v<ServiceBase::FunctionResultHandler, std::decay_t<T>>;

// Helper to create a fixed string
template <size_t N>
struct FixedString {
    char data[N];

    constexpr FixedString(const char (&str)[N])
    {
        std::copy_n(str, N, data);
    }
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

    static bool is_running()
    {
        static_assert(DerivedMeta<Derived>, "Derived must satisfy DerivedMeta concept");
        return is_available() && ServiceManager::get_instance().get_service(Derived::get_name().data())->is_running();
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

    // Concept to check if a type is string-like (for event_name parameter)
    template<typename T>
    static constexpr bool is_string_type_v =
        std::is_same_v<std::decay_t<T>, std::string> ||
        std::is_same_v<std::decay_t<T>, std::string_view> ||
        std::is_convertible_v<T, std::string>;

    // Helper to extract first parameter type from callable
    template<typename T>
    struct first_param_type;

    // Function pointer specialization
    template<typename Ret, typename First, typename... Args>
    struct first_param_type<Ret(*)(First, Args...)> {
        using type = First;
    };

    // Const member function specialization
    template<typename Ret, typename Class, typename First, typename... Args>
    struct first_param_type<Ret(Class::*)(First, Args...) const> {
        using type = First;
    };

    // Non-const member function specialization
    template<typename Ret, typename Class, typename First, typename... Args>
    struct first_param_type<Ret(Class::*)(First, Args...)> {
        using type = First;
    };

    // Helper to check if Callable's first parameter is event_name (string type)
    template<typename Callable, typename = void>
    struct has_event_name_first_param_impl : std::false_type {};

    // Specialization for types where we can get operator() or function type
    template<typename Callable>
    struct has_event_name_first_param_impl<Callable,
           std::void_t<typename first_param_type<decltype(&std::decay_t<Callable>::operator())>::type>>
       : std::bool_constant<is_string_type_v<typename first_param_type<decltype(&std::decay_t<Callable>::operator())>::type>> {};

    // Specialization for function pointers
    template<typename Ret, typename First, typename... Args>
    struct has_event_name_first_param_impl<Ret(*)(First, Args...), void>
: std::bool_constant<is_string_type_v<First>> {};

    template<typename Callable>
    static constexpr bool has_event_name_first_param_v = has_event_name_first_param_impl<std::decay_t<Callable>>::value;

    // Helper to extract second parameter type from callable
    template<typename T>
    struct second_param_type;

    // Function pointer specialization (with at least 2 parameters)
    template<typename Ret, typename First, typename Second, typename... Args>
    struct second_param_type<Ret(*)(First, Second, Args...)> {
        using type = Second;
    };

    // Const member function specialization (with at least 2 parameters)
    template<typename Ret, typename Class, typename First, typename Second, typename... Args>
    struct second_param_type<Ret(Class::*)(First, Second, Args...) const> {
        using type = Second;
    };

    // Non-const member function specialization (with at least 2 parameters)
    template<typename Ret, typename Class, typename First, typename Second, typename... Args>
    struct second_param_type<Ret(Class::*)(First, Second, Args...)> {
        using type = Second;
    };

    // Helper to check if Callable's second parameter is EventItemMap
    template<typename Callable, typename = void>
    struct has_event_items_second_param_impl : std::false_type {};

    // Specialization for types where we can get operator() or function type with at least 2 parameters
    template<typename Callable>
    struct has_event_items_second_param_impl<Callable,
           std::void_t<typename second_param_type<decltype(&std::decay_t<Callable>::operator())>::type>>
       : std::bool_constant<std::is_same_v<std::decay_t<typename second_param_type<decltype(&std::decay_t<Callable>::operator())>::type>, EventItemMap>> {};

    // Specialization for function pointers with at least 2 parameters
    template<typename Ret, typename First, typename Second, typename... Args>
    struct has_event_items_second_param_impl<Ret(*)(First, Second, Args...), void>
: std::bool_constant<std::is_same_v<std::decay_t<Second>, EventItemMap>> {};

    template<typename Callable>
    static constexpr bool has_event_items_second_param_v = has_event_items_second_param_impl<std::decay_t<Callable>>::value;

    // Helper to get parameter count from callable
    template<typename T>
    struct param_count;

    // Function pointer specialization
    template<typename Ret, typename... Args>
    struct param_count<Ret(*)(Args...)> {
        static constexpr size_t value = sizeof...(Args);
    };

    // Const member function specialization
    template<typename Ret, typename Class, typename... Args>
    struct param_count<Ret(Class::*)(Args...) const> {
        static constexpr size_t value = sizeof...(Args);
    };

    // Non-const member function specialization
    template<typename Ret, typename Class, typename... Args>
    struct param_count<Ret(Class::*)(Args...)> {
        static constexpr size_t value = sizeof...(Args);
    };

    // Helper to get parameter count from callable (lambda, functor, function)
    template<typename Callable, typename = void>
    struct callable_param_count_impl {
        static constexpr size_t value = 0;
    };

    // Specialization for types with operator()
    template<typename Callable>
    struct callable_param_count_impl<Callable,
           std::void_t<decltype(&std::decay_t<Callable>::operator())>> {
               static constexpr size_t value = param_count<decltype(&std::decay_t<Callable>::operator())>::value;
           };

    // Specialization for function pointers
    template<typename Ret, typename... Args>
    struct callable_param_count_impl<Ret(*)(Args...), void> {
        static constexpr size_t value = sizeof...(Args);
    };

    template<typename Callable>
    static constexpr size_t callable_param_count_v = callable_param_count_impl<std::decay_t<Callable>>::value;

    // Helper to get EventItemType from C++ type
    template<typename T>
    static constexpr EventItemType get_event_item_type_for_cpp_type()
    {
        using DecayedT = std::decay_t<T>;
        if constexpr (std::is_same_v<DecayedT, bool>) {
            return EventItemType::Boolean;
        } else if constexpr (std::is_arithmetic_v<DecayedT>) {
            return EventItemType::Number;
        } else if constexpr (std::is_same_v<DecayedT, std::string> ||
                             std::is_same_v<DecayedT, std::string_view> ||
                             std::is_convertible_v<DecayedT, std::string>) {
            return EventItemType::String;
        } else if constexpr (std::is_same_v<DecayedT, boost::json::object>) {
            return EventItemType::Object;
        } else if constexpr (std::is_same_v<DecayedT, boost::json::array>) {
            return EventItemType::Array;
        } else if constexpr (std::is_same_v<DecayedT, RawBuffer>) {
            return EventItemType::RawBuffer;
        } else if constexpr (std::is_same_v<DecayedT, EventItem>) {
            // EventItem can match any type, so we'll handle this specially
            return EventItemType::String;  // Placeholder, will be checked separately
        } else {
            // Unknown type - will fail at runtime
            return EventItemType::String;
        }
    }

    // Helper to check if a C++ type can be converted from EventItemType
    template<typename T>
    static bool is_compatible_with_event_item_type(EventItemType schema_type)
    {
        using DecayedT = std::decay_t<T>;

        // EventItem can accept any type
        if constexpr (std::is_same_v<DecayedT, EventItem>) {
            return true;
        }

        switch (schema_type) {
        case EventItemType::Boolean:
            return std::is_same_v<DecayedT, bool>;
        case EventItemType::Number:
            return std::is_arithmetic_v<DecayedT> &&
                   !std::is_same_v<DecayedT, bool>;
        case EventItemType::String:
            return std::is_same_v<DecayedT, std::string> ||
                   std::is_same_v<DecayedT, std::string_view> ||
                   std::is_convertible_v<DecayedT, std::string>;
        case EventItemType::Object:
            return std::is_same_v<DecayedT, boost::json::object>;
        case EventItemType::Array:
            return std::is_same_v<DecayedT, boost::json::array>;
        case EventItemType::RawBuffer:
            return std::is_same_v<DecayedT, RawBuffer>;
        default:
            return false;
        }
    }

    // Helper to validate callable parameters against event schema
    template<typename... Args>
    static bool validate_params_against_schema(const EventSchema *schema, std::string &error_msg)
    {
        if (!schema) {
            error_msg = "Invalid event schema";
            return false;
        }

        constexpr size_t param_count = sizeof...(Args);
        const size_t schema_items_count = schema->items.size();

        if (param_count != schema_items_count) {
            error_msg = (boost::format(
                             "Parameter count mismatch: callback has %1% parameters (excluding event_name), "
                             "but event schema defines %2% items"
                         ) % param_count % schema_items_count).str();
            return false;
        }

        // Check each parameter type
        bool all_compatible = true;
        size_t param_index = 0;

        // Fold expression to check each parameter type
        ([&]<typename T>() {
            if (param_index < schema_items_count) {
                const auto &item_schema = schema->items[param_index];
                if (!is_compatible_with_event_item_type<T>(item_schema.type)) {
                    error_msg = (boost::format(
                                     "Parameter type mismatch at index %1% ('%2%'): expected type %3%, "
                                     "but callback parameter type is not compatible"
                                 ) % param_index % item_schema.name % BROOKESIA_DESCRIBE_ENUM_TO_STR(item_schema.type)).str();
                    all_compatible = false;
                }
                param_index++;
            }
        } .template operator()<Args>(), ...);

        return all_compatible;
    }

    // Helper to extract EventItem value with type conversion
    template<typename T>
    static T extract_event_item(const EventItem &item)
    {
        using DecayedT = std::decay_t<T>;

        if constexpr (std::is_same_v<DecayedT, EventItem>) {
            // Direct EventItem access
            return item;
        } else if (auto *val = std::get_if<DecayedT>(&item)) {
            // Direct type match
            return *val;
        } else {
            // Type mismatch - throw exception with helpful message
            throw std::runtime_error("Event item type mismatch");
        }
    }

    // Helper to extract parameters from EventItemMap by schema order
    template<typename... Args, std::size_t... Is>
    static std::tuple<std::decay_t<Args>...> extract_parameters_impl(
        const EventSchema *schema,
        const EventItemMap &items,
        std::index_sequence<Is...>
    )
    {
        if (!schema || schema->items.size() < sizeof...(Args)) {
            throw std::runtime_error("Event schema mismatch");
        }

        return std::make_tuple(
                   extract_event_item<std::decay_t<Args>>(items.at(schema->items[Is].name))...
               );
    }

    template<typename... Args>
    static std::tuple<std::decay_t<Args>...> extract_parameters(
        const EventSchema *schema,
        const EventItemMap &items
    )
    {
        return extract_parameters_impl<Args...>(schema, items, std::index_sequence_for<Args...> {});
    }

    // Helper to extract parameters with event_name as first parameter
    // FirstArg should be string type (event_name), RestArgs are from schema
    template<typename FirstArg, typename... RestArgs>
    static auto extract_parameters_with_event_name(
        const std::string &event_name,
        const EventSchema *schema,
        const EventItemMap &items
    )
    {
        // First parameter is always event_name
        if constexpr (sizeof...(RestArgs) == 0) {
            // Only event_name parameter
            return std::make_tuple(event_name);
        } else {
            // event_name + rest from items
            auto rest_params = extract_parameters<RestArgs...>(schema, items);
            return std::tuple_cat(std::make_tuple(event_name), rest_params);
        }
    }

    // Helper to skip first type in parameter pack
    template<typename First, typename... Rest>
    static bool validate_params_skip_first(const EventSchema *schema, std::string &error_msg)
    {
        return validate_params_against_schema<Rest...>(schema, error_msg);
    }

    // Implementation for function pointers
    // First parameter (Args[0]) is always event_name (string type)
    // Rest parameters (Args[1]...) are from event schema
    template <typename EventIdType, typename ReturnType, typename... Args>
    static EventRegistry::SignalConnection subscribe_event_impl(
        EventIdType event_id, const EventSchema *schema, ReturnType(*callback)(Args...)
    )
    {
        static_assert(sizeof...(Args) >= 1, "Callback must have at least event_name parameter");
        static_assert((ConvertibleToEventItem<Args> &&...),
                      "All callback parameter types must be convertible to/from EventItem");

        // Validate parameter types (skip first parameter which is event_name)
        std::string error_msg;
        if (!validate_params_skip_first<Args...>(schema, error_msg)) {
            error_msg = (boost::format("Event [%1%] parameter type validation failed: %2%") %
                         BROOKESIA_DESCRIBE_ENUM_TO_STR(event_id) % error_msg).str();
            printf("%s\n", error_msg.c_str());
            return EventRegistry::SignalConnection();
        }

        auto adapter = [schema, callback](const std::string & event_name, const EventItemMap & items) {
            try {
                // First parameter is always event_name, rest are from items
                auto params = extract_parameters_with_event_name<Args...>(event_name, schema, items);
                std::apply(callback, params);
            } catch (const std::exception &e) {
                // Handle parameter extraction errors
                // Could log error or silently ignore
                auto error_msg = (boost::format("Event [%1%] exception: %2%") % event_name % e.what()).str();
                printf("%s\n", error_msg.c_str());
            }
        };

        auto service = ServiceManager::get_instance().get_service(Derived::get_name().data());
        if (!service) {
            return EventRegistry::SignalConnection();
        }
        return service->subscribe_event(BROOKESIA_DESCRIBE_ENUM_TO_STR(event_id), adapter);
    }

    // Implementation for lambdas and functors
    template <typename EventIdType, typename Callable>
    static EventRegistry::SignalConnection subscribe_event_impl(
        EventIdType event_id, const EventSchema *schema, Callable &&callback
    )
    {
        using CallableType = std::decay_t<Callable>;

        // Check if it's a lambda or functor with operator()
        if constexpr (requires { &CallableType::operator(); }) {
            return subscribe_event_lambda_impl(event_id, schema, std::forward<Callable>(callback),
                                               &CallableType::operator());
        } else {
            // Fallback: assume it's a function pointer or std::function
            return subscribe_event_impl(event_id, schema, +callback);
        }
    }

    // Helper for lambdas with const operator()
    // First parameter (Args[0]) is always event_name (string type)
    // Rest parameters (Args[1]...) are from event schema
    template <typename EventIdType, typename Callable, typename ReturnType, typename Class, typename... Args>
    static EventRegistry::SignalConnection subscribe_event_lambda_impl(
        EventIdType event_id, const EventSchema *schema, Callable &&callback,
        ReturnType(Class::*)(Args...) const
    )
    {
        static_assert(sizeof...(Args) >= 1, "Callback must have at least event_name parameter");
        static_assert((ConvertibleToEventItem<Args> &&...),
                      "All callback parameter types must be convertible to/from EventItem");

        // Validate parameter types (skip first parameter which is event_name)
        std::string error_msg;
        if (!validate_params_skip_first<Args...>(schema, error_msg)) {
            error_msg = (boost::format("Event [%1%] parameter type validation failed: %2%") %
                         BROOKESIA_DESCRIBE_ENUM_TO_STR(event_id) % error_msg).str();
            printf("%s\n", error_msg.c_str());
            return EventRegistry::SignalConnection();
        }

        auto adapter = [schema, cb = std::forward<Callable>(callback)](
        const std::string & event_name, const EventItemMap & items) mutable {
            try
            {
                // First parameter is always event_name, rest are from items
                auto params = extract_parameters_with_event_name<Args...>(event_name, schema, items);
                std::apply(cb, params);
            } catch (const std::exception &e)
            {
                // Handle parameter extraction errors
                auto error_msg = (boost::format("Event [%1%] exception: %2%") % event_name % e.what()).str();
                printf("%s\n", error_msg.c_str());
            }
        };

        auto service = ServiceManager::get_instance().get_service(Derived::get_name().data());
        if (!service) {
            return EventRegistry::SignalConnection();
        }
        return service->subscribe_event(BROOKESIA_DESCRIBE_ENUM_TO_STR(event_id), adapter);
    }

    // Helper for non-const lambdas
    // First parameter (Args[0]) is always event_name (string type)
    // Rest parameters (Args[1]...) are from event schema
    template <typename EventIdType, typename Callable, typename ReturnType, typename Class, typename... Args>
    static EventRegistry::SignalConnection subscribe_event_lambda_impl(
        EventIdType event_id, const EventSchema *schema, Callable &&callback,
        ReturnType(Class::*)(Args...)
    )
    {
        static_assert(sizeof...(Args) >= 1, "Callback must have at least event_name parameter");
        static_assert((ConvertibleToEventItem<Args> &&...),
                      "All callback parameter types must be convertible to/from EventItem");

        // Validate parameter types (skip first parameter which is event_name)
        std::string error_msg;
        if (!validate_params_skip_first<Args...>(schema, error_msg)) {
            error_msg = (boost::format("Event [%1%] parameter type validation failed: %2%") %
                         BROOKESIA_DESCRIBE_ENUM_TO_STR(event_id) % error_msg).str();
            printf("%s\n", error_msg.c_str());
            return EventRegistry::SignalConnection();
        }

        auto adapter = [schema, cb = std::forward<Callable>(callback)](
        const std::string & event_name, const EventItemMap & items) mutable {
            try
            {
                // First parameter is always event_name, rest are from items
                auto params = extract_parameters_with_event_name<Args...>(event_name, schema, items);
                std::apply(cb, params);
            } catch (const std::exception &e)
            {
                // Handle parameter extraction errors
                auto error_msg = (boost::format("Event [%1%] exception: %2%") % event_name % e.what()).str();
                printf("%s\n", error_msg.c_str());
            }
        };

        auto service = ServiceManager::get_instance().get_service(Derived::get_name().data());
        if (!service) {
            return EventRegistry::SignalConnection();
        }
        return service->subscribe_event(BROOKESIA_DESCRIBE_ENUM_TO_STR(event_id), adapter);
    }

public:
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

    /**
     * @brief Call a function synchronously with variadic arguments (timeout at end)
     *
     * @tparam ReturnType Expected return type (default: void)
     * @tparam FunctionIdType Type of the function identifier (enum)
     * @tparam Args Types of function arguments (must be convertible to FunctionValue)
     * @param function_id Function identifier
     * @param args Function arguments in order, last argument can be Timeout(ms)
     * @return std::expected<ReturnType, std::string> Function result or error
     *
     * @note Arguments are packed into std::vector<FunctionValue> in the order provided
     * @note Last argument can be Timeout(milliseconds) to specify custom timeout
     * @note All non-Timeout arguments must satisfy `ConvertibleToFunctionValue` concept
     *
     * @code{.cpp}
     * // No arguments with default timeout
     * auto result = call_function_sync<int>(FunctionId::GetValue);
     *
     * // With arguments and default timeout
     * auto result = call_function_sync<int>(FunctionId::Add, 1.0, 2.0);
     *
     * // With custom timeout at the end
     * auto result = call_function_sync<int>(FunctionId::Add, 1.0, 2.0, Timeout(500));
     * @endcode
     */
    template <typename ReturnType = void, typename FunctionIdType, typename... Args>
    static std::expected<ReturnType, std::string> call_function_sync(
        FunctionIdType function_id, Args && ... args
    )
    {
        if constexpr (sizeof...(Args) == 0) {
            // No arguments, use default timeout with empty parameter list
            auto service_and_schema = get_service_and_schema(function_id);
            if (!service_and_schema) {
                return std::unexpected(service_and_schema.error());
            }
            auto &[service, function_schema] = *service_and_schema;

            auto result = service->call_function_sync(function_schema->name, FunctionParameterMap{});
            return process_function_result<ReturnType>(result);
        } else {
            // Check if last argument is Timeout
            constexpr bool has_timeout = IsTimeout < std::tuple_element_t < sizeof...(Args) - 1, std::tuple<Args... >>>;

            if constexpr (has_timeout) {
                // Extract timeout from last argument and forward the rest
                // Note: explicit return type needed because std::unexpected and std::expected are different types
                return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> std::expected<ReturnType, std::string> {
                    auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);
                    uint32_t timeout_ms = std::get < sizeof...(Args) - 1 > (args_tuple).value;

                    auto service_and_schema = get_service_and_schema(function_id);
                    if (!service_and_schema)
                    {
                        return std::unexpected(service_and_schema.error());
                    }
                    auto &[service, function_schema] = *service_and_schema;

                    // Pack arguments into std::vector<FunctionValue> (excluding last Timeout)
                    std::vector<FunctionValue> parameters_values;
                    parameters_values.reserve(sizeof...(Is));
                    (parameters_values.emplace_back(std::get<Is>(std::move(args_tuple))), ...);

                    auto result = service->call_function_sync(function_schema->name, std::move(parameters_values), timeout_ms);
                    return process_function_result<ReturnType>(result);
                }(std::make_index_sequence < sizeof...(Args) - 1 > {});
            } else {
                // No timeout specified, use default
                static_assert(
                    (ConvertibleToFunctionValue<Args> &&...), "All arguments must be convertible to FunctionValue"
                );

                auto service_and_schema = get_service_and_schema(function_id);
                if (!service_and_schema) {
                    return std::unexpected(service_and_schema.error());
                }
                auto &[service, function_schema] = *service_and_schema;

                // Pack arguments into std::vector<FunctionValue>
                std::vector<FunctionValue> parameters_values;
                parameters_values.reserve(sizeof...(Args));
                (parameters_values.emplace_back(std::forward<Args>(args)), ...);

                auto result = service->call_function_sync(function_schema->name, std::move(parameters_values));
                return process_function_result<ReturnType>(result);
            }
        }
    }

    /**
     * @brief Call a function asynchronously with variadic arguments
     *
     * @tparam FunctionIdType Type of the function identifier (enum)
     * @tparam Args Types of function arguments (must be convertible to FunctionValue, optionally with
     *              FunctionResultHandler at the end)
     * @param function_id Function identifier
     * @param args Function arguments in order, optionally with FunctionResultHandler at the end
     * @return true if the call was successfully submitted, false otherwise
     *
     * @note Arguments are packed into std::vector<FunctionValue> in the order provided
     * @note All arguments (except optional FunctionResultHandler) must satisfy ConvertibleToFunctionValue concept
     * @note This overload is not used when the first argument is FunctionParameterMap or boost::json::object
     *
     * @code{.cpp}
     * // No arguments
     * call_function_async(FunctionId::GetValue);
     *
     * // With arguments
     * call_function_async(FunctionId::Add, 1.0, 2.0);
     *
     * // With handler
     * call_function_async(FunctionId::Add, 1.0, 2.0, [](FunctionResult &&result) {
     *     // Handle result
     * });
     * @endcode
     */
    template <typename FunctionIdType, typename... Args>
    static bool call_function_async(FunctionIdType function_id, Args &&... args)
    {
        if constexpr (sizeof...(Args) == 0) {
            // No arguments, use empty parameter list
            auto service_and_schema = get_service_and_schema(function_id);
            if (!service_and_schema) {
                return false;
            }
            auto &[service, function_schema] = *service_and_schema;

            return service->call_function_async(function_schema->name, FunctionParameterMap{});
        } else {
            // Check if last argument is FunctionResultHandler
            constexpr bool has_handler = IsFunctionResultHandler < std::tuple_element_t < sizeof...(Args) - 1, std::tuple<Args... >>>;

            if constexpr (has_handler) {
                // Extract handler from last argument and forward the rest
                return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> bool {
                    auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);
                    auto handler = std::get < sizeof...(Args) - 1 > (args_tuple);

                    auto service_and_schema = get_service_and_schema(function_id);
                    if (!service_and_schema)
                    {
                        return false;
                    }
                    auto &[service, function_schema] = *service_and_schema;

                    if constexpr (sizeof...(Is) == 0)
                    {
                        // Only handler, no parameters
                        return service->call_function_async(function_schema->name, FunctionParameterMap{}, handler);
                    } else
                    {
                        // Pack arguments into std::vector<FunctionValue> (excluding last handler)
                        std::vector<FunctionValue> parameters_values;
                        parameters_values.reserve(sizeof...(Is));
                        (parameters_values.emplace_back(std::get<Is>(std::move(args_tuple))), ...);

                        return service->call_function_async(function_schema->name, std::move(parameters_values), handler);
                    }
                }(std::make_index_sequence < sizeof...(Args) - 1 > {});
            } else {
                // No handler specified
                static_assert(
                    (ConvertibleToFunctionValue<Args> &&...), "All arguments must be convertible to FunctionValue"
                );

                auto service_and_schema = get_service_and_schema(function_id);
                if (!service_and_schema) {
                    return false;
                }
                auto &[service, function_schema] = *service_and_schema;

                // Pack arguments into std::vector<FunctionValue>
                std::vector<FunctionValue> parameters_values;
                parameters_values.reserve(sizeof...(Args));
                (parameters_values.emplace_back(std::forward<Args>(args)), ...);

                return service->call_function_async(function_schema->name, std::move(parameters_values), nullptr);
            }
        }
    }

    /**
     * @brief Subscribe to an event with a raw SignalSlot
     *
     * @tparam EventIdType Type of the event identifier (enum)
     * @param event_id Event identifier
     * @param slot Signal slot function with signature: void(const std::string &event_name, const EventItemMap &event_items)
     * @return EventRegistry::SignalConnection Connection object (scoped, automatically unsubscribes on destruction)
     *
     * @code{.cpp}
     * auto conn = subscribe_event(EventId::ValueChanged,
     *     [](const std::string &event_name, const EventItemMap &items) {
     *         // Handle event with raw items directly
     *     });
     * @endcode
     */
    template <typename EventIdType>
    static EventRegistry::SignalConnection subscribe_event(EventIdType event_id, EventRegistry::SignalSlot slot)
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
     * @brief Subscribe to an event with automatic parameter extraction
     *
     * @tparam EventIdType Type of the event identifier (enum)
     * @tparam Callable Type of the callable object (lambda, function, std::function, etc.)
     * Event identifier is passed as `event_id`.
     * Callable object is passed as `callback`, with first parameter as event_name and remaining parameters
     * matching the event schema order.
     * @return EventRegistry::SignalConnection Connection object (scoped, automatically unsubscribes on destruction)
     *
     * @note REQUIRED: First parameter must be `std::string` or `std::string_view` to receive `event_name`.
     * @note Second parameter must not be `EventItemMap`; use the other overload for that case.
     * @note Remaining parameters are extracted from `EventItemMap` based on the event-schema order.
     * @note All non-`event_name` parameter types must satisfy the `ConvertibleToEventItem` concept.
     * @note Parameter types must match the actual `EventItem` types in the map.
     *
     * @code{.cpp}
     * // Event with two schema parameters: (string name, double value)
     * auto conn = subscribe_event(EventId::ValueChanged,
     *     [](const std::string &event_name, const std::string &name, double value) {
     *         std::cout << event_name << ": " << name << " = " << value << std::endl;
     *     });
     *
     * // Event with no schema parameters, only event_name
     * auto conn = subscribe_event(EventId::Ready,
     *     [](const std::string &event_name) {
     *         std::cout << event_name << " triggered!" << std::endl;
     *     });
     *
     * // Using string_view for event_name
     * auto conn = subscribe_event(EventId::StatusChanged,
     *     [](std::string_view event_name, bool status) {
     *         std::cout << event_name << ": " << (status ? "ON" : "OFF") << std::endl;
     *     });
     * @endcode
     */
    template <typename EventIdType, typename Callable>
    requires (has_event_name_first_param_v<Callable> &&
              !has_event_items_second_param_v<Callable>)
    static EventRegistry::SignalConnection subscribe_event(
        EventIdType event_id, Callable &&callback
    )
    {
        static_assert(DerivedMeta<Derived>, "Derived must satisfy DerivedMeta concept");
        static_assert(
            std::is_same_v<EventIdType, typename Derived::EventId>, "EventIdType must be Derived::EventId"
        );

        // Get event schema to know parameter order
        auto event_schema = Derived::get_event_schema(event_id);
        if (!event_schema) {
            return EventRegistry::SignalConnection();
        }

        // Validate parameter count: callable params (minus event_name) should match schema items
        constexpr size_t callable_params = callable_param_count_v<Callable>;
        constexpr size_t expected_params_with_event_name = 1;  // event_name is mandatory

        // Check if parameter count matches (callable_params should be schema_items + 1 for event_name)
        const size_t schema_items_count = event_schema->items.size();
        const size_t expected_callable_params = schema_items_count + expected_params_with_event_name;

        if (callable_params != expected_callable_params) {
            auto error_msg = (boost::format(
                                  "Event [%1%] parameter count mismatch: callback has %2% parameters "
                                  "(including event_name), but event schema defines %3% items. Expected %4% total parameters."
                              ) % BROOKESIA_DESCRIBE_ENUM_TO_STR(event_id) % callable_params % schema_items_count %
                              expected_callable_params).str();
            printf("%s\n", error_msg.c_str());
            return EventRegistry::SignalConnection();
        }

        // Create adapter that extracts parameters and calls user callback
        return subscribe_event_impl(event_id, event_schema, std::forward<Callable>(callback));
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

    /**
     * @brief Event monitor for monitoring and waiting for specific events
     *
     * @tparam EventIdValue The specific EventId enum value to monitor
     *
     * @code{.cpp}
     * // Create a monitor for WiFi GeneralEventHappened
     * WifiHelper::EventMonitor<WifiHelper::EventId::GeneralEventHappened> monitor;
     * monitor.start();
     * // ... trigger some action ...
     * bool got_event = monitor.wait_for(std::vector<service::EventItem>{"Connected", false}, 5000);
     * monitor.stop();
     * @endcode
     */
    template<auto EventIdValue>
    requires std::is_same_v<decltype(EventIdValue), typename Derived::EventId>
    class EventMonitor {
    public:
        using ReceivedItmes = std::vector<EventItem>;

        EventMonitor() = default;
        ~EventMonitor() = default;

        EventMonitor(const EventMonitor &) = delete;
        EventMonitor &operator=(const EventMonitor &) = delete;
        EventMonitor(EventMonitor &&) = default;
        EventMonitor &operator=(EventMonitor &&) = default;

        /**
         * @brief Start monitoring for events
         * @return true if successfully started monitoring, false if already monitoring or failed
         */
        bool start()
        {
            if (is_running()) {
                return true;
            }

            std::lock_guard<std::mutex> lock(mutex_);
            connection_ = subscribe_event(EventIdValue,
            [this](const std::string & event_name, const EventItemMap & items) {
                std::lock_guard<std::mutex> lock(mutex_);
                ReceivedItmes event;
                event.reserve(items.size());
                for (const auto &[name, item] : items) {
                    event.push_back(item);
                }
                received_items_.push_back(std::move(event));
                cv_.notify_all();
            });

            return connection_.connected();
        }

        /**
         * @brief Stop monitoring for events
         */
        void stop()
        {
            if (!is_running()) {
                return;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                connection_.disconnect();
            }

            clear();
        }

        /**
         * @brief Clear all received events
         */
        void clear()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            received_items_.clear();
        }

        /**
         * @brief Wait for an event containing specific items
         *
         * @param expected_items The expected items to find
         * @param timeout_ms Maximum time to wait in milliseconds
         * @return true if matching items were received, false on timeout
         */
        bool wait_for(const ReceivedItmes &expected_items, uint32_t timeout_ms)
        {
            if (!is_running()) {
                return false;
            }

            std::unique_lock<std::mutex> lock(mutex_);

            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

            while (true) {
                for (const auto &items : received_items_) {
                    if (items == expected_items) {
                        return true;
                    }
                }

                if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
                    return false;
                }
            }
        }

        /**
         * @brief Wait for any event to be received within a timeout period
         *
         * @param timeout_ms Maximum time to wait in milliseconds
         * @return true if any event was received, false on timeout
         */
        bool wait_for_any(uint32_t timeout_ms)
        {
            std::unique_lock<std::mutex> lock(mutex_);

            if (!received_items_.empty()) {
                return true;
            }

            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
            return cv_.wait_until(lock, deadline, [this]() {
                return !received_items_.empty();
            });
        }

        /**
         * @brief Get the number of received events
         * @return Number of events received since start() or last clear()
         */
        size_t get_count() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return received_items_.size();
        }

        /**
         * @brief Check if specific items have been received
         *
         * @param expected_items The expected items to find
         * @return true if matching items exists in received items
         */
        bool has(const ReceivedItmes &expected_items) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto &items : received_items_) {
                if (items == expected_items) {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Get all received events (unfiltered)
         * @return Copy of all received events
         */
        const std::vector<ReceivedItmes> &get_all() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return received_items_;
        }

        /**
         * @brief Get the last received event
         * @return std::optional containing the last received items, std::nullopt if no items were received
         */
        std::optional<ReceivedItmes> get_last() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (received_items_.empty()) {
                return std::nullopt;
            }
            return received_items_.back();
        }

    private:
        template<typename... Args, size_t... Is>
        static std::optional<std::tuple<Args...>> try_extract_items(
                const ReceivedItmes &items, std::index_sequence<Is...>)
        {
            if (((std::holds_alternative<Args>(items[Is])) && ...)) {
                return std::make_tuple(std::get<Args>(items[Is])...);
            }
            return std::nullopt;
        }

    public:
        /**
         * @brief Get received events filtered by item types and extracted as tuples
         *
         * @tparam Args Expected types for each position in the event (bool, double, std::string,
         *              boost::json::object, boost::json::array, RawBuffer)
         * @return Vector of tuples containing extracted values from matching events
         *
         * @code{.cpp}
         * // Get events where first item is string and second is bool
         * auto events = monitor.get_all<std::string, bool>();
         * for (const auto& [str_val, bool_val] : events) {
         *     // use str_val and bool_val
         * }
         * @endcode
         */
        template<typename... Args>
        std::vector<std::tuple<Args...>> get_all() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::vector<std::tuple<Args...>> result;

            for (const auto &items : received_items_) {
                if (items.size() >= sizeof...(Args)) {
                    auto extracted = try_extract_items<Args...>(items, std::index_sequence_for<Args...> {});
                    if (extracted) {
                        result.push_back(std::move(*extracted));
                    }
                }
            }

            return result;
        }

        /**
         * @brief Get the last received event filtered by item types and extracted as tuple
         *
         * @tparam Args Expected types for each position in the event (bool, double, std::string,
         *              boost::json::object, boost::json::array, RawBuffer)
         * @return std::optional containing tuple of extracted values if the last event matches,
         *         std::nullopt if no events or type mismatch
         *
         * @code{.cpp}
         * // Get last event where first item is string and second is bool
         * auto last_event = monitor.get_last<std::string, bool>();
         * if (last_event.has_value()) {
         *     const auto &[event_str, is_unexpected] = last_event.value();
         *     // Or use std::get<0>/std::get<1>
         *     const auto &event_str = std::get<0>(last_event.value());
         *     bool is_unexpected = std::get<1>(last_event.value());
         * }
         *
         * // Get last event with single boost::json::array item
         * auto last_scan = monitor.get_last<boost::json::array>();
         * if (last_scan.has_value()) {
         *     const auto &ap_infos_array = std::get<0>(last_scan.value());
         * }
         * @endcode
         */
        template<typename... Args>
        std::optional<std::tuple<Args...>> get_last() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (received_items_.empty()) {
                return std::nullopt;
            }
            return try_extract_items<Args...>(received_items_.back(), std::index_sequence_for<Args...> {});
        }

        /**
         * @brief Check if currently running
         * @return true if actively running
         */
        bool is_running() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return connection_.connected();
        }

    private:
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        EventRegistry::SignalConnection connection_;
        std::vector<ReceivedItmes> received_items_;
    };
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
