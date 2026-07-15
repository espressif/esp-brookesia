/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cctype>
#include <chrono>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "brookesia/system_core/app/iapp.hpp"
#include "brookesia/system_core/app/package.hpp"
#include "brookesia/system_core/system/system.hpp"
#include "brookesia/service_helper.hpp"
#include "private/filesystem.hpp"

namespace esp_brookesia::system::core {
namespace {

constexpr uint32_t STORAGE_FS_TIMEOUT_MS = 5000;

bool is_safe_app_directory_name(std::string_view name)
{
    return !name.empty() &&
           name != "." &&
           name != ".." &&
           (name.find('/') == std::string_view::npos) &&
           (name.find('\\') == std::string_view::npos);
}

std::optional<AppInfo> find_app_by_manifest_id(const System &system, std::string_view manifest_id)
{
    for (const auto &app : system.list_apps()) {
        if (app.manifest.id == manifest_id) {
            return app;
        }
    }
    return std::nullopt;
}

std::string make_unique_install_directory_name(std::string_view app_id)
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(app_id) + "-" + std::to_string(now);
}

std::expected<void, std::string> remove_path_tree_if_exists(
    const std::filesystem::path &path,
    std::string_view label
)
{
    auto info = service::helper::Storage::fs_stat(path.generic_string(), STORAGE_FS_TIMEOUT_MS);
    if (!info) {
        return std::unexpected(
                   "Failed to check " + std::string(label) + " '" + path.generic_string() +
                   "': " + info.error()
               );
    }
    if (!info->exists) {
        return {};
    }
    auto remove_result = remove_path_tree(path, label);
    if (!remove_result) {
        return std::unexpected(remove_result.error());
    }
    return {};
}

std::expected<void, std::string> merge_directory_contents(
    const std::filesystem::path &source,
    const std::filesystem::path &destination,
    bool overwrite_existing
)
{
    auto source_info = service::helper::Storage::fs_stat(source.generic_string(), STORAGE_FS_TIMEOUT_MS);
    if (!source_info) {
        return std::unexpected(
                   "Failed to check source directory '" + source.generic_string() + "': " +
                   source_info.error()
               );
    }
    if (!source_info->exists) {
        return {};
    }
    if (source_info->type != service::helper::Storage::FileType::Directory) {
        return std::unexpected("Source is not a directory: " + source.generic_string());
    }

    auto copy_result = service::helper::Storage::fs_copy_tree(
                           source.generic_string(), destination.generic_string(), overwrite_existing,
                           STORAGE_FS_TIMEOUT_MS
                       );
    if (!copy_result) {
        return std::unexpected(
                   "Failed to copy directory '" + source.generic_string() + "' to '" +
                   destination.generic_string() + "': " + copy_result.error()
               );
    }
    return {};
}

std::expected<void, std::string> replace_directory_contents(
    const std::filesystem::path &source,
    const std::filesystem::path &destination,
    std::string_view label
)
{
    auto source_info = service::helper::Storage::fs_stat(source.generic_string(), STORAGE_FS_TIMEOUT_MS);
    if (!source_info) {
        return std::unexpected(
                   "Failed to check " + std::string(label) + " '" + source.generic_string() +
                   "': " + source_info.error()
               );
    }
    if (!source_info->exists) {
        return {};
    }
    auto remove_result = remove_path_tree_if_exists(destination, label);
    if (!remove_result) {
        return remove_result;
    }
    return merge_directory_contents(source, destination, true);
}

std::expected<void, std::string> preserve_runtime_app_private_data(
    const std::filesystem::path &old_app_dir,
    const std::filesystem::path &staged_app_dir
)
{
    auto preserve_cache = replace_directory_contents(
                              old_app_dir / "cache",
                              staged_app_dir / "cache",
                              "runtime app cache"
                          );
    if (!preserve_cache) {
        return preserve_cache;
    }
    auto preserve_data = replace_directory_contents(
                             old_app_dir / "data",
                             staged_app_dir / "data",
                             "runtime app data"
                         );
    if (!preserve_data) {
        return preserve_data;
    }
    return merge_directory_contents(old_app_dir / "files", staged_app_dir / "files", false);
}

std::expected<std::filesystem::path, std::string> get_default_install_app_root(const StorageLayout &layout)
{
    auto make_app_root = [](const StorageVolume & volume) {
        return (std::filesystem::path(volume.root_path) / "apps").lexically_normal();
    };

    auto find_external = [&layout](std::string_view volume_id) -> const StorageVolume * {
        if (volume_id.empty())
        {
            return nullptr;
        }
        for (const auto &volume : layout.external)
        {
            if (volume.id == volume_id && volume.available) {
                return &volume;
            }
        }
        return nullptr;
    };

    if (layout.default_install_target == StorageInstallTarget::Internal) {
        if (!layout.internal.available) {
            return std::unexpected("Internal storage is not available for app installation");
        }
        return make_app_root(layout.internal);
    }
    if (layout.default_install_target == StorageInstallTarget::External ||
            layout.default_install_target == StorageInstallTarget::Auto) {
        if (const auto *preferred = find_external(layout.preferred_external_id); preferred != nullptr) {
            return make_app_root(*preferred);
        }
        for (const auto &volume : layout.external) {
            if (volume.available) {
                return make_app_root(volume);
            }
        }
    }
    if (layout.internal.available) {
        return make_app_root(layout.internal);
    }
    return std::unexpected("No storage volume is available for app installation");
}

} // namespace

AppTimerRuntime::AppTimerRuntime(System &system, AppId app_id)
    : system_(&system)
    , app_id_(app_id)
{}

std::expected<TimerId, std::string> AppTimerRuntime::start_periodic(
    std::string_view name,
    int interval_ms
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->timer_start_periodic(app_id_, name, interval_ms);
}

std::expected<TimerId, std::string> AppTimerRuntime::start_delayed(
    std::string_view name,
    int delay_ms
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->timer_start_delayed(app_id_, name, delay_ms);
}

bool AppTimerRuntime::stop(TimerId timer_id) const
{
    return (system_ != nullptr) && system_->timer_stop(app_id_, timer_id);
}

AppGuiRuntime::AppGuiRuntime(System &system, AppId app_id)
    : system_(&system)
    , app_id_(app_id)
{}

std::expected<void, std::string> AppGuiRuntime::set_binding_value(
    std::string_view absolute_path,
    std::string_view key,
    std::string value
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_set_binding_value(app_id_, absolute_path, key, std::move(value));
}

std::expected<void, std::string> AppGuiRuntime::set_binding_values(
    const std::vector<gui::BindingValueUpdate> &updates
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_set_binding_values(app_id_, updates);
}

std::expected<GuiBatchResult, std::string> AppGuiRuntime::execute_batch(
    const std::vector<GuiBatchCommand> &commands
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_execute_batch(app_id_, commands);
}

std::optional<std::string> AppGuiRuntime::get_binding_value(std::string_view absolute_path, std::string_view key) const
{
    if (system_ == nullptr) {
        return std::nullopt;
    }
    return system_->gui_get_binding_value(app_id_, absolute_path, key);
}

std::expected<boost::json::value, std::string> AppGuiRuntime::get_constant_value(std::string_view path) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_get_constant_value(app_id_, path);
}

std::expected<void, std::string> AppGuiRuntime::set_text(
    std::string_view absolute_path,
    std::string_view text
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_set_text(app_id_, absolute_path, text);
}

std::expected<void, std::string> AppGuiRuntime::set_value(std::string_view absolute_path, int32_t value) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_set_value(app_id_, absolute_path, value);
}

std::expected<void, std::string> AppGuiRuntime::set_checked(std::string_view absolute_path, bool checked) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_set_checked(app_id_, absolute_path, checked);
}

std::expected<gui::View, std::string> AppGuiRuntime::create_view(
    std::string_view template_id,
    std::string_view parent_absolute_path,
    std::string_view instance_id
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_create_view(app_id_, template_id, parent_absolute_path, instance_id);
}

bool AppGuiRuntime::destroy_view(std::string_view absolute_path) const
{
    return (system_ != nullptr) && system_->gui_destroy_view(app_id_, absolute_path);
}

std::expected<void, std::string> AppGuiRuntime::subscribe_action(std::string_view action) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_subscribe_action(app_id_, action);
}

std::expected<void, std::string> AppGuiRuntime::subscribe_actions(const std::vector<std::string> &actions) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_subscribe_actions(app_id_, actions);
}

gui::ScopedConnection AppGuiRuntime::subscribe_action(
    std::string_view action,
    std::function<void(const gui::Event &)> handler
) const
{
    if (system_ == nullptr) {
        return {};
    }
    return system_->gui_subscribe_action(app_id_, action, std::move(handler));
}

std::vector<gui::ScopedConnection> AppGuiRuntime::subscribe_actions(
    const std::vector<GuiActionHandlerSubscription> &subscriptions
) const
{
    if (system_ == nullptr) {
        return {};
    }
    return system_->gui_subscribe_actions(app_id_, subscriptions);
}

std::expected<void, std::string> AppGuiRuntime::trigger_screen_flow(
    std::string_view screen_flow,
    std::string_view action) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_trigger_screen_flow(app_id_, screen_flow, action);
}

std::optional<std::string> AppGuiRuntime::get_screen_flow_state(std::string_view screen_flow) const
{
    if (system_ == nullptr) {
        return std::nullopt;
    }
    return system_->gui_get_screen_flow_state(app_id_, screen_flow);
}

std::optional<gui::ViewFrame> AppGuiRuntime::get_view_frame(std::string_view absolute_path) const
{
    if (system_ == nullptr) {
        return std::nullopt;
    }
    return system_->gui_get_view_frame(app_id_, absolute_path);
}

std::expected<void, std::string> AppGuiRuntime::scroll_to(
    std::string_view absolute_path,
    int32_t x,
    int32_t y,
    bool animated
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_scroll_to(app_id_, absolute_path, x, y, animated);
}

std::expected<void, std::string> AppGuiRuntime::scroll_to_view(
    std::string_view absolute_path,
    bool animated
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_scroll_to_view(app_id_, absolute_path, animated);
}

std::expected<void, std::string> AppGuiRuntime::set_view_src(
    std::string_view absolute_path,
    std::string_view src
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_set_view_src(app_id_, absolute_path, src);
}

std::expected<void, std::string> AppGuiRuntime::preload_image(std::string_view image_id) const
{
    return preload_images({std::string(image_id)});
}

std::expected<void, std::string> AppGuiRuntime::preload_images(
    const std::vector<std::string> &image_ids
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_preload_images(app_id_, image_ids);
}

std::expected<void, std::string> AppGuiRuntime::release_preloaded_image(std::string_view image_id) const
{
    return release_preloaded_images({std::string(image_id)});
}

std::expected<void, std::string> AppGuiRuntime::release_preloaded_images(
    const std::vector<std::string> &image_ids
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_release_images(app_id_, image_ids);
}

std::expected<gui::SubscriptionId, std::string> AppGuiRuntime::start_view_animation_with_id(
    std::string_view absolute_path,
    const gui::Animation &animation,
    gui::Runtime::AnimationCompletedHandler completed_handler
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_start_view_animation_with_id(app_id_, absolute_path, animation, std::move(completed_handler));
}

std::expected<gui::RuntimeAnimationStartResult, std::string> AppGuiRuntime::start_view_animation_with_result(
    std::string_view absolute_path,
    const gui::Animation &animation,
    gui::Runtime::AnimationCompletedHandler completed_handler
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->gui_start_view_animation_with_result(app_id_, absolute_path, animation, std::move(completed_handler));
}

bool AppGuiRuntime::stop_animation(gui::SubscriptionId subscription_id) const
{
    return (system_ != nullptr) && system_->gui_stop_animation(app_id_, subscription_id);
}

std::expected<void, std::string> AppGuiRuntime::set_view_debug_enabled(bool enabled) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->set_gui_view_debug_enabled(enabled);
}

bool AppGuiRuntime::is_view_debug_enabled() const
{
    return (system_ != nullptr) && system_->is_gui_view_debug_enabled();
}

std::expected<void, std::string> AppGuiRuntime::enable_live_preview(const gui::LivePreviewOptions &options) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->enable_app_gui_live_preview(app_id_, options);
}

bool AppGuiRuntime::disable_live_preview() const
{
    return (system_ != nullptr) && system_->disable_app_gui_live_preview(app_id_);
}

std::expected<void, std::string> AppGuiRuntime::load_theme_file(
    std::string_view resource_dir,
    std::string_view path
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can load GUI themes");
    }
    return system_->system_gui().load_theme_file(resource_dir, path);
}

std::expected<void, std::string> AppGuiRuntime::set_theme(
    std::string_view theme_id,
    bool reapply_loaded_documents
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can set GUI themes");
    }
    return system_->system_gui().set_theme(theme_id, reapply_loaded_documents);
}

std::string AppGuiRuntime::get_theme() const
{
    if (system_ == nullptr || !is_native_system_app()) {
        return {};
    }
    return system_->system_gui().get_theme();
}

GuiThemeLanguage AppGuiRuntime::get_theme_language() const
{
    if (system_ == nullptr || !is_native_system_app()) {
        return {};
    }
    return system_->system_gui().get_theme_language();
}

std::expected<void, std::string> AppGuiRuntime::register_font_file(
    std::string_view resource_dir,
    std::string_view path
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can register GUI fonts");
    }
    return system_->system_gui().register_font_file(resource_dir, path);
}

std::expected<void, std::string> AppGuiRuntime::register_image(const gui::RuntimeImageResource &resource) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can register GUI images");
    }
    return system_->system_gui().register_image(resource);
}

bool AppGuiRuntime::unregister_image(std::string_view id) const
{
    return (system_ != nullptr) && is_native_system_app() && system_->system_gui().unregister_image(id);
}

std::vector<std::string> AppGuiRuntime::list_supported_fonts(std::string_view language) const
{
    if (system_ == nullptr || !is_native_system_app()) {
        return {};
    }
    return system_->system_gui().list_supported_fonts(language);
}

std::vector<std::string> AppGuiRuntime::list_supported_languages() const
{
    if (system_ == nullptr || !is_native_system_app()) {
        return {};
    }
    return system_->system_gui().list_supported_languages();
}

std::vector<std::string> AppGuiRuntime::list_supported_languages(std::string_view font_id) const
{
    if (system_ == nullptr || !is_native_system_app()) {
        return {};
    }
    return system_->system_gui().list_supported_languages(font_id);
}

std::expected<void, std::string> AppGuiRuntime::set_default_font_for_language(
    std::string_view language,
    std::string_view font_id
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can set default GUI fonts");
    }
    return system_->system_gui().set_default_font_for_language(language, font_id);
}

std::expected<void, std::string> AppGuiRuntime::set_language(
    std::string_view language,
    bool reapply_loaded_documents
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can set GUI language");
    }
    return system_->system_gui().set_language(language, reapply_loaded_documents);
}

std::string AppGuiRuntime::get_language() const
{
    if (system_ == nullptr || !is_native_system_app()) {
        return {};
    }
    return system_->system_gui().get_language();
}

std::expected<gui::DocumentId, std::string> AppGuiRuntime::load_file(
    std::string_view resource_dir,
    std::string_view path
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can load system GUI documents");
    }
    return system_->system_gui_load_file(resource_dir, path);
}

std::expected<gui::DocumentId, std::string> AppGuiRuntime::load_json(
    std::string_view root_path,
    std::string_view json,
    std::string_view resource_dir
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can load system GUI documents");
    }
    return system_->system_gui_load_json(root_path, json, resource_dir);
}

std::expected<void, std::string> AppGuiRuntime::set_binding_values(
    gui::DocumentId document_id,
    const std::vector<gui::BindingValueUpdate> &updates
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can update system GUI documents");
    }
    return system_->system_gui().set_binding_values(document_id, updates);
}

std::expected<boost::json::value, std::string> AppGuiRuntime::get_constant_value(
    gui::DocumentId document_id,
    std::string_view path
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can query system GUI documents");
    }
    return system_->system_gui().get_constant_value(document_id, path);
}

std::expected<void, std::string> AppGuiRuntime::set_text(
    gui::DocumentId document_id,
    std::string_view absolute_path,
    std::string_view text
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can update system GUI documents");
    }
    return system_->system_gui().set_text(document_id, absolute_path, text);
}

std::expected<gui::View, std::string> AppGuiRuntime::mount_screen(
    gui::DocumentId document_id,
    std::string_view absolute_path,
    const gui::MountTarget &target
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can mount system GUI documents");
    }
    return system_->system_gui_mount_screen(document_id, absolute_path, target);
}

bool AppGuiRuntime::unmount_screen(gui::DocumentId document_id, std::string_view absolute_path) const
{
    return (system_ != nullptr) && is_native_system_app() &&
           system_->system_gui_unmount_screen(document_id, absolute_path);
}

std::expected<void, std::string> AppGuiRuntime::start_screen_flow(
    gui::DocumentId document_id,
    std::string_view screen_flow,
    const gui::MountTarget &target
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can start system GUI screen flows");
    }
    return system_->system_gui().start_screen_flow(document_id, screen_flow, target);
}

std::expected<void, std::string> AppGuiRuntime::trigger_screen_flow(
    gui::DocumentId document_id,
    std::string_view screen_flow,
    std::string_view action
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can trigger system GUI screen flows");
    }
    return system_->system_gui().trigger_screen_flow(document_id, screen_flow, action);
}

bool AppGuiRuntime::stop_screen_flow(gui::DocumentId document_id, std::string_view screen_flow) const
{
    return (system_ != nullptr) && is_native_system_app() &&
           system_->system_gui().stop_screen_flow(document_id, screen_flow);
}

std::optional<std::string> AppGuiRuntime::get_screen_flow_state(
    gui::DocumentId document_id,
    std::string_view screen_flow) const
{
    if (system_ == nullptr || !is_native_system_app()) {
        return std::nullopt;
    }
    return system_->system_gui().get_screen_flow_state(document_id, screen_flow);
}

bool AppGuiRuntime::unload(gui::DocumentId document_id) const
{
    return (system_ != nullptr) && is_native_system_app() && system_->system_gui_unload(document_id);
}

gui::ScopedConnection AppGuiRuntime::subscribe_action(
    gui::DocumentId document_id,
    std::string_view action,
    gui::Runtime::ActionHandler handler
) const
{
    if (system_ == nullptr || !is_native_system_app()) {
        return {};
    }
    return system_->system_gui_subscribe_action(document_id, action, std::move(handler));
}

std::expected<gui::RuntimeAnimationStartResult, std::string> AppGuiRuntime::start_view_animation_with_result(
    gui::DocumentId document_id,
    std::string_view absolute_path,
    const gui::Animation &animation,
    gui::Runtime::AnimationCompletedHandler completed_handler
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can animate system GUI documents");
    }
    return system_->system_gui().start_view_animation_with_result(
               document_id,
               absolute_path,
               animation,
               std::move(completed_handler)
           );
}

std::expected<void, std::string> AppGuiRuntime::preload_image(
    gui::DocumentId document_id,
    std::string_view image_id
) const
{
    return preload_images(document_id, {std::string(image_id)});
}

std::expected<void, std::string> AppGuiRuntime::preload_images(
    gui::DocumentId document_id,
    const std::vector<std::string> &image_ids
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can preload system GUI images");
    }
    return system_->system_gui().preload_images(document_id, image_ids);
}

std::expected<void, std::string> AppGuiRuntime::release_preloaded_image(
    gui::DocumentId document_id,
    std::string_view image_id
) const
{
    return release_preloaded_images(document_id, {std::string(image_id)});
}

std::expected<void, std::string> AppGuiRuntime::release_preloaded_images(
    gui::DocumentId document_id,
    const std::vector<std::string> &image_ids
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can release system GUI images");
    }
    return system_->system_gui().release_images(document_id, image_ids);
}

bool AppGuiRuntime::is_native_system_app() const
{
    if (system_ == nullptr || app_id_ == INVALID_APP_ID) {
        return false;
    }
    auto app = system_->get_app(app_id_);
    return app.has_value() && app->manifest.kind == AppKind::Native;
}

SystemApi::SystemApi(System &system, AppId app_id)
    : system_(&system)
    , app_id_(app_id)
{}

std::string SystemApi::get_system_type() const
{
    return system_ == nullptr ? std::string() : system_->get_system_type();
}

SystemInfo SystemApi::get_system_info() const
{
    return system_ == nullptr ? SystemInfo{} : system_->get_system_info();
}

boost::json::object SystemApi::get_environment() const
{
    return system_ == nullptr ? boost::json::object{} : system_->get_environment_json();
}

StorageLayout SystemApi::get_storage_layout() const
{
    return system_ == nullptr ? StorageLayout{} : system_->get_storage_layout();
}

std::expected<AppStoragePaths, std::string> SystemApi::get_app_storage_paths(std::optional<AppId> app_id) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    const AppId target_app_id = app_id.value_or(app_id_);
    if (target_app_id == INVALID_APP_ID) {
        return std::unexpected("App id is invalid");
    }
    if ((target_app_id != app_id_) && !is_native_system_app()) {
        return std::unexpected("App can only access its own storage directories");
    }
    return system_->get_app_storage_paths(target_app_id);
}

std::expected<PublicStoragePaths, std::string> SystemApi::get_public_storage_paths() const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->get_public_storage_paths();
}

std::expected<void, std::string> SystemApi::set_default_install_storage(
    StorageInstallTarget target,
    std::string preferred_external_id
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can set default install storage");
    }
    return system_->set_default_install_storage(target, std::move(preferred_external_id));
}

std::expected<std::vector<StorageFileEntry>, std::string> SystemApi::storage_list(
    const StorageFileLocation &location
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->storage_list(app_id_, location);
}

std::expected<StorageFileInfo, std::string> SystemApi::storage_stat(const StorageFileLocation &location) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->storage_stat(app_id_, location);
}

std::expected<void, std::string> SystemApi::storage_mkdir(const StorageFileLocation &location) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->storage_mkdir(app_id_, location);
}

std::expected<std::string, std::string> SystemApi::storage_read(const StorageFileLocation &location) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->storage_read(app_id_, location);
}

std::expected<void, std::string> SystemApi::storage_write(
    const StorageFileLocation &location,
    std::string_view data
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->storage_write(app_id_, location, data);
}

std::expected<void, std::string> SystemApi::storage_remove(const StorageFileLocation &location) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->storage_remove(app_id_, location);
}

std::expected<void, std::string> SystemApi::storage_rename(
    const StorageFileLocation &from,
    const StorageFileLocation &to
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    return system_->storage_rename(app_id_, from, to);
}

std::optional<AppInfo> SystemApi::get_active_app() const
{
    return system_ == nullptr ? std::nullopt : system_->get_active_app();
}

std::vector<AppInfo> SystemApi::list_apps() const
{
    return system_ == nullptr ? std::vector<AppInfo> {} : system_->list_apps();
}

std::expected<void, std::string> SystemApi::request_close_app(AppId app_id) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app() && (app_id != app_id_) && (app_id_ != INVALID_APP_ID)) {
        return std::unexpected("App can only request closing itself");
    }
    return system_->request_close_app(app_id);
}

std::expected<void, std::string> SystemApi::start_app(AppId app_id) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can start apps");
    }
    return system_->start_app(app_id);
}

std::expected<void, std::string> SystemApi::stop_app(AppId app_id) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can stop apps");
    }
    return system_->stop_app(app_id);
}

std::expected<void, std::string> SystemApi::uninstall_app(AppId app_id) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can uninstall apps");
    }
    return system_->uninstall_app(app_id);
}

std::expected<AppId, std::string> SystemApi::install_runtime_app_package(
    std::string_view package_path,
    bool replace_existing
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (!is_native_system_app()) {
        return std::unexpected("Only native system apps can install runtime app packages");
    }

    auto package_manifest = read_app_package_manifest(package_path, system_->get_system_type());
    if (!package_manifest) {
        return std::unexpected("Failed to read runtime app package manifest: " + package_manifest.error());
    }
    if (!is_safe_app_directory_name(package_manifest->id)) {
        return std::unexpected("Runtime app manifest id is not a safe directory name: " + package_manifest->id);
    }

    auto self = system_->get_app(app_id_);
    if (self && self->manifest.id == package_manifest->id) {
        return std::unexpected("App cannot replace itself");
    }

    auto installed = find_app_by_manifest_id(*system_, package_manifest->id);
    if (installed && !replace_existing) {
        return std::unexpected("Runtime app package is already installed: " + package_manifest->id);
    }

    std::filesystem::path app_root;
    std::filesystem::path old_app_dir;
    if (installed) {
        old_app_dir = std::filesystem::path(installed->manifest.app_path).lexically_normal();
        if (old_app_dir.empty()) {
            return std::unexpected("Installed runtime app path is empty: " + package_manifest->id);
        }
        if (old_app_dir.filename().generic_string() != package_manifest->id) {
            return std::unexpected(
                       "Installed runtime app path does not match manifest id: " + old_app_dir.generic_string()
                   );
        }
        app_root = old_app_dir.parent_path().lexically_normal();
        if (app_root.empty()) {
            return std::unexpected("Installed runtime app root is empty: " + package_manifest->id);
        }
    } else {
        auto app_root_result = get_default_install_app_root(system_->get_storage_layout());
        if (!app_root_result) {
            return std::unexpected(app_root_result.error());
        }
        app_root = app_root_result->lexically_normal();
    }

    auto app_root_result = service::helper::Storage::fs_mkdir(app_root.generic_string(), STORAGE_FS_TIMEOUT_MS);
    if (!app_root_result) {
        return std::unexpected(
                   "Failed to create app root '" + app_root.generic_string() + "': " + app_root_result.error()
               );
    }

    const auto staging_parent = app_root / ".installing" / make_unique_install_directory_name(package_manifest->id);
    const auto staged_app_dir = staging_parent / package_manifest->id;
    const auto install_dir = app_root / package_manifest->id;

    auto cleanup_staging = [&staging_parent]() {
        (void)remove_path_tree_if_exists(staging_parent, "runtime app staging directory");
    };

    auto unpacked_manifest = unpack_app_package_to(
                                 package_path,
                                 staging_parent.generic_string(),
                                 system_->get_system_type()
                             );
    if (!unpacked_manifest) {
        cleanup_staging();
        return std::unexpected("Failed to unpack runtime app package: " + unpacked_manifest.error());
    }
    if (unpacked_manifest->id != package_manifest->id) {
        cleanup_staging();
        return std::unexpected(
                   "Runtime app package manifest changed while unpacking: " + package_manifest->id +
                   " -> " + unpacked_manifest->id
               );
    }
    auto descriptor_result = read_runtime_app_resource_descriptor(*unpacked_manifest);
    if (!descriptor_result) {
        cleanup_staging();
        return std::unexpected("Failed to validate staged runtime app package: " + descriptor_result.error());
    }

    if (installed) {
        auto preserve_result = preserve_runtime_app_private_data(old_app_dir, staged_app_dir);
        if (!preserve_result) {
            cleanup_staging();
            return std::unexpected("Failed to preserve existing app data: " + preserve_result.error());
        }
    } else {
        auto remove_stale_result = remove_path_tree_if_exists(install_dir, "stale runtime app directory");
        if (!remove_stale_result) {
            cleanup_staging();
            return std::unexpected(remove_stale_result.error());
        }
    }

    if (installed) {
        auto uninstall_result = system_->uninstall_app(installed->app_id);
        if (!uninstall_result) {
            cleanup_staging();
            return std::unexpected("Failed to uninstall existing app: " + uninstall_result.error());
        }
    }

    auto rename_result = service::helper::Storage::fs_rename(
                             staged_app_dir.generic_string(), install_dir.generic_string(), STORAGE_FS_TIMEOUT_MS
                         );
    if (!rename_result) {
        cleanup_staging();
        return std::unexpected(
                   "Failed to move staged runtime app to final directory '" + install_dir.generic_string() +
                   "': " + rename_result.error()
               );
    }
    cleanup_staging();
    unpacked_manifest->app_path = install_dir.generic_string();

    auto install_result = system_->install_runtime_app(*unpacked_manifest);
    if (!install_result) {
        auto cleanup_result = remove_path_tree(install_dir, "runtime app directory");
        if (!cleanup_result) {
            return std::unexpected(
                       "Failed to install runtime app package: " + install_result.error() +
                       "; additionally failed to remove runtime app directory: " + cleanup_result.error()
                   );
        }
        return std::unexpected("Failed to install runtime app package: " + install_result.error());
    }
    return *install_result;
}

std::expected<void, std::string> SystemApi::show_loading() const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (app_id_ == INVALID_APP_ID) {
        return std::unexpected("App is not available");
    }
    return system_->show_app_loading(app_id_);
}

std::expected<void, std::string> SystemApi::hide_loading() const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (app_id_ == INVALID_APP_ID) {
        return std::unexpected("App is not available");
    }
    return system_->hide_app_loading(app_id_);
}

std::expected<KeyboardRequestId, std::string> SystemApi::show_keyboard(
    KeyboardRequestOptions options,
    KeyboardResultHandler handler
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (app_id_ == INVALID_APP_ID) {
        return std::unexpected("App is not available");
    }
    return system_->show_app_keyboard(app_id_, std::move(options), std::move(handler));
}

std::expected<void, std::string> SystemApi::hide_keyboard(KeyboardRequestId request_id) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (app_id_ == INVALID_APP_ID) {
        return std::unexpected("App is not available");
    }
    return system_->hide_app_keyboard(app_id_, request_id);
}

std::expected<MessageDialogRequestId, std::string> SystemApi::show_message_dialog(
    MessageDialogOptions options,
    MessageDialogResultHandler handler
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (app_id_ == INVALID_APP_ID) {
        return std::unexpected("App is not available");
    }
    return system_->show_app_message_dialog(app_id_, std::move(options), std::move(handler));
}

std::expected<void, std::string> SystemApi::hide_message_dialog(MessageDialogRequestId request_id) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (app_id_ == INVALID_APP_ID) {
        return std::unexpected("App is not available");
    }
    return system_->hide_app_message_dialog(app_id_, request_id);
}

std::expected<void, std::string> SystemApi::update_message_dialog(
    MessageDialogRequestId request_id,
    MessageDialogOptions options
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System is not available");
    }
    if (app_id_ == INVALID_APP_ID) {
        return std::unexpected("App is not available");
    }
    return system_->update_app_message_dialog(app_id_, request_id, std::move(options));
}

bool SystemApi::is_native_system_app() const
{
    if (system_ == nullptr || app_id_ == INVALID_APP_ID) {
        return false;
    }
    auto app = system_->get_app(app_id_);
    return app.has_value() && app->manifest.kind == AppKind::Native;
}

AppContext::AppContext(System &system, AppId app_id)
    : app_id_(app_id)
    , system_api_(system, app_id)
    , timer_runtime_(system, app_id)
    , gui_runtime_(system, app_id)
{}

AppGuiDescriptor IApp::get_gui_descriptor() const
{
    return {};
}

std::expected<void, std::string> IApp::on_install(AppContext &)
{
    return {};
}

void IApp::on_uninstall(AppContext &)
{}

std::expected<void, std::string> IApp::on_start(AppContext &)
{
    return {};
}

std::expected<void, std::string> IApp::on_pause(AppContext &)
{
    return {};
}

std::expected<void, std::string> IApp::on_resume(AppContext &)
{
    return {};
}

std::expected<void, std::string> IApp::on_stop(AppContext &)
{
    return {};
}

std::expected<void, std::string> IApp::on_action(AppContext &, std::string_view)
{
    return {};
}

std::expected<void, std::string> IApp::on_timer(AppContext &, TimerId, std::string_view)
{
    return {};
}

std::shared_ptr<IApp> IAppProvider::create_app()
{
    return nullptr;
}

} // namespace esp_brookesia::system::core
