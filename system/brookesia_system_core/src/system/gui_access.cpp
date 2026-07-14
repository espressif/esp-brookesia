/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include <filesystem>
#include <mutex>

#include "brookesia/gui_interface/parser.hpp"
#include "private/utils.hpp"
#include "private/system/impl.hpp"

namespace esp_brookesia::system::core {
namespace {

std::string join_resource_path(std::string_view resource_dir, std::string_view path)
{
    if (path.empty()) {
        return {};
    }
    std::filesystem::path source(path);
    if (source.is_absolute() || resource_dir.empty()) {
        return source.string();
    }
    return (std::filesystem::path(resource_dir) / source).string();
}

std::expected<void, std::string> make_unavailable_error()
{
    return std::unexpected("System GUI access is not available");
}

} // namespace

GuiThemeLanguage System::Impl::get_gui_theme_language_snapshot() const
{
    std::lock_guard lock(mutex_);
    return gui_theme_language_snapshot_;
}

void System::Impl::set_gui_theme_snapshot(std::string_view theme_id)
{
    std::lock_guard lock(mutex_);
    gui_theme_language_snapshot_.theme_id = std::string(theme_id);
}

void System::Impl::set_gui_language_snapshot(std::string_view language)
{
    std::lock_guard lock(mutex_);
    gui_theme_language_snapshot_.language = std::string(language);
}

SystemGuiAccess::SystemGuiAccess(System &system)
    : system_(&system)
{}

std::expected<gui::DocumentId, std::string> SystemGuiAccess::load_file(
    std::string_view resource_dir,
    std::string_view path
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System GUI access is not available");
    }
    return system_->impl_->run_task_sync<std::expected<gui::DocumentId, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, resource_dir = std::string(resource_dir), path = std::string(path)]() {
        if (!system_->impl_->gui_runtime_) {
            return std::expected<gui::DocumentId, std::string>(std::unexpected("GUI runtime is not available"));
        }
        auto load_result = system_->impl_->gui_runtime_->load_file(
                               join_resource_path(resource_dir, path),
                               system_->impl_->environment_
                           );
        if (!load_result || !system_->impl_->config_.enable_gui_live_preview) {
            return load_result;
        }
        auto preview_result = system_->impl_->enable_live_preview_for_document(
                                  *load_result,
                                  system_->impl_->config_.gui_live_preview_options
                              );
        if (!preview_result) {
            system_->impl_->gui_runtime_->unload(*load_result);
            return std::expected<gui::DocumentId, std::string>(std::unexpected(preview_result.error()));
        }
        return load_result;
    },
    std::unexpected("Failed to post system GUI load file task")
           );
}

std::expected<gui::DocumentId, std::string> SystemGuiAccess::load_json(
    std::string_view root_path,
    std::string_view json,
    std::string_view resource_dir
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System GUI access is not available");
    }
    return system_->impl_->run_task_sync<std::expected<gui::DocumentId, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
               [this,
                root_path = std::string(root_path),
                json = std::string(json),
    resource_dir = std::string(resource_dir)]() {
        if (!system_->impl_->gui_runtime_) {
            return std::expected<gui::DocumentId, std::string>(std::unexpected("GUI runtime is not available"));
        }
        return system_->impl_->gui_runtime_->load_json(
                   root_path,
                   json,
                   resource_dir,
                   system_->impl_->environment_
               );
    },
    std::unexpected("Failed to post system GUI load JSON task")
           );
}

bool SystemGuiAccess::unload(gui::DocumentId document_id) const
{
    if (system_ == nullptr) {
        return false;
    }
    return system_->impl_->run_task_sync<bool>(
               SYSTEM_GUI_TASK_GROUP,
    [this, document_id]() {
        system_->impl_->remove_live_preview_document(document_id);
        return system_->impl_->gui_runtime_ && system_->impl_->gui_runtime_->unload(document_id);
    },
    false
           );
}

std::expected<gui::View, std::string> SystemGuiAccess::mount_screen(
    gui::DocumentId document_id,
    std::string_view absolute_path,
    const gui::MountTarget &target
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System GUI access is not available");
    }
    return system_->impl_->run_task_sync<std::expected<gui::View, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, document_id, absolute_path = std::string(absolute_path), target]() {
        if (!system_->impl_->gui_runtime_) {
            return std::expected<gui::View, std::string>(std::unexpected("GUI runtime is not available"));
        }
        return system_->impl_->gui_runtime_->mount_screen(document_id, absolute_path, target);
    },
    std::unexpected("Failed to post system GUI mount screen task")
           );
}

bool SystemGuiAccess::unmount_screen(gui::DocumentId document_id, std::string_view absolute_path) const
{
    if (system_ == nullptr) {
        return false;
    }
    return system_->impl_->run_task_sync<bool>(
               SYSTEM_GUI_TASK_GROUP,
    [this, document_id, absolute_path = std::string(absolute_path)]() {
        return system_->impl_->gui_runtime_ &&
               system_->impl_->gui_runtime_->unmount_screen(document_id, absolute_path);
    },
    false
           );
}

std::expected<gui::TransientMountId, std::string> SystemGuiAccess::push_transient_screen(
    gui::DocumentId document_id,
    std::string_view absolute_path,
    const gui::MountTarget &target
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System GUI access is not available");
    }
    return system_->impl_->run_task_sync<std::expected<gui::TransientMountId, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, document_id, absolute_path = std::string(absolute_path), target]() {
        if (!system_->impl_->gui_runtime_) {
            return std::expected<gui::TransientMountId, std::string>(
                       std::unexpected("GUI runtime is not available")
                   );
        }
        return system_->impl_->gui_runtime_->push_transient_screen(document_id, absolute_path, target);
    },
    std::unexpected("Failed to post system GUI transient screen push task")
           );
}

bool SystemGuiAccess::pop_transient_screen(gui::TransientMountId id) const
{
    if (system_ == nullptr) {
        return false;
    }
    return system_->impl_->run_task_sync<bool>(
               SYSTEM_GUI_TASK_GROUP,
    [this, id]() {
        return system_->impl_->gui_runtime_ && system_->impl_->gui_runtime_->pop_transient_screen(id);
    },
    false
           );
}

std::expected<void, std::string> SystemGuiAccess::start_screen_flow(
    gui::DocumentId document_id,
    std::string_view screen_flow,
    const gui::MountTarget &target
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System GUI access is not available");
    }
    return system_->impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, document_id, screen_flow = std::string(screen_flow), target]() -> std::expected<void, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        return system_->impl_->gui_runtime_->start_screen_flow(document_id, screen_flow, target);
    },
    std::unexpected("Failed to post system GUI screen flow start task")
           );
}

std::expected<void, std::string> SystemGuiAccess::trigger_screen_flow(
    gui::DocumentId document_id,
    std::string_view screen_flow,
    std::string_view action
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System GUI access is not available");
    }
    return system_->impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
               [this,
                document_id,
                screen_flow = std::string(screen_flow),
    action = std::string(action)]() -> std::expected<void, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        return system_->impl_->gui_runtime_->trigger_screen_flow(document_id, screen_flow, action);
    },
    std::unexpected("Failed to post system GUI screen flow trigger task")
           );
}

bool SystemGuiAccess::stop_screen_flow(gui::DocumentId document_id, std::string_view screen_flow) const
{
    if (system_ == nullptr) {
        return false;
    }
    return system_->impl_->run_task_sync<bool>(
               SYSTEM_GUI_TASK_GROUP,
    [this, document_id, screen_flow = std::string(screen_flow)]() {
        return system_->impl_->gui_runtime_ &&
               system_->impl_->gui_runtime_->stop_screen_flow(document_id, screen_flow);
    },
    false
           );
}

std::optional<std::string> SystemGuiAccess::get_screen_flow_state(
    gui::DocumentId document_id,
    std::string_view screen_flow) const
{
    if (system_ == nullptr) {
        return std::nullopt;
    }
    return system_->impl_->run_task_sync<std::optional<std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, document_id, screen_flow = std::string(screen_flow)]() -> std::optional<std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::nullopt;
        }
        return system_->impl_->gui_runtime_->get_screen_flow_state(document_id, screen_flow);
    },
    std::nullopt
           );
}

std::expected<void, std::string> SystemGuiAccess::set_binding_value(
    gui::DocumentId document_id,
    std::string_view absolute_path,
    std::string_view key,
    std::string value
) const
{
    std::vector<gui::BindingValueUpdate> updates;
    updates.push_back(gui::BindingValueUpdate{
        .absolute_path = std::string(absolute_path),
        .key = std::string(key),
        .value = std::move(value),
    });
    return set_binding_values(document_id, updates);
}

std::expected<void, std::string> SystemGuiAccess::set_binding_values(
    gui::DocumentId document_id,
    const std::vector<gui::BindingValueUpdate> &updates
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System GUI access is not available");
    }
    if (updates.empty()) {
        return {};
    }
    if (system_->impl_->should_defer_gui_task(SYSTEM_GUI_INPUT_TASK_GROUP)) {
        auto post_result = system_->impl_->post_gui_input_task([this, document_id, updates]() {
            if (!system_->impl_->gui_runtime_) {
                BROOKESIA_LOGW("Skip deferred system GUI binding update: GUI runtime is not available");
                return;
            }
            system_->impl_->gui_runtime_->set_binding_values(document_id, updates);
        });
        if (!post_result) {
            return std::unexpected(post_result.error());
        }
        return {};
    }
    return system_->impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
    [this, document_id, updates]() -> std::expected<void, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        system_->impl_->gui_runtime_->set_binding_values(document_id, updates);
        return {};
    },
    std::unexpected("Failed to post system GUI binding task")
           );
}

std::expected<boost::json::value, std::string> SystemGuiAccess::get_constant_value(
    gui::DocumentId document_id,
    std::string_view path
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System GUI access is not available");
    }
    return system_->impl_->run_task_sync<std::expected<boost::json::value, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, document_id, path = std::string(path)]() -> std::expected<boost::json::value, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        return system_->impl_->gui_runtime_->get_constant_value(document_id, path);
    },
    std::unexpected("Failed to post system GUI constant query task")
           );
}

std::expected<void, std::string> SystemGuiAccess::set_text(
    gui::DocumentId document_id,
    std::string_view absolute_path,
    std::string_view text
) const
{
    if (system_ == nullptr) {
        return make_unavailable_error();
    }
    if (system_->impl_->should_defer_gui_task(SYSTEM_GUI_TASK_GROUP)) {
        auto post_result = system_->impl_->post_gui_task(
        [this, document_id, absolute_path = std::string(absolute_path), text = std::string(text)]() {
            if (!system_->impl_->gui_runtime_) {
                BROOKESIA_LOGW("Skip deferred system GUI text update: GUI runtime is not available");
                return;
            }
            auto view = system_->impl_->gui_runtime_->find_view(document_id, absolute_path);
            if (!view.valid()) {
                BROOKESIA_LOGW("Skip deferred system GUI text update because view is missing: %1%", absolute_path);
                return;
            }
            auto label = view.as_label();
            if (!label.valid() || !label.set_text(text)) {
                BROOKESIA_LOGW("Skip deferred system GUI text update because view is not a label: %1%", absolute_path);
            }
        }
                           );
        if (!post_result) {
            return std::unexpected(post_result.error());
        }
        return {};
    }
    return system_->impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
               [this, document_id, absolute_path = std::string(absolute_path), text = std::string(text)]()
    -> std::expected<void, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        auto view = system_->impl_->gui_runtime_->find_view(document_id, absolute_path);
        if (!view.valid())
        {
            return std::unexpected("System GUI text view is missing: " + absolute_path);
        }
        auto label = view.as_label();
        if (!label.valid() || !label.set_text(text))
        {
            return std::unexpected("System GUI view is not a label: " + absolute_path);
        }
        return {};
    },
    std::unexpected("Failed to post system GUI text task")
           );
}

std::expected<void, std::string> SystemGuiAccess::set_view_src(
    gui::DocumentId document_id,
    std::string_view absolute_path,
    std::string_view src
) const
{
    if (system_ == nullptr) {
        return make_unavailable_error();
    }
    if (system_->impl_->should_defer_gui_task(SYSTEM_GUI_INPUT_TASK_GROUP)) {
        auto post_result = system_->impl_->post_gui_input_task(
        [this, document_id, absolute_path = std::string(absolute_path), src = std::string(src)]() {
            if (!system_->impl_->gui_runtime_) {
                BROOKESIA_LOGW("Skip deferred system GUI image source update: GUI runtime is not available");
                return;
            }
            if (!system_->impl_->gui_runtime_->set_view_src(document_id, absolute_path, src)) {
                BROOKESIA_LOGW("Deferred system GUI image source update failed: %1%", absolute_path);
            }
        }
                           );
        if (!post_result) {
            return std::unexpected(post_result.error());
        }
        return {};
    }
    return system_->impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
               [this, document_id, absolute_path = std::string(absolute_path), src = std::string(src)]()
    -> std::expected<void, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        if (!system_->impl_->gui_runtime_->set_view_src(document_id, absolute_path, src))
        {
            return std::unexpected("Failed to set system GUI image source: " + absolute_path);
        }
        return {};
    },
    std::unexpected("Failed to post system GUI image source task")
           );
}

std::expected<void, std::string> SystemGuiAccess::preload_images(
    gui::DocumentId document_id,
    const std::vector<std::string> &image_ids
) const
{
    if (system_ == nullptr) {
        return make_unavailable_error();
    }
    return system_->impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
    [this, document_id, image_ids]() -> std::expected<void, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        return system_->impl_->gui_runtime_->preload_images(document_id, image_ids);
    },
    std::unexpected("Failed to post system GUI image preload task")
           );
}

std::expected<void, std::string> SystemGuiAccess::release_images(
    gui::DocumentId document_id,
    const std::vector<std::string> &image_ids
) const
{
    if (system_ == nullptr) {
        return make_unavailable_error();
    }
    return system_->impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
    [this, document_id, image_ids]() -> std::expected<void, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        return system_->impl_->gui_runtime_->release_preloaded_images(document_id, image_ids);
    },
    std::unexpected("Failed to post system GUI image release task")
           );
}

std::expected<gui::RuntimeAnimationStartResult, std::string> SystemGuiAccess::start_view_animation_with_result(
    gui::DocumentId document_id,
    std::string_view absolute_path,
    const gui::Animation &animation,
    gui::Runtime::AnimationCompletedHandler completed_handler
) const
{
    if (system_ == nullptr) {
        return std::unexpected("System GUI access is not available");
    }
    return system_->impl_->run_task_sync<std::expected<gui::RuntimeAnimationStartResult, std::string>>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
               [this,
                document_id,
                absolute_path = std::string(absolute_path),
                animation,
                completed_handler = std::move(completed_handler)]() mutable
    -> std::expected<gui::RuntimeAnimationStartResult, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        auto scheduled_completed_handler =
        [this, completed_handler = std::move(completed_handler)]() mutable {
            if (!completed_handler)
            {
                return;
            }
            auto post_result = system_->impl_->post_task(
                SYSTEM_APP_INPUT_TASK_GROUP,
            [completed_handler = std::move(completed_handler)]() mutable {
                completed_handler();
            }
            );
            if (!post_result)
            {
                BROOKESIA_LOGW("Post system GUI animation completed handler failed: %1%", post_result.error());
            }
        };
        auto start_result = system_->impl_->gui_runtime_->start_view_animation_with_result(
                                document_id,
                                absolute_path,
                                animation,
                                std::move(scheduled_completed_handler)
                            );
        if (start_result.subscription_id == 0)
        {
            return std::unexpected("Failed to start system GUI animation");
        }
        return start_result;
    },
    std::unexpected("Failed to post system GUI animation task")
           );
}

bool SystemGuiAccess::stop_animation(gui::SubscriptionId subscription_id) const
{
    if (system_ == nullptr) {
        return false;
    }
    return system_->impl_->run_task_sync<bool>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
    [this, subscription_id]() {
        return system_->impl_->gui_runtime_ &&
               system_->impl_->gui_runtime_->unsubscribe_subscription(subscription_id);
    },
    false
           );
}

gui::ScopedConnection SystemGuiAccess::subscribe_action(
    gui::DocumentId document_id,
    std::string_view action,
    gui::Runtime::ActionHandler handler
) const
{
    if (system_ == nullptr) {
        return {};
    }
    return system_->impl_->run_task_sync<gui::ScopedConnection>(
               SYSTEM_GUI_TASK_GROUP,
    [this, document_id, action = std::string(action), handler = std::move(handler)]() mutable {
        if (!system_->impl_->gui_runtime_)
        {
            return gui::ScopedConnection {};
        }
        auto scheduled_handler = [this, handler = std::move(handler)](const gui::Event & event) mutable {
            auto event_copy = event;
            auto post_result = system_->impl_->post_task(
                SYSTEM_APP_INPUT_TASK_GROUP,
            [handler, event_copy = std::move(event_copy)]() mutable {
                handler(event_copy);
            }
            );
            if (!post_result)
            {
                BROOKESIA_LOGW("Post system GUI action handler failed: %1%", post_result.error());
            }
        };
        return system_->impl_->gui_runtime_->subscribe_event_action(document_id, action, std::move(scheduled_handler));
    },
    gui::ScopedConnection {}
           );
}

std::expected<void, std::string> SystemGuiAccess::enable_live_preview(
    gui::DocumentId document_id,
    const gui::LivePreviewOptions &options
) const
{
    if (system_ == nullptr) {
        return make_unavailable_error();
    }
    return system_->impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, document_id, options]() {
        return system_->impl_->enable_live_preview_for_document(document_id, options);
    },
    std::unexpected("Failed to post system GUI live preview enable task")
           );
}

bool SystemGuiAccess::disable_live_preview(gui::DocumentId document_id) const
{
    if (system_ == nullptr) {
        return false;
    }
    return system_->impl_->run_task_sync<bool>(
               SYSTEM_GUI_TASK_GROUP,
    [this, document_id]() {
        return system_->impl_->disable_live_preview_for_document(document_id);
    },
    false
           );
}

std::expected<void, std::string> SystemGuiAccess::load_theme_file(
    std::string_view resource_dir,
    std::string_view path
) const
{
    if (system_ == nullptr) {
        return make_unavailable_error();
    }
    return system_->impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, resource_dir = std::string(resource_dir), path = std::string(path)]() -> std::expected<void, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        auto theme_path = join_resource_path(resource_dir, path);
        auto theme = gui::parse_theme_asset_file(theme_path, system_->impl_->environment_);
        if (!theme)
        {
            return std::unexpected(theme.error());
        }
        return system_->impl_->gui_runtime_->load_theme(*theme);
    },
    std::unexpected("Failed to post system GUI theme load task")
           );
}

std::expected<void, std::string> SystemGuiAccess::set_theme(
    std::string_view theme_id,
    bool reapply_loaded_documents
) const
{
    if (system_ == nullptr) {
        return make_unavailable_error();
    }
    auto result = system_->impl_->run_task_sync<std::expected<void, std::string>>(
                      SYSTEM_GUI_TASK_GROUP,
    [this, theme_id = std::string(theme_id), reapply_loaded_documents]() -> std::expected<void, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        auto result = system_->impl_->gui_runtime_->set_theme(theme_id, reapply_loaded_documents);
        if (result)
        {
            system_->impl_->environment_.theme_id = theme_id;
        }
        return result;
    },
    std::unexpected("Failed to post system GUI theme set task")
                  );
    if (!result) {
        return result;
    }
    system_->impl_->set_gui_theme_snapshot(theme_id);
    auto hook_result = system_->on_theme_changed(theme_id);
    if (!hook_result) {
        BROOKESIA_LOGW("System theme changed hook failed: %1%", hook_result.error());
    }
    system_->impl_->persist_gui_theme_preference(theme_id);
    return {};
}

std::string SystemGuiAccess::get_theme() const
{
    if (system_ == nullptr) {
        return {};
    }
    return system_->impl_->get_gui_theme_language_snapshot().theme_id;
}

GuiThemeLanguage SystemGuiAccess::get_theme_language() const
{
    if (system_ == nullptr) {
        return {};
    }
    return system_->impl_->get_gui_theme_language_snapshot();
}

std::expected<void, std::string> SystemGuiAccess::register_font_file(
    std::string_view resource_dir,
    std::string_view path
) const
{
    if (system_ == nullptr) {
        return make_unavailable_error();
    }
    return system_->impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, resource_dir = std::string(resource_dir), path = std::string(path)]() -> std::expected<void, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        return system_->impl_->gui_runtime_->register_font_file(join_resource_path(resource_dir, path));
    },
    std::unexpected("Failed to post system GUI font register task")
           );
}

std::expected<void, std::string> SystemGuiAccess::register_image(const gui::RuntimeImageResource &resource) const
{
    if (system_ == nullptr) {
        return make_unavailable_error();
    }
    if (system_->impl_->should_defer_gui_task(SYSTEM_GUI_TASK_GROUP)) {
        auto post_result = system_->impl_->post_gui_task([this, resource]() {
            if (!system_->impl_->gui_runtime_) {
                BROOKESIA_LOGW("Skip deferred system GUI image register: GUI runtime is not available");
                return;
            }
            auto result = system_->impl_->gui_runtime_->register_image(resource);
            if (!result) {
                BROOKESIA_LOGW(
                    "Deferred system GUI image register failed: id(%1%), error(%2%)",
                    resource.id, result.error()
                );
            }
        });
        if (!post_result) {
            return std::unexpected(post_result.error());
        }
        return {};
    }
    return system_->impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, resource]() -> std::expected<void, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        return system_->impl_->gui_runtime_->register_image(resource);
    },
    std::unexpected("Failed to post system GUI image register task")
           );
}

bool SystemGuiAccess::unregister_image(std::string_view id) const
{
    if (system_ == nullptr) {
        return false;
    }
    return system_->impl_->run_task_sync<bool>(
               SYSTEM_GUI_TASK_GROUP,
    [this, id = std::string(id)]() {
        return system_->impl_->gui_runtime_ && system_->impl_->gui_runtime_->unregister_image(id);
    },
    false
           );
}

std::vector<std::string> SystemGuiAccess::list_supported_fonts(std::string_view language) const
{
    if (system_ == nullptr) {
        return {};
    }
    return system_->impl_->run_task_sync<std::vector<std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, language = std::string(language)]() {
        return system_->impl_->gui_runtime_ ?
               system_->impl_->gui_runtime_->list_supported_fonts(language) :
               std::vector<std::string>();
    },
    {}
           );
}

std::vector<std::string> SystemGuiAccess::list_supported_languages() const
{
    if (system_ == nullptr) {
        return {};
    }
    return system_->impl_->run_task_sync<std::vector<std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this]() {
        return system_->impl_->gui_runtime_ ?
               system_->impl_->gui_runtime_->list_supported_languages() :
               std::vector<std::string>();
    },
    {}
           );
}

std::vector<std::string> SystemGuiAccess::list_supported_languages(std::string_view font_id) const
{
    if (system_ == nullptr) {
        return {};
    }
    return system_->impl_->run_task_sync<std::vector<std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, font_id = std::string(font_id)]() {
        return system_->impl_->gui_runtime_ ?
               system_->impl_->gui_runtime_->list_supported_languages(font_id) :
               std::vector<std::string>();
    },
    {}
           );
}

std::expected<void, std::string> SystemGuiAccess::set_default_font_for_language(
    std::string_view language,
    std::string_view font_id
) const
{
    if (system_ == nullptr) {
        return make_unavailable_error();
    }
    return system_->impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, language = std::string(language), font_id = std::string(font_id)]() -> std::expected<void, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        return system_->impl_->gui_runtime_->set_default_font_for_language(language, font_id);
    },
    std::unexpected("Failed to post system GUI default font task")
           );
}

std::expected<void, std::string> SystemGuiAccess::set_language(
    std::string_view language,
    bool reapply_loaded_documents
) const
{
    if (system_ == nullptr) {
        return make_unavailable_error();
    }
    auto result = system_->impl_->run_task_sync<std::expected<void, std::string>>(
                      SYSTEM_GUI_TASK_GROUP,
    [this, language = std::string(language), reapply_loaded_documents]() -> std::expected<void, std::string> {
        if (!system_->impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        auto result = system_->impl_->gui_runtime_->set_language(language, reapply_loaded_documents);
        if (result)
        {
            system_->impl_->environment_.language = language;
        }
        return result;
    },
    std::unexpected("Failed to post system GUI language set task")
                  );
    if (!result) {
        return result;
    }
    system_->impl_->set_gui_language_snapshot(language);
    auto hook_result = system_->on_language_changed(language);
    if (!hook_result) {
        BROOKESIA_LOGW("System language changed hook failed: %1%", hook_result.error());
    }
    system_->impl_->persist_gui_language_preference(language);
    return {};
}

std::string SystemGuiAccess::get_language() const
{
    if (system_ == nullptr) {
        return {};
    }
    return system_->impl_->get_gui_theme_language_snapshot().language;
}

} // namespace esp_brookesia::system::core
