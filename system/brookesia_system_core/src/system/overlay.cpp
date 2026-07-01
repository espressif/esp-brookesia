/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>

#include "boost/chrono.hpp"
#include "boost/thread/thread.hpp"
#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/system/impl.hpp"

namespace esp_brookesia::system::core {
namespace {

std::string resolve_resource_file(std::string_view root, std::string_view path)
{
    if (path.empty()) {
        return {};
    }
    std::filesystem::path file(path);
    if (file.is_absolute() || root.empty()) {
        return file.generic_string();
    }
    return (std::filesystem::path(root) / file).generic_string();
}

std::string bool_to_binding(bool value)
{
    return value ? "true" : "false";
}

std::string int_to_binding(int32_t value)
{
    return std::to_string(value);
}

void add_binding(
    std::vector<gui::BindingValueUpdate> &updates,
    std::string_view path,
    std::string_view key,
    std::string value
)
{
    updates.push_back(gui::BindingValueUpdate{
        .absolute_path = std::string(path),
        .key = std::string(key),
        .value = std::move(value),
    });
}

gui::Animation make_animation(gui::AnimationProperty property, int32_t to, int32_t duration_ms)
{
    return gui::Animation{
        .id = {},
        .trigger = gui::AnimationTrigger::Manual,
        .property = property,
        .from_mode = gui::AnimationValueMode::Current,
        .from = 0,
        .to_mode = gui::AnimationValueMode::Absolute,
        .to = to,
        .duration = duration_ms,
        .delay = 0,
        .easing = gui::AnimationEasing::EaseOut,
    };
}

gui::ViewFrame make_fallback_origin(const gui::Environment &environment, int32_t icon_size)
{
    const auto width = std::max(environment.width_px, icon_size);
    const auto height = std::max(environment.height_px, icon_size);
    return gui::ViewFrame{
        .x = (width - icon_size) / 2,
        .y = (height - icon_size) / 2,
        .width = icon_size,
        .height = icon_size,
    };
}

} // namespace

std::expected<void, std::string> System::Impl::show_startup_overlay()
{
    const auto &config = config_.startup_overlay;
    if (!config.enabled || config.root_path.empty() || !gui_runtime_) {
        return {};
    }

    return run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, &config]() -> std::expected<void, std::string> {
        auto document_result = gui_runtime_->load_file(
            resolve_resource_file(get_system_storage_dir(), config.root_path),
            environment_
        );
        if (!document_result)
        {
            return std::unexpected(document_result.error());
        }

        auto mount_result = gui_runtime_->push_transient_screen(*document_result, config.screen_path, config.target);
        if (!mount_result)
        {
            gui_runtime_->unload(*document_result);
            return std::unexpected(mount_result.error());
        }

        startup_overlay_ = TransientOverlayRecord{
            .document_id = *document_result,
            .mount_id = *mount_result,
            .shown_at = std::chrono::steady_clock::now(),
        };
        gui_runtime_->process_backend();
        return {};
    },
    std::unexpected("Failed to post startup overlay task")
           );
}

void System::Impl::hide_startup_overlay()
{
    if (!startup_overlay_.has_value()) {
        return;
    }

    auto overlay = *startup_overlay_;
    startup_overlay_.reset();
    const auto min_present_ms = std::max<int32_t>(config_.startup_overlay.min_present_ms, 0);
    if (min_present_ms > 0) {
        const auto elapsed = std::chrono::steady_clock::now() - overlay.shown_at;
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        if (elapsed_ms < min_present_ms) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(min_present_ms - elapsed_ms));
        }
    }
    auto cleanup_result = run_task_sync<bool>(
                              SYSTEM_GUI_TASK_GROUP,
    [this, overlay]() {
        if (!gui_runtime_) {
            return false;
        }
        (void)gui_runtime_->pop_transient_screen(overlay.mount_id);
        return gui_runtime_->unload(overlay.document_id);
    },
    false
                          );
    if (!cleanup_result) {
        BROOKESIA_LOGW("Failed to hide startup overlay");
    }
}

std::expected<std::optional<System::Impl::TransientOverlayRecord>, std::string>
System::Impl::show_app_launch_transition(
    const AppRecord &record,
    const System::AppStartOptions &options
)
{
    const auto &config = config_.app_launch_transition;
    if (!config.enabled || config.root_path.empty() || !record.info.manifest.visible || !gui_runtime_) {
        return std::optional<TransientOverlayRecord> {};
    }

    auto overlay_result = run_task_sync<std::expected<std::optional<TransientOverlayRecord>, std::string>>(
                              SYSTEM_GUI_INPUT_TASK_GROUP,
    [this, &config, &record, &options]() -> std::expected<std::optional<TransientOverlayRecord>, std::string> {
        auto document_result = gui_runtime_->load_file(
            resolve_resource_file(get_system_storage_dir(), config.root_path),
            environment_
        );
        if (!document_result)
        {
            return std::unexpected(document_result.error());
        }

        auto mount_result = gui_runtime_->push_transient_screen(*document_result, config.screen_path, config.target);
        if (!mount_result)
        {
            gui_runtime_->unload(*document_result);
            return std::unexpected(mount_result.error());
        }

        const auto displays = gui_runtime_->list_displays();
        const auto display_it = std::find_if(displays.begin(), displays.end(), [](const gui::GuiDisplayInfo & display)
        {
            return display.is_default;
        });
        const int32_t screen_width = display_it == displays.end() ? environment_.width_px : display_it->width_px;
        const int32_t screen_height = display_it == displays.end() ? environment_.height_px : display_it->height_px;
        const auto origin = options.launch_origin_frame.value_or(make_fallback_origin(
                    environment_,
                    std::max<int32_t>(config.final_icon_size, 1)
                ));
        const int32_t final_icon_size = std::max<int32_t>(config.final_icon_size, 1);
        const int32_t final_icon_x = (std::max(screen_width, final_icon_size) - final_icon_size) / 2;
        const int32_t final_icon_y = (std::max(screen_height, final_icon_size) - final_icon_size) / 2;

        std::vector<gui::BindingValueUpdate> updates;
        add_binding(updates, config.surface_path, "x", int_to_binding(origin.x));
        add_binding(updates, config.surface_path, "y", int_to_binding(origin.y));
        add_binding(updates, config.surface_path, "width", int_to_binding(origin.width));
        add_binding(updates, config.surface_path, "height", int_to_binding(origin.height));
        const bool has_icon = !record.registered_icon_resource_id.empty();
        if (has_icon)
        {
            add_binding(updates, config.icon_path, "src", record.registered_icon_resource_id);
        }
        add_binding(updates, config.icon_path, "hidden", bool_to_binding(!has_icon));
        add_binding(updates, config.fallback_label_path, "text", record.info.manifest.name.empty() ? "?" :
                    record.info.manifest.name.substr(0, 1));
        add_binding(updates, config.fallback_label_path, "hidden", bool_to_binding(has_icon));

        const auto icon_box_path = std::filesystem::path(config.icon_path).parent_path().generic_string();
        add_binding(updates, icon_box_path, "x", int_to_binding(origin.x));
        add_binding(updates, icon_box_path, "y", int_to_binding(origin.y));
        add_binding(updates, icon_box_path, "width", int_to_binding(origin.width));
        add_binding(updates, icon_box_path, "height", int_to_binding(origin.height));
        gui_runtime_->set_binding_values(*document_result, updates);

        const auto duration = std::max<int32_t>(config.duration_ms, 0);
        (void)gui_runtime_->start_view_animation_with_result(
            *document_result,
            config.surface_path,
            make_animation(gui::AnimationProperty::X, 0, duration)
        );
        (void)gui_runtime_->start_view_animation_with_result(
            *document_result,
            config.surface_path,
            make_animation(gui::AnimationProperty::Y, 0, duration)
        );
        (void)gui_runtime_->start_view_animation_with_result(
            *document_result,
            config.surface_path,
            make_animation(gui::AnimationProperty::Width, screen_width, duration)
        );
        (void)gui_runtime_->start_view_animation_with_result(
            *document_result,
            config.surface_path,
            make_animation(gui::AnimationProperty::Height, screen_height, duration)
        );
        (void)gui_runtime_->start_view_animation_with_result(
            *document_result,
            icon_box_path,
            make_animation(gui::AnimationProperty::X, final_icon_x, duration)
        );
        (void)gui_runtime_->start_view_animation_with_result(
            *document_result,
            icon_box_path,
            make_animation(gui::AnimationProperty::Y, final_icon_y, duration)
        );
        (void)gui_runtime_->start_view_animation_with_result(
            *document_result,
            icon_box_path,
            make_animation(gui::AnimationProperty::Width, final_icon_size, duration)
        );
        (void)gui_runtime_->start_view_animation_with_result(
            *document_result,
            icon_box_path,
            make_animation(gui::AnimationProperty::Height, final_icon_size, duration)
        );
        gui_runtime_->process_backend();

        return TransientOverlayRecord{
            .document_id = *document_result,
            .mount_id = *mount_result,
            .shown_at = std::chrono::steady_clock::now(),
        };
    },
    std::unexpected("Failed to post app launch transition task")
                          );
    if (!overlay_result || !overlay_result->has_value()) {
        return overlay_result;
    }

    const int32_t wait_ms = std::clamp<int32_t>(
                                config.duration_ms,
                                0,
                                std::max<int32_t>(config.timeout_ms, 0)
                            );
    if (wait_ms > 0) {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(wait_ms));
    }
    return overlay_result;
}

void System::Impl::hide_transient_overlay(std::optional<TransientOverlayRecord> &record)
{
    if (!record.has_value()) {
        return;
    }

    auto overlay = *record;
    record.reset();
    auto cleanup_result = run_task_sync<bool>(
                              SYSTEM_GUI_INPUT_TASK_GROUP,
    [this, overlay]() {
        if (!gui_runtime_) {
            return false;
        }
        (void)gui_runtime_->pop_transient_screen(overlay.mount_id);
        return gui_runtime_->unload(overlay.document_id);
    },
    false
                          );
    if (!cleanup_result) {
        BROOKESIA_LOGW("Failed to hide transient overlay");
    }
}

} // namespace esp_brookesia::system::core
