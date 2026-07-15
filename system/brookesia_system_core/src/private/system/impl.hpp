/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <expected>
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "boost/json.hpp"
#include "boost/thread/future.hpp"
#if defined(__EMSCRIPTEN__)
#include "brookesia/gui_interface/wasm/gui_task_queue.hpp"
#endif
#include "brookesia/system_core/service/gui.hpp"
#include "brookesia/system_core/service/system.hpp"
#include "brookesia/system_core/service/timer.hpp"
#include "brookesia/system_core/system/system.hpp"
#include "private/runtime/host_bridge.hpp"

namespace esp_brookesia::system::core {

inline constexpr const char *APP_MODULE_NAME = "brookesia_app";
inline constexpr const char *LIFECYCLE_ON_INSTALL = "on_install";
inline constexpr const char *LIFECYCLE_ON_UNINSTALL = "on_uninstall";
inline constexpr const char *LIFECYCLE_ON_START = "on_start";
inline constexpr const char *LIFECYCLE_ON_PAUSE = "on_pause";
inline constexpr const char *LIFECYCLE_ON_RESUME = "on_resume";
inline constexpr const char *LIFECYCLE_ON_STOP = "on_stop";
inline constexpr const char *LIFECYCLE_ON_ACTION = "on_action";
inline constexpr const char *LIFECYCLE_ON_EVENT = "on_event";
inline constexpr const char *LIFECYCLE_ON_TIMER = "on_timer";
inline constexpr const char *APP_MANIFEST_FILE = "manifest.json";
inline constexpr const char *SYSTEM_GUI_TASK_GROUP = "SystemGui";
inline constexpr const char *SYSTEM_GUI_INPUT_TASK_GROUP = "SystemGuiInput";
inline constexpr const char *SYSTEM_APP_TASK_GROUP = "SystemApp";
inline constexpr const char *SYSTEM_APP_INPUT_TASK_GROUP = "SystemAppInput";
inline constexpr const char *SYSTEM_TIMER_TASK_GROUP = "SystemTimer";

extern thread_local const void *current_system_task_owner;
extern thread_local lib_utils::TaskScheduler::Group current_system_task_group;

class System::Impl {
public:
    struct AppRecord {
        AppInfo info;
        AppGuiDescriptor gui_descriptor;
        std::shared_ptr<IApp> native_app;
        std::unique_ptr<AppContext> context;
        std::optional<gui::DocumentId> document_id;
        std::optional<runtime::AppId> runtime_app_id;
        std::vector<gui::ScopedConnection> action_connections;
        std::set<std::string> action_connection_keys;
        std::vector<std::string> registered_image_resource_ids;
        std::vector<std::string> registered_font_resource_ids;
        std::string registered_icon_resource_id;
        struct TimerRecord {
            std::string name;
            bool periodic = false;
        };
        std::map<TimerId, TimerRecord> timers;
        bool runtime_loaded = false;
        bool runtime_started = false;
        bool preload_dom = false;
    };

    struct PendingTimer {
        AppId app_id = INVALID_APP_ID;
        TimerId timer_id = INVALID_TIMER_ID;
        std::string name;
    };

    struct PendingBindingBuffer {
        std::vector<gui::BindingValueUpdate> updates;
        std::map<std::string, std::size_t> indexes;
        bool flush_posted = false;
    };

    struct TransientOverlayRecord {
        gui::DocumentId document_id;
        gui::TransientMountId mount_id;
        std::chrono::steady_clock::time_point shown_at;
    };

    struct KeyboardRequestRecord {
        KeyboardRequestId request_id = INVALID_KEYBOARD_REQUEST_ID;
        AppId app_id = INVALID_APP_ID;
        KeyboardResultHandler handler;
    };

    struct MessageDialogRequestRecord {
        MessageDialogRequestId request_id = INVALID_MESSAGE_DIALOG_REQUEST_ID;
        AppId app_id = INVALID_APP_ID;
        MessageDialogOptions options;
        MessageDialogResultHandler handler;
    };

    explicit Impl(System &owner);

    bool is_current_task_group(std::string_view group) const;
    bool is_current_task_domain(std::string_view group) const;
    lib_utils::TaskScheduler::GroupConfig make_task_group_config(
        const lib_utils::TaskScheduler::Group &group,
        bool use_gui_thread_guard = false,
        bool use_gui_runtime_gate = false,
        bool use_app_callback_gate = false
    );
    bool configure_task_groups();
    bool should_schedule_app_task() const;
    bool should_defer_gui_task(std::string_view group) const;
    void execute_task_with_group_context(
        const lib_utils::TaskScheduler::Group &group,
        lib_utils::TaskScheduler::OnceTask task
    );
    std::expected<void, std::string> post_task(
        const lib_utils::TaskScheduler::Group &group,
        lib_utils::TaskScheduler::OnceTask task
    );
    std::expected<GuiBatchResult, std::string> execute_gui_batch_now(
        AppId app_id,
        const std::vector<GuiBatchCommand> &commands
    );
    template <typename Result, typename Fn>
    Result run_task_sync(
        const lib_utils::TaskScheduler::Group &group,
        Fn &&fn,
        Result post_error_result
    )
    {
        if (is_current_task_domain(group) || !task_scheduler_ || !task_scheduler_->is_running()) {
            return fn();
        }

#if defined(__EMSCRIPTEN__)
        if (esp_brookesia::gui::wasm::is_gui_task_blocked()) {
            return std::move(post_error_result);
        }
        std::optional<Result> direct_result;
        execute_task_with_group_context(
            group,
        [&direct_result, fn = std::forward<Fn>(fn)]() mutable {
            direct_result.emplace(fn());
        }
        );
        if (direct_result.has_value()) {
            return std::move(*direct_result);
        }
        return std::move(post_error_result);
#endif

        auto result_promise = std::make_shared<boost::promise<Result>>();
        auto result_future = result_promise->get_future();
        auto task = [result_promise, fn = std::forward<Fn>(fn)]() mutable {
            result_promise->set_value(fn());
        };
        if (!task_scheduler_->post(std::move(task), nullptr, group)) {
            return std::move(post_error_result);
        }
        return result_future.get();
    }

    std::expected<void, std::string> post_gui_task(lib_utils::TaskScheduler::OnceTask task);
    std::expected<void, std::string> post_gui_input_task(lib_utils::TaskScheduler::OnceTask task);

    std::expected<void, std::string> enqueue_gui_binding_update(
        AppId app_id,
        std::string_view absolute_path,
        std::string_view key,
        std::string value
    );
    std::expected<void, std::string> enqueue_gui_binding_updates(
        AppId app_id,
        std::vector<gui::BindingValueUpdate> updates
    );
    void flush_pending_gui_bindings(AppId app_id);
    void clear_pending_gui_bindings(AppId app_id);
    gui::Runtime::ActionHandler make_app_action_forwarder(AppId app_id);
    std::expected<void, std::string> subscribe_gui_action_now(AppId app_id, std::string action);
    std::expected<void, std::string> subscribe_gui_actions_now(AppId app_id, const std::vector<std::string> &actions);

    std::expected<AppRecord *, std::string> get_record(AppId app_id);
    std::expected<const AppRecord *, std::string> get_record(AppId app_id) const;

    std::expected<void, std::string> ensure_runtime_initialized();
    std::expected<void, std::string> ensure_gui_loaded(AppRecord &record);
    std::expected<void, std::string> prepare_installed_app_gui(AppRecord &record);
    void release_app_gui_presentation(AppRecord &record);
    void cleanup_stopped_app_gui(AppRecord &record);
    void rollback_installed_app_gui(AppRecord &record);
    void unload_gui(AppRecord &record);
    std::expected<void, std::string> enable_live_preview_for_document(
        gui::DocumentId document_id,
        const gui::LivePreviewOptions &options
    );
    bool disable_live_preview_for_document(gui::DocumentId document_id);
    void remove_live_preview_document(gui::DocumentId document_id);
    void poll_live_preview_documents();
    void stop_live_preview_poll();
    void stop_live_preview_poll_if_idle();
    bool ensure_live_preview_poll_started();
    void unregister_app_gui_resources(AppRecord &record);
    std::expected<void, std::string> register_app_gui_resources(AppRecord &record);
    void unregister_app_icon_resource(AppRecord &record);
    std::expected<void, std::string> register_app_icon_resource(AppRecord &record);
    std::string resolve_app_resource_dir(const AppManifest &manifest) const;
    std::string resolve_app_resource_path(const AppManifest &manifest, std::string_view path) const;
    void cancel_app_timers(AppRecord &record);
    std::expected<void, std::string> call_runtime_lifecycle(
        AppRecord &record,
        std::string_view function_name,
        const runtime::NativeArgs &args = {}
    );
    std::expected<void, std::string> ensure_runtime_loaded(AppRecord &record);
    void unload_runtime(AppRecord &record);
    std::expected<void, std::string> dispatch_action(
        AppId app_id,
        std::string action,
        std::string path = {},
        std::string payload_json = {}
    );
    std::expected<void, std::string> dispatch_event(
        AppId app_id,
        std::string service_name,
        std::string event_name,
        std::string items_json
    );
    std::expected<void, std::string> dispatch_timer(AppId app_id, TimerId timer_id, std::string name);
    std::expected<void, std::string> install_unpacked_apps();
    std::expected<void, std::string> initialize_storage_layout();
    void load_gui_preferences();
    void persist_gui_theme_preference(std::string_view theme_id);
    void persist_gui_language_preference(std::string_view language);
    GuiThemeLanguage get_gui_theme_language_snapshot() const;
    void set_gui_theme_snapshot(std::string_view theme_id);
    void set_gui_language_snapshot(std::string_view language);
    std::expected<void, std::string> ensure_standard_storage_dirs();
    std::expected<AppStoragePaths, std::string> ensure_app_storage_paths(std::string_view manifest_id);
    std::expected<std::filesystem::path, std::string> get_default_install_apps_root() const;
    std::vector<std::filesystem::path> get_app_scan_roots() const;
    bool is_managed_app_directory(const std::filesystem::path &path) const;
    std::string get_system_storage_dir() const;
    std::expected<std::filesystem::path, std::string> resolve_storage_location_path(
        AppId app_id,
        const StorageFileLocation &location,
        bool ensure_base
    );
    std::expected<std::string, std::string> resolve_storage_file_path(
        AppId app_id,
        const StorageFileLocation &location
    );
    std::expected<void, std::string> show_startup_overlay();
    void hide_startup_overlay();
    std::expected<std::optional<TransientOverlayRecord>, std::string> show_app_launch_transition(
        const AppRecord &record,
        const System::AppStartOptions &options
    );
    void hide_transient_overlay(std::optional<TransientOverlayRecord> &record);

    System &owner_;
    mutable std::mutex mutex_;
    mutable std::mutex timer_mutex_;
    // Serializes all access to AppRecord::timers. Held across the timer
    // registration + map insertion so a delayed/periodic timer that fires
    // immediately (on the timer/app task) cannot observe the map before the
    // entry is inserted, and to prevent concurrent map mutation from HTTP/
    // worker threads and the app task.
    mutable std::recursive_mutex timer_map_mutex_;
    mutable std::mutex gui_binding_mutex_;
    mutable std::recursive_mutex app_callback_mutex_;
    mutable std::recursive_mutex gui_runtime_mutex_;
    Config config_;
    std::string system_type_ = BROOKESIA_SYSTEM_CORE_DEFAULT_SYSTEM_TYPE;
    gui::Environment environment_;
    std::unique_ptr<gui::Runtime> gui_runtime_;
    std::optional<gui::IBackend::ThreadGuard> gui_thread_guard_;
    std::shared_ptr<SystemHostBridge> host_bridge_;
    std::shared_ptr<runtime::RuntimeFunctionBridge> runtime_function_bridge_;
    std::unique_ptr<runtime::Runtime> runtime_;
    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;
    std::shared_ptr<SystemService> system_service_;
    std::shared_ptr<GuiService> gui_service_;
    std::shared_ptr<TimerService> timer_service_;
    service::ServiceBinding system_service_binding_;
    service::ServiceBinding gui_service_binding_;
    service::ServiceBinding timer_service_binding_;
    std::map<AppId, AppRecord> apps_;
    std::map<std::string, AppId> manifest_id_to_app_;
    std::map<KeyboardRequestId, KeyboardRequestRecord> keyboard_requests_;
    std::map<MessageDialogRequestId, MessageDialogRequestRecord> message_dialog_requests_;
    std::vector<MessageDialogRequestId> queued_message_dialog_requests_;
    std::optional<MessageDialogRequestId> active_message_dialog_request_id_;
    std::map<std::string, AppId> image_resource_owners_;
    std::map<std::string, AppId> font_resource_owners_;
    std::map<runtime::AppId, AppId> runtime_to_app_;
    std::map<AppId, PendingBindingBuffer> pending_gui_bindings_;
    std::set<gui::DocumentId::Value> live_preview_document_ids_;
    std::optional<lib_utils::TaskScheduler::TaskId> live_preview_poll_task_id_;
    StorageLayout storage_layout_;
    std::map<std::string, AppStoragePaths> app_storage_paths_cache_;
    GuiThemeLanguage gui_theme_language_snapshot_ {
        .theme_id = "default",
        .language = "en",
    };
    std::optional<std::string> stored_theme_id_;
    std::optional<std::string> stored_language_;
    AppId next_app_id_ = 1;
    KeyboardRequestId next_keyboard_request_id_ = 1;
    MessageDialogRequestId next_message_dialog_request_id_ = 1;
    AppId active_app_id_ = INVALID_APP_ID;
    std::vector<PendingTimer> pending_timers_;
    std::optional<TransientOverlayRecord> startup_overlay_;
    bool initialized_ = false;
    bool started_ = false;
    bool runtime_initialized_ = false;
    bool gui_preferences_restoring_ = false;
    bool gui_preferences_restored_ = false;

};

} // namespace esp_brookesia::system::core
