/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <map>
#include "boost/thread/shared_mutex.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/service_manager/rpc/server.hpp"
#include "brookesia/service_manager/rpc/client.hpp"
#include "brookesia/service_manager/service/base.hpp"

namespace esp_brookesia::service {

using ServiceRegistry = lib_utils::PluginRegistry<ServiceBase>;

/**
 * @brief Service binding handle
 *
 * RAII wrapper for service binding that automatically releases the service and its dependencies
 * when the binding goes out of scope.
 */
class ServiceBinding {
public:
    friend class ServiceManager;

    ServiceBinding() = default;

    // Disable copy
    ServiceBinding(const ServiceBinding &) = delete;
    ServiceBinding &operator=(const ServiceBinding &) = delete;

    // Enable move
    ServiceBinding(ServiceBinding &&other) noexcept;
    ServiceBinding &operator=(ServiceBinding &&other) noexcept;

    // Automatically release service (and its dependencies) on destruction
    ~ServiceBinding()
    {
        release();
    }

    /**
     * @brief Check if the binding is valid
     *
     * @return true if valid and service is running, false otherwise
     */
    bool is_valid() const
    {
        return (service_ != nullptr) && (service_->is_running());
    }

    /**
     * @brief Explicit conversion to bool
     *
     * @return true if valid, false otherwise
     */
    explicit operator bool() const
    {
        return is_valid();
    }

    /**
     * @brief Get the service object
     *
     * @return std::shared_ptr<ServiceBase> Pointer to the service
     */
    std::shared_ptr<ServiceBase> get_service() const
    {
        return service_;
    }

    /**
     * @brief Get a dependency service by name
     *
     * @param[in] name Dependency service name
     * @return std::shared_ptr<ServiceBase> Pointer to the dependency service, or nullptr if not found
     */
    std::shared_ptr<ServiceBase> get_dependency_service(const std::string &name) const
    {
        for (const auto &dependency : dependencies_) {
            auto dependency_service = dependency.get_service();
            if (dependency_service && (dependency_service->get_attributes().name == name)) {
                return dependency_service;
            }
        }
        return nullptr;
    }

    /**
     * @brief Release the service binding
     */
    void release();

private:
    using UnbindCallback = std::function<void(const std::string &)>;

    ServiceBinding(
        UnbindCallback unbind_callback, std::shared_ptr<ServiceBase> service,
        std::vector<ServiceBinding> &&dependencies
    )
        : unbind_callback_(std::move(unbind_callback))
        , service_(service)
        , dependencies_(std::move(dependencies))
    {}

    UnbindCallback unbind_callback_;
    std::shared_ptr<ServiceBase> service_;
    std::vector<ServiceBinding> dependencies_;  // Dependent service bindings
};

/**
 * @brief Service manager singleton
 *
 * Manages service lifecycle, dependencies, and RPC communication.
 */
class ServiceManager {
public:
    inline static const lib_utils::TaskScheduler::StartConfig DEFAULT_TASK_SCHEDULER_START_CONFIG = {
        .worker_configs = {
            lib_utils::ThreadConfig{
                .name = BROOKESIA_SERVICE_MANAGER_WORKER_NAME "0",
                .core_id = BROOKESIA_SERVICE_MANAGER_WORKER_0_CORE_ID,
                .priority = BROOKESIA_SERVICE_MANAGER_WORKER_PRIORITY,
                .stack_size = BROOKESIA_SERVICE_MANAGER_WORKER_STACK_SIZE,
                .stack_in_ext = BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT,
            },
            lib_utils::ThreadConfig{
                .name = BROOKESIA_SERVICE_MANAGER_WORKER_NAME "1",
                .core_id = BROOKESIA_SERVICE_MANAGER_WORKER_1_CORE_ID,
                .priority = BROOKESIA_SERVICE_MANAGER_WORKER_PRIORITY,
                .stack_size = BROOKESIA_SERVICE_MANAGER_WORKER_STACK_SIZE,
                .stack_in_ext = BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT,
            },
        },
        .worker_poll_interval_ms = BROOKESIA_SERVICE_MANAGER_WORKER_POLL_INTERVAL_MS,
    };

    struct RPC_ClientConfig {
        rpc::Client::DisconnectCallback on_disconnect_callback;
        rpc::Client::DeinitCallback on_deinit_callback;

        RPC_ClientConfig()
            : on_disconnect_callback(nullptr)
            , on_deinit_callback(nullptr)
        {}
    };

    /**
     * @brief Initialize the service manager
     *
     * @return true if initialized successfully, false otherwise
     */
    bool init();

    /**
     * @brief Deinitialize the service manager
     */
    void deinit();

    /**
     * @brief Start the service manager
     *
     * @param[in] config Task scheduler start configuration
     * @return true if started successfully, false otherwise
     */
    bool start(const lib_utils::TaskScheduler::StartConfig &config = DEFAULT_TASK_SCHEDULER_START_CONFIG);

    /**
     * @brief Stop the service manager
     */
    void stop();

    /*/**
     * @brief Add a service to the service manager
     *
     * @param[in] service Service to add
     * @return true if added successfully, false otherwise
     */
    bool add_service(std::shared_ptr<ServiceBase> service);

    /**
     * @brief Remove a service by name
     *
     * @param[in] name Service name
     * @return true if removed successfully, false otherwise
     */
    bool remove_service(const std::string &name);

    /**
     * @brief Bind a service by name
     *
     * @param[in] name Service name
     * @return ServiceBinding Service binding handle
     */
    ServiceBinding bind(const std::string &name);

    /**
     * @brief Start the RPC server
     *
     * @param[in] config RPC server configuration
     * @param[in] timeout_ms Timeout in milliseconds (default: 100ms)
     * @return true if started successfully, false otherwise
     */
    bool start_rpc_server(const rpc::Server::Config &config = rpc::Server::Config(), uint32_t timeout_ms = 100);

    /**
     * @brief Stop the RPC server
     */
    void stop_rpc_server();

    /**
     * @brief Connect RPC server to services
     *
     * @param[in] names Service names to connect (empty means all services)
     * @return true if connected successfully, false otherwise
     */
    bool connect_rpc_server_to_services(std::vector<std::string> &&names = {});

    /**
     * @brief Disconnect RPC server from services
     *
     * @param[in] names Service names to disconnect (empty means all services)
     * @return true if disconnected successfully, false otherwise
     */
    bool disconnect_rpc_server_from_services(std::vector<std::string> &&names = {});

    /**
     * @brief Create a new RPC client
     *
     * @param[in] config RPC client configuration
     * @return std::shared_ptr<rpc::Client> Shared pointer to the RPC client
     */
    std::shared_ptr<rpc::Client> new_rpc_client(const RPC_ClientConfig &config = RPC_ClientConfig());

    /**
     * @brief Call an RPC function synchronously
     *
     * @param[in] host Host address
     * @param[in] service_name Service name
     * @param[in] function_name Function name
     * @param[in] params Function parameters in JSON format
     * @param[in] timeout_ms Timeout in milliseconds (default: configured timeout)
     * @param[in] port Server port (default: configured port)
     * @return FunctionResult Result of the function call
     */
    FunctionResult call_rpc_function_sync(
        std::string host, const std::string &service_name, const std::string &function_name, boost::json::object &&params,
        uint32_t timeout_ms = BROOKESIA_SERVICE_MANAGER_RPC_CLIENT_CALL_FUNCTION_TIMEOUT_MS,
        uint16_t port = BROOKESIA_SERVICE_MANAGER_RPC_SERVER_LISTEN_PORT
    );

    /**
     * @brief Check if the service manager is initialized
     *
     * @return true if initialized, false otherwise
     */
    bool is_initialized() const
    {
        return is_initialized_.load();
    }

    /**
     * @brief Check if the service manager is running
     *
     * @return true if running, false otherwise
     */
    bool is_running() const
    {
        return is_running_.load();
    }

    /**
     * @brief Check if the RPC server is running
     *
     * @return true if running, false otherwise
     */
    bool is_rpc_server_running() const
    {
        boost::shared_lock lock(rpc_mutex_);
        return (rpc_server_ && rpc_server_->is_running());
    }

    /**
     * @brief Get a service by name
     *
     * @param[in] name Service name
     * @return std::shared_ptr<ServiceBase> Pointer to the service, or nullptr if not found
     */
    std::shared_ptr<ServiceBase> get_service(const std::string &name)
    {
        boost::lock_guard lock(service_mutex_);
        auto it = services_.find(name);
        return (it != services_.end()) ? it->second.service : nullptr;
    }

    /**
     * @brief Get the task scheduler
     *
     * @note  Exposes the task scheduler for external code that needs to execute tasks in the service's context.
     *
     * @return std::shared_ptr<lib_utils::TaskScheduler> Shared pointer to the task scheduler
     */
    std::shared_ptr<lib_utils::TaskScheduler> get_task_scheduler() const
    {
        return task_scheduler_;
    }

    /**
     * @brief Get the singleton instance
     *
     * @return ServiceManager& Reference to the singleton instance
     */
    static ServiceManager &get_instance()
    {
        static ServiceManager instance;
        return instance;
    }

private:
    enum class ServiceState {
        Idle,       // Service not started (ref_count == 0)
        Starting,   // Service is being started by another thread
        Running,    // Service is running (ref_count > 0)
    };
    struct ServiceInfo {
        size_t ref_count;
        std::shared_ptr<ServiceBase> service;
        ServiceState state;
        boost::condition_variable_any start_cv;  // Condition variable for waiting start completion
    };

    ServiceManager() = default;
    ~ServiceManager();

    void unbind(const std::string &name);
    bool init_internal();  // Internal init without lock (assumes state_mutex_ is already held)
    void stop_internal();  // Internal stop without lock (assumes state_mutex_ is already held)
    std::vector<std::string> topological_sort(
        const std::map<std::string, std::shared_ptr<ServiceBase>> &all_services
    );

    void add_all_registered_services();
    void remove_all_registered_services();

    boost::mutex state_mutex_;  // Protect state transitions (init/deinit/start/stop)
    std::atomic<bool> is_initialized_{false};
    std::atomic<bool> is_running_{false};

    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;

    // Service management
    boost::shared_mutex service_mutex_;  // Use recursive mutex to support recursive bind calls
    std::map<std::string, ServiceInfo> services_;
    std::list<std::string> service_init_order_;

    mutable boost::shared_mutex rpc_mutex_;  // Protect RPC server and client access
    std::unique_ptr<rpc::Server> rpc_server_;
    std::list<std::weak_ptr<rpc::Client>> rpc_clients_;
};

} // namespace esp_brookesia::service
