/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_manager/service/manager.hpp"

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

    // Atomic flag to ensure promise is only set once
    auto promise_set = std::make_shared<std::atomic<bool>>(false);

    // Helper lambda to set error result (thread-safe, only sets once)
    auto set_error = [result_promise, promise_set](const std::string & error_msg) {
        bool expected = false;
        if (!promise_set->compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            // Promise already set, ignore
            BROOKESIA_LOGW("Promise already satisfied, ignoring error: %1%", error_msg);
            return;
        }
        FunctionResult result{
            .success = false,
            .error_message = error_msg,
        };
        BROOKESIA_LOGE("%1%", error_msg);
        try {
            result_promise->set_value(std::move(result));
        } catch (const std::future_error &e) {
            BROOKESIA_LOGW("Failed to set promise value: %1%", e.what());
        }
    };

    if (!is_initialized()) {
        set_error("Service is not initialized");
        return result_future;
    }

    // Thread-safe get the copies of registry and scheduler
    std::shared_ptr<FunctionRegistry> registry;
    std::shared_ptr<lib_utils::TaskScheduler> scheduler;
    {
        boost::shared_lock lock(resources_mutex_);
        registry = function_registry_;
        scheduler = task_scheduler_;
    }
    if (!registry || !scheduler) {
        set_error("Invalid state");
        return result_future;
    }

    // Get the function schema
    auto *func_schema = registry->get_schema(name);
    if (!func_schema) {
        set_error("Function not found: " + name);
        return result_future;
    }

    // Create the call function task
    auto call_function_task = [this, registry, result_promise, promise_set, name, parameters_map =
          std::move(parameters_map)]() mutable {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        bool expected = false;
        if (!promise_set->compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            // Promise already set, ignore
            BROOKESIA_LOGW("Promise already satisfied, ignoring function call result for: %1%", name);
            return;
        }
        FunctionResult result;
        BROOKESIA_CHECK_EXCEPTION_EXECUTE(result = registry->call(name, std::move(parameters_map)), {
            result.success = false;
            result.error_message = "Failed to call function: " + name;
        }, {
            BROOKESIA_LOGE("Failed to call function: %1%", name);
        });
        try
        {
            result_promise->set_value(std::move(result));
        } catch (const std::future_error &e)
        {
            BROOKESIA_LOGW("Failed to set promise value for '%1%': %2%", name, e.what());
        }
    };

    // If the function does not require asynchronous, use synchronous call instead
    if (!func_schema->require_scheduler) {
        BROOKESIA_LOGD("Function '%1%' does not require async, using sync call instead", name);
        call_function_task();
        return result_future;
    }

    // Only allow asynchronous calls when the service is running
    if (!is_running()) {
        set_error("Service is not running");
        return result_future;
    }

    // Post the call function task to the task scheduler
    if (!scheduler->post(std::move(call_function_task), nullptr, get_call_task_group())) {
        set_error("Failed to post task");
    }

    return result_future;
}

FunctionResult ServiceBase::call_function_sync(
    const std::string &name, FunctionParameterMap &&parameters_map, uint32_t timeout_ms
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: name(%1%), parameters_map(%2%), timeout_ms(%3%)", name, BROOKESIA_DESCRIBE_TO_STR(parameters_map),
        timeout_ms
    );

    std::shared_ptr<FunctionResult> result = std::make_shared<FunctionResult>();
    result->success = false;
    // Helper lambda to set error result (thread-safe, only sets once)
    auto set_error = [&result](const std::string & error_msg) {
        result->error_message = error_msg;
        BROOKESIA_LOGE("%1%", error_msg);
    };

    // Thread-safe get the copies of registry and scheduler
    std::shared_ptr<FunctionRegistry> registry;
    std::shared_ptr<lib_utils::TaskScheduler> scheduler;
    {
        boost::shared_lock lock(resources_mutex_);
        registry = function_registry_;
        scheduler = task_scheduler_;
    }
    if (!registry || !scheduler) {
        set_error("Invalid state");
        return *result;
    }

    // Get the function schema
    auto *func_schema = registry->get_schema(name);
    if (!func_schema) {
        set_error("Function not found: " + name);
        return *result;
    }

    // Create the call function task
    auto call_function_task = [this, registry, result, name, parameters_map = std::move(parameters_map)] () mutable {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        *result = registry->call(name, std::move(parameters_map));
    };

    // If the function does not require asynchronous, use synchronous call instead
    if (!func_schema->require_scheduler) {
        BROOKESIA_LOGD("Function '%1%' does not require async, using sync call instead", name);
        call_function_task();
        return *result;
    }

    // Only allow synchronous calls when the service is running
    if (!is_running()) {
        set_error("Service is not running");
        return *result;
    }

    // Post the call function task to the task scheduler
    lib_utils::TaskScheduler::TaskId task_id;
    if (!scheduler->post(std::move(call_function_task), &task_id, get_call_task_group())) {
        set_error("Failed to post task");
        return *result;
    }
    if (!scheduler->wait(task_id, timeout_ms)) {
        set_error((boost::format("Timeout after %1%ms") % timeout_ms).str());
        return *result;
    }

    return *result;
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

    // Get the function schemas
    auto function_schemas = get_function_schemas();

    // Find the corresponding function definition
    const FunctionSchema *function_schema = nullptr;
    for (const auto &def : function_schemas) {
        if (def.name == name) {
            function_schema = &def;
            break;
        }
    }

    if (!function_schema) {
        set_error("Function definition not found: " + name);
        return result_future;
    }

    // Convert the vector to map according to the function definition parameter order
    auto parameters_size = std::min(parameters_values.size(), function_schema->parameters.size());
    FunctionParameterMap parameters_map;
    for (size_t i = 0; i < parameters_size; i++) {
        parameters_map[function_schema->parameters[i].name] = std::move(parameters_values[i]);
    }

    return call_function_async(name, std::move(parameters_map));
}

FunctionResult ServiceBase::call_function_sync(
    const std::string &name, std::vector<FunctionValue> &&parameters_values, uint32_t timeout_ms
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: name(%1%), parameters_values(%2%), timeout_ms(%3%)", name,
        BROOKESIA_DESCRIBE_TO_STR(parameters_values), timeout_ms
    );

    // Get the function schemas
    auto function_schemas = get_function_schemas();

    // Find the corresponding function definition
    const FunctionSchema *function_schema = nullptr;
    for (const auto &def : function_schemas) {
        if (def.name == name) {
            function_schema = &def;
            break;
        }
    }

    if (!function_schema) {
        return FunctionResult{
            .success = false,
            .error_message = "Function definition not found: " + name,
        };
    }

    // Convert the vector to map according to the function definition parameter order
    auto parameters_size = std::min(parameters_values.size(), function_schema->parameters.size());
    FunctionParameterMap parameters_map;
    for (size_t i = 0; i < parameters_size; i++) {
        parameters_map[function_schema->parameters[i].name] = std::move(parameters_values[i]);
    }

    return call_function_sync(name, std::move(parameters_map), timeout_ms);
}

std::future<FunctionResult> ServiceBase::call_function_async(
    const std::string &name, boost::json::object &&parameters_json
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%), parameters_json(%2%)", name, BROOKESIA_DESCRIBE_TO_STR(parameters_json));

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
        "Params: name(%1%), parameters_json(%2%), timeout_ms(%3%)", name,
        BROOKESIA_DESCRIBE_TO_STR(parameters_json), timeout_ms
    );

    FunctionParameterMap parameters_map;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(parameters_json, parameters_map)) {
        return FunctionResult{
            .success = false,
            .error_message = (boost::format("Invalid parameters: %1%") % boost::json::serialize(parameters_json)).str(),
        };
    }

    return call_function_sync(name, std::move(parameters_map), timeout_ms);
}

EventRegistry::SignalConnection ServiceBase::subscribe_event(
    const std::string &event_name, const EventRegistry::SignalSlot &slot
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event_name(%1%), slot(%2%)", event_name, BROOKESIA_DESCRIBE_TO_STR(slot));

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), EventRegistry::SignalConnection(), "Not initialized");

    // Thread-safe get the copy of event_registry
    std::shared_ptr<EventRegistry> registry;
    {
        boost::shared_lock lock(resources_mutex_);
        registry = event_registry_;
    }
    if (!registry) {
        BROOKESIA_LOGE("Invalid state");
        return EventRegistry::SignalConnection();
    }

    auto signal = registry->get_signal(event_name);
    BROOKESIA_CHECK_NULL_RETURN(
        signal, EventRegistry::SignalConnection(), "Event signal not found: %1%", event_name
    );

    return signal->connect(slot);
}

bool ServiceBase::register_functions(std::vector<FunctionSchema> &&schemas, FunctionHandlerMap &&handlers)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: schemas(%1%), handlers(%2%)", BROOKESIA_DESCRIBE_TO_STR(schemas),
        BROOKESIA_DESCRIBE_TO_STR(handlers)
    );

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), false, "Not initialized");

    // Use write lock to protect the registry modifications
    boost::lock_guard lock(resources_mutex_);

    size_t registered_count = 0;
    const size_t total_count = schemas.size();
    // Avoid compiler warning about unused variable
    (void)total_count;

    for (auto &&schema : schemas) {
        // Save name before moving (to avoid use-after-move in error logs)
        const std::string func_name = schema.name;
        BROOKESIA_LOGD("Registering function: %1%", func_name);

        auto it = handlers.find(func_name);
        if (it == handlers.end()) {
            BROOKESIA_LOGE("Handler not found for function: %1%", func_name);
            continue;
        }

        if (!function_registry_->add(std::move(schema), std::move(it->second))) {
            BROOKESIA_LOGE("Failed to register function: %1%", func_name);
            continue;
        }

        registered_count++;
    }

    BROOKESIA_LOGI("[%1%] Registered %2%/%3% functions", attributes_.name, registered_count, total_count);

    return true;
}

bool ServiceBase::unregister_functions(const std::vector<std::string> &names)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: names(%1%)", BROOKESIA_DESCRIBE_TO_STR(names));

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), false, "Not initialized");

    // Use write lock to protect the registry modifications
    boost::lock_guard lock(resources_mutex_);
    for (const auto &name : names) {
        function_registry_->remove(name);
    }

    BROOKESIA_LOGI("[%1%] Unregistered %2% functions", attributes_.name, names.size());

    return true;
}

bool ServiceBase::register_events(std::vector<EventSchema> &&schemas)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: schemas(%1%)", BROOKESIA_DESCRIBE_TO_STR(schemas));

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), false, "Not initialized");

    // Use write lock to protect the registry modifications
    boost::lock_guard lock(resources_mutex_);

    size_t registered_count = 0;
    const size_t total_count = schemas.size();
    // Avoid compiler warning about unused variable
    (void)total_count;

    for (auto &&schema : schemas) {
        // Save name before moving (to avoid use-after-move in error logs)
        const std::string event_name = schema.name;
        BROOKESIA_LOGD("Registering event: %1%", event_name);

        if (!event_registry_->add(std::move(schema))) {
            BROOKESIA_LOGE("Failed to register event: %1%", event_name);
            continue;
        }

        registered_count++;
    }

    BROOKESIA_LOGI("[%1%] Registered %2%/%3% events", attributes_.name, registered_count, total_count);

    return true;
}

bool ServiceBase::unregister_events(const std::vector<std::string> &names)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: names(%1%)", BROOKESIA_DESCRIBE_TO_STR(names));

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), false, "Not initialized");

    // Use write lock to protect the registry modifications
    boost::lock_guard lock(resources_mutex_);
    for (const auto &name : names) {
        event_registry_->remove(name);
    }

    BROOKESIA_LOGI("[%1%] Unregistered %2% events", attributes_.name, names.size());

    return true;
}

bool ServiceBase::publish_event(const std::string &event_name, EventItemMap &&event_items, bool use_dispatch)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event_name(%1%), event_items(%2%), use_dispatch(%3%)", event_name,
        BROOKESIA_DESCRIBE_TO_STR(event_items), BROOKESIA_DESCRIBE_TO_STR(use_dispatch)
    );

    // Thread-safe get the copies of event_registry and task_scheduler
    std::shared_ptr<EventRegistry> registry;
    std::shared_ptr<lib_utils::TaskScheduler> scheduler;
    {
        boost::shared_lock lock(resources_mutex_);

        auto signal = event_registry_->get_signal(event_name);
        BROOKESIA_CHECK_NULL_RETURN(signal, false, "Event signal not found: %1%", event_name);
        if (signal->num_slots() == 0) {
            BROOKESIA_LOGD("Event has no subscribers, skip publish");
            return true;
        }

        registry = event_registry_;
        scheduler = task_scheduler_;
    }
    if (!scheduler || !registry) {
        BROOKESIA_LOGE("Invalid state");
        return false;
    }

    auto *event_schema = registry->get_schema(event_name);
    BROOKESIA_CHECK_NULL_RETURN(event_schema, false, "Event schema not found: %1%", event_name);

    // Validate the event data
    BROOKESIA_CHECK_FALSE_RETURN(
        registry->validate_items(event_name, event_items), false, "Failed to validate event data for: %1%", event_name
    );

    // If connected to the server, publish to the server
    // Should be before the task posted to avoid event_items being moved
    if (is_server_connected()) {
        BROOKESIA_LOGD("Connected to server, publishing event to it");
        BROOKESIA_CHECK_FALSE_EXECUTE(server_connection_->publish_event(event_name, event_items), {}, {
            BROOKESIA_LOGE("Failed to publish event to server: %1%", event_name);
        });
    }

    auto emit_signal_task = [this, registry, event_name, event_items = std::move(event_items)]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        auto signal = registry->get_signal(event_name);
        if (signal) {
            BROOKESIA_CHECK_EXCEPTION_EXECUTE((*signal)(event_name, event_items), {}, {
                BROOKESIA_LOGE("Failed to emit signal for event: %1%", event_name);
            });
        } else {
            BROOKESIA_LOGW("Signal not found for event: %1%", event_name);
        }
    };

    // If the event does not require asynchronous, use sync publish
    if (!event_schema->require_scheduler) {
        BROOKESIA_LOGD("Event '%1%' does not require async, using sync publish", event_name);
        emit_signal_task();
        return true;
    }

    // Check if the event requires asynchronous and if the service is running
    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Service is not running");

    // Emit the local signal
    if (use_dispatch) {
        BROOKESIA_CHECK_FALSE_RETURN(
            scheduler->dispatch(emit_signal_task, nullptr, get_event_task_group()), false,
            "Failed to dispatch emit signal task"
        );
    } else {
        BROOKESIA_CHECK_FALSE_RETURN(
            scheduler->post(emit_signal_task, nullptr, get_event_task_group()), false,
            "Failed to post emit signal task"
        );
    }

    return true;
}

bool ServiceBase::publish_event(const std::string &event_name, std::vector<EventItem> &&data_values, bool use_dispatch)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event_name(%1%), data_values(%2%), use_dispatch(%3%)", event_name,
        BROOKESIA_DESCRIBE_TO_STR(data_values), BROOKESIA_DESCRIBE_TO_STR(use_dispatch)
    );

    auto *event_schema = event_registry_->get_schema(event_name);
    BROOKESIA_CHECK_NULL_RETURN(event_schema, false, "Event schema not found: %1%", event_name);

    // Check if the value count matches
    BROOKESIA_CHECK_FALSE_RETURN(
        data_values.size() == event_schema->items.size(), false,
        "Event value count mismatch: expected %1%, got %2%", event_schema->items.size(), data_values.size()
    );

    // Convert the vector to map according to the event schema data order
    EventItemMap event_items;
    for (size_t i = 0; i < event_schema->items.size(); i++) {
        event_items[event_schema->items[i].name] = std::move(data_values[i]);
    }

    return publish_event(event_name, std::move(event_items), use_dispatch);
}

bool ServiceBase::publish_event(const std::string &event_name, boost::json::object &&data_json, bool use_dispatch)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event_name(%1%), data_json(%2%), use_dispatch(%3%)", event_name,
        BROOKESIA_DESCRIBE_TO_STR(data_json), BROOKESIA_DESCRIBE_TO_STR(use_dispatch)
    );

    EventItemMap event_items;
    BROOKESIA_CHECK_FALSE_RETURN(
        BROOKESIA_DESCRIBE_FROM_JSON(data_json, event_items), false, "Failed to parse data"
    );

    return publish_event(event_name, std::move(event_items), use_dispatch);
}

bool ServiceBase::set_task_scheduler(std::shared_ptr<lib_utils::TaskScheduler> task_scheduler)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(!is_running(), false, "Should not be called when service is running");

    boost::lock_guard lock(resources_mutex_);
    task_scheduler_ = task_scheduler;

    return true;
}

bool ServiceBase::init(std::shared_ptr<lib_utils::TaskScheduler> task_scheduler)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(state_mutex_);
    return init_internal(task_scheduler);
}

bool ServiceBase::init_internal(std::shared_ptr<lib_utils::TaskScheduler> task_scheduler)
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
            task_scheduler_ = std::make_shared<lib_utils::TaskScheduler>(), false, "Failed to create task scheduler"
        );
    } else {
        BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Invalid task scheduler");
        task_scheduler_ = task_scheduler;
    }

    // Create function registry
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        function_registry_ = std::make_shared<FunctionRegistry>(), false, "Failed to create function registry"
    );
    // Create event registry
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        event_registry_ = std::make_shared<EventRegistry>(), false, "Failed to create event registry"
    );

    BROOKESIA_CHECK_FALSE_RETURN(on_init(), false, "Failed to initialize service");

    // Auto-register functions
    auto function_schemas = get_function_schemas();
    auto function_handlers = get_function_handlers();
    if (!function_schemas.empty()) {
        BROOKESIA_CHECK_FALSE_RETURN(
            register_functions(std::move(function_schemas), std::move(function_handlers)), false,
            "Failed to register functions"
        );
    }

    // Auto-register events
    auto event_schemas = get_event_schemas();
    if (!event_schemas.empty()) {
        BROOKESIA_CHECK_FALSE_RETURN(register_events(std::move(event_schemas)), false, "Failed to register events");
    }

    deinit_guard.release();

    BROOKESIA_LOGI("Initialized service: %1%", attributes_.name);

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
        boost::lock_guard lock(resources_mutex_);
        server_connection_.reset();
        task_scheduler_.reset();
        function_registry_.reset();
        event_registry_.reset();
    }

    is_initialized_.store(false);

    BROOKESIA_LOGI("Deinitialized service: %1%", attributes_.name);
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

    is_running_.store(true);

    if (!task_scheduler_->is_running()) {
        BROOKESIA_CHECK_FALSE_RETURN(
            task_scheduler_->start(*attributes_.task_scheduler_config), false, "Failed to start task scheduler"
        );
    }
    BROOKESIA_CHECK_FALSE_RETURN(task_scheduler_->configure_group(get_call_task_group(), {
        .enable_serial_execution = true,
    }), false, "Failed to configure call task group");
    BROOKESIA_CHECK_FALSE_RETURN(task_scheduler_->configure_group(get_event_task_group(), {
        .enable_serial_execution = true,
    }), false, "Failed to configure event task group");
    BROOKESIA_CHECK_FALSE_RETURN(task_scheduler_->configure_group(get_request_task_group(), {
        .enable_serial_execution = true,
    }), false, "Failed to configure request task group");

    BROOKESIA_CHECK_FALSE_RETURN(on_start(), false, "Failed to start service");

    if (is_server_connected()) {
        server_connection_->activate(true);
        try_override_connection_request_handler();
    }

    stop_guard.release();

    BROOKESIA_LOGI("Started service: %1%", attributes_.name);

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

    if (task_scheduler_ && attributes_.task_scheduler_config.has_value()) {
        BROOKESIA_LOGD("Waiting for task scheduler to finish");
        if (!task_scheduler_->wait_all(WAIT_TASK_SCHEDULER_FINISHED_TIMEOUT_MS)) {
            BROOKESIA_LOGW(
                "Task scheduler wait timeout after %1%ms", WAIT_TASK_SCHEDULER_FINISHED_TIMEOUT_MS
            );
        }
        task_scheduler_->stop();
    }

    is_running_ = false;

    BROOKESIA_LOGI("Stopped service: %1%", attributes_.name);
}

std::shared_ptr<rpc::ServerConnection> ServiceBase::connect_to_server()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Use write lock to protect the server_connection_ and registries access
    boost::lock_guard lock(resources_mutex_);

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
    boost::lock_guard lock(resources_mutex_);
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
            task_scheduler_->post(std::move(task), nullptr, get_request_task_group()),
            false, "Failed to post request task"
        );
        return true;
    };
    server_connection_->set_request_handler(request_handler);
}

} // namespace esp_brookesia::service
