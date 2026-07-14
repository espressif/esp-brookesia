/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/runtime_manager/runtime.hpp"

#include <map>
#include <utility>

#include "brookesia/runtime_manager/macro_configs.h"
#if !BROOKESIA_RUNTIME_MANAGER_CORE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

namespace esp_brookesia::runtime {

class Runtime::Impl {
public:
    explicit Impl(std::shared_ptr<IRuntimeHostBridge> host_bridge)
        : host_bridge_(std::move(host_bridge))
        , function_bridge_(host_bridge_->get_function_bridge())
    {}

    std::expected<IRuntimeBackend *, std::string> get_backend(BackendType type) const
    {
        if (type == BackendType::Unknown) {
            if (backends_.empty()) {
                return std::unexpected("Runtime backend is null");
            }
            if (backends_.size() > 1) {
                return std::unexpected("Runtime app type is required when multiple backends are registered");
            }
            return backends_.begin()->second.get();
        }

        auto it = backends_.find(type);
        if (it == backends_.end()) {
            return std::unexpected("Runtime backend for app type is not registered");
        }
        return it->second.get();
    }

    std::expected<IRuntimeBackend *, std::string> get_app_backend(AppId id) const
    {
        auto it = app_backend_types_.find(id);
        if (it == app_backend_types_.end()) {
            return std::unexpected("Runtime app backend not found");
        }
        return get_backend(it->second);
    }

    std::expected<void, std::string> configure_and_init_backend(IRuntimeBackend &backend)
    {
        backend.set_native_modules(function_bridge_->get_modules());
        return backend.init();
    }

    std::expected<void, std::string> discover_registered_backends()
    {
        auto backend_plugins = RuntimeBackendRegistry::get_all_instances();
        for (auto &[name, backend] : backend_plugins) {
            if (!backend) {
                BROOKESIA_LOGW("Runtime backend plugin is null: name(%1%)", name);
                continue;
            }

            const BackendType type = backend->get_type();
            if (type == BackendType::Unknown) {
                BROOKESIA_LOGW("Runtime backend plugin type is unknown: name(%1%)", name);
                return std::unexpected("Runtime backend plugin type is unknown");
            }

            auto [it, inserted] = backends_.emplace(type, backend);
            if (!inserted && (it->second.get() != backend.get())) {
                BROOKESIA_LOGW("Runtime backend type is duplicated: type(%1%), name(%2%)", type, name);
                return std::unexpected("Runtime backend type is already registered");
            }
            if (inserted) {
                BROOKESIA_LOGI("Runtime backend discovered: name(%1%), type(%2%)", name, type);
            }
        }
        return {};
    }

    std::expected<std::pair<RuntimeApp *, IRuntimeBackend *>, std::string> get_app_and_backend(AppId id)
    {
        auto app_it = apps_.find(id);
        if (app_it == apps_.end()) {
            return std::unexpected("Runtime app not found");
        }
        auto backend = get_app_backend(id);
        if (!backend) {
            return std::unexpected(backend.error());
        }
        return std::make_pair(&app_it->second, backend.value());
    }

    std::optional<RuntimeApp> make_app_snapshot(AppId id, const RuntimeApp &app) const
    {
        RuntimeApp snapshot = app;
        auto backend = get_app_backend(id);
        if (backend) {
            snapshot.state = backend.value()->get_app_state(id);
        }
        return snapshot;
    }

    std::expected<void, std::string> stop_app_if_finish_requested(AppId id, IRuntimeBackend &backend, RuntimeApp &app)
    {
        if (!host_bridge_->consume_app_finish_request(id)) {
            return {};
        }

        BROOKESIA_LOGI("App stopping after finish request: id(%1%)", id);
        auto result = backend.stop_app(id);
        host_bridge_->release_app_resources(id);
        if (!result) {
            app.state = AppState::Error;
            app.last_error = result.error();
            BROOKESIA_LOGE("App stop after finish request failed: id(%1%), error(%2%)", id, result.error());
            return result;
        }
        app.state = AppState::Stopped;
        app.last_error.clear();
        BROOKESIA_LOGI("App stopped after finish request: id(%1%)", id);
        return {};
    }

    std::map<BackendType, std::shared_ptr<IRuntimeBackend>> backends_;
    std::shared_ptr<IRuntimeHostBridge> host_bridge_;
    std::shared_ptr<RuntimeFunctionBridge> function_bridge_;
    std::map<AppId, RuntimeApp> apps_;
    std::map<AppId, BackendType> app_backend_types_;
    AppId next_app_id_ = 1;
    bool initialized_ = false;
};

Runtime::Runtime(std::shared_ptr<IRuntimeHostBridge> host_bridge)
    : impl_(std::make_unique<Impl>(
                host_bridge ? std::move(host_bridge) : std::make_shared<RuntimeHostBridge>()
            ))
{}

Runtime::~Runtime()
{
    deinit();
}

std::expected<void, std::string> Runtime::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_RUNTIME_MANAGER_VER_MAJOR, BROOKESIA_RUNTIME_MANAGER_VER_MINOR,
        BROOKESIA_RUNTIME_MANAGER_VER_PATCH
    );
    BROOKESIA_LOGD("Params: backend_count(%1%), initialized(%2%)", impl_->backends_.size(), impl_->initialized_);
    auto discover_result = impl_->discover_registered_backends();
    if (!discover_result) {
        return discover_result;
    }
    auto function_discover_result = impl_->function_bridge_->discover_registered_providers();
    if (!function_discover_result) {
        return function_discover_result;
    }
    if (impl_->backends_.empty()) {
        return std::unexpected("Runtime backend is null");
    }
    if (impl_->initialized_) {
        return {};
    }

    BROOKESIA_LOGI("Runtime initializing with %1% backend(s)", impl_->backends_.size());
    for (auto &backend_item : impl_->backends_) {
        auto &backend = backend_item.second;
        auto result = impl_->configure_and_init_backend(*backend);
        if (!result) {
            BROOKESIA_LOGE(
                "Runtime backend init failed: type(%1%), error(%2%)",
                backend_item.first, result.error()
            );
            return result;
        }
    }
    impl_->initialized_ = true;
    BROOKESIA_LOGI("Runtime initialized");
    return {};
}

void Runtime::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD(
        "Params: initialized(%1%), app_count(%2%), backend_count(%3%)",
        impl_ ? impl_->initialized_ : false, impl_ ? impl_->apps_.size() : 0, impl_ ? impl_->backends_.size() : 0
    );
    if (!impl_ || !impl_->initialized_) {
        return;
    }
    BROOKESIA_LOGI("Runtime deinitializing");
    impl_->host_bridge_->release_all_app_resources();
    for (auto &backend_item : impl_->backends_) {
        backend_item.second->deinit();
    }
    impl_->apps_.clear();
    impl_->app_backend_types_.clear();
    impl_->initialized_ = false;
    BROOKESIA_LOGI("Runtime deinitialized");
}

std::expected<AppId, std::string> Runtime::load_app(const AppConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD(
        "Params: config(%1%), arg_count(%2%)",
        config, config.arguments.size()
    );
    if (!impl_->initialized_) {
        return std::unexpected("Runtime must be initialized before loading apps");
    }
    auto backend = impl_->get_backend(config.type);
    if (!backend) {
        BROOKESIA_LOGW("Load app failed: type(%1%), error(%2%)", config.type, backend.error());
        return std::unexpected(backend.error());
    }

    const AppId id = impl_->next_app_id_++;
    backend.value()->set_native_modules(impl_->function_bridge_->get_modules_for_app(id));
    auto result = backend.value()->load_app(id, config);
    if (!result) {
        BROOKESIA_LOGE(
            "Load app failed: id(%1%), type(%2%), path(%3%), error(%4%)",
            id, config.type, config.app_path, result.error()
        );
        return std::unexpected(result.error());
    }

    const BackendType backend_type = backend.value()->get_type();
    RuntimeApp app;
    app.id = id;
    app.config = config;
    app.state = AppState::Loaded;
    impl_->apps_.emplace(id, std::move(app));
    impl_->app_backend_types_.emplace(id, backend_type);
    BROOKESIA_LOGI("App loaded: id(%1%), type(%2%), path(%3%)", id, backend_type, config.app_path);
    return id;
}

std::expected<void, std::string> Runtime::unload_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        return std::unexpected("Runtime app not found");
    }
    auto backend = impl_->get_app_backend(id);
    if (!backend) {
        BROOKESIA_LOGW("Unload app failed: id(%1%), error(%2%)", id, backend.error());
        return std::unexpected(backend.error());
    }
    impl_->host_bridge_->release_app_resources(id);
    auto result = backend.value()->unload_app(id);
    if (!result) {
        it->second.state = AppState::Error;
        it->second.last_error = result.error();
        BROOKESIA_LOGE("Unload app failed: id(%1%), error(%2%)", id, result.error());
        return result;
    }
    impl_->apps_.erase(it);
    impl_->app_backend_types_.erase(id);
    BROOKESIA_LOGI("App unloaded: id(%1%)", id);
    return {};
}

std::expected<void, std::string> Runtime::start_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto app_backend = impl_->get_app_and_backend(id);
    if (!app_backend) {
        return std::unexpected(app_backend.error());
    }
    auto &[app, backend] = app_backend.value();
    auto app_context = impl_->host_bridge_->make_app_context_guard(id);
    BROOKESIA_LOGI("App starting: id(%1%)", id);
    auto result = backend->start_app(id);
    if (!result) {
        impl_->host_bridge_->release_app_resources(id);
        app->state = AppState::Error;
        app->last_error = result.error();
        BROOKESIA_LOGE("App start failed: id(%1%), error(%2%)", id, result.error());
        return result;
    }
    auto finish_result = impl_->stop_app_if_finish_requested(id, *backend, *app);
    if (!finish_result) {
        return finish_result;
    }
    if (app->state == AppState::Stopped) {
        return {};
    }
    app->state = AppState::Running;
    app->last_error.clear();
    BROOKESIA_LOGI("App started: id(%1%)", id);
    return {};
}

std::expected<void, std::string> Runtime::stop_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto app_backend = impl_->get_app_and_backend(id);
    if (!app_backend) {
        return std::unexpected(app_backend.error());
    }
    auto &[app, backend] = app_backend.value();
    impl_->host_bridge_->consume_app_finish_request(id);
    auto result = backend->stop_app(id);
    impl_->host_bridge_->release_app_resources(id);
    if (!result) {
        app->state = AppState::Error;
        app->last_error = result.error();
        BROOKESIA_LOGE("App stop failed: id(%1%), error(%2%)", id, result.error());
        return result;
    }
    app->state = AppState::Stopped;
    app->last_error.clear();
    BROOKESIA_LOGI("App stopped: id(%1%)", id);
    return {};
}

std::expected<NativeValue, std::string> Runtime::call_function(
    AppId id, std::string_view module_name, std::string_view function_name, const NativeArgs &args
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD(
        "Params: app_id(%1%), module(%2%), function(%3%), arg_count(%4%)",
        id, module_name, function_name, args.size()
    );
    auto app_backend = impl_->get_app_and_backend(id);
    if (!app_backend) {
        return std::unexpected(app_backend.error());
    }
    auto &[app, backend] = app_backend.value();
    auto app_context = impl_->host_bridge_->make_app_context_guard(id);
    auto result = backend->call_function(id, module_name, function_name, args);
    if (!result) {
        BROOKESIA_LOGW(
            "Call app function failed: id(%1%), module(%2%), function(%3%), error(%4%)",
            id, module_name, function_name, result.error()
        );
        return result;
    }
    auto finish_result = impl_->stop_app_if_finish_requested(id, *backend, *app);
    if (!finish_result) {
        return std::unexpected(finish_result.error());
    }
    return result;
}

std::optional<RuntimeApp> Runtime::get_app(AppId id) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        return std::nullopt;
    }
    return impl_->make_app_snapshot(id, it->second);
}

std::vector<RuntimeApp> Runtime::list_apps() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_count(%1%)", impl_->apps_.size());
    std::vector<RuntimeApp> apps;
    apps.reserve(impl_->apps_.size());
    for (const auto &[id, app] : impl_->apps_) {
        auto current = impl_->make_app_snapshot(id, app);
        if (current) {
            apps.push_back(std::move(current.value()));
        }
    }
    return apps;
}

BackendType Runtime::get_type() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: backend_count(%1%)", impl_->backends_.size());
    if (impl_->backends_.size() != 1) {
        return BackendType::Unknown;
    }
    return impl_->backends_.begin()->first;
}

} // namespace esp_brookesia::runtime
