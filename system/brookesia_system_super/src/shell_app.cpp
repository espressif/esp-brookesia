/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_super/macro_configs.h"
#if !BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/font_language.hpp"
#include "private/utils.hpp"
#include "private/shell_app.hpp"
#include "private/system_constants.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "boost/json.hpp"

namespace esp_brookesia::system::super {
namespace {

inline constexpr int32_t LAUNCHER_ITEM_WIDTH = 112;
inline constexpr int32_t LAUNCHER_ITEM_HEIGHT = 112;
inline constexpr int32_t LAUNCHER_ITEM_GAP = 18;

std::optional<std::string> get_launcher_instance_id(const gui::Event &event)
{
    if (!event.path.starts_with(SUPER_LAUNCHER_PATH_PREFIX)) {
        return std::nullopt;
    }
    auto instance_id = event.path.substr(std::string_view(SUPER_LAUNCHER_PATH_PREFIX).size());
    auto child_separator = instance_id.find('/');
    if (child_separator != std::string::npos) {
        instance_id.resize(child_separator);
    }
    if (instance_id.empty()) {
        return std::nullopt;
    }
    return std::string(SUPER_LAUNCHER_INSTANCE_PREFIX) + instance_id;
}

std::string make_launcher_slot_path(size_t index)
{
    return std::string(SUPER_LAUNCHER_SLOT_PATH_PREFIX) + std::to_string(index);
}

std::string make_launcher_item_instance_id(core::AppId app_id)
{
    return std::string(SUPER_LAUNCHER_INSTANCE_PREFIX) + std::to_string(app_id);
}

std::string make_launcher_item_path(core::AppId app_id)
{
    return std::string(SUPER_LAUNCHER_PATH_PREFIX) + std::to_string(app_id);
}

std::string make_launcher_icon_text(std::string_view name)
{
    for (char ch : name) {
        const auto unsigned_ch = static_cast<unsigned char>(ch);
        if (std::isalnum(unsigned_ch) != 0) {
            return std::string(1, static_cast<char>(std::toupper(unsigned_ch)));
        }
    }
    if (!name.empty()) {
        const auto first = static_cast<unsigned char>(name.front());
        size_t char_length = 1;
        if ((first & 0xE0U) == 0xC0U) {
            char_length = 2;
        } else if ((first & 0xF0U) == 0xE0U) {
            char_length = 3;
        } else if ((first & 0xF8U) == 0xF0U) {
            char_length = 4;
        }
        return std::string(name.substr(0, std::min(char_length, name.size())));
    }
    return "?";
}

std::string normalize_theme_id(std::string_view theme_id)
{
    if (theme_id == "shell.light") {
        return SUPER_LIGHT_THEME_ID;
    }
    if (theme_id == "shell.dark") {
        return SUPER_DARK_THEME_ID;
    }
    return std::string(theme_id);
}

std::string get_current_language(core::AppContext *context)
{
    if (context == nullptr) {
        return {};
    }

    const auto supported_languages = context->gui().list_supported_languages();
    const auto language = context->gui().get_language();
    if (!language.empty() && contains_language(supported_languages, language)) {
        return language;
    }
    return select_default_font_language(context->gui());
}

std::string make_system_resource_dir(const System &system)
{
    const auto layout = system.get_storage_layout();
    return (std::filesystem::path(layout.internal.root_path) / "system" / "super").lexically_normal().generic_string();
}

std::string make_share_resource_dir(const System &system)
{
    const auto layout = system.get_storage_layout();
    return (std::filesystem::path(layout.internal.root_path) / "system").lexically_normal().generic_string();
}

std::string make_app_display_name(const core::AppInfo &app, core::AppContext *context)
{
    return core::resolve_app_display_name(app.manifest, get_current_language(context));
}

bool is_zh_cn_language(core::AppContext *context)
{
    return get_current_language(context) == "zh_CN";
}

std::string make_localized_page_title(ShellPage page, core::AppContext *context)
{
    if (!is_zh_cn_language(context)) {
        return to_page_title(page);
    }

    switch (page) {
    case ShellPage::Home:
        return "应用启动器";
    case ShellPage::AppLauncher:
        return "应用启动器";
    case ShellPage::Notifications:
        return "应用启动器";
    }
    return "应用启动器";
}

std::optional<gui::ViewFrame> get_absolute_view_frame(core::AppContext &context, std::string_view absolute_path)
{
    if (absolute_path.empty() || absolute_path.front() != '/') {
        return std::nullopt;
    }

    gui::ViewFrame result;
    std::string current_path;
    size_t start = 1;
    while (start <= absolute_path.size()) {
        const auto slash = absolute_path.find('/', start);
        const auto part_length = slash == std::string_view::npos ? std::string_view::npos : slash - start;
        const auto part = absolute_path.substr(start, part_length);
        if (part.empty()) {
            break;
        }

        current_path += "/";
        current_path += part;
        auto frame = context.gui().get_view_frame(current_path);
        if (!frame.has_value()) {
            return std::nullopt;
        }
        result.x += frame->x;
        result.y += frame->y;
        result.width = frame->width;
        result.height = frame->height;

        if (slash == std::string_view::npos) {
            break;
        }
        start = slash + 1;
    }

    return result.width > 0 && result.height > 0 ? std::optional<gui::ViewFrame>(result) : std::nullopt;
}

std::expected<std::vector<std::string>, std::string> list_theme_files(std::string_view resource_dir)
{
    const auto theme_dir = std::filesystem::path(resource_dir) / SUPER_THEME_DIR;
    std::error_code error_code;
    if (!std::filesystem::exists(theme_dir, error_code) || !std::filesystem::is_directory(theme_dir, error_code)) {
        return std::unexpected("Shell theme directory does not exist: " + theme_dir.string());
    }

    std::vector<std::string> files;
    std::filesystem::directory_iterator iterator(theme_dir, error_code);
    if (error_code) {
        return std::unexpected("Failed to open Shell theme directory: " + theme_dir.string());
    }
    for (const std::filesystem::directory_iterator end; iterator != end; iterator.increment(error_code)) {
        if (error_code) {
            return std::unexpected("Failed to scan Shell theme directory: " + theme_dir.string());
        }
        if (!iterator->is_regular_file(error_code) || error_code) {
            error_code.clear();
            continue;
        }
        const auto file_name = iterator->path().filename().string();
        if (iterator->path().extension() == ".json" && file_name != "index.json") {
            files.push_back((std::filesystem::path(SUPER_THEME_DIR) / file_name).generic_string());
        }
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        return std::unexpected("Shell theme directory contains no JSON themes: " + theme_dir.string());
    }
    return files;
}

std::vector<std::string> get_builtin_theme_files()
{
    return {
        (std::filesystem::path(SUPER_THEME_DIR) / "light.json").generic_string(),
        (std::filesystem::path(SUPER_THEME_DIR) / "dark.json").generic_string(),
    };
}

std::expected<std::vector<std::string>, std::string> list_font_files(std::string_view resource_dir)
{
    const auto font_dir = std::filesystem::path(resource_dir) / SUPER_FONT_DIR;
    std::error_code error_code;
    if (!std::filesystem::exists(font_dir, error_code) || !std::filesystem::is_directory(font_dir, error_code)) {
        return std::unexpected("Shell font directory does not exist: " + font_dir.string());
    }

    std::vector<std::string> files;
    std::filesystem::directory_iterator iterator(font_dir, error_code);
    if (error_code) {
        return std::unexpected("Failed to open Shell font directory: " + font_dir.string());
    }
    for (const std::filesystem::directory_iterator end; iterator != end; iterator.increment(error_code)) {
        if (error_code) {
            return std::unexpected("Failed to scan Shell font directory: " + font_dir.string());
        }
        if (!iterator->is_regular_file(error_code) || error_code) {
            error_code.clear();
            continue;
        }
        if (iterator->path().extension() == ".json") {
            files.push_back((std::filesystem::path(SUPER_FONT_DIR) / iterator->path().filename()).generic_string());
        }
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        return std::unexpected("Shell font directory contains no JSON fonts: " + font_dir.string());
    }
    return files;
}

std::expected<std::string, std::string> read_text_file(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        return std::unexpected("Failed to open file: " + path.string());
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

std::string json_value_to_string(const boost::json::value &value)
{
    if (value.is_string()) {
        return std::string(value.as_string());
    }
    return boost::json::serialize(value);
}

std::expected<std::vector<gui::BindingValueUpdate>, std::string> load_shell_i18n_updates(
    std::string_view resource_dir, std::string_view locale
)
{
    const auto locale_path = std::filesystem::path(std::string(resource_dir)) / "shell" / "i18n" /
                             (std::string(locale) + ".json");
    auto file_content = read_text_file(locale_path);
    if (!file_content) {
        return std::unexpected(file_content.error());
    }

    boost::system::error_code error_code;
    auto parsed = boost::json::parse(*file_content, error_code);
    if (error_code) {
        return std::unexpected("Failed to parse Shell i18n file '" + locale_path.string() + "': " + error_code.message());
    }
    if (!parsed.is_object()) {
        return std::unexpected("Shell i18n file must be a JSON object: " + locale_path.string());
    }
    const auto &root = parsed.as_object();
    const auto *updates_value = root.if_contains("updates");
    if (updates_value == nullptr || !updates_value->is_array()) {
        return std::unexpected("Shell i18n file must contain an array field 'updates': " + locale_path.string());
    }

    std::vector<gui::BindingValueUpdate> updates;
    for (const auto &entry_value : updates_value->as_array()) {
        if (!entry_value.is_object()) {
            return std::unexpected("Shell i18n update entries must be objects: " + locale_path.string());
        }
        const auto &entry = entry_value.as_object();
        const auto *path_value = entry.if_contains("path");
        const auto *key_value = entry.if_contains("key");
        const auto *value = entry.if_contains("value");
        if (path_value == nullptr || key_value == nullptr || value == nullptr ||
                !path_value->is_string() || !key_value->is_string()) {
            return std::unexpected("Shell i18n update entries require string fields 'path' and 'key'");
        }
        updates.push_back(gui::BindingValueUpdate{
            .absolute_path = std::string(path_value->as_string()),
            .key = std::string(key_value->as_string()),
            .value = json_value_to_string(*value),
        });
    }
    return updates;
}

} // namespace

ShellApp::ShellApp(System &owner)
    : owner_(owner)
{}

core::AppManifest ShellApp::get_manifest() const
{
    return {
        .id = SUPER_SHELL_APP_ID,
        .name = "Shell",
        .localized_names = {
            {"en", "Shell"},
            {"zh_CN", "桌面"},
        },
        .version = "0.1.0",
        .kind = core::AppKind::Native,
        .visible = false,
        .icon_id = "super",
        .supported_systems = {},
        .icon_path = "",
        .runtime_type = runtime::BackendType::Unknown,
        .app_path = "",
        .entry = "",
        .resource_dir = make_system_resource_dir(owner_),
        .arguments = {},
    };
}

core::AppGuiDescriptor ShellApp::get_gui_descriptor() const
{
    return {
        .root_kind = core::GuiRootKind::File,
        .root = SUPER_SHELL_ROOT_JSON,
        .resources = {},
        .screen_flows = {
            core::GuiScreenFlowEntry{
                .screen_flow = SUPER_BACKGROUND_FLOW_ID,
                .layer = core::GuiAppLayer::SystemBottom,
            },
            core::GuiScreenFlowEntry{
                .screen_flow = SUPER_SHELL_PAGES_FLOW_ID,
                .layer = core::GuiAppLayer::AppDefault,
            },
            core::GuiScreenFlowEntry{
                .screen_flow = SUPER_OVERLAY_FLOW_ID,
                .layer = core::GuiAppLayer::AppTop,
                .mount_mode = gui::MountStackMode::Stack,
                .z_order = BROOKESIA_SYSTEM_CORE_RUNTIME_APP_MAX_Z_ORDER + 1,
            },
            core::GuiScreenFlowEntry{
                .screen_flow = SUPER_KEYBOARD_FLOW_ID,
                .layer = core::GuiAppLayer::AppTop,
                .mount_mode = gui::MountStackMode::Stack,
                .z_order = BROOKESIA_SYSTEM_CORE_RUNTIME_APP_MAX_Z_ORDER + 2,
            },
            core::GuiScreenFlowEntry{
                .screen_flow = SUPER_MESSAGE_DIALOG_FLOW_ID,
                .layer = core::GuiAppLayer::AppTop,
                .mount_mode = gui::MountStackMode::Stack,
                .z_order = BROOKESIA_SYSTEM_CORE_RUNTIME_APP_MAX_Z_ORDER + 3,
            },
        },
    };
}

std::expected<void, std::string> ShellApp::on_start(core::AppContext &context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    context_ = &context;
    (void)ensure_display_service_binding();

    auto result = load_fonts(context);
    if (!result) {
        context_ = nullptr;
        return result;
    }

    owner_.begin_shell_gui_preferences_restore();
    {
        lib_utils::FunctionGuard preferences_restore_guard([this]() {
            owner_.mark_shell_gui_preferences_restored();
        });

        result = apply_language_preference(context);
        if (!result) {
            context_ = nullptr;
            return result;
        }

        result = load_themes(context);
        if (!result) {
            context_ = nullptr;
            return result;
        }
    }

    result = mount_overlay(context);
    if (!result) {
        context_ = nullptr;
        return result;
    }

    result = populate_launcher(context);
    if (!result) {
        unmount_overlay();
        context_ = nullptr;
        return result;
    }
    result = set_foreground_app(std::nullopt);
    if (!result) {
        unmount_overlay();
        context_ = nullptr;
        return result;
    }
    return {};
}

std::expected<void, std::string> ShellApp::on_stop(core::AppContext &context)
{
    (void)context;
    disconnect_launcher_actions();
    launcher_instance_to_app_.clear();
    launcher_populated_ = false;
    cancel_pending_launch();
    unmount_message_dialog();
    unmount_keyboard();
    unmount_overlay();
    applied_i18n_locale_.clear();
    release_display_service_binding();
    context_ = nullptr;
    return {};
}

std::expected<void, std::string> ShellApp::on_timer(
    core::AppContext &context,
    core::TimerId timer_id,
    std::string_view name
)
{
    (void)context;
    if (name == SUPER_MESSAGE_DIALOG_TIMEOUT_TIMER_NAME && timer_id == message_dialog_auto_close_timer_id_) {
        message_dialog_auto_close_timer_id_ = core::INVALID_TIMER_ID;
        finish_message_dialog(-1, core::MessageDialogCloseReason::Timeout);
        return {};
    }
    if (name == SUPER_GESTURE_EXIT_HOLD_TIMER_NAME && timer_id == gesture_exit_hold_timer_id_) {
        gesture_exit_hold_timer_id_ = core::INVALID_TIMER_ID;
        trigger_gesture_exit();
        return {};
    }
    if (name == SUPER_STATUS_PEEK_AUTO_HIDE_TIMER_NAME && timer_id == status_peek_auto_hide_timer_id_) {
        status_peek_auto_hide_timer_id_ = core::INVALID_TIMER_ID;
        if (status_peek_visible_) {
            reset_status_peek_session(true, false, "auto hide timer");
        }
        return {};
    }
    if (name == SUPER_STATUS_CLOCK_TIMER_NAME && timer_id == status_clock_timer_id_) {
        status_clock_timer_id_ = core::INVALID_TIMER_ID;
        refresh_status_clock();
        schedule_status_clock_timer();
        return {};
    }
    if (name != SUPER_APP_LAUNCH_HOLD_TIMER_NAME || timer_id != launch_hold_timer_id_) {
        return {};
    }

    const auto app_id = pending_launch_app_id_;
    pending_launch_app_id_.reset();
    launch_hold_timer_id_ = core::INVALID_TIMER_ID;
    if (!app_id.has_value()) {
        finish_launch_overlay();
        return {};
    }

    start_app_after_launch(*app_id);
    return {};
}

std::expected<void, std::string> ShellApp::show_page(ShellPage page)
{
    if (context_ == nullptr) {
        return std::unexpected("Shell app is not running");
    }

    auto result = context_->gui().trigger_screen_flow(SUPER_SHELL_PAGES_FLOW_ID, to_screen_flow_action(page));
    if (!result) {
        return result;
    }
    return {};
}

std::string ShellApp::get_current_page_title() const
{
    if (context_ == nullptr) {
        return make_localized_page_title(ShellPage::AppLauncher, context_);
    }

    auto state = context_->gui().get_screen_flow_state(SUPER_SHELL_PAGES_FLOW_ID);
    if (!state.has_value()) {
        return make_localized_page_title(ShellPage::AppLauncher, context_);
    }

    auto page = to_shell_page(*state);
    if (!page.has_value()) {
        BROOKESIA_LOGW("Unknown Shell screen flow state: %1%", *state);
        return make_localized_page_title(ShellPage::AppLauncher, context_);
    }
    return make_localized_page_title(*page, context_);
}

std::string ShellApp::get_app_display_name(const core::AppInfo &app) const
{
    return make_app_display_name(app, context_);
}

std::string ShellApp::get_app_icon_text(std::string_view name) const
{
    return make_launcher_icon_text(name);
}

std::expected<void, std::string> ShellApp::apply_i18n_updates()
{
    if (context_ == nullptr) {
        return {};
    }
    const auto locale = get_current_language(context_);
    if (locale == applied_i18n_locale_) {
        return {};
    }

    const auto resource_dir = make_system_resource_dir(owner_);
    auto updates = load_shell_i18n_updates(resource_dir, locale);
    if (!updates) {
        const auto fallback_locale = select_default_font_language(context_->gui());
        if (!fallback_locale.empty() && locale != fallback_locale) {
            BROOKESIA_LOGW(
                "Failed to load Shell i18n for '%1%', falling back to '%2%': %3%",
                locale,
                fallback_locale,
                updates.error()
            );
            updates = load_shell_i18n_updates(resource_dir, fallback_locale);
        }
        if (!updates) {
            return std::unexpected(updates.error());
        }
    }

    auto result = context_->gui().set_binding_values(*updates);
    if (!result) {
        return result;
    }
    applied_i18n_locale_ = locale;
    return {};
}

std::expected<void, std::string> ShellApp::load_fonts(core::AppContext &context)
{
    if (owner_.shell_fonts_prepared_) {
        BROOKESIA_LOGI("Shell fonts already prepared by Super system; skip duplicate registration");
        return {};
    }

    const auto share_dir = make_share_resource_dir(owner_);
    auto font_files = list_font_files(share_dir);
    if (!font_files) {
        BROOKESIA_LOGI("Shell fonts skipped: %1%", font_files.error());
        return {};
    }

    for (const auto &relative_path : *font_files) {
        auto result = context.gui().register_font_file(share_dir, relative_path);
        if (!result) {
            return std::unexpected("Failed to register Shell font '" + relative_path + "': " + result.error());
        }
    }

    const auto stored_language = owner_.get_stored_gui_language_preference();
    const std::string requested_language = (stored_language.has_value() && !stored_language->empty()) ?
                                           *stored_language :
                                           context.gui().get_language();
    auto fallback = apply_font_language_fallback(
                        context.gui(),
                        requested_language,
                        SUPER_DEFAULT_FONT_ID,
                        false
                    );
    if (!fallback) {
        return std::unexpected("Failed to apply Shell font language fallback: " + fallback.error());
    }
    BROOKESIA_LOGI(
        "Shell fonts registered: languages(%1%), language(%2%), default_font(%3%)",
        context.gui().list_supported_languages(),
        fallback->language,
        fallback->font_id
    );
    return {};
}

std::expected<void, std::string> ShellApp::apply_language_preference(core::AppContext &context)
{
    auto stored_language = owner_.get_stored_gui_language_preference();
    const auto runtime_language_before = context.gui().get_language();
    const bool use_stored_language = stored_language.has_value() && !stored_language->empty();
    const std::string requested_language = use_stored_language ? *stored_language : runtime_language_before;
    auto fallback = apply_font_language_fallback(
                        context.gui(),
                        requested_language,
                        SUPER_DEFAULT_FONT_ID,
                        true
                    );
    if (!fallback) {
        return std::unexpected("Failed to apply Shell language font fallback: " + fallback.error());
    }

    if (use_stored_language && fallback->language == requested_language) {
        BROOKESIA_LOGI("System GUI language '%1%' is restored", fallback->language);
    } else if (use_stored_language) {
        BROOKESIA_LOGW(
            "Stored system GUI language '%1%' is not available; falling back to %2%",
            requested_language,
            fallback->language
        );
    } else if (fallback->language == runtime_language_before) {
        BROOKESIA_LOGI("System GUI language '%1%' is already active; skip duplicate reapply", fallback->language);
    } else {
        BROOKESIA_LOGD("Default system GUI language '%1%' is applied", fallback->language);
    }
    return {};
}

std::expected<void, std::string> ShellApp::load_themes(core::AppContext &context)
{
    if (owner_.shell_themes_prepared_) {
        auto stored_theme_id = owner_.get_stored_gui_theme_preference();
        current_theme_id_ = (stored_theme_id.has_value() && !stored_theme_id->empty()) ?
                            normalize_theme_id(*stored_theme_id) :
                            SUPER_DEFAULT_THEME_ID;
        BROOKESIA_LOGI("Shell themes already prepared by Super system; skip duplicate loading");
        return {};
    }

    const auto resource_dir = make_system_resource_dir(owner_);
    auto theme_files = list_theme_files(resource_dir);
    if (!theme_files) {
        theme_files = get_builtin_theme_files();
    }

    for (const auto &relative_path : *theme_files) {
        auto load_result = context.gui().load_theme_file(resource_dir, relative_path);
        if (!load_result) {
            return std::unexpected("Failed to load Shell theme '" + relative_path + "': " + load_result.error());
        }
    }

    auto stored_theme_id = owner_.get_stored_gui_theme_preference();
    const bool has_stored_theme = stored_theme_id.has_value() && !stored_theme_id->empty();
    bool use_stored_theme = has_stored_theme;
    current_theme_id_ = use_stored_theme ? normalize_theme_id(*stored_theme_id) : SUPER_DEFAULT_THEME_ID;
    auto set_result = context.gui().set_theme(current_theme_id_, true);
    if (!set_result && use_stored_theme) {
        BROOKESIA_LOGW(
            "Failed to restore system GUI theme '%1%': %2%; falling back to %3%",
            current_theme_id_,
            set_result.error(),
            SUPER_DEFAULT_THEME_ID
        );
        use_stored_theme = false;
        current_theme_id_ = SUPER_DEFAULT_THEME_ID;
        set_result = context.gui().set_theme(current_theme_id_, true);
    }
    if (!set_result) {
        return std::unexpected(
                   "Failed to set default Shell theme '" + current_theme_id_ + "': " +
                   set_result.error()
               );
    }
    if (use_stored_theme) {
        BROOKESIA_LOGI("System GUI theme '%1%' is restored", current_theme_id_);
    } else {
        BROOKESIA_LOGD("Default system GUI theme '%1%' is applied", current_theme_id_);
    }
    return {};
}

std::expected<void, std::string> ShellApp::populate_launcher(core::AppContext &context)
{
    disconnect_launcher_actions();
    for (const auto &[instance_id, app_id] : launcher_instance_to_app_) {
        (void)instance_id;
        const auto instance_path = make_launcher_item_path(app_id);
        if (!context.gui().destroy_view(instance_path)) {
            BROOKESIA_LOGW("Failed to destroy launcher item before refresh: path(%1%)", instance_path);
        }
    }
    for (size_t i = 0; i < launcher_slot_count_; ++i) {
        const auto slot_path = make_launcher_slot_path(i);
        if (!context.gui().destroy_view(slot_path)) {
            BROOKESIA_LOGW("Failed to destroy launcher slot before refresh: path(%1%)", slot_path);
        }
    }
    launcher_slot_count_ = 0;
    launcher_order_.clear();
    launcher_instance_to_app_.clear();
    launcher_populated_ = false;

    std::vector<core::AppInfo> visible_apps;
    for (const auto &app : owner_.list_apps()) {
        if (!app.manifest.visible) {
            continue;
        }
        visible_apps.push_back(app);
        launcher_order_.push_back(app.app_id);
    }

    const auto environment = owner_.get_environment();
    const auto density = std::max(environment.density, 0.001F);
    const auto fallback_width = static_cast<int32_t>(
                                    std::max(1.0F, static_cast<float>(environment.width_px) / density)
                                );
    const auto content_frame = context.gui().get_view_frame(SUPER_LAUNCHER_CONTENT_PATH);
    const auto available_width = content_frame.has_value() ?
                                 std::max<int32_t>(content_frame->width, LAUNCHER_ITEM_WIDTH) :
                                 std::max<int32_t>(fallback_width, LAUNCHER_ITEM_WIDTH);
    const auto launcher_columns = std::max<int32_t>(
                                      1,
                                      (available_width + LAUNCHER_ITEM_GAP) / (LAUNCHER_ITEM_WIDTH + LAUNCHER_ITEM_GAP)
                                  );
    const auto launcher_grid_width =
        launcher_columns * LAUNCHER_ITEM_WIDTH + (launcher_columns - 1) * LAUNCHER_ITEM_GAP;
    const auto launcher_grid_x = std::max<int32_t>(0, (available_width - launcher_grid_width) / 2);
    std::vector<gui::BindingValueUpdate> binding_updates;
    add_binding_update(binding_updates, SUPER_LAUNCHER_GRID_STAGE_PATH, "grid_x", std::to_string(launcher_grid_x));
    add_binding_update(
        binding_updates,
        SUPER_LAUNCHER_GRID_STAGE_PATH,
        "grid_width",
        std::to_string(launcher_grid_width)
    );

    launcher_slot_count_ = 0;

    int32_t content_height = content_frame.has_value() ? std::max<int32_t>(content_frame->height, 1) : 1;
    for (size_t i = 0; i < visible_apps.size(); ++i) {
        const auto &app = visible_apps[i];
        const auto column = static_cast<int32_t>(i % static_cast<size_t>(launcher_columns));
        const auto row = static_cast<int32_t>(i / static_cast<size_t>(launcher_columns));
        const auto item_x = column * (LAUNCHER_ITEM_WIDTH + LAUNCHER_ITEM_GAP);
        const auto item_y = row * (LAUNCHER_ITEM_HEIGHT + LAUNCHER_ITEM_GAP);
        content_height = std::max(content_height, item_y + LAUNCHER_ITEM_HEIGHT);

        const auto instance_id = make_launcher_item_instance_id(app.app_id);
        const auto instance_path = make_launcher_item_path(app.app_id);
        const auto label_path = instance_path + "/" + SUPER_LAUNCHER_LABEL_ID;
        const auto fallback_icon_path = instance_path + "/" + SUPER_LAUNCHER_FALLBACK_ICON_ID;
        const auto image_icon_path = instance_path + "/" + SUPER_LAUNCHER_IMAGE_ICON_ID;
        const auto has_image_icon = core::has_app_icon_image(app.manifest);
        const auto display_name = make_app_display_name(app, &context);

        auto create_result = context.gui().create_view(
                                 SUPER_LAUNCHER_BUTTON_TEMPLATE_ID,
                                 SUPER_LAUNCHER_ITEM_LAYER_PATH,
                                 instance_id
                             );
        if (!create_result) {
            return std::unexpected(create_result.error());
        }

        add_binding_update(binding_updates, instance_path, "x", std::to_string(item_x));
        add_binding_update(binding_updates, instance_path, "y", std::to_string(item_y));
        add_binding_update(binding_updates, label_path, "name", display_name);
        if (has_image_icon) {
            add_binding_update(
                binding_updates,
                image_icon_path,
                "src",
                core::resolve_app_icon_resource_id(app.manifest)
            );
            add_binding_update(binding_updates, image_icon_path, "hidden", "false");
            add_binding_update(binding_updates, fallback_icon_path, "hidden", "true");
            add_binding_update(binding_updates, fallback_icon_path, "text", "");
        } else {
            add_binding_update(binding_updates, image_icon_path, "hidden", "true");
            add_binding_update(binding_updates, fallback_icon_path, "hidden", "false");
            add_binding_update(binding_updates, fallback_icon_path, "text", make_launcher_icon_text(display_name));
        }

        launcher_instance_to_app_.emplace(instance_id, app.app_id);
    }

    static constexpr std::array launcher_actions = {
        SUPER_ACTION_LAUNCH_APP,
    };
    for (const auto *action : launcher_actions) {
        auto connection = context.gui().subscribe_action(
                              action,
        [this](const gui::Event & event) {
            handle_launcher_event(event);
        }
                          );
        if (!connection.connected()) {
            disconnect_launcher_actions();
            return std::unexpected(std::string("Failed to subscribe launcher action: ") + action);
        }
        launcher_action_connections_.push_back(std::move(connection));
    }

    add_binding_update(
        binding_updates,
        SUPER_LAUNCHER_SLOT_GRID_PATH,
        "content_height",
        std::to_string(content_height)
    );
    add_binding_update(
        binding_updates,
        SUPER_LAUNCHER_ITEM_LAYER_PATH,
        "content_height",
        std::to_string(content_height)
    );
    add_binding_update(
        binding_updates,
        SUPER_LAUNCHER_DRAG_LAYER_PATH,
        "content_height",
        std::to_string(content_height)
    );
    auto binding_result = context.gui().set_binding_values(binding_updates);
    if (!binding_result) {
        disconnect_launcher_actions();
        return binding_result;
    }

    launcher_populated_ = true;
    return {};
}

std::expected<void, std::string> ShellApp::refresh_launcher()
{
    if (context_ == nullptr) {
        return {};
    }
    return populate_launcher(*context_);
}

std::expected<void, std::string> ShellApp::refresh_environment()
{
    if (context_ == nullptr) {
        return {};
    }
    current_theme_id_ = normalize_theme_id(context_->gui().get_theme());
    auto i18n_result = apply_i18n_updates();
    if (!i18n_result) {
        return i18n_result;
    }
    if (!launcher_populated_) {
        return {};
    }

    std::vector<gui::BindingValueUpdate> updates;
    for (const auto &app : owner_.list_apps()) {
        if (!app.manifest.visible) {
            continue;
        }

        const auto instance_path = std::string(SUPER_LAUNCHER_PATH_PREFIX) + std::to_string(app.app_id);
        const auto label_path = instance_path + "/" + SUPER_LAUNCHER_LABEL_ID;
        const auto fallback_icon_path = instance_path + "/" + SUPER_LAUNCHER_FALLBACK_ICON_ID;
        const auto image_icon_path = instance_path + "/" + SUPER_LAUNCHER_IMAGE_ICON_ID;
        const auto has_image_icon = core::has_app_icon_image(app.manifest);
        const auto display_name = get_app_display_name(app);
        const auto fallback_text = has_image_icon ? std::string() : get_app_icon_text(display_name);

        add_binding_update(updates, label_path, "name", display_name);
        if (has_image_icon) {
            add_binding_update(
                updates,
                image_icon_path,
                "src",
                core::resolve_app_icon_resource_id(app.manifest)
            );
            add_binding_update(updates, image_icon_path, "hidden", "false");
            add_binding_update(updates, fallback_icon_path, "hidden", "true");
            add_binding_update(updates, fallback_icon_path, "text", "");
        } else {
            add_binding_update(updates, image_icon_path, "hidden", "true");
            add_binding_update(updates, fallback_icon_path, "hidden", "false");
            add_binding_update(updates, fallback_icon_path, "text", fallback_text);
        }
    }

    if (!updates.empty()) {
        auto binding_result = context_->gui().set_binding_values(updates);
        if (!binding_result) {
            return binding_result;
        }
    }

    return {};
}

void ShellApp::disconnect_launcher_actions()
{
    for (auto &connection : launcher_action_connections_) {
        connection.disconnect();
    }
    launcher_action_connections_.clear();
}

void ShellApp::handle_launcher_event(const gui::Event &event)
{
    if (launch_overlay_active_) {
        BROOKESIA_LOGW("Ignore launcher action while app launch overlay is active");
        return;
    }

    auto instance_id = get_launcher_instance_id(event);
    if (!instance_id) {
        BROOKESIA_LOGW("Launcher action from unknown path: %1%", event.path);
        return;
    }
    auto app_it = launcher_instance_to_app_.find(*instance_id);
    if (app_it == launcher_instance_to_app_.end()) {
        BROOKESIA_LOGW("Launcher action from unknown instance: %1%", *instance_id);
        return;
    }

    std::optional<gui::ViewFrame> launch_origin_frame;
    if (context_ != nullptr) {
        const auto icon_box_path =
            std::string(SUPER_LAUNCHER_ITEM_LAYER_PATH) + "/" + *instance_id + "/" + SUPER_LAUNCHER_ICON_BOX_ID;
        launch_origin_frame = get_absolute_view_frame(*context_, icon_box_path);
        if (!launch_origin_frame.has_value()) {
            BROOKESIA_LOGW("Launcher icon frame not available: path(%1%)", icon_box_path);
        }
    }

    const auto app_id = app_it->second;
    auto start_app = [this, app_id]() {
        auto result = owner_.start_app(app_id, core::System::AppStartOptions{});
        if (!result) {
            BROOKESIA_LOGW("Failed to launch app: app_id(%1%), error(%2%)", app_id, result.error());
        }
    };
    auto app = owner_.get_app(app_id);
    if (!app.has_value()) {
        start_app();
        return;
    }

    auto collapse_result = set_system_ui_expanded(false);
    if (!collapse_result) {
        BROOKESIA_LOGW("Failed to collapse system UI before app launch: %1%", collapse_result.error());
    }

    auto overlay_result = show_launch_overlay(*app, launch_origin_frame, [this, app_id]() {
        schedule_app_start_after_launch(app_id);
    });
    if (!overlay_result) {
        BROOKESIA_LOGW("Failed to show app launch overlay: %1%", overlay_result.error());
        start_app();
    }
}

} // namespace esp_brookesia::system::super
