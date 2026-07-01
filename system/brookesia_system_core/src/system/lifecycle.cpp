/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include <string>
#include <vector>

#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/system/impl.hpp"

namespace esp_brookesia::system::core {
namespace {

std::vector<lib_utils::ThreadConfig> make_scheduler_worker_configs();

} // namespace

std::expected<void, std::string> System::init(Config config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD(
        "Params: system_type(%1%), install_registered_apps(%2%), install_package_apps(%3%)",
        config.system_type, config.install_registered_apps, config.install_package_apps
    );
    if (impl_->initialized_) {
        return {};
    }

    impl_->system_type_ = config.system_type.empty() ? BROOKESIA_SYSTEM_CORE_DEFAULT_SYSTEM_TYPE : config.system_type;
    impl_->environment_ = config.environment;
    impl_->task_scheduler_ = std::make_shared<lib_utils::TaskScheduler>();
    lib_utils::TaskScheduler::StartConfig scheduler_config;
    scheduler_config.worker_configs = make_scheduler_worker_configs();
    scheduler_config.worker_poll_interval_ms = BROOKESIA_SYSTEM_CORE_WORKER_POLL_INTERVAL_MS;
    if (!impl_->task_scheduler_->start(scheduler_config)) {
        return std::unexpected("Failed to start system task scheduler");
    }
    if (config.gui_backend) {
        impl_->gui_thread_guard_ = config.gui_backend->get_thread_guard();
    }
    if (!impl_->configure_task_groups()) {
        return std::unexpected("Failed to configure system task groups");
    }
    if (config.gui_backend) {
        const bool should_lock_gui_thread = impl_->gui_thread_guard_.has_value() &&
                                            impl_->gui_thread_guard_->lock &&
                                            impl_->gui_thread_guard_->unlock;
        if (should_lock_gui_thread) {
            impl_->gui_thread_guard_->lock();
        }
        lib_utils::FunctionGuard unlock_guard([this, should_lock_gui_thread]() {
            if (should_lock_gui_thread) {
                impl_->gui_thread_guard_->unlock();
            }
        });
        impl_->gui_runtime_ = std::make_unique<gui::Runtime>(
                                  std::move(config.gui_backend),
        gui::RuntimeTaskConfig{
            .task_scheduler = impl_->task_scheduler_,
            .gui_group = SYSTEM_GUI_TASK_GROUP,
            .event_group = SYSTEM_GUI_INPUT_TASK_GROUP,
            .enable_fast_action_dispatch = true,
        }
                              );
        impl_->gui_runtime_->set_view_debug_enabled(config.enable_gui_view_debug);
    }
    impl_->config_ = std::move(config);
    auto storage_path_resolver = [this](
                                     runtime::AppId runtime_app_id,
                                     const StorageFileLocation & location
    ) -> std::expected<std::string, std::string> {
        auto it = impl_->runtime_to_app_.find(runtime_app_id);
        if (it == impl_->runtime_to_app_.end())
        {
            return std::unexpected("Runtime app owner not found");
        }
        return impl_->resolve_storage_file_path(it->second, location);
    };
    auto event_dispatcher = [this](
                                runtime::AppId runtime_app_id,
                                std::string service_name,
                                std::string event_name,
                                std::string items_json
    ) -> std::expected<void, std::string> {
        auto it = impl_->runtime_to_app_.find(runtime_app_id);
        if (it == impl_->runtime_to_app_.end())
        {
            return std::unexpected("Runtime app owner not found");
        }
        const AppId app_id = it->second;
        auto task = [
        this,
        app_id,
        service_name = std::move(service_name),
        event_name = std::move(event_name),
        items_json = std::move(items_json)
        ]() mutable {
            auto result = impl_->dispatch_event(
                app_id,
                std::move(service_name),
                std::move(event_name),
                std::move(items_json)
            );
            if (!result)
            {
                BROOKESIA_LOGW("Failed to dispatch runtime app event: %1%", result.error());
            }
        };
        return impl_->post_task(SYSTEM_APP_INPUT_TASK_GROUP, std::move(task));
    };
    impl_->host_bridge_ = std::make_shared<SystemHostBridge>(
                              std::make_shared<runtime::RuntimeFunctionBridge>(),
                              impl_->task_scheduler_,
                              std::move(storage_path_resolver),
                              std::move(event_dispatcher)
                          );
    impl_->runtime_function_bridge_ = impl_->host_bridge_->get_function_bridge();
    impl_->runtime_ = std::make_unique<runtime::Runtime>(impl_->host_bridge_);

    auto &service_manager = service::ServiceManager::get_instance();
    if (!service_manager.is_initialized() && !service_manager.init()) {
        return std::unexpected("Failed to initialize ServiceManager");
    }
    impl_->system_service_ = std::make_shared<SystemService>(*this);
    impl_->gui_service_ = std::make_shared<GuiService>(*this);
    impl_->timer_service_ = std::make_shared<TimerService>(*this);
    if (!service_manager.add_service(impl_->system_service_)) {
        return std::unexpected("Failed to add SystemCore service");
    }
    if (!service_manager.add_service(impl_->gui_service_)) {
        return std::unexpected("Failed to add SystemGui service");
    }
    if (!service_manager.add_service(impl_->timer_service_)) {
        return std::unexpected("Failed to add SystemTimer service");
    }
    if (impl_->config_.start_service_manager && !service_manager.start()) {
        return std::unexpected("Failed to start ServiceManager");
    }
    impl_->system_service_binding_ = service_manager.bind(BROOKESIA_SYSTEM_CORE_SERVICE_NAME);
    impl_->gui_service_binding_ = service_manager.bind(BROOKESIA_SYSTEM_CORE_GUI_SERVICE_NAME);
    impl_->timer_service_binding_ = service_manager.bind(BROOKESIA_SYSTEM_CORE_TIMER_SERVICE_NAME);
    if (!impl_->system_service_binding_.is_valid() || !impl_->gui_service_binding_.is_valid() ||
            !impl_->timer_service_binding_.is_valid()) {
        return std::unexpected("Failed to bind system services");
    }

    auto storage_result = impl_->initialize_storage_layout();
    if (!storage_result) {
        return std::unexpected(storage_result.error());
    }
    impl_->load_gui_preferences();

    auto prepare_startup_overlay_result = on_prepare_startup_overlay();
    if (!prepare_startup_overlay_result) {
        return prepare_startup_overlay_result;
    }

    auto startup_overlay_result = impl_->show_startup_overlay();
    if (!startup_overlay_result) {
        return startup_overlay_result;
    }
    lib_utils::FunctionGuard startup_overlay_cleanup([this]() {
        impl_->hide_startup_overlay();
    });

    impl_->initialized_ = true;

    auto init_result = on_init();
    if (!init_result) {
        return init_result;
    }
    if (impl_->config_.install_registered_apps) {
        auto install_result = install_registered_apps();
        if (!install_result) {
            return install_result;
        }
    }
    if (impl_->config_.install_package_apps) {
        auto install_result = impl_->install_unpacked_apps();
        if (!install_result) {
            return install_result;
        }
    }
    BROOKESIA_LOGI("System core initialized: type(%1%)", impl_->system_type_);
    startup_overlay_cleanup.release();
    return {};
}

std::expected<void, std::string> System::start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    if (!impl_->initialized_) {
        return std::unexpected("System is not initialized");
    }
    if (impl_->started_) {
        return {};
    }
    auto result = on_start();
    if (!result) {
        impl_->hide_startup_overlay();
        return result;
    }
    impl_->hide_startup_overlay();
    impl_->started_ = true;
    BROOKESIA_LOGI("System core started");
    return {};
}

void System::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    if (!impl_->started_) {
        return;
    }
    on_stop();
    impl_->started_ = false;
    BROOKESIA_LOGI("System core stopped");
}

void System::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    if (!impl_ || !impl_->initialized_) {
        return;
    }
    stop();
    impl_->hide_startup_overlay();
    for (auto &[_, record] : impl_->apps_) {
        if (record.info.state == AppState::Running || record.info.state == AppState::Paused) {
            auto stop_result = stop_app(record.info.app_id);
            if (!stop_result) {
                BROOKESIA_LOGW(
                    "Failed to stop app while deinitializing system: app_id(%1%), error(%2%)",
                    record.info.app_id,
                    stop_result.error()
                );
            }
        }
        if (record.native_app && record.context) {
            record.native_app->on_uninstall(*record.context);
        }
        impl_->cancel_app_timers(record);
        impl_->unload_runtime(record);
        const AppId app_id = record.info.app_id;
        auto *record_ptr = &record;
        auto cleanup_result = impl_->run_task_sync<std::expected<void, std::string>>(
                                  SYSTEM_GUI_TASK_GROUP,
        [this, record_ptr, app_id]() -> std::expected<void, std::string> {
            impl_->clear_pending_gui_bindings(app_id);
            impl_->unload_gui(*record_ptr);
            impl_->unregister_app_gui_resources(*record_ptr);
            impl_->unregister_app_icon_resource(*record_ptr);
            return {};
        },
        std::unexpected("Failed to post app GUI cleanup task")
                              );
        if (!cleanup_result) {
            BROOKESIA_LOGW(
                "Failed to cleanup app GUI while deinitializing system: app_id(%1%), error(%2%)",
                app_id,
                cleanup_result.error()
            );
        }
    }
    impl_->apps_.clear();
    impl_->manifest_id_to_app_.clear();
    impl_->image_resource_owners_.clear();
    impl_->font_resource_owners_.clear();
    impl_->system_service_binding_.release();
    impl_->gui_service_binding_.release();
    impl_->timer_service_binding_.release();
    auto &service_manager = service::ServiceManager::get_instance();
    service_manager.remove_service(BROOKESIA_SYSTEM_CORE_TIMER_SERVICE_NAME);
    service_manager.remove_service(BROOKESIA_SYSTEM_CORE_GUI_SERVICE_NAME);
    service_manager.remove_service(BROOKESIA_SYSTEM_CORE_SERVICE_NAME);
    if (impl_->runtime_) {
        impl_->runtime_->deinit();
    }
    auto gui_cleanup_result = impl_->run_task_sync<std::expected<void, std::string>>(
                                  SYSTEM_GUI_TASK_GROUP,
    [this]() -> std::expected<void, std::string> {
        impl_->stop_live_preview_poll();
        impl_->gui_runtime_.reset();
        return {};
    },
    std::unexpected("Failed to post GUI runtime cleanup task")
                              );
    if (!gui_cleanup_result) {
        BROOKESIA_LOGW("Failed to cleanup GUI runtime while deinitializing system: %1%", gui_cleanup_result.error());
    }
    impl_->gui_thread_guard_.reset();
    if (impl_->task_scheduler_) {
        impl_->task_scheduler_->stop();
        impl_->task_scheduler_.reset();
    }
    impl_->runtime_initialized_ = false;
    on_deinit();
    impl_->initialized_ = false;
    BROOKESIA_LOGI("System core deinitialized");
}

void System::process_timers()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
}

namespace {

std::vector<lib_utils::ThreadConfig> make_scheduler_worker_configs()
{
    constexpr int max_worker_num = 4;
    const std::array<int, max_worker_num> worker_core_ids = {
        BROOKESIA_SYSTEM_CORE_WORKER_0_CORE_ID,
        BROOKESIA_SYSTEM_CORE_WORKER_1_CORE_ID,
        BROOKESIA_SYSTEM_CORE_WORKER_2_CORE_ID,
        BROOKESIA_SYSTEM_CORE_WORKER_3_CORE_ID,
    };
    int worker_num = BROOKESIA_SYSTEM_CORE_WORKER_NUM;
    if (worker_num < 1) {
        worker_num = 1;
    } else if (worker_num > max_worker_num) {
        worker_num = max_worker_num;
    }

    std::vector<lib_utils::ThreadConfig> worker_configs;
    worker_configs.reserve(worker_num);
    for (int index = 0; index < worker_num; ++index) {
        worker_configs.emplace_back(lib_utils::ThreadConfig{
            .name = std::string(BROOKESIA_SYSTEM_CORE_WORKER_NAME_PREFIX) + std::to_string(index),
            .core_id = worker_core_ids[index],
            .priority = BROOKESIA_SYSTEM_CORE_WORKER_PRIORITY,
            .stack_size = BROOKESIA_SYSTEM_CORE_WORKER_STACK_SIZE,
            .stack_in_ext = BROOKESIA_SYSTEM_CORE_WORKER_STACK_IN_EXT,
        });
    }
    return worker_configs;
}

} // namespace

} // namespace esp_brookesia::system::core
