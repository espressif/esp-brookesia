/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <chrono>
#include "esp_netif.h"
#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/service/manager.hpp"

namespace esp_brookesia::service {

ServiceBinding::ServiceBinding(ServiceBinding &&other) noexcept
    : unbind_callback_(std::move(other.unbind_callback_))
    , service_(other.service_)
    , dependencies_(std::move(other.dependencies_))
{
    BROOKESIA_LOGI("Moving binding: %1%", service_ ? service_->get_attributes().name : "null");
    other.unbind_callback_ = nullptr;
    other.service_ = nullptr;
}

ServiceBinding &ServiceBinding::operator=(ServiceBinding &&other) noexcept
{
    if (this != &other) {
        BROOKESIA_LOGI("Move assignment: %1%", other.service_ ? other.service_->get_attributes().name : "null");
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
        BROOKESIA_LOGI("Releasing binding: %1%", service_->get_attributes().name);
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

    if (is_initialized()) {
        deinit();
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

    is_initialized_.store(false);
}

bool ServiceManager::start(const StartConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(state_mutex_);

    if (is_running()) {
        BROOKESIA_LOGD("Already running");
        return true;
    }

    if (!is_initialized()) {
        BROOKESIA_LOGI("Not initialized, initializing...");
        BROOKESIA_CHECK_FALSE_RETURN(init_internal(), false, "Failed to initialize");
    }

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_internal();
    });

    // Start IO thread (low priority to ensure timely network event processing)
    if (io_context_.stopped()) {
        io_context_.restart();
    }
    io_work_guard_.emplace(io_context_.get_executor());
    {
        auto io_thread_func = [this, config]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

            BROOKESIA_LOGI(
                "IO thread started (%1%)",
                BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(config.io_thread_config, BROOKESIA_DESCRIBE_FORMAT_VERBOSE)
            );

            // Use polling mode to reduce latency
            while (!io_context_.stopped() && !boost::this_thread::interruption_requested()) {
                try {
                    // poll() non-blocking check for ready operations
                    size_t executed = io_context_.poll();
                    if (executed == 0) {
                        // No operations ready, briefly yield CPU
                        boost::this_thread::sleep_for(boost::chrono::milliseconds(config.io_poll_interval_ms));
                    }
                } catch (const std::exception &e) {
                    BROOKESIA_LOGE("IO thread error: %3%", e.what());
                }
            }

            BROOKESIA_LOGI("IO thread stopped");
        };
        BROOKESIA_THREAD_CONFIG_GUARD(config.io_thread_config);
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            io_thread_ = boost::thread(std::move(io_thread_func)), false, "Failed to create IO thread"
        );
    }

    is_running_.store(true);

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

    // Stop RPC server first
    if (is_rpc_server_running()) {
        stop_rpc_server();
    }

    // Wait for IO thread to finish
    io_work_guard_.reset();
    io_context_.stop();
    if (io_thread_.joinable()) {
        io_thread_.interrupt();
        if (!io_thread_.try_join_for(boost::chrono::milliseconds(0))) {
            io_thread_.join();
        }
    }

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
        BROOKESIA_CHECK_FALSE_RETURN(service->init(io_context_), false, "Failed to initialize service");
    }

    {
        boost::lock_guard lock(service_mutex_);
        services_[name] = std::make_tuple(0, service);
        service_init_order_.push_back(name);
    }

    BROOKESIA_LOGI("Service added: %1%", name);

    return true;
}

bool ServiceManager::remove_service(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    BROOKESIA_CHECK_FALSE_RETURN(!name.empty(), false, "Invalid service name");

    std::shared_ptr<ServiceBase> service;
    {
        boost::lock_guard lock(service_mutex_);
        auto service_it = services_.find(name);
        if (service_it == services_.end()) {
            BROOKESIA_LOGD("Service not found: %1%", name);
            return true;
        }
        service = std::get<1>(service_it->second);
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

    boost::unique_lock lock(service_mutex_);

    // Check if the service is in the initialization list
    auto service_it = services_.find(name);
    if (service_it == services_.end()) {
        BROOKESIA_LOGW("Service not found: %1%", name);
        return ServiceBinding();
    }

    auto & [ref_count, service] = service_it->second;
    BROOKESIA_CHECK_NULL_RETURN(service, ServiceBinding(), "Service instance is null: %1%", name);

    // First recursively bind all dependencies
    std::vector<ServiceBinding> dependency_bindings;
    const auto &dependencies = service->get_attributes().dependencies;
    for (const auto &dep_name : dependencies) {
        BROOKESIA_LOGD("ServiceBinding dependency: %1% for service: %2%", dep_name, name);
        auto dep_binding = bind(dep_name);
        if (!dep_binding.is_valid()) {
            BROOKESIA_LOGW("Failed to bind dependency: %1% for service: %2%", dep_name, name);
            // If the dependency binding fails, break the loop and return an invalid ServiceBinding
            break;
        }
        dependency_bindings.push_back(std::move(dep_binding));
    }

    // If the reference count is 0, start the service (without holding the lock)
    bool need_start = (ref_count == 0);
    if (need_start) {
        std::shared_ptr<ServiceBase> service_to_start = service;  // Save service pointer before releasing lock

        // Release lock before calling start() to avoid blocking other operations
        lock.unlock();

        bool start_success = service_to_start->start();

        // Re-acquire lock to update ref_count
        lock.lock();

        // Re-find service in case the map was modified while unlocked
        service_it = services_.find(name);
        if (service_it == services_.end()) {
            BROOKESIA_LOGE("Service removed while starting: %1%", name);
            return ServiceBinding();
        }

        // Re-get ref_count and service reference after re-acquiring lock
        auto & [ref_count_after, service_after] = service_it->second;

        if (!start_success) {
            BROOKESIA_LOGE("Failed to start service: %1%", name);
            return ServiceBinding();
        }

        // Double-check ref_count after re-acquiring lock
        if (ref_count_after == 0) {
            BROOKESIA_LOGI("Service started: %1%", name);
        } else {
            // Another thread already started the service, just log
            BROOKESIA_LOGD("Service already started by another thread: %1%", name);
        }

        // Update ref_count and service pointer
        ref_count_after++;
        service = service_after;
        BROOKESIA_LOGI("Service bound: %1% (ref_count: %2%)", name, ref_count_after);
    } else {
        // Service already started, just increment ref_count
        ref_count++;
        BROOKESIA_LOGI("Service bound: %1% (ref_count: %2%)", name, ref_count);
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
    return ServiceBinding(unbind_callback, service, std::move(dependency_bindings));
}

bool ServiceManager::start_rpc_server(const rpc::Server::Config &config, uint32_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%), timeout_ms(%2%)", BROOKESIA_DESCRIBE_TO_STR(config), timeout_ms);

    // Use write lock to protect RPC server operations
    boost::unique_lock lock(rpc_mutex_);

    if (rpc_server_ && rpc_server_->is_running()) {
        BROOKESIA_LOGD("RPC server already started");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        // Directly operate in the guard, no need to acquire the lock again (already held externally)
        if (rpc_server_ && rpc_server_->is_running()) {
            rpc_server_->stop();
        }
        rpc_server_.reset();
    });

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_netif_init(), false, "Failed to initialize ESP-NETIF");

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        rpc_server_ = std::make_unique<rpc::Server>(io_context_, config), false, "Failed to create RPC server"
    );
    BROOKESIA_CHECK_FALSE_RETURN(rpc_server_->start(timeout_ms), false, "Failed to start RPC server");

    stop_guard.release();

    BROOKESIA_LOGI("RPC server started with config: %1%", BROOKESIA_DESCRIBE_TO_STR(config));

    return true;
}

void ServiceManager::stop_rpc_server()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Use write lock to protect RPC server operations
    boost::unique_lock lock(rpc_mutex_);

    if (rpc_server_ && rpc_server_->is_running()) {
        rpc_server_->stop();
    }
    rpc_server_.reset();

    BROOKESIA_LOGI("RPC server stopped");
}

bool ServiceManager::connect_rpc_server_to_services(std::vector<std::string> &&names)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Use read lock to check the RPC server status
    {
        boost::shared_lock lock(rpc_mutex_);
        BROOKESIA_CHECK_FALSE_RETURN(rpc_server_ && rpc_server_->is_running(), false, "RPC server is not running");
    }

    if (names.empty()) {
        boost::lock_guard lock(service_mutex_);
        names = std::vector<std::string>(service_init_order_.begin(), service_init_order_.end());
    }

    for (const auto &name : names) {
        boost::lock_guard lock(service_mutex_);
        auto service_it = services_.find(name);
        BROOKESIA_CHECK_FALSE_RETURN(
            service_it != services_.end(), false, "Service not found: %1%", name
        );

        auto & [ref_count, service] = service_it->second;
        if (!service) {
            BROOKESIA_LOGW("Service instance is null: %1%", name);
            continue;
        }

        auto connection = service->connect_to_server();
        if (!connection) {
            BROOKESIA_LOGW("Failed to connect to server: %1%", name);
            continue;
        }

        lib_utils::FunctionGuard disconnect_guard([this, &service]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            service->disconnect_from_server();
        });

        // Again acquire the read lock to access rpc_server_
        {
            boost::shared_lock rpc_lock(rpc_mutex_);
            if (!rpc_server_) {
                BROOKESIA_LOGE("RPC server was stopped");
                return false;
            }
            BROOKESIA_CHECK_FALSE_RETURN(
                rpc_server_->add_connection(connection), false, "Failed to add RPC server connection to server"
            );
        }

        disconnect_guard.release();

        BROOKESIA_LOGI("Connected RPC server to service: %1%", name);
    }

    return true;
}

bool ServiceManager::disconnect_rpc_server_from_services(std::vector<std::string> &&names)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Use read lock to check the RPC server status
    {
        boost::shared_lock lock(rpc_mutex_);
        if (!rpc_server_ || !rpc_server_->is_running()) {
            BROOKESIA_LOGE("RPC server not started");
            return false;
        }
    }

    if (names.empty()) {
        boost::lock_guard lock(service_mutex_);
        names = std::vector<std::string>(service_init_order_.begin(), service_init_order_.end());
    }

    for (const auto &name : names) {
        boost::lock_guard lock(service_mutex_);
        auto service_it = services_.find(name);
        BROOKESIA_CHECK_FALSE_RETURN(
            service_it != services_.end(), false, "Service not found: %1%", name
        );

        auto & [ref_count, service] = service_it->second;
        if (!service) {
            BROOKESIA_LOGW("Service instance is null: %1%", name);
            continue;
        }

        // Again acquire the read lock to access rpc_server_
        {
            boost::shared_lock rpc_lock(rpc_mutex_);
            if (!rpc_server_) {
                BROOKESIA_LOGE("RPC server was stopped");
                return false;
            }
            rpc_server_->remove_connection(name);
        }
        service->disconnect_from_server();

        BROOKESIA_LOGI("Disconnected RPC server from service: %1%", name);
    }

    return true;
}

std::shared_ptr<rpc::Client> ServiceManager::new_rpc_client(const RPC_ClientConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::shared_ptr<rpc::Client> client;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        client = std::make_shared<rpc::Client>(config.on_deinit_callback), nullptr,
        "Failed to create RPC client"
    );

    BROOKESIA_CHECK_FALSE_RETURN(
        client->init(io_context_, config.on_disconnect_callback), nullptr, "Failed to initialize RPC client"
    );

    // Use write lock to add the client to the list (for tracking)
    {
        boost::unique_lock lock(rpc_mutex_);
        rpc_clients_.push_back(client);
    }

    return client;
}

FunctionResult ServiceManager::call_rpc_function_sync(
    std::string host, const std::string &service_name, const std::string &function_name,
    boost::json::object &&params, uint32_t timeout_ms, uint16_t port
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    FunctionResult result{
        .success = false,
    };
    auto &error_message = result.error_message;

    BROOKESIA_LOGD(
        "Params: host(%1%), service_name(%2%), function_name(%3%), params(%4%), timeout_ms(%5%), port(%6%)",
        host, service_name, function_name, boost::json::serialize(params), timeout_ms, port
    );

    auto start_time = std::chrono::steady_clock::now();

    std::shared_ptr<rpc::Client> client = new_rpc_client();
    if (!client) {
        error_message = "Failed to create RPC client";
        BROOKESIA_LOGE("%1%", error_message);
        return result;
    }

    if (!client->connect(host, port, timeout_ms)) {
        error_message = (boost::format("Failed to connect to RPC server: %1%:%2%") % host % port).str();
        BROOKESIA_LOGE("%1%", error_message);
        return result;
    }

    // Calculate the remaining timeout after connection
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
    auto remaining_ms = (elapsed < std::chrono::milliseconds(timeout_ms))
                        ? (std::chrono::milliseconds(timeout_ms) - elapsed).count()
                        : 0;

    if (remaining_ms <= 0) {
        error_message = (boost::format("Timeout after connection, elapsed: %1%ms") % elapsed.count()).str();
        BROOKESIA_LOGE("%1%", error_message);
        return result;
    }

    BROOKESIA_LOGD("Calling RPC function with remaining timeout: %1%ms", remaining_ms);

    result = client->call_function_sync(service_name, function_name, std::move(params), remaining_ms);
    if (!result.success) {
        BROOKESIA_LOGE("Failed to call RPC function: %1%", result.error_message);
        return result;
    }

    return result;
}

void ServiceManager::unbind(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "Not initialized");


    std::shared_ptr<ServiceBase> service_to_stop;
    {
        boost::unique_lock lock(service_mutex_);

        // Check if the service exists
        auto service_it = services_.find(name);
        if (service_it == services_.end()) {
            BROOKESIA_LOGW("Service not found: %1%", name);
            return;
        }

        auto & [ref_count, service] = service_it->second;
        if (ref_count <= 0) {
            BROOKESIA_LOGW("Service ref_count is not greater than 0: %1%", name);
            return;
        }

        // Decrease the reference count
        ref_count--;
        BROOKESIA_LOGI("Service unbound: %1% (ref_count: %2%)", name, ref_count);

        if ((ref_count == 0) && service && service->is_running()) {
            service_to_stop = service;
        }
    }

    // If the reference count becomes 0, stop the service (but not deinitialize)
    if (service_to_stop) {
        service_to_stop->stop();
        // Remove RPC connection if needed (without holding service_mutex_)
        {
            boost::shared_lock rpc_lock(rpc_mutex_);
            if (rpc_server_ && rpc_server_->is_running()) {
                rpc_server_->remove_connection(name);
            }
        }

        BROOKESIA_LOGI("Service stopped: %1%", name);
    }
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
                auto & [ref_count, service] = service_it->second;
                if (service && service->is_running()) {
                    service_to_stop = service;
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
