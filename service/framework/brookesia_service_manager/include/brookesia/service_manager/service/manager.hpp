/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "boost/thread/shared_mutex.hpp"

#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_manager/service/manager_service.hpp"
#include "brookesia/service_manager/service/utils_service.hpp"

namespace esp_brookesia::service {

/**
 * @brief Registry of statically registered service plugins.
 */
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
        std::vector<ServiceBinding> dependencies
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
 * Manages service lifecycle, dependencies, local function calls, and event dispatch.
 */
class ServiceManager {
public:
    using ServiceState = ManagerService::ServiceState;
    using ServiceInfo = ManagerService::ServiceInfo;

    /**
     * @brief Default worker configuration used by `start()`.
     *
     * The configuration creates two worker threads for service dispatching and
     * uses the module-level scheduling defaults defined in `macro_configs.h`.
     */
    static lib_utils::TaskScheduler::StartConfig make_default_task_scheduler_start_config()
    {
        lib_utils::TaskScheduler::StartConfig config{
            .worker_configs = {},
        };
        lib_utils::ThreadConfig worker0;
        worker0.name = std::string(BROOKESIA_SERVICE_MANAGER_WORKER_NAME) + "0";
        worker0.core_id = BROOKESIA_SERVICE_MANAGER_WORKER_0_CORE_ID;
        worker0.priority = BROOKESIA_SERVICE_MANAGER_WORKER_PRIORITY;
        worker0.stack_size = BROOKESIA_SERVICE_MANAGER_WORKER_STACK_SIZE;
        worker0.stack_in_ext = BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT;
        config.worker_configs.push_back(worker0);
#if BROOKESIA_SERVICE_MANAGER_WORKER_NUM >= 2
        lib_utils::ThreadConfig worker1;
        worker1.name = std::string(BROOKESIA_SERVICE_MANAGER_WORKER_NAME) + "1";
        worker1.core_id = BROOKESIA_SERVICE_MANAGER_WORKER_1_CORE_ID;
        worker1.priority = BROOKESIA_SERVICE_MANAGER_WORKER_PRIORITY;
        worker1.stack_size = BROOKESIA_SERVICE_MANAGER_WORKER_STACK_SIZE;
        worker1.stack_in_ext = BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT;
        config.worker_configs.push_back(worker1);
#endif
#if BROOKESIA_SERVICE_MANAGER_WORKER_NUM >= 3
        lib_utils::ThreadConfig worker2;
        worker2.name = std::string(BROOKESIA_SERVICE_MANAGER_WORKER_NAME) + "2";
        worker2.core_id = BROOKESIA_SERVICE_MANAGER_WORKER_2_CORE_ID;
        worker2.priority = BROOKESIA_SERVICE_MANAGER_WORKER_PRIORITY;
        worker2.stack_size = BROOKESIA_SERVICE_MANAGER_WORKER_STACK_SIZE;
        worker2.stack_in_ext = BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT;
        config.worker_configs.push_back(worker2);
#endif
#if BROOKESIA_SERVICE_MANAGER_WORKER_NUM >= 4
        lib_utils::ThreadConfig worker3;
        worker3.name = std::string(BROOKESIA_SERVICE_MANAGER_WORKER_NAME) + "3";
        worker3.core_id = BROOKESIA_SERVICE_MANAGER_WORKER_3_CORE_ID;
        worker3.priority = BROOKESIA_SERVICE_MANAGER_WORKER_PRIORITY;
        worker3.stack_size = BROOKESIA_SERVICE_MANAGER_WORKER_STACK_SIZE;
        worker3.stack_in_ext = BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT;
        config.worker_configs.push_back(worker3);
#endif
        config.worker_poll_interval_ms = BROOKESIA_SERVICE_MANAGER_WORKER_POLL_INTERVAL_MS;
        return config;
    }

    inline static const lib_utils::TaskScheduler::StartConfig DEFAULT_TASK_SCHEDULER_START_CONFIG =
        make_default_task_scheduler_start_config();

    /**
     * @brief Default worker configuration used by the secondary scheduler.
     *
     * The secondary scheduler is intended for services that need manager-owned
     * workers with internal SRAM stacks.
     */
    static lib_utils::TaskScheduler::StartConfig make_default_secondary_task_scheduler_start_config()
    {
        lib_utils::TaskScheduler::StartConfig config{
            .worker_configs = {},
        };
        lib_utils::ThreadConfig worker0;
        worker0.name = std::string(BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_NAME) + "0";
        worker0.core_id = BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_0_CORE_ID;
        worker0.priority = BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_PRIORITY;
        worker0.stack_size = BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_STACK_SIZE;
        worker0.stack_in_ext = false;
        config.worker_configs.push_back(worker0);
#if BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_NUM >= 2
        lib_utils::ThreadConfig worker1;
        worker1.name = std::string(BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_NAME) + "1";
        worker1.core_id = BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_1_CORE_ID;
        worker1.priority = BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_PRIORITY;
        worker1.stack_size = BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_STACK_SIZE;
        worker1.stack_in_ext = false;
        config.worker_configs.push_back(worker1);
#endif
#if BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_NUM >= 3
        lib_utils::ThreadConfig worker2;
        worker2.name = std::string(BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_NAME) + "2";
        worker2.core_id = BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_2_CORE_ID;
        worker2.priority = BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_PRIORITY;
        worker2.stack_size = BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_STACK_SIZE;
        worker2.stack_in_ext = false;
        config.worker_configs.push_back(worker2);
#endif
#if BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_NUM >= 4
        lib_utils::ThreadConfig worker3;
        worker3.name = std::string(BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_NAME) + "3";
        worker3.core_id = BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_3_CORE_ID;
        worker3.priority = BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_PRIORITY;
        worker3.stack_size = BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_STACK_SIZE;
        worker3.stack_in_ext = false;
        config.worker_configs.push_back(worker3);
#endif
        config.worker_poll_interval_ms = BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_WORKER_POLL_INTERVAL_MS;
        return config;
    }

    inline static const lib_utils::TaskScheduler::StartConfig DEFAULT_SECONDARY_TASK_SCHEDULER_START_CONFIG =
        make_default_secondary_task_scheduler_start_config();

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

    /**
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
     * @brief Get all registered service names in lexical order.
     *
     * @return std::vector<std::string> Registered service names.
     */
    std::vector<std::string> get_service_names() const;

    /**
     * @brief Get a snapshot for one registered service.
     *
     * @param[in] name Registered service name.
     * @return std::optional<ServiceInfo> Snapshot, or empty when the service is not registered.
     */
    std::optional<ServiceInfo> get_service_info(const std::string &name) const;

    /**
     * @brief Get the metadata and member names exposed by one service.
     */
    std::optional<ManagerService::ServiceSchemaOverview> get_service_schema(const std::string &name) const;

    /**
     * @brief Copy one registered function schema.
     */
    std::optional<FunctionSchema> get_service_function_schema(
        const std::string &service_name, const std::string &function_name
    ) const;

    /**
     * @brief Copy one registered event schema.
     */
    std::optional<EventSchema> get_service_event_schema(
        const std::string &service_name, const std::string &event_name
    ) const;

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
    /**
     * @brief Internal bookkeeping record for a managed service.
     */
    struct RuntimeServiceInfo {
        size_t ref_count; ///< Number of active bindings referencing the service.
        std::shared_ptr<ServiceBase> service; ///< Managed service instance.
        ServiceState state; ///< Current lifecycle state.
        boost::condition_variable_any transition_cv; ///< Signals completion of start or stop transitions.
    };

    ServiceManager() = default;
    ~ServiceManager();

    void unbind(const std::string &name);
    bool init_internal();  // Internal init without lock (assumes state_mutex_ is already held)
    void stop_internal();  // Internal stop without lock (assumes state_mutex_ is already held)
    std::shared_ptr<lib_utils::TaskScheduler> get_service_task_scheduler(const ServiceBase::Attributes &attributes);
    std::vector<std::string> topological_sort(
        const std::map<std::string, std::shared_ptr<ServiceBase>> &all_services
    );

    void add_all_registered_services();
    void remove_all_registered_services();

    boost::mutex state_mutex_;  // Protect state transitions (init/deinit/start/stop)
    std::atomic<bool> is_initialized_{false};
    std::atomic<bool> is_running_{false};

    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;
    std::shared_ptr<lib_utils::TaskScheduler> secondary_task_scheduler_;
    std::shared_ptr<ManagerService> manager_service_;
    std::shared_ptr<UtilsService> utils_service_;
    ServiceBinding manager_binding_;
    ServiceBinding utils_binding_;

    // Service management
    mutable boost::shared_mutex service_mutex_;
    std::map<std::string, RuntimeServiceInfo> services_;
    std::list<std::string> service_init_order_;
};

} // namespace esp_brookesia::service
