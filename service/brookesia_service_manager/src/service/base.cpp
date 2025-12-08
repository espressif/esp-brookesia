/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/service/base.hpp"

namespace esp_brookesia::service {

#if BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG
constexpr uint32_t WAIT_TASK_SCHEDULER_FINISHED_TIMEOUT_MS = 1000;
#else
constexpr uint32_t WAIT_TASK_SCHEDULER_FINISHED_TIMEOUT_MS = 500;
#endif

ServiceBase::~ServiceBase()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_initialized()) {
        deinit();
    }
}

std::future<FunctionResult> ServiceBase::call_function_async(
    const std::string &name, FunctionParameterMap &&parameters_map
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%), parameters_map(%2%)", name, BROOKESIA_DESCRIBE_TO_STR(parameters_map));

    // Use shared_ptr to wrap the promise, so that it can be copied (std::function requires copyable)
    auto result_promise = std::make_shared<std::promise<FunctionResult>>();
    auto result_future = result_promise->get_future();

    // Helper lambda to set error result
    auto set_error = [result_promise](const std::string & error_msg) {
        FunctionResult result{
            .success = false,
            .error_message = error_msg,
        };
        BROOKESIA_LOGE("%1%", error_msg);
        result_promise->set_value(std::move(result));
    };

    if (!is_running()) {
        set_error("Service is not running");
        return result_future;
    }

    // Thread-safe get the copies of registry and scheduler
    std::shared_ptr<FunctionRegistry> registry;
    std::shared_ptr<lib_utils::TaskScheduler> scheduler;
    boost::asio::io_context *io_ctx;
    {
        boost::shared_lock lock(registry_mutex_);
        registry = function_registry_;
        scheduler = task_scheduler_;
        io_ctx = io_context_;
    }

    if (!registry) {
        set_error("Function registry not initialized");
        return result_future;
    }

    if (!registry->has(name)) {
        set_error("Function not found: " + name);
        return result_future;
    }

    auto task = [registry, result_promise, name, parameters_map = std::move(parameters_map)]() mutable {
        BROOKESIA_LOG_TRACE_GUARD();
        auto result = registry->call(name, std::move(parameters_map));
        result_promise->set_value(std::move(result));
    };

    if (scheduler) {
        if (!scheduler->post(std::move(task), nullptr, SERVICE_REQUEST_TASK_GROUP)) {
            set_error("Failed to post task");
            return result_future;
        }
    } else if (io_ctx) {
        boost::asio::post(*io_ctx, std::move(task));
    } else {
        set_error("Neither task scheduler nor io_context available");
        return result_future;
    }

    return result_future;
}

FunctionResult ServiceBase::call_function_sync(
    const std::string &name, FunctionParameterMap &&parameters_map, uint32_t timeout_ms
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: name(%1%), parameters_map(%2%), timeout_ms(%3%)", name,
        BROOKESIA_DESCRIBE_TO_STR(parameters_map), timeout_ms
    );

    auto result_future = call_function_async(name, std::move(parameters_map));

    if (result_future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
        FunctionResult result{
            .success = false,
            .error_message = (boost::format("Timeout after %1%ms") % timeout_ms).str(),
        };
        BROOKESIA_LOGE("%1%", result.error_message);
        return result;
    }

    return result_future.get();
}

std::future<FunctionResult> ServiceBase::call_function_async(
    const std::string &name, std::vector<FunctionValue> &&parameters_values
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%), parameters_values(%2%)", name, BROOKESIA_DESCRIBE_TO_STR(parameters_values));

    // Use shared_ptr to wrap the promise for error handling
    auto result_promise = std::make_shared<std::promise<FunctionResult>>();
    auto result_future = result_promise->get_future();

    // Helper lambda to set error result
    auto set_error = [result_promise](const std::string & error_msg) {
        FunctionResult result{
            .success = false,
            .error_message = error_msg,
        };
        BROOKESIA_LOGE("%1%", error_msg);
        result_promise->set_value(std::move(result));
    };

    // Get the function definitions
    auto function_definitions = get_function_definitions();

    // Find the corresponding function definition
    const FunctionSchema *function_def = nullptr;
    for (const auto &def : function_definitions) {
        if (def.name == name) {
            function_def = &def;
            break;
        }
    }

    if (!function_def) {
        set_error("Function definition not found: " + name);
        return result_future;
    }

    // Check if the parameter count matches
    if (parameters_values.size() != function_def->parameters.size()) {
        set_error((boost::format("Function parameter count mismatch: expected %1%, got %2%")
                   % function_def->parameters.size() % parameters_values.size()).str());
        return result_future;
    }

    // Convert the vector to map according to the function definition parameter order
    FunctionParameterMap parameters_map;
    for (size_t i = 0; i < function_def->parameters.size(); i++) {
        parameters_map[function_def->parameters[i].name] = std::move(parameters_values[i]);
    }

    return call_function_async(name, std::move(parameters_map));
}

FunctionResult ServiceBase::call_function_sync(
    const std::string &name, std::vector<FunctionValue> &&parameters_values, uint32_t timeout_ms
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: name(%1%), parameters_values(%2%), timeout_ms(%3%)", name, BROOKESIA_DESCRIBE_TO_STR(parameters_values),
        timeout_ms
    );

    auto result_future = call_function_async(name, std::move(parameters_values));

    if (result_future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
        FunctionResult result{
            .success = false,
            .error_message = (boost::format("Timeout after %1%ms") % timeout_ms).str(),
        };
        BROOKESIA_LOGE("%1%", result.error_message);
        return result;
    }

    return result_future.get();
}

std::future<FunctionResult> ServiceBase::call_function_async(
    const std::string &name, boost::json::object &&parameters_json
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: name(%1%), parameters_json(%2%)", name, boost::json::serialize(parameters_json)
    );

    // Use shared_ptr to wrap the promise for error handling
    auto result_promise = std::make_shared<std::promise<FunctionResult>>();
    auto result_future = result_promise->get_future();

    // Helper lambda to set error result
    auto set_error = [result_promise](const std::string & error_msg) {
        FunctionResult result{
            .success = false,
            .error_message = error_msg,
        };
        BROOKESIA_LOGE("%1%", error_msg);
        result_promise->set_value(std::move(result));
    };

    FunctionParameterMap parameters;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(parameters_json, parameters)) {
        set_error((boost::format("Invalid parameters: %1%") % boost::json::serialize(parameters_json)).str());
        return result_future;
    }

    return call_function_async(name, std::move(parameters));
}

FunctionResult ServiceBase::call_function_sync(
    const std::string &name, boost::json::object &&parameters_json, uint32_t timeout_ms
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: name(%1%), parameters_json(%2%), timeout_ms(%3%)", name, boost::json::serialize(parameters_json),
        timeout_ms
    );

    auto result_future = call_function_async(name, std::move(parameters_json));

    if (result_future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
        FunctionResult result{
            .success = false,
            .error_message = (boost::format("Timeout after %1%ms") % timeout_ms).str(),
        };
        BROOKESIA_LOGE("%1%", result.error_message);
        return result;
    }

    return result_future.get();
}

EventRegistry::SignalConnection ServiceBase::subscribe_event(
    const std::string &event_name, const EventRegistry::SignalSlot &slot
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), EventRegistry::SignalConnection(), "Not running");

    // Thread-safe get the copy of event_registry
    std::shared_ptr<EventRegistry> registry;
    {
        boost::shared_lock lock(registry_mutex_);
        registry = event_registry_;
    }

    if (!registry) {
        BROOKESIA_LOGE("Event registry not initialized");
        return EventRegistry::SignalConnection();
    }

    auto signal = registry->get_signal(event_name);
    BROOKESIA_CHECK_NULL_RETURN(
        signal, EventRegistry::SignalConnection(), "Event signal not found: %1%", event_name
    );

    return signal->connect(slot);
}

bool ServiceBase::publish_event(const std::string &event_name, EventItemMap &&event_items)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event_name(%1%), event_items(%2%)", event_name, BROOKESIA_DESCRIBE_TO_STR(event_items)
    );

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    // Thread-safe get the copies of event_registry and task_scheduler
    std::shared_ptr<EventRegistry> registry;
    std::shared_ptr<lib_utils::TaskScheduler> scheduler;
    boost::asio::io_context *io_ctx;
    {
        boost::shared_lock lock(registry_mutex_);
        registry = event_registry_;
        scheduler = task_scheduler_;
        io_ctx = io_context_;
    }

    if (!registry) {
        BROOKESIA_LOGE("Event registry not initialized");
        return false;
    }

    // Validate the event data
    BROOKESIA_CHECK_FALSE_RETURN(
        registry->validate_items(event_name, event_items), false, "Failed to validate event data for: %1%", event_name
    );

    // If connected to the server, publish to the server
    if (is_server_connected()) {
        BROOKESIA_LOGD("Connected to server, publishing event to it");
        BROOKESIA_CHECK_FALSE_EXECUTE(server_connection_->publish_event(event_name, event_items), {}, {
            BROOKESIA_LOGE("Failed to publish event to server: %1%", event_name);
        });
    }

    // Emit the local signal
    auto emit_signal_task = [registry, event_name, event_items = std::move(event_items)]() {
        auto signal = registry->get_signal(event_name);
        if (signal) {
            (*signal)(event_name, event_items);
        } else {
            BROOKESIA_LOGW("Signal not found for event: %1%", event_name);
        }
    };
    if (scheduler) {
        BROOKESIA_CHECK_FALSE_EXECUTE(scheduler->post(emit_signal_task, nullptr, SERVICE_REQUEST_TASK_GROUP), {
            BROOKESIA_LOGE("Failed to post emit signal task");
            return false;
        });
    } else if (io_ctx) {
        boost::asio::post(*io_ctx, emit_signal_task);
    } else {
        BROOKESIA_LOGE("No task scheduler or io_context available");
        return false;
    }

    return true;
}

bool ServiceBase::publish_event(const std::string &event_name, std::vector<EventItem> &&data_values)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event_name(%1%), data_values(%2%)", event_name, BROOKESIA_DESCRIBE_TO_STR(data_values)
    );

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    // Get the event definitions
    auto event_definitions = get_event_definitions();

    // Find the corresponding event definition
    const EventSchema *event_def = nullptr;
    for (const auto &def : event_definitions) {
        if (def.name == event_name) {
            event_def = &def;
            break;
        }
    }
    BROOKESIA_CHECK_NULL_RETURN(event_def, false, "Event definition not found: %1%", event_name);

    // Check if the value count matches
    BROOKESIA_CHECK_FALSE_RETURN(
        data_values.size() == event_def->items.size(), false,
        "Event value count mismatch: expected %1%, got %2%", event_def->items.size(), data_values.size()
    );

    // Convert the vector to map according to the event definition data order
    EventItemMap event_items;
    for (size_t i = 0; i < event_def->items.size(); i++) {
        event_items[event_def->items[i].name] = std::move(data_values[i]);
    }

    return publish_event(event_name, std::move(event_items));
}

bool ServiceBase::publish_event(const std::string &event_name, boost::json::object &&data_json)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event_name(%1%), data_json(%2%)", event_name, boost::json::serialize(data_json));

    EventItemMap event_items;
    BROOKESIA_CHECK_FALSE_RETURN(
        BROOKESIA_DESCRIBE_FROM_JSON(data_json, event_items), false, "Failed to parse data"
    );

    return publish_event(event_name, std::move(event_items));
}

bool ServiceBase::init(boost::asio::io_context &io_context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(state_mutex_);
    return init_internal(io_context);
}

bool ServiceBase::init_internal(boost::asio::io_context &io_context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_initialized()) {
        BROOKESIA_LOGD("Already initialized");
        return true;
    }

    lib_utils::FunctionGuard deinit_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        deinit_internal();
    });

    is_initialized_.store(true);

    if (attributes_.task_scheduler_config.has_value()) {
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            task_scheduler_ = std::make_unique<lib_utils::TaskScheduler>(), false, "Failed to create task scheduler"
        );
    }

    // Create function registry
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        function_registry_ = std::make_unique<FunctionRegistry>(), false, "Failed to create function registry"
    );
    // Create event registry
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        event_registry_ = std::make_unique<EventRegistry>(), false, "Failed to create event registry"
    );

    io_context_ = &io_context;

    BROOKESIA_CHECK_FALSE_RETURN(on_init(), false, "Failed to initialize service");

    deinit_guard.release();

    return true;
}

void ServiceBase::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(state_mutex_);
    deinit_internal();
}

void ServiceBase::deinit_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_initialized()) {
        BROOKESIA_LOGD("Already deinitialized");
        return;
    }

    if (is_running()) {
        stop_internal();
    }

    on_deinit();

    // Use write lock to protect the registry reset
    {
        boost::lock_guard lock(registry_mutex_);
        io_context_ = nullptr;
        task_scheduler_.reset();
        server_connection_.reset();
        function_registry_.reset();
        event_registry_.reset();
    }

    is_initialized_.store(false);
}

bool ServiceBase::start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(state_mutex_);
    return start_internal();
}

bool ServiceBase::start_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_running()) {
        BROOKESIA_LOGD("Already started");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), false, "Not initialized");

    BROOKESIA_LOGD("Starting service with attributes: %1%", BROOKESIA_DESCRIBE_TO_STR(attributes_));

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_internal();
    });

    if (task_scheduler_) {
        BROOKESIA_CHECK_FALSE_RETURN(
            task_scheduler_->start(*attributes_.task_scheduler_config), false, "Failed to start task scheduler"
        );
        BROOKESIA_CHECK_FALSE_RETURN(task_scheduler_->configure_group(SERVICE_REQUEST_TASK_GROUP, {
            .enable_post_execute_in_order = true,
        }), false, "Failed to configure task scheduler group");
    }

    BROOKESIA_CHECK_FALSE_RETURN(on_start(), false, "Failed to start service");

    // Auto-register functions
    auto function_definitions = get_function_definitions();
    auto function_handlers = get_function_handlers();
    if (!function_definitions.empty()) {
        BROOKESIA_CHECK_FALSE_RETURN(
            register_functions(std::move(function_definitions), std::move(function_handlers)), false,
            "Failed to register functions"
        );
    }

    // Auto-register events
    auto event_definitions = get_event_definitions();
    if (!event_definitions.empty()) {
        BROOKESIA_CHECK_FALSE_RETURN(register_events(std::move(event_definitions)), false, "Failed to register events");
    }

    if (is_server_connected()) {
        server_connection_->activate(true);
        try_override_connection_request_handler();
    }

    is_running_ = true;

    stop_guard.release();

    return true;
}

void ServiceBase::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(state_mutex_);
    stop_internal();
}

void ServiceBase::stop_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        BROOKESIA_LOGD("Already stopped");
        return;
    }

    on_stop();

    if (is_server_connected()) {
        server_connection_->activate(false);
    }

    if (task_scheduler_) {
        BROOKESIA_LOGI("Waiting for task scheduler to finish");
        if (!task_scheduler_->wait_all(WAIT_TASK_SCHEDULER_FINISHED_TIMEOUT_MS)) {
            BROOKESIA_LOGW(
                "Task scheduler wait timeout after %1%ms", WAIT_TASK_SCHEDULER_FINISHED_TIMEOUT_MS
            );
        }
        task_scheduler_->stop();
    }

    // Use write lock to protect the registry modifications
    {
        boost::lock_guard lock(registry_mutex_);
        if (function_registry_) {
            function_registry_->remove_all();
        }
        if (event_registry_) {
            event_registry_->remove_all();
        }
    }

    is_running_ = false;
}

std::shared_ptr<rpc::ServerConnection> ServiceBase::connect_to_server()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Use write lock to protect the server_connection_ and registries access
    boost::lock_guard lock(registry_mutex_);

    if (is_server_connected()) {
        BROOKESIA_LOGD("Already connected to server");
        return server_connection_;
    }

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        server_connection_ = std::make_shared<rpc::ServerConnection>(
                                 attributes_.name, *function_registry_, *event_registry_
                             ), nullptr, "Failed to create server connection"
    );

    if (is_running()) {
        server_connection_->activate(true);
    }

    try_override_connection_request_handler();

    return server_connection_;
}

void ServiceBase::disconnect_from_server()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Use write lock to protect the server_connection_ access
    boost::lock_guard lock(registry_mutex_);
    server_connection_.reset();
}

void ServiceBase::try_override_connection_request_handler()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_server_connected()) {
        BROOKESIA_LOGD("Not connected to server");
        return;
    }

    if (!task_scheduler_) {
        BROOKESIA_LOGD("Task scheduler is not supported");
        return;
    }

    // Use the task_scheduler to handle the request
    auto request_handler =
        [this]( size_t connection_id, std::string && request_id, std::string && method,
                FunctionParameterMap && parameters
    ) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        auto task = [
                        this, connection_id, request_id = std::move(request_id), method = std::move(method),
                        parameters = std::move(parameters)
        ]() mutable {
            rpc::Response response{
                .id = std::move(request_id),
            };
            auto result = function_registry_->call(method, std::move(parameters));
            if (result.success)
            {
                response.result = std::move(BROOKESIA_DESCRIBE_TO_JSON(result.data));
            } else
            {
                response.error = rpc::ResponseError{
                    .code = -1,
                    .message = result.error_message,
                };
            }
            BROOKESIA_CHECK_FALSE_EXIT(
                server_connection_->respond_request(connection_id, std::move(response)),
                "Failed to respond to request"
            );
        };
        BROOKESIA_CHECK_FALSE_RETURN(
            task_scheduler_->post(std::move(task), nullptr, SERVICE_REQUEST_TASK_GROUP),
            false, "Failed to post request"
        );
        return true;
    };
    server_connection_->set_request_handler(request_handler);
}

bool ServiceBase::register_functions(std::vector<FunctionSchema> &&definitions, FunctionHandlerMap &&handlers)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), false, "Not initialized");

    // Use write lock to protect the registry modifications
    boost::lock_guard lock(registry_mutex_);

    size_t registered_count = 0;
    const size_t total_count = definitions.size();
    // Avoid compiler warning about unused variable
    (void)total_count;

    for (auto &&def : definitions) {
        // Save name before moving (to avoid use-after-move in error logs)
        const std::string func_name = def.name;
        BROOKESIA_LOGD("Registering function: %1%", func_name);

        auto it = handlers.find(func_name);
        if (it == handlers.end()) {
            BROOKESIA_LOGE("Handler not found for function: %1%", func_name);
            continue;
        }

        if (!function_registry_->add(std::move(def), std::move(it->second))) {
            BROOKESIA_LOGE("Failed to register function: %1%", func_name);
            continue;
        }

        registered_count++;
    }

    BROOKESIA_LOGI("[%1%] Registered %2%/%3% functions", attributes_.name, registered_count, total_count);

    return true;
}

bool ServiceBase::register_events(std::vector<EventSchema> &&definitions)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), false, "Not initialized");

    // Use write lock to protect the registry modifications
    boost::lock_guard lock(registry_mutex_);

    size_t registered_count = 0;
    const size_t total_count = definitions.size();
    // Avoid compiler warning about unused variable
    (void)total_count;

    for (auto &&def : definitions) {
        // Save name before moving (to avoid use-after-move in error logs)
        const std::string event_name = def.name;
        BROOKESIA_LOGD("Registering event: %1%", event_name);

        if (!event_registry_->add(std::move(def))) {
            BROOKESIA_LOGE("Failed to register event: %1%", event_name);
            continue;
        }

        registered_count++;
    }

    BROOKESIA_LOGI("[%1%] Registered %2%/%3% events", attributes_.name, registered_count, total_count);

    return true;
}

} // namespace esp_brookesia::service
