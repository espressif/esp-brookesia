/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <exception>
#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/service/manager.hpp"

namespace esp_brookesia::service {

ServiceBinding::ServiceBinding(ServiceBinding &&other) noexcept
    : unbind_callback_(std::move(other.unbind_callback_))
    , service_(std::move(other.service_))
    , dependencies_(std::move(other.dependencies_))
{
    BROOKESIA_LOGD("Moving binding: %1%", service_ ? service_->get_attributes().name : "null");
    other.unbind_callback_ = nullptr;
    other.service_ = nullptr;
}

ServiceBinding &ServiceBinding::operator=(ServiceBinding &&other) noexcept
{
    if (this != &other) {
        BROOKESIA_LOGD("Move assignment: %1%", other.service_ ? other.service_->get_attributes().name : "null");
        // Release current binding before moving new one to avoid resource leak
        release();
        unbind_callback_ = std::move(other.unbind_callback_);
        service_ = other.service_;
        dependencies_ = std::move(other.dependencies_);
        other.unbind_callback_ = nullptr;
        other.service_ = nullptr;
    }
    return *this;
}

void ServiceBinding::release()
{
    if (unbind_callback_ && service_) {
        BROOKESIA_LOGD("Releasing binding: %1%", service_->get_attributes().name);
        // First release itself
        unbind_callback_(service_->get_attributes().name);
        unbind_callback_ = nullptr;
        service_ = nullptr;
        // Then release the dependencies (in reverse order)
        dependencies_.clear();
    }
}

ServiceManager::~ServiceManager()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    try {
        if (is_initialized()) {
            deinit();
        }
    } catch (const std::exception &e) {
        BROOKESIA_LOGE("Detected exception while destroying service manager: %1%", e.what());
    } catch (...) {
        BROOKESIA_LOGE("Detected unknown exception while destroying service manager");
    }
}

bool ServiceManager::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(state_mutex_);
    return init_internal();
}

bool ServiceManager::init_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_initialized()) {
        BROOKESIA_LOGD("Already initialized");
        return true;
    }

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%",
        BROOKESIA_SERVICE_MANAGER_VER_MAJOR, BROOKESIA_SERVICE_MANAGER_VER_MINOR, BROOKESIA_SERVICE_MANAGER_VER_PATCH
    );

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        task_scheduler_ = std::make_shared<lib_utils::TaskScheduler>(), false, "Failed to create task scheduler"
    );
#if BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_ENABLE
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        secondary_task_scheduler_ = std::make_shared<lib_utils::TaskScheduler>(), false,
        "Failed to create secondary task scheduler"
    );
#endif

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        manager_service_ = std::make_shared<ManagerService>(*this), false,
        "Failed to create the built-in Manager service"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        utils_service_ = std::make_shared<UtilsService>(), false,
        "Failed to create the built-in Utils service"
    );
    if (!add_service(manager_service_)) {
        utils_service_.reset();
        manager_service_.reset();
        task_scheduler_.reset();
        secondary_task_scheduler_.reset();
        BROOKESIA_LOGE("Failed to add the built-in Manager service");
        return false;
    }
    if (!add_service(utils_service_)) {
        remove_service(ManagerService::get_name().data());
        utils_service_.reset();
        manager_service_.reset();
        task_scheduler_.reset();
        secondary_task_scheduler_.reset();
        BROOKESIA_LOGE("Failed to add the built-in Utils service");
        return false;
    }

    // Initialize all registered services
    add_all_registered_services();

    is_initialized_.store(true);

    return true;
}

void ServiceManager::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(state_mutex_);

    if (!is_initialized()) {
        BROOKESIA_LOGD("Already deinitialized");
        return;
    }

    if (is_running()) {
        stop_internal();  // Call internal version to avoid deadlock
    }

    // Deinitialize all services
    remove_all_registered_services();
    utils_service_.reset();
    manager_service_.reset();

    task_scheduler_.reset();
    secondary_task_scheduler_.reset();

    is_initialized_.store(false);
}

bool ServiceManager::start(const lib_utils::TaskScheduler::StartConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(state_mutex_);

    if (is_running()) {
        BROOKESIA_LOGD("Already running");
        return true;
    }

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    if (!is_initialized()) {
        BROOKESIA_LOGI("Not initialized, initializing...");
        BROOKESIA_CHECK_FALSE_RETURN(init_internal(), false, "Failed to initialize");
    }

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (secondary_task_scheduler_) {
            secondary_task_scheduler_->stop();
        }
        if (task_scheduler_) {
            task_scheduler_->stop();
        }
    });

    BROOKESIA_CHECK_FALSE_RETURN(task_scheduler_->start(config), false, "Failed to start task scheduler");
#if BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_ENABLE
    BROOKESIA_CHECK_NULL_RETURN(
        secondary_task_scheduler_, false, "Secondary task scheduler is not available"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        secondary_task_scheduler_->start(DEFAULT_SECONDARY_TASK_SCHEDULER_START_CONFIG), false,
        "Failed to start secondary task scheduler"
    );
#endif

    is_running_.store(true);

    manager_binding_ = bind(ManagerService::get_name().data());
    if (!manager_binding_.is_valid()) {
        is_running_.store(false);
        BROOKESIA_LOGE("Failed to start the built-in Manager service");
        return false;
    }
    utils_binding_ = bind(UtilsService::get_name().data());
    if (!utils_binding_.is_valid()) {
        manager_binding_.release();
        is_running_.store(false);
        BROOKESIA_LOGE("Failed to start the built-in Utils service");
        return false;
    }

    stop_guard.release();

    BROOKESIA_LOGI("Service manager started");

    return true;
}

void ServiceManager::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(state_mutex_);
    stop_internal();
}

void ServiceManager::stop_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        BROOKESIA_LOGD("Already stopped");
        return;
    }

    // Stop Utils first so profiler callbacks finish while the shared service schedulers are still alive.
    utils_binding_.release();
    manager_binding_.release();

    auto stop_builtin = [this](std::string_view name, const std::shared_ptr<ServiceBase> &service) {
        bool should_stop = false;
        {
            boost::unique_lock lock(service_mutex_);
            auto service_it = services_.find(name.data());
            if ((service_it != services_.end()) && service_it->second.service &&
                    (service_it->second.state == ServiceState::Running)) {
                service_it->second.state = ServiceState::Stopping;
                should_stop = true;
            }
        }
        if (!should_stop) {
            return;
        }
        service->stop();
        boost::unique_lock lock(service_mutex_);
        auto service_it = services_.find(name.data());
        if (service_it != services_.end()) {
            service_it->second.state = ServiceState::Stopped;
            service_it->second.transition_cv.notify_all();
        }
    };
    stop_builtin(UtilsService::get_name(), utils_service_);
    stop_builtin(ManagerService::get_name(), manager_service_);

    // Stop task schedulers
    if (secondary_task_scheduler_) {
        secondary_task_scheduler_->stop();
    }
    task_scheduler_->stop();

    is_running_.store(false);

    BROOKESIA_LOGI("Service manager stopped");
}

bool ServiceManager::add_service(std::shared_ptr<ServiceBase> service)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: service(%1%)", BROOKESIA_DESCRIBE_TO_STR(service));

    BROOKESIA_CHECK_NULL_RETURN(service, false, "Invalid service");

    auto name = service->get_attributes().name;
    {
        boost::lock_guard lock(service_mutex_);
        if (services_.find(name) != services_.end()) {
            BROOKESIA_LOGD("Service already exists: %1%", name);
            return true;
        }
    }

    if (!service->is_initialized()) {
        BROOKESIA_LOGI("Initializing service: %1%", name);
        auto task_scheduler = get_service_task_scheduler(service->get_attributes());
        BROOKESIA_CHECK_FALSE_RETURN(service->init(task_scheduler), false, "Failed to initialize service");
    }

    {
        boost::lock_guard lock(service_mutex_);
        // Use emplace to avoid copying/moving condition_variable_any
        auto result = services_.emplace(std::piecewise_construct, std::forward_as_tuple(name), std::forward_as_tuple());
        if (result.second) {
            auto &info = result.first->second;
            info.ref_count = 0;
            info.service = service;
            info.state = ServiceState::Stopped;
            service_init_order_.push_back(name);
        }
    }

    BROOKESIA_LOGI("Service added: %1%", name);

    return true;
}

std::shared_ptr<lib_utils::TaskScheduler> ServiceManager::get_service_task_scheduler(
    const ServiceBase::Attributes &attributes
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (attributes.has_scheduler()) {
        return task_scheduler_;
    }

    switch (attributes.scheduler_type) {
    case ServiceBase::SchedulerType::Main:
        return task_scheduler_;
    case ServiceBase::SchedulerType::Secondary:
#if BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_ENABLE
        return secondary_task_scheduler_;
#else
        BROOKESIA_LOGE("Secondary scheduler is not enabled for service: %1%", attributes.name);
        return nullptr;
#endif
    default:
        BROOKESIA_LOGE("Unsupported scheduler type for service: %1%", attributes.name);
        return nullptr;
    }
}

bool ServiceManager::remove_service(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    BROOKESIA_CHECK_FALSE_RETURN(!name.empty(), false, "Invalid service name");

    std::shared_ptr<ServiceBase> service;
    {
        boost::unique_lock lock(service_mutex_);
        auto service_it = services_.find(name);
        if (service_it == services_.end()) {
            BROOKESIA_LOGD("Service not found: %1%", name);
            return true;
        }
        while ((service_it->second.state == ServiceState::Starting) ||
                (service_it->second.state == ServiceState::Stopping)) {
            service_it->second.transition_cv.wait(lock);
            service_it = services_.find(name);
            if (service_it == services_.end()) {
                return true;
            }
        }
        service = service_it->second.service;
    }
    BROOKESIA_CHECK_NULL_RETURN(service, false, "Service instance is null: %1%", name);

    if (service->is_initialized()) {
        BROOKESIA_LOGI("Deinitializing service: %1%", name);
        service->deinit();
    }

    {
        boost::lock_guard lock(service_mutex_);
        services_.erase(name);
        service_init_order_.remove(name);
    }

    BROOKESIA_LOGI("Service removed: %1%", name);

    return true;
}

ServiceBinding ServiceManager::bind(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), ServiceBinding(), "Not initialized");

    // First collect dependencies without holding the lock
    std::vector<std::string> dependencies;
    {
        boost::shared_lock lock(service_mutex_);
        auto service_it = services_.find(name);
        if (service_it == services_.end()) {
            BROOKESIA_LOGW("Service not found: %1%", name);
            return ServiceBinding();
        }
        auto &service_info = service_it->second;
        BROOKESIA_CHECK_NULL_RETURN(service_info.service, ServiceBinding(), "Service instance is null: %1%", name);
        BROOKESIA_CHECK_FALSE_RETURN(
            service_info.service->get_attributes().bindable, ServiceBinding(), "Service is not bindable: %1%", name
        );
        dependencies = service_info.service->get_attributes().dependencies;
    }

    // Bind all dependencies WITHOUT holding the lock to avoid deadlock
    std::vector<ServiceBinding> dependency_bindings;
    for (const auto &dep_name : dependencies) {
        BROOKESIA_LOGD("ServiceBinding dependency: %1% for service: %2%", dep_name, name);
        auto dep_binding = bind(dep_name);
        BROOKESIA_CHECK_FALSE_RETURN(
            dep_binding.is_valid(), ServiceBinding(), "Failed to bind dependency: %1% for service: %2%",
            dep_name, name
        );
        dependency_bindings.push_back(std::move(dep_binding));
    }

    // Now acquire exclusive lock for starting the service
    boost::unique_lock lock(service_mutex_);

    // Re-check if the service still exists
    auto service_it = services_.find(name);
    if (service_it == services_.end()) {
        BROOKESIA_LOGW("Service not found after binding dependencies: %1%", name);
        return ServiceBinding();
    }

    auto &service_info = service_it->second;
    BROOKESIA_CHECK_NULL_RETURN(service_info.service, ServiceBinding(), "Service instance is null: %1%", name);

    // A new bind must not overlap the previous start or stop transition.
    while ((service_info.state == ServiceState::Starting) || (service_info.state == ServiceState::Stopping)) {
        BROOKESIA_LOGD("Service %1% is changing state, waiting...", name);
        service_info.transition_cv.wait(lock);
    }

    // Check if we need to start the service
    bool need_start = (service_info.state == ServiceState::Stopped);
    std::shared_ptr<ServiceBase> bound_service = service_info.service;

    if (need_start) {
        // Mark service as starting and increment ref_count
        service_info.state = ServiceState::Starting;
        service_info.ref_count++;
        std::shared_ptr<ServiceBase> service_to_start = service_info.service;

        // Release lock before calling start() to avoid blocking other operations
        lock.unlock();

        bool start_success = service_to_start->start();

        // Re-acquire lock to update state
        lock.lock();

        // Re-find service in case the map was modified while unlocked
        service_it = services_.find(name);
        if (service_it == services_.end()) {
            BROOKESIA_LOGE("Service removed while starting: %1%", name);
            // Service was removed, need to stop it if we started it
            if (start_success) {
                lock.unlock();
                service_to_start->stop();
                lock.lock();
            }
            return ServiceBinding();
        }

        // Re-get service info reference after re-acquiring lock
        auto &service_info_after = service_it->second;

        if (!start_success) {
            // Start failed, decrement ref_count and reset state
            service_info_after.ref_count--;
            service_info_after.state = ServiceState::Stopped;
            // Notify waiting threads that start failed
            service_info_after.transition_cv.notify_all();
            BROOKESIA_LOGE("Failed to start service: %1%", name);
            return ServiceBinding();
        }

        // Start succeeded, mark as running
        service_info_after.state = ServiceState::Running;
        // Notify all waiting threads that start completed
        service_info_after.transition_cv.notify_all();
        bound_service = service_info_after.service;

        if (service_info_after.ref_count == 1) {
            BROOKESIA_LOGI("Service started: %1%", name);
        } else {
            // Other threads also bound while we were starting
            BROOKESIA_LOGD("Service started and bound by multiple threads: %1% (ref_count: %2%)",
                           name, service_info_after.ref_count);
        }

        BROOKESIA_LOGD("Service bound: %1% (ref_count: %2%)", name, service_info_after.ref_count);
    } else {
        // Service already running, just increment ref_count
        service_info.ref_count++;
        BROOKESIA_LOGD("Service bound: %1% (ref_count: %2%)", name, service_info.ref_count);
    }

    // Create the unbind callback
    auto unbind_callback = [this](const std::string & service_name) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_LOGD("Params: service_name(%1%)", service_name);
        if (is_initialized()) {
            this->unbind(service_name);
        }
    };

    // Return the valid binding object (including dependencies)
    return ServiceBinding(unbind_callback, std::move(bound_service), std::move(dependency_bindings));
}

void ServiceManager::unbind(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "Not initialized");

    std::shared_ptr<ServiceBase> service_to_stop;
    bool should_stop = false;
    {
        boost::unique_lock lock(service_mutex_);

        // Check if the service exists
        auto service_it = services_.find(name);
        if (service_it == services_.end()) {
            BROOKESIA_LOGW("Service not found: %1%", name);
            return;
        }

        auto &service_info = service_it->second;
        if (service_info.ref_count <= 0) {
            BROOKESIA_LOGW("Service ref_count is not greater than 0: %1%", name);
            return;
        }

        // Check if we need to stop the service BEFORE decreasing ref_count
        // This prevents race condition where bind() sees ref_count == 0 and starts the service
        // while we're trying to stop it
        if ((service_info.ref_count == 1) && service_info.service && service_info.service->is_running()) {
            service_to_stop = service_info.service;
            should_stop = true;
        }

        // Decrease the reference count AFTER determining if we need to stop
        service_info.ref_count--;
        BROOKESIA_LOGD("Service unbound: %1% (ref_count: %2%)", name, service_info.ref_count);

        // Keep the service in Stopping until stop() has completed so a new bind cannot overlap it.
        if (service_info.ref_count == 0) {
            if (should_stop) {
                service_info.state = ServiceState::Stopping;
            } else {
                service_info.state = ServiceState::Stopped;
                service_info.transition_cv.notify_all();
            }
        }
    }

    // Stop the service AFTER releasing the lock to avoid deadlock
    // But we've already checked ref_count == 1 before decrementing, so bind() won't interfere
    if (should_stop && service_to_stop) {
        service_to_stop->stop();

        boost::unique_lock lock(service_mutex_);
        auto service_it = services_.find(name);
        if ((service_it != services_.end()) && (service_it->second.state == ServiceState::Stopping)) {
            service_it->second.state = ServiceState::Stopped;
            service_it->second.transition_cv.notify_all();
        }

        BROOKESIA_LOGI("Service stopped: %1%", name);
    }
}

std::vector<std::string> ServiceManager::get_service_names() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::shared_lock lock(service_mutex_);
    std::vector<std::string> result;
    result.reserve(services_.size());
    for (const auto &[name, runtime_info] : services_) {
        if (runtime_info.service != nullptr) {
            result.push_back(name);
        }
    }
    return result;
}

std::optional<ServiceManager::ServiceInfo> ServiceManager::get_service_info(const std::string &name) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: name(%1%)", name);

    boost::shared_lock lock(service_mutex_);
    auto service_it = services_.find(name);
    if ((service_it == services_.end()) || (service_it->second.service == nullptr)) {
        return std::nullopt;
    }
    const auto &runtime_info = service_it->second;
    const auto &attributes = runtime_info.service->get_attributes();
    const bool is_builtin = (name == ManagerService::get_name()) || (name == UtilsService::get_name());
    const auto reference_count = is_builtin && is_running() && (runtime_info.ref_count > 0)
                                 ? runtime_info.ref_count - 1 : runtime_info.ref_count;
    return ServiceInfo{
        .name = name,
        .version = attributes.version,
        .state = runtime_info.state,
        .reference_count = reference_count,
        .bindable = attributes.bindable,
        .dependencies = attributes.dependencies,
    };
}

std::optional<ManagerService::ServiceSchemaOverview> ServiceManager::get_service_schema(
    const std::string &name
) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: name(%1%)", name);

    std::shared_ptr<ServiceBase> service;
    std::string description;
    {
        boost::shared_lock lock(service_mutex_);
        auto service_it = services_.find(name);
        if ((service_it == services_.end()) || (service_it->second.service == nullptr)) {
            return std::nullopt;
        }
        service = service_it->second.service;
        description = service->get_attributes().description;
    }

    auto function_registry = service->get_function_registry();
    auto event_registry = service->get_event_registry();
    return ManagerService::ServiceSchemaOverview{
        .name = name,
        .description = std::move(description),
        .function_names = function_registry ? function_registry->get_names() : std::vector<std::string>{},
        .event_names = event_registry ? event_registry->get_names() : std::vector<std::string>{},
    };
}

std::optional<FunctionSchema> ServiceManager::get_service_function_schema(
    const std::string &service_name, const std::string &function_name
) const
{
    std::shared_ptr<ServiceBase> service;
    {
        boost::shared_lock lock(service_mutex_);
        auto service_it = services_.find(service_name);
        if ((service_it == services_.end()) || (service_it->second.service == nullptr)) {
            return std::nullopt;
        }
        service = service_it->second.service;
    }
    auto registry = service->get_function_registry();
    return registry ? registry->get_schema_copy(function_name) : std::nullopt;
}

std::optional<EventSchema> ServiceManager::get_service_event_schema(
    const std::string &service_name, const std::string &event_name
) const
{
    std::shared_ptr<ServiceBase> service;
    {
        boost::shared_lock lock(service_mutex_);
        auto service_it = services_.find(service_name);
        if ((service_it == services_.end()) || (service_it->second.service == nullptr)) {
            return std::nullopt;
        }
        service = service_it->second.service;
    }
    auto registry = service->get_event_registry();
    return registry ? registry->get_schema_copy(event_name) : std::nullopt;
}

std::vector<std::string> ServiceManager::topological_sort(
    const std::map<std::string, std::shared_ptr<ServiceBase>> &all_services
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::vector<std::string> result;
    std::map<std::string, int> in_degree;
    std::map<std::string, std::vector<std::string>> adj_list;

    // Build the graph and in-degree table
    for (const auto &[name, service] : all_services) {
        in_degree[name] = 0;
        adj_list[name] = {};
    }

    for (const auto &[name, service] : all_services) {
        for (const auto &dep : service->get_attributes().dependencies) {
            if (all_services.find(dep) != all_services.end()) {
                adj_list[dep].push_back(name);
                in_degree[name]++;
            } else {
                BROOKESIA_LOGW("Service %1% depends on %2%, but %3% is not registered", name, dep, dep);
            }
        }
    }

    // Kahn algorithm to perform topological sort
    std::vector<std::string> queue;
    for (const auto &[name, degree] : in_degree) {
        if (degree == 0) {
            queue.push_back(name);
        }
    }

    while (!queue.empty()) {
        std::string current = queue.back();
        queue.pop_back();
        result.push_back(current);

        for (const auto &neighbor : adj_list[current]) {
            in_degree[neighbor]--;
            if (in_degree[neighbor] == 0) {
                queue.push_back(neighbor);
            }
        }
    }

    // Check if there is a circular dependency
    if (result.size() != all_services.size()) {
        BROOKESIA_LOGE("Circular dependency detected in services");
        // Find which services have circular dependencies
        for (const auto &[name, degree] : in_degree) {
            if (degree > 0) {
                BROOKESIA_LOGE("Service %1% is part of circular dependency (in_degree: %2%)", name, degree);
            }
        }
        return {};
    }

    BROOKESIA_LOGI("Service initialization order: ");
    for (size_t i = 0; i < result.size(); i++) {
        BROOKESIA_LOGI("  %1%. %2%", i + 1, result[i]);
    }

    return result;
}

void ServiceManager::add_all_registered_services()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Get all registered services
    auto service_instances = ServiceRegistry::get_all_instances();
    if (service_instances.empty()) {
        BROOKESIA_LOGD("No services registered");
        return;
    }

    // Calculate initialization order using topological sort
    auto sorted_order = topological_sort(service_instances);
    if (sorted_order.empty() && !service_instances.empty()) {
        BROOKESIA_LOGE("Failed to determine service initialization order");
        return;
    }

    // Add services in topological order using add_service
    for (const auto &name : sorted_order) {
        auto service = service_instances[name];
        if (!add_service(service)) {
            BROOKESIA_LOGE("Failed to add service: %1%", name);
        }
    }

    BROOKESIA_LOGI("All services added");
}

void ServiceManager::remove_all_registered_services()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Remove services in reverse initialization order using remove_service
    // Process from back to front since remove_service will modify service_init_order_
    while (true) {
        std::shared_ptr<ServiceBase> service_to_stop;
        std::string name;

        // Safely get the last service name with lock protection
        {
            boost::lock_guard lock(service_mutex_);
            if (service_init_order_.empty()) {
                break;
            }
            name = service_init_order_.back();

            // Stop the service if it's still running
            auto service_it = services_.find(name);
            if (service_it != services_.end()) {
                auto &service_info = service_it->second;
                if (service_info.service && service_info.service->is_running()) {
                    service_to_stop = service_info.service;
                }
            }
        }
        if (service_to_stop) {
            service_to_stop->stop();
        }

        // Remove the service using remove_service (has its own lock)
        if (!remove_service(name)) {
            BROOKESIA_LOGW("Failed to remove service: %1%", name);
            // Manually remove from list if remove_service failed to avoid infinite loop
            boost::lock_guard lock(service_mutex_);
            service_init_order_.remove(name);
        }
    }

    BROOKESIA_LOGI("All services removed");
}

} // namespace esp_brookesia::service
