/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <algorithm>
#include <map>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "cmd_service.hpp"
#include "brookesia/service_manager.hpp"

using namespace esp_brookesia::service;

static const char *TAG = "cmd_service";
static auto &service_manager = ServiceManager::get_instance();
static std::vector<ServiceBinding> g_bindings;

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

// RPC subscription information structure
struct RpcSubscriptionInfo {
    std::string host;
    uint16_t port;
    std::string service;
    std::string event;
    std::string subscription_id;
    std::shared_ptr<rpc::Client> client;

    RpcSubscriptionInfo(std::string h, uint16_t p, std::string svc, std::string evt, std::string sub_id, std::shared_ptr<rpc::Client> cli)
        : host(std::move(h))
        , port(p)
        , service(std::move(svc))
        , event(std::move(evt))
        , subscription_id(std::move(sub_id))
        , client(std::move(cli))
    {}
};

static std::vector<std::unique_ptr<RpcSubscriptionInfo>> g_rpc_subscriptions;

// RPC Client cache: map from "host:port" to shared_ptr<rpc::Client>
static std::map<std::string, std::shared_ptr<rpc::Client>> g_rpc_clients;

/**
 * @brief Get or create an RPC client for the given host and port
 * @param host Host address
 * @param port Port number
 * @param timeout_ms Connection timeout in milliseconds
 * @return Shared pointer to RPC client, or nullptr if failed
 */
static std::shared_ptr<rpc::Client> get_or_create_rpc_client(const std::string &host, uint16_t port, uint32_t timeout_ms)
{
    std::string key = host + ":" + std::to_string(port);

    // Check if client already exists and is connected
    auto it = g_rpc_clients.find(key);
    if (it != g_rpc_clients.end() && it->second) {
        if (it->second->is_connected()) {
            ESP_LOGD(TAG, "Reusing existing connected RPC client for %s", key.c_str());
            return it->second;
        } else {
            // Client exists but not connected, try to reconnect
            ESP_LOGD(TAG, "Reconnecting existing RPC client for %s", key.c_str());
            if (it->second->connect(host, port, timeout_ms)) {
                return it->second;
            } else {
                // Reconnection failed, remove from cache and create new one
                ESP_LOGW(TAG, "Reconnection failed for %s, removing from cache", key.c_str());
                g_rpc_clients.erase(it);
            }
        }
    }

    // Create new client
    ESP_LOGD(TAG, "Creating new RPC client for %s", key.c_str());
    std::shared_ptr<rpc::Client> client = service_manager.new_rpc_client();
    if (!client) {
        ESP_LOGE(TAG, "Failed to create RPC client");
        return nullptr;
    }

    if (!client->connect(host, port, timeout_ms)) {
        ESP_LOGE(TAG, "Failed to connect to RPC server %s", key.c_str());
        return nullptr;
    }

    // Store in cache
    g_rpc_clients[key] = client;
    ESP_LOGD(TAG, "RPC client created and cached for %s", key.c_str());

    return client;
}

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
    struct arg_str *action;
    struct arg_int *port;
    struct arg_str *services;
    struct arg_end *end;
} rpc_server_args;

static struct {
    struct arg_str *host;
    struct arg_str *service;
    struct arg_str *function;
    struct arg_str *params;
    struct arg_int *port;
    struct arg_int *timeout;
    struct arg_end *end;
} rpc_call_args;

static struct {
    struct arg_str *host;
    struct arg_str *service;
    struct arg_str *event;
    struct arg_int *port;
    struct arg_int *timeout;
    struct arg_end *end;
} rpc_subscribe_args;

static struct {
    struct arg_str *host;
    struct arg_str *service;
    struct arg_str *event;
    struct arg_int *port;
    struct arg_int *timeout;
    struct arg_end *end;
} rpc_unsubscribe_args;

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
    ESP_LOGD(TAG, "Creating new binding for '%s'", service_name);
    auto binding = service_manager.bind(service_name);
    if (!binding.is_valid()) {
        ESP_LOGE(TAG, "Failed to bind service '%s'", service_name);
        return nullptr;
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

    // Get or create binding for the service
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

    // Get or create binding for the service
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

    // Ensure service is bound
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
 * @brief Parse comma-separated service names into a vector
 */
static std::vector<std::string> parse_service_names(const char *services_str)
{
    std::vector<std::string> services;
    if (!services_str || strlen(services_str) == 0) {
        return services;
    }

    std::string str(services_str);
    size_t pos = 0;
    while ((pos = str.find(',')) != std::string::npos) {
        std::string service = str.substr(0, pos);
        // Trim whitespace
        service.erase(0, service.find_first_not_of(" \t"));
        service.erase(service.find_last_not_of(" \t") + 1);
        if (!service.empty()) {
            services.push_back(service);
        }
        str.erase(0, pos + 1);
    }
    // Add last service
    str.erase(0, str.find_first_not_of(" \t"));
    str.erase(str.find_last_not_of(" \t") + 1);
    if (!str.empty()) {
        services.push_back(str);
    }

    return services;
}

/**
 * @brief Start, stop, connect or disconnect RPC server
 */
static int do_rpc_server_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&rpc_server_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, rpc_server_args.end, argv[0]);
        return 1;
    }

    const char *action = rpc_server_args.action->sval[0];
    int port = rpc_server_args.port->count > 0 ?
               rpc_server_args.port->ival[0] : BROOKESIA_SERVICE_MANAGER_RPC_SERVER_LISTEN_PORT;  // Default port
    const char *services_str = rpc_server_args.services->count > 0 ?
                               rpc_server_args.services->sval[0] : nullptr;

    if (strcmp(action, "start") == 0) {
        printf("\nStarting RPC server on port %d...\n", port);

        // Ensure ServiceManager is running
        if (!service_manager.is_running()) {
            printf("Starting service manager first...\n");
            if (!service_manager.start()) {
                printf("Error: Failed to start service manager\n");
                return 1;
            }
        }

        // Start RPC server
        rpc::Server::Config rpc_config;
        rpc_config.listen_port = port;

        if (!service_manager.start_rpc_server(rpc_config, 5000)) {
            printf("Error: Failed to start RPC server\n");
            return 1;
        }

        printf("RPC server started successfully on port %d\n\n", port);
        return 0;

    } else if (strcmp(action, "stop") == 0) {
        printf("\nStopping RPC server...\n");

        if (!service_manager.is_rpc_server_running()) {
            printf("Error: RPC server is not running\n");
            return 1;
        }

        service_manager.stop_rpc_server();
        printf("RPC server stopped successfully\n\n");
        return 0;

    } else if (strcmp(action, "connect") == 0) {
        printf("\nConnecting services to RPC server...\n");

        if (!service_manager.is_rpc_server_running()) {
            printf("Error: RPC server is not running. Please start it first.\n");
            return 1;
        }

        // Parse service names
        auto services = parse_service_names(services_str);

        if (services.empty()) {
            printf("Connecting all services to RPC server...\n");
        } else {
            printf("Services to connect: ");
            for (size_t i = 0; i < services.size(); i++) {
                printf("%s%s", services[i].c_str(), (i < services.size() - 1) ? ", " : "\n");
            }
        }

        if (!service_manager.connect_rpc_server_to_services(std::move(services))) {
            printf("Error: Failed to connect services to RPC server\n");
            return 1;
        }

        printf("Services connected successfully to RPC server\n\n");
        return 0;

    } else if (strcmp(action, "disconnect") == 0) {
        printf("\nDisconnecting services from RPC server...\n");

        if (!service_manager.is_rpc_server_running()) {
            printf("Error: RPC server is not running\n");
            return 1;
        }

        // Parse service names
        auto services = parse_service_names(services_str);

        if (services.empty()) {
            printf("Disconnecting all services from RPC server...\n");
        } else {
            printf("Services to disconnect: ");
            for (size_t i = 0; i < services.size(); i++) {
                printf("%s%s", services[i].c_str(), (i < services.size() - 1) ? ", " : "\n");
            }
        }

        if (!service_manager.disconnect_rpc_server_from_services(std::move(services))) {
            printf("Error: Failed to disconnect services from RPC server\n");
            return 1;
        }

        printf("Services disconnected successfully from RPC server\n\n");
        return 0;

    } else {
        printf("Error: Invalid action '%s'. Use 'start', 'stop', 'connect', or 'disconnect'\n", action);
        return 1;
    }
}

/**
 * @brief Call a remote service function via RPC
 */
static int do_rpc_call_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&rpc_call_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, rpc_call_args.end, argv[0]);
        return 1;
    }

    const char *host = rpc_call_args.host->sval[0];
    const char *service_name = rpc_call_args.service->sval[0];
    const char *function_name = rpc_call_args.function->sval[0];
    const char *params_str = rpc_call_args.params->count > 0 ?
                             rpc_call_args.params->sval[0] : "{}";
    int port = rpc_call_args.port->count > 0 ?
               rpc_call_args.port->ival[0] : BROOKESIA_SERVICE_MANAGER_RPC_SERVER_LISTEN_PORT;  // Default port
    int timeout_ms = rpc_call_args.timeout->count > 0 ?
                     rpc_call_args.timeout->ival[0] : BROOKESIA_SERVICE_MANAGER_RPC_CLIENT_CALL_FUNCTION_TIMEOUT_MS;  // Default 5s timeout

    // Parse JSON parameters
    boost::json::object params_json;
    try {
        if (strlen(params_str) > 0 && strcmp(params_str, "{}") != 0) {
            auto parsed = boost::json::parse(params_str);
            if (parsed.is_object()) {
                params_json = parsed.as_object();
            } else {
                printf("Error: Parameters must be a JSON object\n");
                printf("Example: svc_rpc_call 192.168.1.100 audio set_volume {\"volume\":80}\n");
                return 1;
            }
        }
    } catch (const std::exception &e) {
        printf("Error: Invalid JSON parameters: %s\n", e.what());
        printf("Example: svc_rpc_call 192.168.1.100 audio set_volume {\"volume\":80}\n");
        return 1;
    }

    // Get or create RPC client
    std::shared_ptr<rpc::Client> client = get_or_create_rpc_client(std::string(host), static_cast<uint16_t>(port), timeout_ms);
    if (!client) {
        printf("Error: Failed to get or create RPC client for %s:%d\n", host, port);
        return 1;
    }

    // Call remote function via RPC
    printf("\nCalling RPC: %s:%d/%s.%s(%s)\n", host, port, service_name, function_name, params_str);
    printf("Timeout: %d ms\n", timeout_ms);

    try {
        auto result = client->call_function_sync(
                          service_name,
                          function_name,
                          std::move(params_json),
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

/**
 * @brief Subscribe to a remote service event via RPC
 */
static int do_rpc_subscribe_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&rpc_subscribe_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, rpc_subscribe_args.end, argv[0]);
        return 1;
    }

    std::string host = std::string(rpc_subscribe_args.host->sval[0]);
    std::string service_name = std::string(rpc_subscribe_args.service->sval[0]);
    std::string event_name = std::string(rpc_subscribe_args.event->sval[0]);
    uint16_t port = rpc_subscribe_args.port->count > 0 ?
                    static_cast<uint16_t>(rpc_subscribe_args.port->ival[0]) :
                    BROOKESIA_SERVICE_MANAGER_RPC_SERVER_LISTEN_PORT;
    uint32_t timeout_ms = rpc_subscribe_args.timeout->count > 0 ?
                          static_cast<uint32_t>(rpc_subscribe_args.timeout->ival[0]) :
                          BROOKESIA_SERVICE_MANAGER_RPC_CLIENT_CALL_FUNCTION_TIMEOUT_MS;

    // Check if already subscribed to this event
    auto it = std::find_if(g_rpc_subscriptions.begin(), g_rpc_subscriptions.end(),
    [host, port, service_name, event_name](const std::unique_ptr<RpcSubscriptionInfo> &sub) {
        return sub->host == host && sub->port == port &&
               sub->service == service_name && sub->event == event_name;
    });

    if (it != g_rpc_subscriptions.end()) {
        printf("Error: Already subscribed to '%s:%d/%s.%s'\n", host.c_str(), port, service_name.c_str(), event_name.c_str());
        printf("  Use 'svc_rpc_unsubscribe %s %s %s' to unsubscribe first\n\n", host.c_str(), service_name.c_str(), event_name.c_str());
        return 1;
    }

    // Get or create RPC client
    printf("\nConnecting to RPC server: %s:%d...\n", host.c_str(), port);

    std::shared_ptr<rpc::Client> client = get_or_create_rpc_client(host, port, timeout_ms);
    if (!client) {
        printf("Error: Failed to get or create RPC client for %s:%d\n", host.c_str(), port);
        return 1;
    }

    // Create event handler callback
    auto event_handler = [host, port, service_name, event_name](const EventItemMap & data_map) {
        ESP_LOGI(TAG, "RPC Event received: %s:%d/%s.%s with %zu parameters", host.c_str(), port, service_name.c_str(), event_name.c_str(), data_map.size());
        printf("\n[RPC Event] %s:%d/%s.%s\n", host.c_str(), port, service_name.c_str(), event_name.c_str());
        printf("  Parameters:\n");

        for (const auto &[key, value] : data_map) {
            printf("    %s: %s\n", key.c_str(), BROOKESIA_DESCRIBE_TO_STR(value).c_str());
        }
        printf("\n");
    };

    // Subscribe to event
    printf("Subscribing to: %s:%d/%s.%s\n", host.c_str(), port, service_name.c_str(), event_name.c_str());

    try {
        std::string subscription_id = client->subscribe_event(
                                          service_name,
                                          event_name,
                                          event_handler,
                                          timeout_ms
                                      );

        if (subscription_id.empty()) {
            printf("Error: Failed to subscribe to event '%s:%d/%s.%s'\n", host.c_str(), port, service_name.c_str(), event_name.c_str());
            // Don't disconnect here - client might be used by other subscriptions
            return 1;
        }

        // Save subscription information
        g_rpc_subscriptions.push_back(
            std::make_unique<RpcSubscriptionInfo>(host, port, service_name, event_name, subscription_id, client)
        );
        ESP_LOGI(TAG, "RPC subscribed successfully, subscription_id: %s", subscription_id.c_str());

        printf("Successfully subscribed!\n");
        printf("  Subscription ID: %s\n", subscription_id.c_str());
        printf("  Total RPC subscriptions: %zu\n\n", g_rpc_subscriptions.size());

        return 0;
    } catch (const std::exception &e) {
        printf("\nError: %s\n\n", e.what());
        // Don't disconnect here - client might be used by other subscriptions
        return 1;
    }
}

/**
 * @brief Unsubscribe from a remote service event via RPC
 */
static int do_rpc_unsubscribe_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&rpc_unsubscribe_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, rpc_unsubscribe_args.end, argv[0]);
        return 1;
    }

    std::string host = std::string(rpc_unsubscribe_args.host->sval[0]);
    std::string service_name = std::string(rpc_unsubscribe_args.service->sval[0]);
    std::string event_name = std::string(rpc_unsubscribe_args.event->sval[0]);
    uint16_t port = rpc_unsubscribe_args.port->count > 0 ?
                    static_cast<uint16_t>(rpc_unsubscribe_args.port->ival[0]) :
                    BROOKESIA_SERVICE_MANAGER_RPC_SERVER_LISTEN_PORT;
    uint32_t timeout_ms = rpc_unsubscribe_args.timeout->count > 0 ?
                          static_cast<uint32_t>(rpc_unsubscribe_args.timeout->ival[0]) :
                          BROOKESIA_SERVICE_MANAGER_RPC_CLIENT_CALL_FUNCTION_TIMEOUT_MS;

    // Find subscription by host, port, service and event name
    auto it = std::find_if(g_rpc_subscriptions.begin(), g_rpc_subscriptions.end(),
    [host, port, service_name, event_name](const std::unique_ptr<RpcSubscriptionInfo> &sub) {
        return sub->host == host && sub->port == port &&
               sub->service == service_name && sub->event == event_name;
    });

    if (it == g_rpc_subscriptions.end()) {
        printf("Error: No active RPC subscription found for '%s:%d/%s.%s'\n", host.c_str(), port, service_name.c_str(), event_name.c_str());
        return 1;
    }

    // Unsubscribe from event
    printf("\nUnsubscribing from: %s:%d/%s.%s\n", host.c_str(), port, service_name.c_str(), event_name.c_str());

    try {
        auto &sub_info = *it;
        bool success = sub_info->client->unsubscribe_events(sub_info->service, {sub_info->subscription_id}, timeout_ms);
        if (!success) {
            printf("Error: Failed to unsubscribe from event '%s:%d/%s.%s'\n", host.c_str(), port, service_name.c_str(), event_name.c_str());
            return 1;
        }

        // Remove subscription from our list
        g_rpc_subscriptions.erase(it);

        // Check if there are any other subscriptions using this client
        std::string key = host + ":" + std::to_string(port);
        bool has_other_subscriptions = std::any_of(g_rpc_subscriptions.begin(), g_rpc_subscriptions.end(),
        [host, port](const std::unique_ptr<RpcSubscriptionInfo> &sub) {
            return sub->host == host && sub->port == port;
        });

        // If no more subscriptions, disconnect the client but keep it in cache for potential reuse
        if (!has_other_subscriptions) {
            ESP_LOGD(TAG, "No more subscriptions for %s, disconnecting client", key.c_str());
            sub_info->client->disconnect();
            // Note: We keep the client in g_rpc_clients cache for potential reuse
        }

        ESP_LOGI(TAG, "RPC unsubscribed successfully");
        printf("Successfully unsubscribed!\n");
        printf("  Remaining RPC subscriptions: %zu\n\n", g_rpc_subscriptions.size());

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

    // Ensure service is bound
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

    // Call service function
    printf("\nCalling: %s.%s(%s)\n", service_name, function_name, params_str);

    try {
        auto result = service->call_function_sync(
                          function_name,
                          std::move(parameters),
                          5000  // 5 seconds timeout
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

        return 0;
    } catch (const std::exception &e) {
        printf("\nError: %s\n\n", e.what());
        return 1;
    }
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
    call_args.end = arg_end(4);

    const esp_console_cmd_t call_cmd = {
        .command = "svc_call",
        .help = "Call a service function with JSON parameters",
        .hint = NULL,
        .func = &do_call_cmd,
        .argtable = &call_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&call_cmd));

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

    // Command: svc_rpc_server
    rpc_server_args.action = arg_str1(NULL, NULL, "<action>", "Action: 'start', 'stop', 'connect', or 'disconnect'");
    rpc_server_args.port = arg_int0("p", "port", "<port>", "Port number (default: 65500, for 'start' action)");
    rpc_server_args.services = arg_str0("s", "services", "<services>", "Comma-separated service names (for 'connect'/'disconnect', empty=all)");
    rpc_server_args.end = arg_end(4);

    const esp_console_cmd_t rpc_server_cmd = {
        .command = "svc_rpc_server",
        .help = "Manage RPC server: start, stop, connect/disconnect services",
        .hint = NULL,
        .func = &do_rpc_server_cmd,
        .argtable = &rpc_server_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&rpc_server_cmd));

    // Command: svc_rpc_call
    rpc_call_args.host = arg_str1(NULL, NULL, "<host>", "Remote host IP or hostname");
    rpc_call_args.service = arg_str1(NULL, NULL, "<service>", "Service name");
    rpc_call_args.function = arg_str1(NULL, NULL, "<function>", "Function name");
    rpc_call_args.params = arg_str0(NULL, NULL, "<json>", "JSON parameters (optional, default: {})");
    rpc_call_args.port = arg_int0("p", "port", "<port>", "Remote port (default: 65500)");
    rpc_call_args.timeout = arg_int0("t", "timeout", "<ms>", "Timeout in milliseconds (default: 2000)");
    rpc_call_args.end = arg_end(7);

    const esp_console_cmd_t rpc_call_cmd = {
        .command = "svc_rpc_call",
        .help = "Call a remote service function via RPC",
        .hint = NULL,
        .func = &do_rpc_call_cmd,
        .argtable = &rpc_call_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&rpc_call_cmd));

    // Command: svc_rpc_subscribe
    rpc_subscribe_args.host = arg_str1(NULL, NULL, "<host>", "Remote host IP or hostname");
    rpc_subscribe_args.service = arg_str1(NULL, NULL, "<service>", "Service name");
    rpc_subscribe_args.event = arg_str1(NULL, NULL, "<event>", "Event name");
    rpc_subscribe_args.port = arg_int0("p", "port", "<port>", "Remote port (default: 65500)");
    rpc_subscribe_args.timeout = arg_int0("t", "timeout", "<ms>", "Timeout in milliseconds (default: 2000)");
    rpc_subscribe_args.end = arg_end(6);

    const esp_console_cmd_t rpc_subscribe_cmd = {
        .command = "svc_rpc_subscribe",
        .help = "Subscribe to a remote service event via RPC",
        .hint = NULL,
        .func = &do_rpc_subscribe_cmd,
        .argtable = &rpc_subscribe_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&rpc_subscribe_cmd));

    // Command: svc_rpc_unsubscribe
    rpc_unsubscribe_args.host = arg_str1(NULL, NULL, "<host>", "Remote host IP or hostname");
    rpc_unsubscribe_args.service = arg_str1(NULL, NULL, "<service>", "Service name");
    rpc_unsubscribe_args.event = arg_str1(NULL, NULL, "<event>", "Event name");
    rpc_unsubscribe_args.port = arg_int0("p", "port", "<port>", "Remote port (default: 65500)");
    rpc_unsubscribe_args.timeout = arg_int0("t", "timeout", "<ms>", "Timeout in milliseconds (default: 2000)");
    rpc_unsubscribe_args.end = arg_end(6);

    const esp_console_cmd_t rpc_unsubscribe_cmd = {
        .command = "svc_rpc_unsubscribe",
        .help = "Unsubscribe from a remote service event via RPC",
        .hint = NULL,
        .func = &do_rpc_unsubscribe_cmd,
        .argtable = &rpc_unsubscribe_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&rpc_unsubscribe_cmd));

    ESP_LOGI(TAG, "Service commands registered successfully");
}
