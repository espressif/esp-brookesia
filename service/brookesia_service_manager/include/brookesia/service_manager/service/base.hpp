/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <functional>
#include <future>
#include <map>
#include <string>
#include <type_traits>
#include <vector>
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_manager/function/registry.hpp"
#include "brookesia/service_manager/event/registry.hpp"
#include "brookesia/service_manager/rpc/connection.hpp"

namespace esp_brookesia::service {

class ServiceBase {
public:
    friend class ServiceManager;

    using FunctionHandlerMap = std::map<std::string, FunctionHandler>;

    static constexpr uint32_t DEFAULT_CALL_TIMEOUT_MS = 100;

    /**
     * @brief Service attributes configuration
     */
    struct Attributes {
        std::string name;  ///< Service name
        std::vector<std::string> dependencies = {};  ///< Optional: List of dependent service names, will be started in order
        std::optional<lib_utils::TaskScheduler::StartConfig> task_scheduler_config = std::nullopt;
        ///< Optional: Task scheduler configuration. If configured, service request tasks will be scheduled to this scheduler;
        ///< otherwise, ServiceManager's scheduler will be used
        bool bindable = true;  ///< Optional: Whether the service can be bound
    };

    ServiceBase(const Attributes &attributes)
        : attributes_(attributes)
    {}

    virtual ~ServiceBase();

    /**
     * @brief Get the function schemas list
     *
     * Subclasses should override this method to return an array of function schemas
     *
     * @return std::vector<FunctionSchema> List of function schemas
     *
     * @example
     * std::vector<FunctionSchema> get_function_schemas() override {
     *     return {
     *         {
     *             "add", "Add numbers", {
     *                 {"a", "First", FunctionValueType::Number},
     *                 {"b", "Second", FunctionValueType::Number}
     *             }
     *         },
     *         {
     *             "sub", "Subtract", {
     *                 {"a", "First", FunctionValueType::Number},
     *                 {"b", "Second", FunctionValueType::Number}
     *             }
     *         }
     *     };
     * }
     */
    virtual std::vector<FunctionSchema> get_function_schemas()
    {
        return {};
    }

    /**
     * @brief Get the event schemas list
     *
     * Subclasses should override this method to return an array of event schemas
     *
     * @return std::vector<EventSchema> List of event schemas
     *
     * @example
     * std::vector<EventSchema> get_event_schemas() override {
     *     return {
     *         {
     *             "value_change", "Value changed", {
     *                 {"value", "New value", EventItemType::Number}
     *             }
     *         }
     *     };
     * }
     */
    virtual std::vector<EventSchema> get_event_schemas()
    {
        return {};
    }

    /**
     * @brief Call a function asynchronously with parameters map (non-blocking)
     *
     * @param[in] name Function name to call
     * @param[in] parameters_map FunctionParameterMap map (key-value pairs)
     * @return std::future<FunctionResult> Future that will contain the result
     */
    std::future<FunctionResult> call_function_async(
        const std::string &name, FunctionParameterMap &&parameters_map
    );

    /**
     * @brief Call a function asynchronously with parameters values (non-blocking)
     *
     * @param[in] name Function name to call
     * @param[in] parameters_values FunctionParameterMap values (ordered array)
     * @return std::future<FunctionResult> Future that will contain the result
     */
    std::future<FunctionResult> call_function_async(
        const std::string &name, std::vector<FunctionValue> &&parameters_values
    );

    /**
     * @brief Call a function asynchronously with JSON parameters (non-blocking)
     *
     * @param[in] name Function name to call
     * @param[in] parameters_json FunctionParameterMap in JSON object format
     * @return std::future<FunctionResult> Future that will contain the result
     */
    std::future<FunctionResult> call_function_async(
        const std::string &name, boost::json::object &&parameters_json
    );

    /**
     * @brief Call a function synchronously with parameters map (blocking with timeout)
     *
     * @param[in] name Function name to call
     * @param[in] parameters_map FunctionParameterMap map (key-value pairs)
     * @param[in] timeout_ms Timeout in milliseconds (default: 100ms)
     * @return FunctionResult Result of the function call
     */
    FunctionResult call_function_sync(
        const std::string &name, FunctionParameterMap &&parameters_map,
        uint32_t timeout_ms = DEFAULT_CALL_TIMEOUT_MS
    );

    /**
     * @brief Call a function synchronously with parameters values (blocking with timeout)
     *
     * @param[in] name Function name to call
     * @param[in] parameters_values FunctionParameterMap values (ordered array)
     * @param[in] timeout_ms Timeout in milliseconds (default: 100ms)
     * @return FunctionResult Result of the function call
     */
    FunctionResult call_function_sync(
        const std::string &name, std::vector<FunctionValue> &&parameters_values,
        uint32_t timeout_ms = DEFAULT_CALL_TIMEOUT_MS
    );

    /**
     * @brief Call a function synchronously with JSON parameters (blocking with timeout)
     *
     * @param[in] name Function name to call
     * @param[in] parameters_json FunctionParameterMap in JSON object format
     * @param[in] timeout_ms Timeout in milliseconds (default: 100ms)
     * @return FunctionResult Result of the function call
     */
    FunctionResult call_function_sync(
        const std::string &name, boost::json::object &&parameters_json,
        uint32_t timeout_ms = DEFAULT_CALL_TIMEOUT_MS
    );

    /**
     * @brief Subscribe to an event
     *
     * @param[in] event_name Event name to subscribe
     * @param[in] slot Callback slot to be invoked when event is published
     * @return EventRegistry::SignalConnection RAII scoped connection object for managing the subscription,
     *         automatically disconnects the subscription when the connection object is destroyed.
     */
    EventRegistry::SignalConnection subscribe_event(
        const std::string &event_name, const EventRegistry::SignalSlot &slot
    );

    /**
     * @brief Check if the service is initialized
     *
     * @return true if initialized, false otherwise
     */
    bool is_initialized() const
    {
        return is_initialized_.load();
    }

    /**
     * @brief Check if the service is running
     *
     * @return true if running, false otherwise
     */
    bool is_running() const
    {
        return is_running_.load();
    }

    /**
     * @brief Check if the service is connected to server
     *
     * @return true if connected, false otherwise
     */
    bool is_server_connected() const
    {
        return (server_connection_ != nullptr);
    }

    /**
     * @brief Get the service attributes
     *
     * @return const Attributes& Reference to service attributes
     */
    const Attributes &get_attributes() const
    {
        return attributes_;
    }

    /**
     * @brief Get the call task group name
     *
     * @return std::string Call task group name
     */
    virtual std::string get_call_task_group() const
    {
        return get_attributes().name + "_call";
    }

    /**
     * @brief Get the event task group name
     *
     * @return std::string Event task group name
     */
    virtual std::string get_event_task_group() const
    {
        return get_attributes().name + "_event";
    }

    /**
     * @brief Get the request task group name
     *
     * @return std::string Request task group name
     */
    virtual std::string get_request_task_group() const
    {
        return get_attributes().name + "_request";
    }

    /**
     * @brief Get the task scheduler
     *
     * @note  Sometimes external code needs to use the specified service's task scheduler to execute tasks.
     *        For example, in ISR (Interrupt Service Routine) context, we may need to send events to a specific service.
     *        In these cases, we can use it in combination with `atomic + post_periodic()`.
     *        Therefore, we expose the task scheduler here.
     *
     * @return std::shared_ptr<lib_utils::TaskScheduler> Shared pointer to the task scheduler
     */
    std::shared_ptr<lib_utils::TaskScheduler> get_task_scheduler() const
    {
        return task_scheduler_;
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

protected:
    /**
     * @brief Initialization callback
     *
     * @note Subclasses can override this method for custom initialization
     * @note Functions and events will be automatically registered before this method is called
     * @note If task scheduler is available, this method will be called by task scheduler worker thread
     *
     * @return true if initialization succeeds, false otherwise
     */
    virtual bool on_init()
    {
        return true;
    }

    /**
     * @brief Deinitialization callback
     *
     * @note If task scheduler is available, this method will be called by task scheduler worker thread
     */
    virtual void on_deinit()
    {
    }

    /**
     * @brief Start callback
     *
     * @note If task scheduler is available, this method will be called by task scheduler worker thread
     *
     * @return true if start succeeds, false otherwise
     */
    virtual bool on_start()
    {
        return true;
    }

    /**
     * @brief Stop callback
     *
     * @note If task scheduler is available, this method will be called by task scheduler worker thread
     */
    virtual void on_stop()
    {
    }

    /**
     * @brief Get function handlers map
     *
     * Subclasses should override this method to return a map from function names to handlers
     *
     * @return FunctionHandlerMap Function handlers map
     *
     * @example
     * FunctionHandlerMap get_function_handlers() override {
     *     return {
     *         {
     *             "add",
     *             [this](const FunctionParameterMap &args) -> FunctionResult {
     *                 return handle_add(args);
     *             }
     *         },
     *         {
     *             "sub",
     *             [this](const FunctionParameterMap &args) -> FunctionResult {
     *                 return handle_sub(args);
     *             }
     *         }
     *     };
     * }
     */
    virtual FunctionHandlerMap get_function_handlers()
    {
        return {};
    }

    /**
     * @brief Register function list (internal use)
     *
     * @param[in] schemas Function schemas list
     * @param[in] handlers Function handler map
     * @return true if registered successfully, false otherwise
     */
    bool register_functions(std::vector<FunctionSchema> &&schemas, FunctionHandlerMap &&handlers);

    /**
     * @brief Unregister function list (internal use)
     *
     * @param[in] names Function names list
     * @return true if unregistered successfully, false otherwise
     */
    bool unregister_functions(const std::vector<std::string> &names);

    /**
     * @brief Register event list (internal use)
     *
     * @param[in] schemas Event schemas list
     * @return true if registered successfully, false otherwise
     */
    bool register_events(std::vector<EventSchema> &&schemas);

    /**
     * @brief Unregister event list (internal use)
     *
     * @param[in] names Event names list
     * @return true if unregistered successfully, false otherwise
     */
    bool unregister_events(const std::vector<std::string> &names);

    /**
     * @brief Publish an event with data map
     *
     * @param[in] event_name Event name
     * @param[in] event_items Event data map (key-value pairs)
     * @param[in] use_dispatch Whether to use dispatch to publish the event
     * @return true if published successfully, false otherwise
     */
    bool publish_event(const std::string &event_name, EventItemMap &&event_items, bool use_dispatch = false);

    /**
     * @brief Publish an event with data values
     *
     * @param[in] event_name Event name
     * @param[in] data_values Event data values (ordered array according to event schema)
     * @param[in] use_dispatch Whether to use dispatch to publish the event
     * @return true if published successfully, false otherwise
     *
     * @example
     * // Assuming event schema: {"value_change", "...", {{"value", "...", Number}}}
     * publish_event("value_change", {42.0});
     */
    bool publish_event(
        const std::string &event_name, std::vector<EventItem> &&data_values, bool use_dispatch = false
    );

    /**
     * @brief Publish an event with JSON data
     *
     * @param[in] event_name Event name
     * @param[in] data_json Event data in JSON object format
     * @param[in] use_dispatch Whether to use dispatch to publish the event
     * @return true if published successfully, false otherwise
     */
    bool publish_event(const std::string &event_name, boost::json::object &&data_json, bool use_dispatch = false);

    bool set_task_scheduler(std::shared_ptr<lib_utils::TaskScheduler> task_scheduler);

    bool start();
    void stop();

private:
    bool init(std::shared_ptr<lib_utils::TaskScheduler> task_scheduler);
    void deinit();

    bool init_internal(std::shared_ptr<lib_utils::TaskScheduler> task_scheduler);  // Internal init without lock
    void deinit_internal();  // Internal deinit without lock
    bool start_internal();  // Internal start without lock
    void stop_internal();  // Internal stop without lock

    std::shared_ptr<rpc::ServerConnection> connect_to_server();
    void disconnect_from_server();
    void try_override_connection_request_handler();

    Attributes attributes_;

    boost::mutex state_mutex_;  // Protect state transitions (init/deinit/start/stop)
    std::atomic<bool> is_initialized_{false};
    std::atomic<bool> is_running_{false};

    // Use shared_ptr instead of unique_ptr to support thread-safe access
    mutable boost::shared_mutex resources_mutex_;  // Protect resources access
    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;
    std::shared_ptr<FunctionRegistry> function_registry_;
    std::shared_ptr<EventRegistry> event_registry_;
    std::shared_ptr<rpc::ServerConnection> server_connection_;
};

BROOKESIA_DESCRIBE_STRUCT(ServiceBase::Attributes, (), (name, dependencies, task_scheduler_config))

// ============================================================================
// Helper macros: Simplify FunctionHandlerMap writing
// ============================================================================

/**
 * @brief Create a zero-parameter function handler
 *
 * @param func_name Function name string (e.g., "get_volume")
 * @param func_call Actual function call (e.g., function_get_volume())
 *
 * Example:
 * BROOKESIA_SERVICE_FUNC_HANDLER_0("get_volume", function_get_volume())
 */
#define BROOKESIA_SERVICE_FUNC_HANDLER_0(func_name, func_call) \
    { \
        func_name, \
        [this](esp_brookesia::service::FunctionParameterMap &&) -> esp_brookesia::service::FunctionResult { \
            return esp_brookesia::service::ServiceBase::to_function_result(func_call); \
        } \
    }

/**
 * @brief Create a single-parameter function handler
 *
 * @param func_name Function name string
 * @param param_name Parameter name string
 * @param param_type Parameter C++ type (e.g., std::string, double)
 * @param func_call Function call, use PARAM as parameter placeholder
 *
 * Example:
 * BROOKESIA_SERVICE_FUNC_HANDLER_1("play_url", "url", std::string, function_play_url(PARAM))
 * BROOKESIA_SERVICE_FUNC_HANDLER_1("set_volume", "volume", uint8_t,
 *                     function_set_volume(static_cast<uint8_t>(PARAM)))
 */
#define BROOKESIA_SERVICE_FUNC_HANDLER_1(func_name, param_name, param_type, func_call) \
    { \
        func_name, \
        [this](esp_brookesia::service::FunctionParameterMap &&args) -> esp_brookesia::service::FunctionResult { \
            auto &PARAM = std::get<param_type>(args.at(param_name)); \
            return esp_brookesia::service::ServiceBase::to_function_result(func_call); \
        } \
    }

/**
 * @brief Create a two-parameter function handler
 *
 * @param func_name Function name string
 * @param param1_name First parameter name
 * @param param1_type First parameter type
 * @param param2_name Second parameter name
 * @param param2_type Second parameter type
 * @param func_call Function call, use PARAM1, PARAM2 as parameter placeholders
 *
 * Example:
 * BROOKESIA_SERVICE_FUNC_HANDLER_2("add", "a", double, "b", double,
 *                     function_add(PARAM1, PARAM2))
 */
#define BROOKESIA_SERVICE_FUNC_HANDLER_2(func_name, param1_name, param1_type, param2_name, param2_type, func_call) \
    { \
        func_name, \
        [this](esp_brookesia::service::FunctionParameterMap &&args) -> esp_brookesia::service::FunctionResult { \
            auto &PARAM1 = std::get<param1_type>(args.at(param1_name)); \
            auto &PARAM2 = std::get<param2_type>(args.at(param2_name)); \
            return esp_brookesia::service::ServiceBase::to_function_result(func_call); \
        } \
    }

/**
 * @brief Create a three-parameter function handler
 *
 * Usage is similar to BROOKESIA_SERVICE_FUNC_HANDLER_2, supports PARAM1, PARAM2, PARAM3
 */
#define BROOKESIA_SERVICE_FUNC_HANDLER_3(func_name, p1_name, p1_type, p2_name, p2_type, p3_name, p3_type, func_call) \
    { \
        func_name, \
        [this](esp_brookesia::service::FunctionParameterMap &&args) -> esp_brookesia::service::FunctionResult { \
            auto &PARAM1 = std::get<p1_type>(args.at(p1_name)); \
            auto &PARAM2 = std::get<p2_type>(args.at(p2_name)); \
            auto &PARAM3 = std::get<p3_type>(args.at(p3_name)); \
            return esp_brookesia::service::ServiceBase::to_function_result(func_call); \
        } \
    }

} // namespace esp_brookesia::service
