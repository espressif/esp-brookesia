/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include <exception>
#include <memory>
#include "boost/chrono.hpp"
#include "private/utils.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_manager/service/manager.hpp"

namespace esp_brookesia::service {

namespace {

FunctionCallResult make_call_result(const std::string &name, FunctionResult &&result)
{
    return {
        .name = name,
        .success = result.success,
        .error_message = std::move(result.error_message),
        .data = std::move(result.data),
    };
}

} // namespace

ServiceBase::~ServiceBase()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    try {
        if (is_initialized()) {
            deinit();
        }
    } catch (const std::exception &e) {
        BROOKESIA_LOGE("Detected exception while destroying service '%1%': %2%", attributes_.name, e.what());
    } catch (...) {
        BROOKESIA_LOGE("Detected unknown exception while destroying service '%1%'", attributes_.name);
    }
}

bool ServiceBase::call_function_async(
    const std::string &name, FunctionParameterMap parameters_map, FunctionResultHandler handler
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%), parameters_map(%2%), handler(%3%)", name, parameters_map, handler);

    std::string error_prefix = (boost::format("[%1%:%2%] ") % attributes_.name % name).str();

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), false, "%1%: service not initialized", error_prefix);

    // Thread-safe get the copies of resources
    std::shared_ptr<FunctionRegistry> registry;
    std::shared_ptr<lib_utils::TaskScheduler> scheduler;
    bool require_scheduler = false;
    {
        boost::shared_lock lock(resources_mutex_);

        BROOKESIA_CHECK_NULL_RETURN(
            function_registry_ && task_scheduler_, false,
            "%1%: function registry or task scheduler is not available", error_prefix
        );

        registry = function_registry_;
        scheduler = task_scheduler_;

        auto *func_schema = registry->get_schema(name);
        BROOKESIA_CHECK_NULL_RETURN(func_schema, false, "%1%: not found", error_prefix);
        require_scheduler = func_schema->require_scheduler;
    }

    // Create the call function task
    auto call_context = get_current_call_context();
    auto call_function_task =
        [this, error_prefix, registry, require_scheduler, name, parameters_map = std::move(parameters_map),
          handler, call_context = std::move(call_context)]() mutable {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        boost::shared_lock lock(state_mutex_);

        FunctionResult result;
        if (is_running() || !require_scheduler)
        {
            ScopedCallContext context_guard(call_context);
            BROOKESIA_CHECK_EXCEPTION_EXECUTE(result = registry->call(name, std::move(parameters_map)), {
                result.success = false;
                result.error_message =
                (boost::format("%1%: detected exception: %2%") % error_prefix % e.what()).str();
            }, {});
        } else
        {
            result.success = false;
            result.error_message = (boost::format("%1%: service is not running") % error_prefix).str();
        }

        // Release the shared state lock before invoking the handler. The function
        // execution above needs state protection, but the handler only delivers the
        // result and may re-enter another subsystem (e.g. the JS backend, which holds
        // its own mutex while driving on_stop -> stop()). Holding state_mutex_ across
        // the handler deadlocks with ServiceBase::stop()'s exclusive state_mutex_ lock.
        if (lock.owns_lock())
        {
            lock.unlock();
        }

        if (handler)
        {
            BROOKESIA_LOGD("Has handler, calling it");
            BROOKESIA_CHECK_EXCEPTION_EXECUTE(handler(std::move(result)), {
                BROOKESIA_LOGE("%1%: detected exception when calling handler: %2%", error_prefix, e.what());
            }, {});
        }

        BROOKESIA_LOGD("%1%: call completed, result: %2%", error_prefix, result);
    };

    // If the function does not require scheduler, invoke the task directly
    if (!require_scheduler) {
        BROOKESIA_LOGD("Not require scheduler, invoke the task directly");
        call_function_task();
        return true;
    }

    // Check if the service is running, otherwise the scheduler will not be able to post the task
    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "%1%: service is not running", error_prefix);

    // Post the call function task to the task scheduler
    auto post_result = scheduler->post(std::move(call_function_task), nullptr, get_call_task_group());
    BROOKESIA_CHECK_FALSE_RETURN(post_result, false, "%1%: failed to post task", error_prefix);

    return true;
}

bool ServiceBase::call_function_async(
    const std::string &name, std::vector<FunctionValue> parameters_values, FunctionResultHandler handler
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%), parameters_values(%2%), handler(%3%)", name, parameters_values, handler);

    std::string error_prefix = (boost::format("[%1%:%2%] ") % attributes_.name % name).str();

    // Thread-safe generate the parameters map
    FunctionParameterMap parameters_map;
    {
        boost::shared_lock lock(resources_mutex_);

        BROOKESIA_CHECK_NULL_RETURN(
            function_registry_, false, "%1%: function registry is not available", error_prefix
        );

        // Get the function schema
        auto *function_schema = function_registry_->get_schema(name);
        BROOKESIA_CHECK_NULL_RETURN(function_schema, false, "%1%: not found", error_prefix);

        // Convert the vector to map according to the function definition parameter order
        auto parameters_size = std::min(parameters_values.size(), function_schema->parameters.size());
        for (size_t i = 0; i < parameters_size; i++) {
            parameters_map[function_schema->parameters[i].name] = std::move(parameters_values[i]);
        }
    }

    return call_function_async(name, std::move(parameters_map), std::move(handler));
}

bool ServiceBase::call_function_async(
    const std::string &name, const boost::json::object &parameters_json, FunctionResultHandler handler
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%), parameters_json(%2%), handler(%3%)", name, parameters_json, handler);

    std::string error_prefix = (boost::format("[%1%:%2%] ") % attributes_.name % name).str();

    // Convert the JSON to map
    FunctionParameterMap parameters_map;
    auto result = BROOKESIA_DESCRIBE_FROM_JSON(parameters_json, parameters_map);
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "%1%: failed to parse parameters", error_prefix);

    return call_function_async(name, std::move(parameters_map), std::move(handler));
}

bool ServiceBase::call_functions_async(std::vector<FunctionCall> calls, FunctionBatchResultHandler handler)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: calls(%1%), handler(%2%)", calls, handler);

    const std::string error_prefix = (boost::format("[%1%:batch] ") % attributes_.name).str();

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), false, "%1%: service not initialized", error_prefix);

    std::shared_ptr<FunctionRegistry> registry;
    std::shared_ptr<lib_utils::TaskScheduler> scheduler;
    bool require_scheduler = false;
    {
        boost::shared_lock lock(resources_mutex_);

        BROOKESIA_CHECK_NULL_RETURN(
            function_registry_ && task_scheduler_, false,
            "%1%: function registry or task scheduler is not available", error_prefix
        );

        registry = function_registry_;
        scheduler = task_scheduler_;

        for (const auto &call : calls) {
            auto *func_schema = registry->get_schema(call.name);
            BROOKESIA_CHECK_NULL_RETURN(
                func_schema, false, "%1%: function '%2%' not found", error_prefix, call.name
            );
            require_scheduler = require_scheduler || func_schema->require_scheduler;
        }
    }

    auto call_context = get_current_call_context();
    auto call_functions_task =
        [this, error_prefix, registry, require_scheduler, calls = std::move(calls),
          handler, call_context = std::move(call_context)]() mutable {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        boost::shared_lock lock(state_mutex_);

        FunctionBatchResult batch_result;
        batch_result.success = true;

        if (!is_running() && require_scheduler)
        {
            batch_result.success = false;
            batch_result.results.push_back({
                .name = "",
                .success = false,
                .error_message = (boost::format("%1%: service is not running") % error_prefix).str(),
            });
        } else
        {
            ScopedCallContext context_guard(call_context);
            for (auto &call : calls) {
                FunctionResult result;
                BROOKESIA_CHECK_EXCEPTION_EXECUTE(result = registry->call(call.name, std::move(call.parameters)), {
                    result.success = false;
                    result.error_message =
                    (boost::format("%1%: function '%2%' detected exception: %3%") %
                     error_prefix % call.name % e.what()).str();
                }, {});

                const bool success = result.success;
                batch_result.results.push_back(make_call_result(call.name, std::move(result)));
                if (!success) {
                    batch_result.success = false;
                    break;
                }
            }
        }

        if (handler)
        {
            BROOKESIA_LOGD("Has handler, calling it");
            BROOKESIA_CHECK_EXCEPTION_EXECUTE(handler(std::move(batch_result)), {
                BROOKESIA_LOGE("%1%: detected exception when calling batch handler: %2%", error_prefix, e.what());
            }, {});
        }
    };

    if (!require_scheduler) {
        BROOKESIA_LOGD("No function requires scheduler, invoke the batch directly");
        call_functions_task();
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "%1%: service is not running", error_prefix);

    auto post_result = scheduler->post(std::move(call_functions_task), nullptr, get_call_task_group());
    BROOKESIA_CHECK_FALSE_RETURN(post_result, false, "%1%: failed to post batch task", error_prefix);

    return true;
}

FunctionResult ServiceBase::call_function_sync(
    const std::string &name, FunctionParameterMap parameters_map, uint32_t timeout_ms
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%), parameters_map(%2%), timeout_ms(%3%)", name, parameters_map, timeout_ms);

    std::string error_prefix = (boost::format("[%1%:%2%] ") % attributes_.name % name).str();

    // Thread-safe get the copies of resources
    std::shared_ptr<FunctionRegistry> registry;
    bool require_scheduler = false;
    uint32_t effective_timeout_ms = timeout_ms;
    {
        boost::shared_lock lock(resources_mutex_);

        BROOKESIA_CHECK_NULL_RETURN(function_registry_, (FunctionResult{
            .success = false,
            .error_message = "%1%: function registry is not available",
        }), "%1%: function registry is not available", error_prefix);

        registry = function_registry_;

        auto *func_schema = registry->get_schema(name);
        BROOKESIA_CHECK_NULL_RETURN(func_schema, (FunctionResult{
            .success = false,
            .error_message = "Function not found",
        }), "%1%: not found", error_prefix);
        require_scheduler = func_schema->require_scheduler;
        if (effective_timeout_ms == 0) {
            effective_timeout_ms = func_schema->default_timeout_ms.value_or(
                                       BROOKESIA_SERVICE_MANAGER_DEFAULT_CALL_FUNCTION_TIMEOUT_MS
                                   );
        }
    }

    using ResultPromise = boost::promise<FunctionResult>;
    std::shared_ptr<ResultPromise> result_promise;
    BROOKESIA_CHECK_EXCEPTION_RETURN(result_promise = std::make_shared<ResultPromise>(), (FunctionResult{
        .success = false,
        .error_message = "No memory",
    }), "%1%: no memory", error_prefix);

    auto result_future = result_promise->get_future();
    auto result_handler = [this, result_promise](FunctionResult && result) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        result_promise->set_value(std::move(result));
    };

    auto async_result = call_function_async(name, std::move(parameters_map), result_handler);
    BROOKESIA_CHECK_FALSE_RETURN(async_result, (FunctionResult{
        .success = false,
        .error_message = "Failed to call function asynchronously",
    }), "%1%: failed to call function asynchronously", error_prefix);
    if (require_scheduler) {
        BROOKESIA_LOGD("Require scheduler, wait for the result");
        auto wait_result = result_future.wait_for(boost::chrono::milliseconds(effective_timeout_ms));
        BROOKESIA_CHECK_FALSE_RETURN(wait_result == boost::future_status::ready, (FunctionResult{
            .success = false,
            .error_message = "Wait timeout after " + std::to_string(effective_timeout_ms) + " ms",
        }), "%1%: wait timeout", error_prefix);
    }

    return result_future.get();
}

FunctionResult ServiceBase::call_function_sync(
    const std::string &name, std::vector<FunctionValue> parameters_values, uint32_t timeout_ms
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%), parameters_values(%2%), timeout_ms(%3%)", name, parameters_values, timeout_ms);

    std::string error_prefix = (boost::format("[%1%:%2%] ") % attributes_.name % name).str();

    // Thread-safe generate the parameters map
    FunctionParameterMap parameters_map;
    {
        boost::shared_lock lock(resources_mutex_);

        BROOKESIA_CHECK_NULL_RETURN(
        function_registry_, (FunctionResult{
            .success = false,
            .error_message = "%1%: function registry is not available",
        }), "%1%: function registry is not available", error_prefix
        );

        // Get the function schema
        auto *function_schema = function_registry_->get_schema(name);
        BROOKESIA_CHECK_NULL_RETURN(function_schema, (FunctionResult{
            .success = false,
            .error_message = "Function not found",
        }), "%1%: not found", error_prefix);

        // Convert the vector to map according to the function definition parameter order
        auto parameters_size = std::min(parameters_values.size(), function_schema->parameters.size());
        for (size_t i = 0; i < parameters_size; i++) {
            parameters_map[function_schema->parameters[i].name] = std::move(parameters_values[i]);
        }
    }

    return call_function_sync(name, std::move(parameters_map), timeout_ms);
}

FunctionResult ServiceBase::call_function_sync(
    const std::string &name, const boost::json::object &parameters_json, uint32_t timeout_ms
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%), parameters_json(%2%), timeout_ms(%3%)", name, parameters_json, timeout_ms);

    std::string error_prefix = (boost::format("[%1%:%2%] ") % attributes_.name % name).str();

    // Convert the JSON to map
    FunctionParameterMap parameters_map;
    auto result = BROOKESIA_DESCRIBE_FROM_JSON(parameters_json, parameters_map);
    BROOKESIA_CHECK_FALSE_RETURN(result, (FunctionResult{
        .success = false,
        .error_message = "%1%: failed to parse parameters",
    }), "%1%: failed to parse parameters", error_prefix);

    return call_function_sync(name, std::move(parameters_map), timeout_ms);
}

FunctionBatchResult ServiceBase::call_functions_sync(std::vector<FunctionCall> calls, uint32_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: calls(%1%), timeout_ms(%2%)", calls, timeout_ms);

    const std::string error_prefix = (boost::format("[%1%:batch] ") % attributes_.name).str();

    using ResultPromise = boost::promise<FunctionBatchResult>;
    std::shared_ptr<ResultPromise> result_promise;
    BROOKESIA_CHECK_EXCEPTION_RETURN(result_promise = std::make_shared<ResultPromise>(), (FunctionBatchResult{
        .success = false,
        .results = {{
                .name = "",
                .success = false,
                .error_message = "No memory",
            }
        },
    }), "%1%: no memory", error_prefix);

    auto result_future = result_promise->get_future();
    auto result_handler = [this, result_promise](FunctionBatchResult && result) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        result_promise->set_value(std::move(result));
    };

    auto async_result = call_functions_async(std::move(calls), result_handler);
    BROOKESIA_CHECK_FALSE_RETURN(async_result, (FunctionBatchResult{
        .success = false,
        .results = {{
                .name = "",
                .success = false,
                .error_message = "Failed to call functions asynchronously",
            }
        },
    }), "%1%: failed to call functions asynchronously", error_prefix);

    auto wait_result = result_future.wait_for(boost::chrono::milliseconds(timeout_ms));
    BROOKESIA_CHECK_FALSE_RETURN(wait_result == boost::future_status::ready, (FunctionBatchResult{
        .success = false,
        .results = {{
                .name = "",
                .success = false,
                .error_message = "Wait timeout after " + std::to_string(timeout_ms) + " ms",
            }
        },
    }), "%1%: wait timeout", error_prefix);

    return result_future.get();
}

EventRegistry::SignalConnection ServiceBase::subscribe_event(
    const std::string &event_name, const EventRegistry::SignalSlot &slot
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event_name(%1%), slot(%2%)", event_name, BROOKESIA_DESCRIBE_TO_STR(slot));

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), EventRegistry::SignalConnection(), "Not initialized");

    std::string error_prefix = (boost::format("[%1%:%2%] ") % attributes_.name % event_name).str();

    // Thread-safe get the copy of event_registry
    std::shared_ptr<EventRegistry> registry;
    {
        boost::shared_lock lock(resources_mutex_);
        registry = event_registry_;
    }
    if (!registry) {
        BROOKESIA_LOGE("%1%: invalid state", error_prefix);
        return EventRegistry::SignalConnection();
    }

    auto signal = registry->get_signal(event_name);
    BROOKESIA_CHECK_NULL_RETURN(
        signal, EventRegistry::SignalConnection(), "%1%: event signal not found", error_prefix
    );

    auto connection = signal->connect(slot);
    if (connection.connected()) {
        on_event_subscribed(event_name);
    }

    return connection;
}

bool ServiceBase::register_functions(std::vector<FunctionSchema> schemas, FunctionHandlerMap handlers)
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

    BROOKESIA_LOGD("[%1%] Registered %2%/%3% functions", attributes_.name, registered_count, total_count);

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

    BROOKESIA_LOGD("[%1%] Unregistered %2% functions", attributes_.name, names.size());

    return true;
}

bool ServiceBase::register_events(std::vector<EventSchema> schemas)
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

    for (auto &schema : schemas) {
        // Save name before moving (to avoid use-after-move in error logs)
        const std::string event_name = schema.name;
        BROOKESIA_LOGD("Registering event: %1%", event_name);

        if (!event_registry_->add(std::move(schema))) {
            BROOKESIA_LOGE("Failed to register event: %1%", event_name);
            continue;
        }

        registered_count++;
    }

    BROOKESIA_LOGD("[%1%] Registered %2%/%3% events", attributes_.name, registered_count, total_count);

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

    BROOKESIA_LOGD("[%1%] Unregistered %2% events", attributes_.name, names.size());

    return true;
}

bool ServiceBase::publish_event(const std::string &event_name, EventItemMap event_items, bool use_dispatch)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event_name(%1%), event_items(%2%), use_dispatch(%3%)", event_name,
        BROOKESIA_DESCRIBE_TO_STR(event_items), BROOKESIA_DESCRIBE_TO_STR(use_dispatch)
    );

    // Thread-safe get the copies of event_registry and task_scheduler
    std::shared_ptr<EventRegistry> registry;
    std::shared_ptr<lib_utils::TaskScheduler> scheduler;
    bool require_scheduler = false;
    {
        boost::shared_lock lock(resources_mutex_);

        BROOKESIA_CHECK_NULL_RETURN(
            event_registry_ && task_scheduler_, false,
            "Event '%1%': event registry or task scheduler is not available", event_name
        );

        auto signal = event_registry_->get_signal(event_name);
        BROOKESIA_CHECK_NULL_RETURN(signal, false, "Event '%1%': signal not found", event_name);
        if (signal->num_slots() == 0) {
            BROOKESIA_LOGD("Event '%1%': has no subscribers, skip publish", event_name);
            return true;
        }

        registry = event_registry_;
        scheduler = task_scheduler_;

        auto *event_schema = registry->get_schema(event_name);
        BROOKESIA_CHECK_NULL_RETURN(event_schema, false, "Event '%1%': schema not found", event_name);
        require_scheduler = event_schema->require_scheduler;
    }

    // Validate the event data
    BROOKESIA_CHECK_FALSE_RETURN(
        registry->validate_items(event_name, event_items), false, "Event '%1%': failed to validate data", event_name
    );

    auto emit_signal_task = [this, registry, require_scheduler, event_name, event_items = std::move(event_items)]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        boost::shared_lock lock(state_mutex_);

        if (is_running() || !require_scheduler) {
            auto signal = registry->get_signal(event_name);
            if (signal) {
                BROOKESIA_CHECK_EXCEPTION_EXECUTE((*signal)(event_name, event_items), {}, {
                    BROOKESIA_LOGE("Event '%1%': failed to emit signal", event_name);
                });
            } else {
                BROOKESIA_LOGW("Event '%1%': signal not found", event_name);
            }
        } else {
            BROOKESIA_LOGW("Event '%1%': service is not running, skip emit signal", event_name);
        }
    };

    // If the event does not require scheduler, invoke the task directly
    if (!require_scheduler) {
        BROOKESIA_LOGD("Not require scheduler, invoke the task directly");
        emit_signal_task();
        return true;
    }

    // Check if the service is running, otherwise the scheduler will not be able to post the task
    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Event '%1%': service is not running", event_name);

    // Post the emit signal task to the task scheduler
    if (use_dispatch) {
        BROOKESIA_CHECK_FALSE_RETURN(
            scheduler->dispatch(emit_signal_task, nullptr, get_event_task_group()), false,
            "Event '%1%': failed to dispatch emit signal task", event_name
        );
    } else {
        BROOKESIA_CHECK_FALSE_RETURN(
            scheduler->post(emit_signal_task, nullptr, get_event_task_group()), false,
            "Event '%1%': failed to post emit signal task", event_name
        );
    }

    return true;
}

bool ServiceBase::publish_event(const std::string &event_name, std::vector<EventItem> data_values, bool use_dispatch)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event_name(%1%), data_values(%2%), use_dispatch(%3%)", event_name,
        BROOKESIA_DESCRIBE_TO_STR(data_values), BROOKESIA_DESCRIBE_TO_STR(use_dispatch)
    );

    // Thread-safe generate the event items map
    EventItemMap event_items;
    {
        boost::shared_lock lock(resources_mutex_);

        BROOKESIA_CHECK_NULL_RETURN(
            event_registry_, false, "Event '%1%': event registry is not available", event_name
        );

        auto *event_schema = event_registry_->get_schema(event_name);
        BROOKESIA_CHECK_NULL_RETURN(event_schema, false, "Event '%1%': schema not found", event_name);

        // Check if the value count matches
        BROOKESIA_CHECK_FALSE_RETURN(
            data_values.size() == event_schema->items.size(), false,
            "Event '%1%': value count mismatch: expected %2%, got %3%", event_name, event_schema->items.size(),
            data_values.size()
        );

        // Convert the vector to map according to the event schema data order
        for (size_t i = 0; i < event_schema->items.size(); i++) {
            event_items[event_schema->items[i].name] = std::move(data_values[i]);
        }
    }

    return publish_event(event_name, std::move(event_items), use_dispatch);
}

bool ServiceBase::publish_event(const std::string &event_name, boost::json::object data_json, bool use_dispatch)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event_name(%1%), data_json(%2%), use_dispatch(%3%)", event_name,
        BROOKESIA_DESCRIBE_TO_STR(data_json), BROOKESIA_DESCRIBE_TO_STR(use_dispatch)
    );

    EventItemMap event_items;
    BROOKESIA_CHECK_FALSE_RETURN(
        BROOKESIA_DESCRIBE_FROM_JSON(data_json, event_items), false, "Event '%1%': failed to parse data", event_name
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

    boost::unique_lock<boost::shared_mutex> lock(state_mutex_);
    deinit_internal_locked(&lock);
}

void ServiceBase::deinit_internal()
{
    deinit_internal_locked(nullptr);
}

void ServiceBase::deinit_internal_locked(boost::unique_lock<boost::shared_mutex> *state_lock)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_initialized()) {
        BROOKESIA_LOGD("Already deinitialized");
        return;
    }

    if (is_running()) {
        // stop_internal may release `state_lock` to drain the scheduler safely.
        stop_internal(state_lock);
        // Re-acquire the lock if it was released so the rest of deinit runs
        // serialized against other state transitions.
        if (state_lock != nullptr && !state_lock->owns_lock()) {
            state_lock->lock();
        }
    }

    on_deinit();

    // Use write lock to protect the registry reset
    {
        boost::lock_guard lock(resources_mutex_);
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

    // Acquire write lock to protect resource access and prevent concurrent execution with call_function_task
    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler;
    {
        boost::lock_guard lock(resources_mutex_);

        // Get copies of resources under lock
        task_scheduler = task_scheduler_;
    }

    // Start and configure task scheduler outside lock to avoid holding lock during potentially long operations
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Task scheduler is not available");
    if (!task_scheduler->is_running()) {
        BROOKESIA_CHECK_FALSE_RETURN(
            get_attributes().has_scheduler(), false,
            "Service does not have own scheduler and also no one provided"
        );
        BROOKESIA_CHECK_FALSE_RETURN(
            task_scheduler->start(get_attributes().get_scheduler_config()), false, "Failed to start task scheduler"
        );
    }
    BROOKESIA_CHECK_FALSE_RETURN(task_scheduler->configure_group(get_call_task_group(), {
        .enable_serial_execution = true,
    }), false, "Failed to configure call task group");
    BROOKESIA_CHECK_FALSE_RETURN(task_scheduler->configure_group(get_event_task_group(), {
        .enable_serial_execution = true,
    }), false, "Failed to configure event task group");
    BROOKESIA_CHECK_FALSE_RETURN(task_scheduler->configure_group(get_request_task_group(), {
        .enable_serial_execution = true,
    }), false, "Failed to configure request task group");

    BROOKESIA_CHECK_FALSE_RETURN(on_start(), false, "Failed to start service");

    stop_guard.release();

    BROOKESIA_LOGI("Started service: %1%", attributes_.name);

    return true;
}

void ServiceBase::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::unique_lock<boost::shared_mutex> lock(state_mutex_);
    stop_internal(&lock);
}

void ServiceBase::stop_internal()
{
    stop_internal(nullptr);
}

void ServiceBase::stop_internal(boost::unique_lock<boost::shared_mutex> *state_lock)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        BROOKESIA_LOGD("Already stopped");
        return;
    }

    on_stop();

    // Acquire write lock to prevent concurrent execution with call_function_task
    // This ensures on_stop() can safely clean up resources without race conditions
    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler;
    {
        boost::lock_guard lock(resources_mutex_);

        // Get copies of resources under lock
        task_scheduler = task_scheduler_;
    }

    // Mark the service as stopped now so any pending publish_event::emit_signal_task
    // that is blocked acquiring shared_lock(state_mutex_) will, once we release the
    // unique lock below, see is_running()==false and skip its emit instead of
    // touching torn-down state.
    is_running_.store(false);

    // Release the caller's unique lock on state_mutex_ before the potentially long
    // task_scheduler->stop() call. The scheduler joins its worker threads; if any
    // worker is blocked on shared_lock(state_mutex_) (e.g. a queued emit_signal_task
    // posted from on_stop()), holding the unique lock here would deadlock the join.
    if (state_lock != nullptr && state_lock->owns_lock()) {
        state_lock->unlock();
    }

    // Stop task scheduler outside lock to avoid holding lock during potentially long operations
    if (task_scheduler) {
        if (get_attributes().has_scheduler()) {
            BROOKESIA_LOGD("Has own task scheduler, stop it");
            task_scheduler->stop();
        } else {
            BROOKESIA_LOGD("No own task scheduler, cancel task groups");
            task_scheduler->cancel_group(get_call_task_group());
            task_scheduler->cancel_group(get_event_task_group());
            task_scheduler->cancel_group(get_request_task_group());
        }
    }

    BROOKESIA_LOGI("Stopped service: %1%", attributes_.name);
}

} // namespace esp_brookesia::service
