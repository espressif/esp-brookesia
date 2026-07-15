/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "brookesia/system_super/macro_configs.h"
#if !BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "brookesia/service_helper/network/sntp.hpp"
#include "brookesia/service_helper/framework/utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/system_super/system.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "private/font_language.hpp"
#include "private/utils.hpp"
#include "private/shell_app.hpp"
#include "private/system_constants.hpp"

namespace esp_brookesia::system::super {
namespace {

using SNTPHelper = service::helper::SNTP;
using UtilsHelper = service::helper::Utils;
using UtilsService = service::UtilsService;

inline constexpr uint32_t STORAGE_FS_TIMEOUT_MS = 5000;

template <typename Value>
std::optional<Value> parse_utils_event_object(
    const service::EventItemMap &items, const std::string &item_name
)
{
    auto item = items.find(item_name);
    if ((item == items.end()) || !std::holds_alternative<boost::json::object>(item->second)) {
        return std::nullopt;
    }
    Value value;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(std::get<boost::json::object>(item->second), value)) {
        return std::nullopt;
    }
    return value;
}

void synchronize_shell_debug_snapshot(const std::weak_ptr<ShellApp> &weak_shell_app)
{
    auto shell_app = weak_shell_app.lock();
    if (shell_app == nullptr) {
        return;
    }
    auto snapshot = UtilsHelper::get_debug_snapshot();
    if (!snapshot) {
        BROOKESIA_LOGW("Failed to synchronize Utils debug snapshot: %1%", snapshot.error());
        return;
    }
    shell_app->apply_debug_config(snapshot->config);
    shell_app->apply_debug_state(snapshot->state);
    if (snapshot->memory) {
        shell_app->apply_memory_debug_snapshot(*snapshot->memory);
    }
    if (snapshot->thread) {
        shell_app->apply_thread_debug_snapshot(*snapshot->thread);
    }
}

std::string make_share_resource_dir(const System &system)
{
    const auto layout = system.get_storage_layout();
    return (std::filesystem::path(layout.internal.root_path) / "system").lexically_normal().generic_string();
}

std::string make_system_resource_dir(const System &system)
{
    const auto layout = system.get_storage_layout();
    return (std::filesystem::path(layout.internal.root_path) / "system" / "super").lexically_normal().generic_string();
}

std::string make_font_index_relative_path()
{
    return (std::filesystem::path(SUPER_FONT_DIR) / "index.json").generic_string();
}

std::string make_theme_relative_path(std::string_view file_name)
{
    return (std::filesystem::path(SUPER_THEME_DIR) / file_name).generic_string();
}

std::string make_preload_theme_id(const std::optional<std::string> &stored_theme_id)
{
    if (!stored_theme_id.has_value() || stored_theme_id->empty()) {
        return SUPER_DEFAULT_THEME_ID;
    }
    if (*stored_theme_id == SUPER_LIGHT_THEME_ID || *stored_theme_id == SUPER_DARK_THEME_ID) {
        return *stored_theme_id;
    }
    return SUPER_DEFAULT_THEME_ID;
}

std::optional<SNTPHelper::State> get_sntp_state()
{
    auto state_text = SNTPHelper::call_function_sync<std::string>(SNTPHelper::FunctionId::GetState);
    if (!state_text) {
        BROOKESIA_LOGW("Failed to get SNTP state before Super startup sync: %1%", state_text.error());
        return std::nullopt;
    }

    SNTPHelper::State state = SNTPHelper::State::Max;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(*state_text, state)) {
        BROOKESIA_LOGW("Invalid SNTP state before Super startup sync: %1%", *state_text);
        return std::nullopt;
    }
    return state;
}

} // namespace

std::expected<void, std::string> System::prepare_shell_fonts()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    if (shell_fonts_prepared_) {
        return {};
    }

    const auto share_dir = make_share_resource_dir(*this);
    const auto font_index = std::filesystem::path(share_dir) / make_font_index_relative_path();
    auto font_index_info = service::helper::Storage::fs_stat(font_index.generic_string(), STORAGE_FS_TIMEOUT_MS);
    if (!font_index_info || !font_index_info->exists ||
            font_index_info->type != service::helper::Storage::FileType::File) {
#if BROOKESIA_SYSTEM_SUPER_ENABLE_RESOURCE_FONT_COPY
        return std::unexpected("Shell font index does not exist: " + font_index.generic_string());
#else
        BROOKESIA_LOGI(
            "Super shell fonts skipped because built-in font resource staging is disabled: %1%",
            font_index.generic_string()
        );
        return {};
#endif
    }
    auto &gui_access = system_gui();
    auto font_result = gui_access.register_font_file(share_dir, make_font_index_relative_path());
    if (!font_result) {
        return std::unexpected("Failed to register Shell font index: " + font_result.error());
    }
    const auto stored_language = get_stored_gui_language_preference();
    const std::string requested_language = (stored_language.has_value() && !stored_language->empty()) ?
                                           *stored_language :
                                           gui_access.get_language();
    auto language_result = apply_font_language_fallback(
                               gui_access,
                               requested_language,
                               SUPER_DEFAULT_FONT_ID,
                               false
                           );
    if (!language_result) {
        return std::unexpected("Failed to prepare Shell font language fallback: " + language_result.error());
    }
    BROOKESIA_LOGI(
        "Super shell fonts preloaded: %1%, language(%2%), default_font(%3%)",
        font_index.generic_string(),
        language_result->language,
        language_result->font_id
    );
    shell_fonts_prepared_ = true;
    return {};
}

std::expected<void, std::string> System::prepare_shell_themes()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    if (shell_themes_prepared_) {
        return {};
    }

    const auto resource_dir = make_system_resource_dir(*this);
    for (const auto &relative_path : {
                make_theme_relative_path("light.json"),
                make_theme_relative_path("dark.json"),
            }) {
        auto result = system_gui().load_theme_file(resource_dir, relative_path);
        if (!result) {
            return std::unexpected("Failed to preload Shell theme '" + relative_path + "': " + result.error());
        }
    }

    std::string theme_id = make_preload_theme_id(get_stored_gui_theme_preference());
    auto result = system_gui().set_theme(theme_id, false);
    if (!result && theme_id != SUPER_DEFAULT_THEME_ID) {
        BROOKESIA_LOGW(
            "Failed to preload stored system GUI theme '%1%': %2%; falling back to %3%",
            theme_id,
            result.error(),
            SUPER_DEFAULT_THEME_ID
        );
        theme_id = SUPER_DEFAULT_THEME_ID;
        result = system_gui().set_theme(theme_id, false);
    }
    if (!result) {
        return std::unexpected("Failed to preload Shell theme '" + theme_id + "': " + result.error());
    }

    BROOKESIA_LOGI("Super shell theme preloaded: %1%", theme_id);
    shell_themes_prepared_ = true;
    return {};
}

void System::start_sntp_if_needed()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!SNTPHelper::is_available()) {
        BROOKESIA_LOGW("SNTP service is not available, skip Super startup time sync");
        return;
    }

    if (!sntp_service_binding_.is_valid()) {
        sntp_service_binding_ = service::ServiceManager::get_instance().bind(SNTPHelper::get_name().data());
        if (!sntp_service_binding_.is_valid()) {
            BROOKESIA_LOGW("Failed to bind SNTP service for Super startup time sync");
            return;
        }
    }

    auto state = get_sntp_state();
    if (state && *state != SNTPHelper::State::Stopped) {
        BROOKESIA_LOGI("SNTP already active for Super startup: state(%1%)", BROOKESIA_DESCRIBE_TO_STR(*state));
        return;
    }

    auto start_result = SNTPHelper::call_function_sync<void>(SNTPHelper::FunctionId::Start);
    if (!start_result) {
        BROOKESIA_LOGW("Failed to start SNTP for Super startup time sync: %1%", start_result.error());
        return;
    }
    BROOKESIA_LOGI("SNTP started by Super startup");
}

std::expected<void, std::string> System::on_prepare_startup_overlay()
{
    return {};
}

std::expected<void, std::string> System::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    set_system_type(BROOKESIA_SYSTEM_SUPER_SYSTEM_TYPE);

    auto font_result = prepare_shell_fonts();
    if (!font_result) {
        return font_result;
    }

    auto theme_result = prepare_shell_themes();
    if (!theme_result) {
        return theme_result;
    }

    if (shell_app_id_ == core::INVALID_APP_ID) {
        shell_app_ = std::make_shared<ShellApp>(*this);
        auto result = install_app(shell_app_);
        if (!result) {
            shell_app_.reset();
            return std::unexpected(result.error());
        }
        shell_app_id_ = *result;
    }
    utils_service_binding_ = service::ServiceManager::get_instance().bind(UtilsHelper::get_name().data());
    if (!utils_service_binding_.is_valid()) {
        return std::unexpected("Failed to bind Utils service for debug events");
    }

    const std::weak_ptr<ShellApp> weak_shell_app = shell_app_;
    utils_debug_state_connection_ = UtilsHelper::subscribe_event(
                                        UtilsHelper::EventId::DebugStateChanged,
    [weak_shell_app](const std::string &, const service::EventItemMap & items) {
        auto shell_app = weak_shell_app.lock();
        auto state = parse_utils_event_object<UtilsService::DebugRuntimeState>(
                         items, BROOKESIA_DESCRIBE_TO_STR(UtilsService::EventDebugStateChangedItem::State)
                     );
        if (shell_app && state) {
            shell_app->apply_debug_state(*state);
        }
    }
                                    );
    utils_memory_snapshot_connection_ = UtilsHelper::subscribe_event(
                                            UtilsHelper::EventId::MemoryDebugSnapshotUpdated,
    [weak_shell_app](const std::string &, const service::EventItemMap & items) {
        auto shell_app = weak_shell_app.lock();
        auto snapshot = parse_utils_event_object<UtilsService::MemoryDebugSnapshot>(
                            items,
                            BROOKESIA_DESCRIBE_TO_STR(UtilsService::EventMemoryDebugSnapshotUpdatedItem::Snapshot)
                        );
        if (shell_app && snapshot) {
            if (auto latest = UtilsHelper::get_debug_snapshot(); latest) {
                shell_app->apply_debug_config(latest->config);
                shell_app->apply_debug_state(latest->state);
            }
            shell_app->apply_memory_debug_snapshot(*snapshot);
        }
    }
                                        );
    utils_thread_snapshot_connection_ = UtilsHelper::subscribe_event(
                                            UtilsHelper::EventId::ThreadDebugSnapshotUpdated,
    [weak_shell_app](const std::string &, const service::EventItemMap & items) {
        auto shell_app = weak_shell_app.lock();
        auto snapshot = parse_utils_event_object<UtilsService::ThreadDebugSnapshot>(
                            items,
                            BROOKESIA_DESCRIBE_TO_STR(UtilsService::EventThreadDebugSnapshotUpdatedItem::Snapshot)
                        );
        if (shell_app && snapshot) {
            if (auto latest = UtilsHelper::get_debug_snapshot(); latest) {
                shell_app->apply_debug_config(latest->config);
                shell_app->apply_debug_state(latest->state);
            }
            shell_app->apply_thread_debug_snapshot(*snapshot);
        }
    }
                                        );
    synchronize_shell_debug_snapshot(weak_shell_app);

    BROOKESIA_LOGI("Super system initialized");
    return {};
}

std::expected<void, std::string> System::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    stopping_ = false;
    pending_shell_page_ = ShellPage::AppLauncher;

    if (shell_app_id_ != core::INVALID_APP_ID) {
        auto result = restore_shell();
        if (!result) {
            return result;
        }
    }
    start_sntp_if_needed();
    BROOKESIA_LOGI("Super shell started");
    return {};
}

void System::on_stop()
{
    stopping_ = true;
    sntp_service_binding_.release();
}

void System::on_deinit()
{
    sntp_service_binding_.release();
    utils_debug_state_connection_.disconnect();
    utils_memory_snapshot_connection_.disconnect();
    utils_thread_snapshot_connection_.disconnect();
    utils_service_binding_.release();
    shell_app_.reset();
    shell_app_id_ = core::INVALID_APP_ID;
    pending_shell_page_ = ShellPage::AppLauncher;
    stopping_ = false;
    shell_fonts_prepared_ = false;
    shell_themes_prepared_ = false;
}

std::optional<std::string> System::get_stored_gui_theme_preference() const
{
    return get_stored_gui_theme_id();
}

std::optional<std::string> System::get_stored_gui_language_preference() const
{
    return get_stored_gui_language();
}

void System::begin_shell_gui_preferences_restore()
{
    begin_gui_preferences_restore();
}

void System::mark_shell_gui_preferences_restored()
{
    mark_gui_preferences_restored();
}

std::expected<void, std::string> System::on_app_installed(const core::AppInfo &app)
{
    (void)app;
    BROOKESIA_LOGD("Super launcher app available: id(%1%), name(%2%)", app.app_id, app.manifest.name);
    if (!shell_app_) {
        return {};
    }
    return shell_app_->refresh_launcher();
}

std::expected<void, std::string> System::on_app_uninstalled(const core::AppInfo &app)
{
    (void)app;
    BROOKESIA_LOGD("Super launcher app removed: id(%1%), name(%2%)", app.app_id, app.manifest.name);
    if (!shell_app_) {
        return {};
    }
    return shell_app_->refresh_launcher();
}

std::expected<void, std::string> System::on_app_started(const core::AppInfo &app)
{
    BROOKESIA_LOGI("Super foreground app: id(%1%), name(%2%)", app.app_id, app.manifest.name);
    if (app.app_id == shell_app_id_) {
        return {};
    }
    if (shell_app_) {
        auto result = shell_app_->set_foreground_app(app);
        if (!result) {
            BROOKESIA_LOGW("Failed to update Shell foreground state: %1%", result.error());
        }
    }
    return {};
}

void System::on_app_start_failed(const core::AppInfo &app, std::string_view reason)
{
    BROOKESIA_LOGW(
        "Super app start failed: id(%1%), name(%2%), reason(%3%)",
        app.app_id,
        app.manifest.name,
        reason
    );
    if (stopping_ || app.app_id == shell_app_id_) {
        return;
    }
    auto result = restore_shell();
    if (!result) {
        BROOKESIA_LOGW("Failed to restore Super shell after app start failure: %1%", result.error());
    }
}

void System::on_app_stopped(const core::AppInfo &app)
{
    BROOKESIA_LOGI("Super app stopped: id(%1%), name(%2%)", app.app_id, app.manifest.name);
    if (stopping_ || app.app_id == shell_app_id_) {
        return;
    }
    auto result = restore_shell();
    if (!result) {
        BROOKESIA_LOGW("Failed to restore Super shell: %1%", result.error());
    }
}

void System::on_app_stop_failed(const core::AppInfo &app, std::string_view reason)
{
    BROOKESIA_LOGW(
        "Super app stop failed: id(%1%), name(%2%), reason(%3%)",
        app.app_id,
        app.manifest.name,
        reason
    );
    if (stopping_ || app.app_id == shell_app_id_) {
        return;
    }
    auto result = restore_shell();
    if (!result) {
        BROOKESIA_LOGW("Failed to restore Super shell after app stop failure: %1%", result.error());
    }
}

std::expected<void, std::string> System::on_language_changed(std::string_view language)
{
    auto &gui_access = system_gui();
    if (gui_access.list_supported_languages().empty()) {
        BROOKESIA_LOGW(
            "Skip Shell font language fallback after language change: no registered font languages"
        );
    } else {
        auto fallback = apply_font_language_fallback(
                            gui_access,
                            language,
                            SUPER_DEFAULT_FONT_ID,
                            false
                        );
        if (!fallback) {
            BROOKESIA_LOGW("Failed to apply Shell font language fallback after language change: %1%", fallback.error());
        }
    }
    if (!shell_app_) {
        return {};
    }
    return shell_app_->refresh_environment();
}

std::expected<void, std::string> System::on_theme_changed(std::string_view)
{
    if (!shell_app_) {
        return {};
    }
    return shell_app_->refresh_environment();
}

std::expected<void, std::string> System::on_show_app_loading(core::AppId app_id)
{
    if (!shell_app_) {
        return std::unexpected("Shell app is not available");
    }
    return shell_app_->show_loading(app_id);
}

void System::on_hide_app_loading(core::AppId app_id)
{
    if (shell_app_) {
        shell_app_->hide_loading(app_id);
    }
}

std::expected<void, std::string> System::on_show_app_keyboard(
    core::AppId app_id,
    core::KeyboardRequestId request_id,
    const core::KeyboardRequestOptions &options
)
{
    if (!shell_app_) {
        return std::unexpected("Shell app is not available");
    }
    return shell_app_->show_keyboard(app_id, request_id, options);
}

void System::on_hide_app_keyboard(core::AppId app_id, core::KeyboardRequestId request_id)
{
    if (shell_app_) {
        shell_app_->hide_keyboard(app_id, request_id);
    }
}

std::expected<void, std::string> System::on_show_message_dialog(
    core::AppId app_id,
    core::MessageDialogRequestId request_id,
    const core::MessageDialogOptions &options
)
{
    if (!shell_app_) {
        return std::unexpected("Shell app is not available");
    }
    return shell_app_->show_message_dialog(app_id, request_id, options);
}

std::expected<void, std::string> System::on_update_message_dialog(
    core::AppId app_id,
    core::MessageDialogRequestId request_id,
    const core::MessageDialogOptions &options
)
{
    if (!shell_app_) {
        return std::unexpected("Shell app is not available");
    }
    return shell_app_->update_message_dialog(app_id, request_id, options);
}

void System::on_hide_message_dialog(core::AppId app_id, core::MessageDialogRequestId request_id)
{
    if (shell_app_) {
        shell_app_->hide_message_dialog(app_id, request_id);
    }
}

void System::on_message_dialog_idle()
{
    if (shell_app_) {
        shell_app_->handle_message_dialog_idle();
    }
}

} // namespace esp_brookesia::system::super
