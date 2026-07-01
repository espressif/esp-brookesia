/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/gui_interface.hpp"
#include "brookesia/system_core/system/batch.hpp"
#include "brookesia/system_core/app/types.hpp"

namespace esp_brookesia::system::core {

class System;

class AppGuiRuntime {
public:
    AppGuiRuntime() = default;
    AppGuiRuntime(System &system, AppId app_id);

    std::expected<void, std::string> set_binding_value(
        std::string_view absolute_path,
        std::string_view key,
        std::string value
    ) const;
    std::expected<void, std::string> set_binding_values(const std::vector<gui::BindingValueUpdate> &updates) const;
    std::expected<GuiBatchResult, std::string> execute_batch(const std::vector<GuiBatchCommand> &commands) const;
    std::optional<std::string> get_binding_value(std::string_view absolute_path, std::string_view key) const;
    std::expected<boost::json::value, std::string> get_constant_value(std::string_view path) const;
    std::expected<void, std::string> set_text(std::string_view absolute_path, std::string_view text) const;
    std::expected<void, std::string> set_value(std::string_view absolute_path, int32_t value) const;
    std::expected<void, std::string> set_checked(std::string_view absolute_path, bool checked) const;
    std::expected<gui::View, std::string> create_view(
        std::string_view template_id,
        std::string_view parent_absolute_path,
        std::string_view instance_id
    ) const;
    bool destroy_view(std::string_view absolute_path) const;
    std::expected<void, std::string> subscribe_action(std::string_view action) const;
    gui::ScopedConnection subscribe_action(
        std::string_view action,
        std::function<void(const gui::Event &)> handler
    ) const;
    std::expected<void, std::string> trigger_screen_flow(
        std::string_view screen_flow,
        std::string_view action
    ) const;
    std::optional<std::string> get_screen_flow_state(std::string_view screen_flow) const;
    std::optional<gui::ViewFrame> get_view_frame(std::string_view absolute_path) const;
    std::expected<void, std::string> set_view_src(
        std::string_view absolute_path,
        std::string_view src
    ) const;
    std::expected<gui::SubscriptionId, std::string> start_view_animation_with_id(
        std::string_view absolute_path,
        const gui::Animation &animation,
        gui::Runtime::AnimationCompletedHandler completed_handler = {}
    ) const;
    std::expected<gui::RuntimeAnimationStartResult, std::string> start_view_animation_with_result(
        std::string_view absolute_path,
        const gui::Animation &animation,
        gui::Runtime::AnimationCompletedHandler completed_handler = {}
    ) const;
    bool stop_animation(gui::SubscriptionId subscription_id) const;
    void set_view_debug_enabled(bool enabled) const;
    bool is_view_debug_enabled() const;
    std::expected<void, std::string> enable_live_preview(const gui::LivePreviewOptions &options = {}) const;
    bool disable_live_preview() const;
    std::expected<void, std::string> load_theme_file(
        std::string_view resource_dir,
        std::string_view path
    ) const;
    std::expected<void, std::string> set_theme(
        std::string_view theme_id,
        bool reapply_loaded_documents = true
    ) const;
    std::string get_theme() const;
    std::expected<void, std::string> register_font_file(
        std::string_view resource_dir,
        std::string_view path
    ) const;
    std::expected<void, std::string> register_image(const gui::RuntimeImageResource &resource) const;
    bool unregister_image(std::string_view id) const;
    std::vector<std::string> list_supported_languages() const;
    std::expected<void, std::string> set_default_font_for_language(
        std::string_view language,
        std::string_view font_id
    ) const;
    std::expected<void, std::string> set_language(
        std::string_view language,
        bool reapply_loaded_documents = true
    ) const;
    std::string get_language() const;

    std::expected<gui::DocumentId, std::string> load_file(
        std::string_view resource_dir,
        std::string_view path
    ) const;
    std::expected<gui::DocumentId, std::string> load_json(
        std::string_view root_path,
        std::string_view json,
        std::string_view resource_dir
    ) const;
    std::expected<void, std::string> set_binding_values(
        gui::DocumentId document_id,
        const std::vector<gui::BindingValueUpdate> &updates
    ) const;
    std::expected<boost::json::value, std::string> get_constant_value(
        gui::DocumentId document_id,
        std::string_view path
    ) const;
    std::expected<void, std::string> set_text(
        gui::DocumentId document_id,
        std::string_view absolute_path,
        std::string_view text
    ) const;
    std::expected<gui::View, std::string> mount_screen(
        gui::DocumentId document_id,
        std::string_view absolute_path,
        const gui::MountTarget &target = {}
    ) const;
    bool unmount_screen(gui::DocumentId document_id, std::string_view absolute_path) const;
    std::expected<void, std::string> start_screen_flow(
        gui::DocumentId document_id,
        std::string_view screen_flow,
        const gui::MountTarget &target = {}
    ) const;
    std::expected<void, std::string> trigger_screen_flow(
        gui::DocumentId document_id,
        std::string_view screen_flow,
        std::string_view action
    ) const;
    bool stop_screen_flow(gui::DocumentId document_id, std::string_view screen_flow) const;
    std::optional<std::string> get_screen_flow_state(
        gui::DocumentId document_id,
        std::string_view screen_flow
    ) const;
    bool unload(gui::DocumentId document_id) const;
    gui::ScopedConnection subscribe_action(
        gui::DocumentId document_id,
        std::string_view action,
        gui::Runtime::ActionHandler handler
    ) const;
    std::expected<gui::RuntimeAnimationStartResult, std::string> start_view_animation_with_result(
        gui::DocumentId document_id,
        std::string_view absolute_path,
        const gui::Animation &animation,
        gui::Runtime::AnimationCompletedHandler completed_handler = {}
    ) const;

private:
    bool is_native_system_app() const;

    System *system_ = nullptr;
    AppId app_id_ = INVALID_APP_ID;
};

} // namespace esp_brookesia::system::core
