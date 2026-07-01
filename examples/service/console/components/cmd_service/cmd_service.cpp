/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <string>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "cmd_service.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"

using namespace esp_brookesia::service;
using namespace esp_brookesia;

static const char *TAG = "cmd_service";
static auto &service_manager = ServiceManager::get_instance();
static std::vector<ServiceBinding> g_bindings;
static lib_utils::TaskScheduler g_task_scheduler;
static constexpr int DEFAULT_SVC_CALL_TIMEOUT_MS = 5000;

// Subscription information structure
struct SubscriptionInfo {
    std::string service;
    std::string event;
    EventRegistry::SignalConnection connection;

    SubscriptionInfo(std::string svc, std::string evt, EventRegistry::SignalConnection conn)
        : service(std::move(svc))
        , event(std::move(evt))
        , connection(std::move(conn))
    {}
};

static std::vector<std::unique_ptr<SubscriptionInfo>> g_subscriptions;

// Scheduled call task information structure
struct ScheduledCallInfo {
    lib_utils::TaskScheduler::TaskId task_id;
    std::string service;
    std::string function;
    std::string params;
    bool is_periodic;
    int delay_or_interval_ms;
    int timeout_ms;

    ScheduledCallInfo(
        lib_utils::TaskScheduler::TaskId id, std::string svc, std::string func, std::string p, bool periodic,
        int ms, int timeout
    )
        : task_id(id)
        , service(std::move(svc))
        , function(std::move(func))
        , params(std::move(p))
        , is_periodic(periodic)
        , delay_or_interval_ms(ms)
        , timeout_ms(timeout)
    {}
};

static std::map<lib_utils::TaskScheduler::TaskId, std::unique_ptr<ScheduledCallInfo>> g_scheduled_calls;

// ============================================================================
// Argument structures
// ============================================================================

static struct {
    struct arg_end *end;
} list_services_args;

static struct {
    struct arg_str *service;
    struct arg_end *end;
} list_functions_args;

static struct {
    struct arg_str *service;
    struct arg_end *end;
} list_events_args;

static struct {
    struct arg_str *service;
    struct arg_str *function;
    struct arg_str *params;
    struct arg_int *delay;
    struct arg_int *interval;
    struct arg_int *timeout;
    struct arg_end *end;
} call_args;

static struct {
    struct arg_str *service;
    struct arg_end *end;
} stop_args;

static struct {
    struct arg_str *service;
    struct arg_str *event;
    struct arg_int *timeout;
    struct arg_end *end;
} subscribe_args;

static struct {
    struct arg_str *service;
    struct arg_str *event;
    struct arg_int *timeout;
    struct arg_end *end;
} unsubscribe_args;

static struct {
    struct arg_str *task_id;
    struct arg_end *end;
} call_cancel_args;

static struct {
    struct arg_end *end;
} call_list_args;

// ============================================================================
// Helper functions
// ============================================================================

/**
 * @brief Get or create a binding for a service
 * @param service_name Service name
 * @return Pointer to service, or nullptr if failed
 */
static std::shared_ptr<ServiceBase> get_or_bind_service(const char *service_name)
{
    // Check if service is already bound
    auto it = std::find_if(g_bindings.begin(), g_bindings.end(),
    [service_name](const ServiceBinding & binding) {
        return binding.get_service() &&
               binding.get_service()->get_attributes().name == service_name;
    });

    if (it != g_bindings.end()) {
        ESP_LOGD(TAG, "Reusing existing binding for '%s'", service_name);
        return it->get_service();
    }

    // Create new binding
    ESP_LOGD(TAG, "Trying to create new binding for '%s'", service_name);
    auto binding = service_manager.bind(service_name);
    if (!binding.is_valid()) {
        ESP_LOGW(TAG, "Failed to bind service '%s', trying to get from service manager", service_name);
        auto service = ServiceManager::get_instance().get_service(service_name);
        if (!service) {
            ESP_LOGE(TAG, "Failed to get service '%s'", service_name);
            return nullptr;
        }
        return service;
    }

    auto service = binding.get_service();

    // Move binding into global vector
    ESP_LOGD(TAG, "Moving binding into g_bindings (current size: %zu)", g_bindings.size());
    g_bindings.push_back(std::move(binding));
    ESP_LOGD(TAG, "ServiceBinding moved successfully (new size: %zu)", g_bindings.size());

    return service;
}


// ============================================================================
// Command implementations
// ============================================================================

/**
 * @brief List all registered services
 */
static int do_list_services_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&list_services_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, list_services_args.end, argv[0]);
        return 1;
    }

    printf("\n");
    printf("=== Registered Services ===\n");

    auto all_services = ServiceRegistry::get_all_instances();
    if (all_services.empty()) {
        printf("No services registered\n");
        return 0;
    }

    for (const auto &[name, service] : all_services) {
        printf("  %-20s", name.c_str());

        if (service->is_initialized()) {
            printf(" [initialized]");
        }
        if (service->is_running()) {
            printf(" [running]");
        }
        printf("\n");
    }

    printf("\nTotal: %zu service(s)\n", all_services.size());
    printf("\n");

    return 0;
}

/**
 * @brief List all functions in a service
 */
static int do_list_functions_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&list_functions_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, list_functions_args.end, argv[0]);
        return 1;
    }

    const char *service_name = list_functions_args.service->sval[0];

    // Try to get service instance
    auto service = get_or_bind_service(service_name);
    if (!service) {
        printf("Error: Service '%s' not found\n", service_name);
        return 1;
    }

    auto functions = service->get_function_schemas();

    printf("\n");
    printf("=== Functions in service '%s' ===\n", service_name);

    if (functions.empty()) {
        printf("No functions available\n");
        return 0;
    }

    for (const auto &func : functions) {
        printf("\n  Function: %s\n", func.name.c_str());

        if (!func.description.empty()) {
            printf("    Description: %s\n", func.description.c_str());
        }

        printf("    Parameters:\n");
        for (const auto &param : func.parameters) {
            printf("      %s: %s\n", param.name.c_str(), param.description.c_str());
            printf("        Type: %s\n", BROOKESIA_DESCRIBE_TO_STR(param.type).c_str());
            if (param.default_value.has_value()) {
                printf("        Default: %s\n",
                       BROOKESIA_DESCRIBE_TO_STR(param.default_value.value()).c_str());
            }
        }
    }

    printf("\nTotal: %zu function(s)\n", functions.size());
    printf("\n");

    return 0;
}

/**
 * @brief List all events in a service
 */
static int do_list_events_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&list_events_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, list_events_args.end, argv[0]);
        return 1;
    }

    const char *service_name = list_events_args.service->sval[0];

    // Try to get service instance
    auto service = get_or_bind_service(service_name);
    if (!service) {
        printf("Error: Service '%s' not found\n", service_name);
        return 1;
    }

    auto events = service->get_event_schemas();

    printf("\n");
    printf("=== Events in service '%s' ===\n", service_name);

    if (events.empty()) {
        printf("No events available\n");
        return 0;
    }

    for (const auto &event : events) {
        printf("\n  Event: %s\n", event.name.c_str());

        if (!event.description.empty()) {
            printf("    Description: %s\n", event.description.c_str());
        }

        if (!event.items.empty()) {
            printf("    Parameters:\n");
            for (const auto &item : event.items) {
                printf("      %s: %s\n", item.name.c_str(), item.description.c_str());
                printf("        Type: %s\n", BROOKESIA_DESCRIBE_TO_STR(item.type).c_str());
            }
        } else {
            printf("    Parameters: (none)\n");
        }
    }

    printf("\nTotal: %zu event(s)\n", events.size());
    printf("\n");

    return 0;
}

/**
 * @brief Stop and release a service binding
 */
static int do_stop_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&stop_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, stop_args.end, argv[0]);
        return 1;
    }

    const char *service_name = stop_args.service->sval[0];

    // Find the binding for the service
    auto it = std::find_if(g_bindings.begin(), g_bindings.end(),
    [service_name](const ServiceBinding & binding) {
        return binding.get_service() &&
               binding.get_service()->get_attributes().name == service_name;
    });

    if (it == g_bindings.end()) {
        printf("Error: Service '%s' is not bound\n", service_name);
        return 1;
    }

    // Release the binding
    ESP_LOGI(TAG, "Releasing binding for service '%s'", service_name);
    it->release();

    // Remove from vector
    g_bindings.erase(it);
    ESP_LOGI(TAG, "ServiceBinding released successfully for '%s'", service_name);

    printf("\nService '%s' binding released successfully\n", service_name);
    printf("Remaining bindings: %zu\n\n", g_bindings.size());

    return 0;
}

/**
 * @brief Subscribe to a service event
 */
static int do_subscribe_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&subscribe_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, subscribe_args.end, argv[0]);
        return 1;
    }

    std::string service_name = std::string(subscribe_args.service->sval[0]);
    std::string event_name = std::string(subscribe_args.event->sval[0]);

    // Try to get service instance
    auto service = get_or_bind_service(service_name.c_str());
    if (!service) {
        printf("Error: Service '%s' not found\n", service_name.c_str());
        return 1;
    }

    // Check if already subscribed to this event
    auto it = std::find_if(g_subscriptions.begin(), g_subscriptions.end(),
    [service_name, event_name](const std::unique_ptr<SubscriptionInfo> &sub) {
        return sub->service == service_name && sub->event == event_name;
    });

    if (it != g_subscriptions.end()) {
        if (it->get()->connection.connected()) {
            printf("Error: Already subscribed to '%s.%s'\n", service_name.c_str(), event_name.c_str());
            printf("  Use 'svc_unsubscribe %s %s' to unsubscribe first\n\n", service_name.c_str(), event_name.c_str());
            return 1;
        } else {
            g_subscriptions.erase(it);
            printf("Invalid subscription found, removed\n");
        }
    }

    // Create event handler callback
    auto event_handler = [service_name, event_name](const std::string & evt_name, const EventItemMap & data_map) {
        ESP_LOGI(TAG, "Event received: %s.%s with %zu parameters", service_name.c_str(), event_name.c_str(), data_map.size());
        printf("\n[Event] %s.%s\n", service_name.c_str(), event_name.c_str());
        printf("  Parameters:\n");

        for (const auto &[key, value] : data_map) {
            printf("    %s: %s\n", key.c_str(), BROOKESIA_DESCRIBE_TO_STR(value).c_str());
        }
        printf("\n");
    };

    // Subscribe to event
    printf("\nSubscribing to: %s.%s\n", service_name.c_str(), event_name.c_str());

    try {
        EventRegistry::SignalConnection connection = service->subscribe_event(
                    event_name,
                    event_handler
                );

        if (!connection.connected()) {
            printf("Error: Failed to subscribe to event '%s.%s'\n", service_name.c_str(), event_name.c_str());
            return 1;
        }

        // Save subscription information
        g_subscriptions.push_back(
            std::make_unique<SubscriptionInfo>(service_name, event_name, std::move(connection))
        );
        ESP_LOGI(TAG, "Subscribed successfully");

        printf("Successfully subscribed!\n");
        printf("  Total subscriptions: %zu\n\n", g_subscriptions.size());

        return 0;
    } catch (const std::exception &e) {
        printf("\nError: %s\n\n", e.what());
        return 1;
    }
}

/**
 * @brief Unsubscribe from service events
 */
static int do_unsubscribe_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&unsubscribe_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, unsubscribe_args.end, argv[0]);
        return 1;
    }

    std::string service_name = std::string(unsubscribe_args.service->sval[0]);
    std::string event_name = std::string(unsubscribe_args.event->sval[0]);

    // Find subscription by service and event name
    auto it = std::find_if(g_subscriptions.begin(), g_subscriptions.end(),
    [service_name, event_name](const std::unique_ptr<SubscriptionInfo> &sub) {
        return sub->service == service_name && sub->event == event_name;
    });

    if (it == g_subscriptions.end()) {
        printf("Error: No active subscription found for '%s.%s'\n", service_name.c_str(), event_name.c_str());
        return 1;
    }

    // Unsubscribe from event
    printf("\nUnsubscribing from: %s.%s\n", service_name.c_str(), event_name.c_str());

    try {
        // Disconnect the signal connection
        (*it)->connection.disconnect();

        // Remove subscription from our list
        g_subscriptions.erase(it);

        ESP_LOGI(TAG, "Unsubscribed successfully");
        printf("Successfully unsubscribed!\n");
        printf("  Remaining subscriptions: %zu\n\n", g_subscriptions.size());

        return 0;
    } catch (const std::exception &e) {
        printf("\nError: %s\n\n", e.what());
        return 1;
    }
}

/**
 * @brief Call a service function
 */
static int do_call_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&call_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, call_args.end, argv[0]);
        return 1;
    }

    const char *service_name = call_args.service->sval[0];
    const char *function_name = call_args.function->sval[0];
    const char *params_str = call_args.params->count > 0 ?
                             call_args.params->sval[0] : "{}";
    int delay_ms = call_args.delay->count > 0 ? call_args.delay->ival[0] : -1;
    int interval_ms = call_args.interval->count > 0 ? call_args.interval->ival[0] : -1;
    int timeout_ms = call_args.timeout->count > 0 ? call_args.timeout->ival[0] : DEFAULT_SVC_CALL_TIMEOUT_MS;

    // Validate parameters
    if (delay_ms >= 0 && interval_ms >= 0) {
        printf("Error: Cannot specify both delay and interval. Use either --delay or --interval\n");
        return 1;
    }
    if (timeout_ms <= 0) {
        printf("Error: Timeout must be a positive integer\n");
        return 1;
    }

    if (delay_ms < 0 && interval_ms < 0) {
        // Immediate execution (original behavior)
        delay_ms = -1;
        interval_ms = -1;
    }

    // Try to get service instance
    auto service = get_or_bind_service(service_name);
    if (!service) {
        printf("Error: Service '%s' not found\n", service_name);
        return 1;
    }

    // Parse JSON parameters
    boost::json::object params_json;
    try {
        if (strlen(params_str) > 0 && strcmp(params_str, "{}") != 0) {
            auto parsed = boost::json::parse(params_str);
            if (parsed.is_object()) {
                params_json = parsed.as_object();
            } else {
                printf("Error: Parameters must be a JSON object\n");
                printf("Example: svc_call audio set_volume {\"volume\":80}\n");
                return 1;
            }
        }
    } catch (const std::exception &e) {
        printf("Error: Invalid JSON parameters: %s\n", e.what());
        printf("Example: svc_call audio set_volume {\"volume\":80}\n");
        return 1;
    }

    // Convert JSON to FunctionParameterMap
    FunctionParameterMap parameters;
    for (const auto &[key, value] : params_json) {
        if (value.is_bool()) {
            parameters[key] = FunctionValue(value.as_bool());
        } else if (value.is_number()) {
            double number_value = 0;
            switch (value.kind()) {
            case boost::json::kind::int64:
                number_value = static_cast<double>(value.as_int64());
                break;
            case boost::json::kind::uint64:
                number_value = static_cast<double>(value.as_uint64());
                break;
            default:
                number_value = value.as_double();
                break;
            }
            parameters[key] = FunctionValue(number_value);
        } else if (value.is_string()) {
            parameters[key] = FunctionValue(std::string(value.as_string()));
        } else if (value.is_object()) {
            parameters[key] = FunctionValue(value.as_object());
        } else if (value.is_array()) {
            parameters[key] = FunctionValue(value.as_array());
        }
    }

    // Create call function closure
    // Copy function_name and parameters to ensure they remain valid for delayed/periodic execution
    std::string function_name_str(function_name);
    std::string service_name_str(service_name);
    std::string params_str_copy(params_str);

    // Common call function (used by both delayed and periodic)
    // Copy parameters to ensure they remain valid for delayed/periodic execution
    auto call_function_common = [service, function_name_str, parameters, timeout_ms]() mutable {
        try
        {
            auto result = service->call_function_sync(
                function_name_str.c_str(),
                std::move(parameters),  // Move parameters (mutable lambda allows this)
                timeout_ms
            );

            printf("\n[%s.%s] Result:\n", service->get_attributes().name.c_str(), function_name_str.c_str());
            if (result.success) {
                printf("  - Success!\n");
                if (result.has_data()) {
                    printf("  - Value: %s\n",
                           BROOKESIA_DESCRIBE_TO_STR(result.data.value()).c_str());
                }
            } else {
                printf("  - Error: %s\n", result.error_message.c_str());
            }
            printf("\n");
        } catch (const std::exception &e)
        {
            printf("\n[%s.%s] Error: %s\n\n", service->get_attributes().name.c_str(), function_name_str.c_str(), e.what());
        }
    };

    // Execute based on mode
    if (interval_ms >= 0) {
        // Periodic execution
        printf("\nScheduling periodic call: %s.%s(%s) every %d ms, timeout %d ms\n",
               service_name, function_name, params_str, interval_ms, timeout_ms);

        lib_utils::TaskScheduler::TaskId task_id = 0;
        bool success = g_task_scheduler.post_periodic(
        [call_function_common]() mutable -> bool {
            call_function_common();
            return true;  // Continue periodic execution
        },
        interval_ms,
        &task_id,
        "svc_call_periodic"
                       );

        if (!success) {
            printf("Error: Failed to schedule periodic task\n");
            return 1;
        }

        // Save task information
        g_scheduled_calls[task_id] = std::make_unique<ScheduledCallInfo>(
                                         task_id, std::string(service_name), std::string(function_name),
                                         std::string(params_str), true, interval_ms, timeout_ms
                                     );

        printf("Periodic task scheduled successfully (Task ID: %llu)\n",
               static_cast<unsigned long long>(task_id));
        printf("Use 'svc_call_cancel %llu' to cancel\n\n",
               static_cast<unsigned long long>(task_id));
        return 0;

    } else if (delay_ms >= 0) {
        // Delayed execution
        printf("\nScheduling delayed call: %s.%s(%s) after %d ms, timeout %d ms\n",
               service_name, function_name, params_str, delay_ms, timeout_ms);

        lib_utils::TaskScheduler::TaskId task_id = 0;

        // Create wrapper that removes task from records after execution
        // We capture task_id by creating a shared_ptr that will be updated after scheduling
        auto task_id_ptr = std::make_shared<lib_utils::TaskScheduler::TaskId>(0);
        auto call_function_with_cleanup = [call_function_common, task_id_ptr]() mutable {
            call_function_common();
            // Remove delayed task from records after execution
            if (*task_id_ptr != 0)
            {
                auto it = g_scheduled_calls.find(*task_id_ptr);
                if (it != g_scheduled_calls.end() && !it->second->is_periodic) {
                    g_scheduled_calls.erase(it);
                }
            }
        };

        bool success = g_task_scheduler.post_delayed(
                           call_function_with_cleanup,
                           delay_ms,
                           &task_id,
                           "svc_call_delayed"
                       );

        if (!success) {
            printf("Error: Failed to schedule delayed task\n");
            return 1;
        }

        // Update the shared_ptr with the actual task_id
        *task_id_ptr = task_id;

        // Save task information
        g_scheduled_calls[task_id] = std::make_unique<ScheduledCallInfo>(
                                         task_id, service_name_str, function_name_str,
                                         params_str_copy, false, delay_ms, timeout_ms
                                     );

        printf("Delayed task scheduled successfully (Task ID: %llu)\n",
               static_cast<unsigned long long>(task_id));
        printf("Use 'svc_call_cancel %llu' to cancel\n\n",
               static_cast<unsigned long long>(task_id));
        return 0;

    } else {
        // Immediate execution (original behavior)
        printf("\nCalling: %s.%s(%s)\n", service_name, function_name, params_str);
        printf("Timeout: %d ms\n", timeout_ms);

        try {
            auto result = service->call_function_sync(
                              function_name,
                              std::move(parameters),
                              timeout_ms
                          );

            printf("\nResult:\n");
            if (result.success) {
                printf("  - Success!\n");
                if (result.has_data()) {
                    printf("  - Value: %s\n",
                           BROOKESIA_DESCRIBE_TO_STR(result.data.value()).c_str());
                }
            } else {
                printf("  - Error: %s\n", result.error_message.c_str());
            }
            printf("\n");

            return result.success ? 0 : 1;
        } catch (const std::exception &e) {
            printf("\nError: %s\n\n", e.what());
            return 1;
        }
    }
}

/**
 * @brief Cancel a scheduled service function call
 */
static int do_call_cancel_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&call_cancel_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, call_cancel_args.end, argv[0]);
        return 1;
    }

    // Parse task_id from string (since arg_int64 has issues, we use arg_str and parse it)
    const char *task_id_str = call_cancel_args.task_id->sval[0];
    lib_utils::TaskScheduler::TaskId task_id = 0;
    try {
        task_id = static_cast<lib_utils::TaskScheduler::TaskId>(std::stoull(task_id_str));
    } catch (const std::exception &e) {
        printf("Error: Invalid task ID format: %s\n", task_id_str);
        return 1;
    }

    // Find task in our records
    auto it = g_scheduled_calls.find(task_id);
    if (it == g_scheduled_calls.end()) {
        printf("Error: Task ID %llu not found\n", static_cast<unsigned long long>(task_id));
        printf("Use 'svc_call_list' to see all scheduled tasks\n\n");
        return 1;
    }

    auto &task_info = it->second;
    printf("\nCanceling task: %llu (%s.%s)\n",
           static_cast<unsigned long long>(task_id),
           task_info->service.c_str(), task_info->function.c_str());

    // Get task scheduler and cancel the task
    g_task_scheduler.cancel(task_id);

    // Remove from our records
    g_scheduled_calls.erase(it);

    printf("Task canceled successfully\n\n");
    return 0;
}

/**
 * @brief List all scheduled service function calls
 */
static int do_call_list_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&call_list_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, call_list_args.end, argv[0]);
        return 1;
    }

    printf("\n=== Scheduled Service Calls ===\n");

    if (g_scheduled_calls.empty()) {
        printf("No scheduled calls\n\n");
        return 0;
    }

    for (const auto &[task_id, task_info] : g_scheduled_calls) {
        printf("\n  Task ID: %llu\n", static_cast<unsigned long long>(task_id));
        printf("    Service: %s\n", task_info->service.c_str());
        printf("    Function: %s\n", task_info->function.c_str());
        printf("    Parameters: %s\n", task_info->params.c_str());
        printf("    Type: %s\n", task_info->is_periodic ? "Periodic" : "Delayed");
        printf("    %s: %d ms\n",
               task_info->is_periodic ? "Interval" : "Delay",
               task_info->delay_or_interval_ms);
        printf("    Timeout: %d ms\n", task_info->timeout_ms);
    }

    printf("\nTotal: %zu scheduled call(s)\n\n", g_scheduled_calls.size());
    return 0;
}

// ============================================================================
// Command registration
// ============================================================================

void register_service_commands(void)
{
    // Initialize service manager
    if (!service_manager.is_initialized()) {
        ESP_LOGI(TAG, "Initializing service manager...");
        if (!service_manager.init()) {
            ESP_LOGE(TAG, "Failed to initialize service manager");
            return;
        }
    }

    if (!service_manager.is_running()) {
        ESP_LOGI(TAG, "Starting service manager...");
        if (!service_manager.start()) {
            ESP_LOGE(TAG, "Failed to start service manager");
            return;
        }
    }

    // Command: svc_list
    list_services_args.end = arg_end(1);

    const esp_console_cmd_t list_services_cmd = {
        .command = "svc_list",
        .help = "List all registered services",
        .hint = NULL,
        .func = &do_list_services_cmd,
        .argtable = &list_services_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&list_services_cmd));

    // Command: svc_funcs
    list_functions_args.service = arg_str1(NULL, NULL, "<service>", "Service name");
    list_functions_args.end = arg_end(2);

    const esp_console_cmd_t list_functions_cmd = {
        .command = "svc_funcs",
        .help = "List all functions in a service",
        .hint = NULL,
        .func = &do_list_functions_cmd,
        .argtable = &list_functions_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&list_functions_cmd));

    // Command: svc_events
    list_events_args.service = arg_str1(NULL, NULL, "<service>", "Service name");
    list_events_args.end = arg_end(2);

    const esp_console_cmd_t list_events_cmd = {
        .command = "svc_events",
        .help = "List all events in a service",
        .hint = NULL,
        .func = &do_list_events_cmd,
        .argtable = &list_events_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&list_events_cmd));

    // Command: svc_call
    call_args.service = arg_str1(NULL, NULL, "<service>", "Service name");
    call_args.function = arg_str1(NULL, NULL, "<function>", "Function name");
    call_args.params = arg_str0(NULL, NULL, "<json>", "JSON parameters (optional, default: {})");
    call_args.delay = arg_int0("d", "delay", "<ms>", "Delay execution by milliseconds (mutually exclusive with --interval)");
    call_args.interval = arg_int0("i", "interval", "<ms>", "Execute periodically every milliseconds (mutually exclusive with --delay)");
    call_args.timeout = arg_int0("t", "timeout", "<ms>", "Function call timeout in milliseconds (default: 5000)");
    call_args.end = arg_end(8);

    const esp_console_cmd_t call_cmd = {
        .command = "svc_call",
        .help = "Call a service function with JSON parameters. Use --timeout to set call timeout",
        .hint = NULL,
        .func = &do_call_cmd,
        .argtable = &call_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&call_cmd));

    // Command: svc_call_cancel
    call_cancel_args.task_id = arg_str1(NULL, NULL, "<task_id>", "Task ID to cancel");
    call_cancel_args.end = arg_end(2);

    const esp_console_cmd_t call_cancel_cmd = {
        .command = "svc_call_cancel",
        .help = "Cancel a scheduled service function call by task ID",
        .hint = NULL,
        .func = &do_call_cancel_cmd,
        .argtable = &call_cancel_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&call_cancel_cmd));

    // Command: svc_call_list
    call_list_args.end = arg_end(1);

    const esp_console_cmd_t call_list_cmd = {
        .command = "svc_call_list",
        .help = "List all scheduled service function calls",
        .hint = NULL,
        .func = &do_call_list_cmd,
        .argtable = &call_list_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&call_list_cmd));

    // Command: svc_stop
    stop_args.service = arg_str1(NULL, NULL, "<service>", "Service name");
    stop_args.end = arg_end(2);

    const esp_console_cmd_t stop_cmd = {
        .command = "svc_stop",
        .help = "Stop and release a service binding",
        .hint = NULL,
        .func = &do_stop_cmd,
        .argtable = &stop_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&stop_cmd));

    // Command: svc_subscribe
    subscribe_args.service = arg_str1(NULL, NULL, "<service>", "Service name");
    subscribe_args.event = arg_str1(NULL, NULL, "<event>", "Event name");
    subscribe_args.timeout = arg_int0(NULL, NULL, "<ms>", "Timeout in milliseconds (deprecated)");
    subscribe_args.end = arg_end(4);

    const esp_console_cmd_t subscribe_cmd = {
        .command = "svc_subscribe",
        .help = "Subscribe to a service event",
        .hint = NULL,
        .func = &do_subscribe_cmd,
        .argtable = &subscribe_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&subscribe_cmd));

    // Command: svc_unsubscribe
    unsubscribe_args.service = arg_str1(NULL, NULL, "<service>", "Service name");
    unsubscribe_args.event = arg_str1(NULL, NULL, "<event>", "Event name");
    unsubscribe_args.timeout = arg_int0(NULL, NULL, "<ms>", "Timeout in milliseconds (deprecated)");
    unsubscribe_args.end = arg_end(4);

    const esp_console_cmd_t unsubscribe_cmd = {
        .command = "svc_unsubscribe",
        .help = "Unsubscribe from service events",
        .hint = NULL,
        .func = &do_unsubscribe_cmd,
        .argtable = &unsubscribe_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&unsubscribe_cmd));

    // Start task scheduler
    auto start_config = lib_utils::TaskScheduler::StartConfig{
        .worker_configs = {
            {
                .name = "CmdWroker",
                .stack_size = 6 * 1024,
            }
        }
    };
    if (!g_task_scheduler.start(start_config)) {
        ESP_LOGE(TAG, "Failed to start task scheduler");
        return;
    }

    ESP_LOGI(TAG, "Service commands registered successfully");
}
