/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/gui_interface.hpp"
#include "brookesia/system_core/app/types.hpp"

namespace esp_brookesia::system::core {

class System;

class SystemGuiAccess {
public:
    SystemGuiAccess() = default;

    std::expected<gui::DocumentId, std::string> load_file(
        std::string_view resource_dir,
        std::string_view path
    ) const;
    std::expected<gui::DocumentId, std::string> load_json(
        std::string_view root_path,
        std::string_view json,
        std::string_view resource_dir
    ) const;
    bool unload(gui::DocumentId document_id) const;
    std::expected<gui::View, std::string> mount_screen(
        gui::DocumentId document_id,
        std::string_view absolute_path,
        const gui::MountTarget &target = {}
    ) const;
    bool unmount_screen(gui::DocumentId document_id, std::string_view absolute_path) const;
    std::expected<gui::TransientMountId, std::string> push_transient_screen(
        gui::DocumentId document_id,
        std::string_view absolute_path,
        const gui::MountTarget &target = {}
    ) const;
    bool pop_transient_screen(gui::TransientMountId id) const;
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
    std::expected<void, std::string> set_binding_value(
        gui::DocumentId document_id,
        std::string_view absolute_path,
        std::string_view key,
        std::string value
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
    std::expected<void, std::string> set_view_src(
        gui::DocumentId document_id,
        std::string_view absolute_path,
        std::string_view src
    ) const;
    std::expected<void, std::string> preload_images(
        gui::DocumentId document_id,
        const std::vector<std::string> &image_ids
    ) const;
    std::expected<void, std::string> release_images(
        gui::DocumentId document_id,
        const std::vector<std::string> &image_ids
    ) const;
    std::expected<gui::RuntimeAnimationStartResult, std::string> start_view_animation_with_result(
        gui::DocumentId document_id,
        std::string_view absolute_path,
        const gui::Animation &animation,
        gui::Runtime::AnimationCompletedHandler completed_handler = {}
    ) const;
    bool stop_animation(gui::SubscriptionId subscription_id) const;
    gui::ScopedConnection subscribe_action(
        gui::DocumentId document_id,
        std::string_view action,
        gui::Runtime::ActionHandler handler
    ) const;
    std::expected<void, std::string> enable_live_preview(
        gui::DocumentId document_id,
        const gui::LivePreviewOptions &options = {}
    ) const;
    bool disable_live_preview(gui::DocumentId document_id) const;
    std::expected<void, std::string> load_theme_file(
        std::string_view resource_dir,
        std::string_view path
    ) const;
    std::expected<void, std::string> set_theme(
        std::string_view theme_id,
        bool reapply_loaded_documents = true
    ) const;
    std::string get_theme() const;
    GuiThemeLanguage get_theme_language() const;
    std::expected<void, std::string> register_font_file(
        std::string_view resource_dir,
        std::string_view path
    ) const;
    std::expected<void, std::string> register_image(const gui::RuntimeImageResource &resource) const;
    bool unregister_image(std::string_view id) const;
    std::vector<std::string> list_supported_fonts(std::string_view language) const;
    std::vector<std::string> list_supported_languages() const;
    std::vector<std::string> list_supported_languages(std::string_view font_id) const;
    std::expected<void, std::string> set_default_font_for_language(
        std::string_view language,
        std::string_view font_id
    ) const;
    std::expected<void, std::string> set_language(
        std::string_view language,
        bool reapply_loaded_documents = true
    ) const;
    std::string get_language() const;

private:
    friend class System;

    explicit SystemGuiAccess(System &system);

    System *system_ = nullptr;
};

} // namespace esp_brookesia::system::core
