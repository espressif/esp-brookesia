/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "brookesia/gui_interface/runtime.hpp"

namespace esp_brookesia::system::core {

// Compile-time stand-in for the JSON GUI runtime in GSP builds. It retains
// SystemCore's published types without introducing references to Runtime.
class JsonGuiUnavailable {
public:
    JsonGuiUnavailable() = delete;

    template <class... Args>
    std::expected<gui::DocumentId, std::string> load_file(Args &&...) const
    {
        return unavailable<gui::DocumentId>();
    }

    template <class... Args>
    std::expected<gui::DocumentId, std::string> load_json(Args &&...) const
    {
        return unavailable<gui::DocumentId>();
    }

    template <class... Args>
    std::expected<void, std::string> load_theme(Args &&...) const
    {
        return unavailable<void>();
    }

    template <class... Args>
    std::expected<void, std::string> set_theme(Args &&...) const
    {
        return unavailable<void>();
    }

    template <class... Args>
    std::expected<void, std::string> set_language(Args &&...) const
    {
        return unavailable<void>();
    }

    template <class... Args>
    std::expected<void, std::string> set_default_font_for_language(Args &&...) const
    {
        return unavailable<void>();
    }

    template <class... Args>
    std::expected<void, std::string> register_font(Args &&...) const
    {
        return unavailable<void>();
    }

    template <class... Args>
    std::expected<void, std::string> register_font_file(Args &&...) const
    {
        return unavailable<void>();
    }

    template <class... Args>
    std::expected<void, std::string> register_image(Args &&...) const
    {
        return unavailable<void>();
    }

    template <class... Args>
    std::expected<void, std::string> preload_images(Args &&...) const
    {
        return unavailable<void>();
    }

    template <class... Args>
    std::expected<void, std::string> release_preloaded_images(Args &&...) const
    {
        return unavailable<void>();
    }

    template <class... Args>
    std::expected<void, std::string> enable_live_preview(Args &&...) const
    {
        return unavailable<void>();
    }

    template <class... Args>
    std::expected<void, std::string> start_screen_flow(Args &&...) const
    {
        return unavailable<void>();
    }

    template <class... Args>
    std::expected<void, std::string> trigger_screen_flow(Args &&...) const
    {
        return unavailable<void>();
    }

    template <class... Args>
    std::expected<gui::View, std::string> mount_screen(Args &&...) const
    {
        return unavailable<gui::View>();
    }

    template <class... Args>
    std::expected<gui::View, std::string> create_view(Args &&...) const
    {
        return unavailable<gui::View>();
    }

    template <class... Args>
    std::expected<gui::TransientMountId, std::string> push_transient_screen(Args &&...) const
    {
        return unavailable<gui::TransientMountId>();
    }

    template <class... Args>
    std::expected<boost::json::value, std::string> get_constant_value(Args &&...) const
    {
        return unavailable<boost::json::value>();
    }

    template <class... Args>
    bool unload(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    bool unregister_font(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    bool unregister_image(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    bool disable_live_preview(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    bool unmount_screen(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    bool pop_transient_screen(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    bool stop_screen_flow(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    bool has_screen_flow(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    bool destroy_view(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    bool unsubscribe_subscription(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    bool scroll_view_to(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    bool scroll_view_to_visible(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    bool set_view_src(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    bool is_view_debug_enabled(Args &&...) const
    {
        return false;
    }

    template <class... Args>
    void process_backend(Args &&...) const
    {}

    template <class... Args>
    void request_idle_screen_warm(Args &&...) const
    {}

    template <class... Args>
    void poll_live_preview(Args &&...) const
    {}

    template <class... Args>
    void set_view_debug_enabled(Args &&...) const
    {}

    template <class... Args>
    void set_binding_values(Args &&...) const
    {}

    template <class... Args>
    gui::View find_view(Args &&...) const
    {
        return {};
    }

    template <class... Args>
    gui::RuntimeAnimationStartResult start_view_animation_with_result(Args &&...) const
    {
        return {};
    }

    template <class... Args>
    gui::ScopedConnection subscribe_event_action(Args &&...) const
    {
        return {};
    }

    template <class... Args>
    std::optional<std::string> get_binding_value(Args &&...) const
    {
        return std::nullopt;
    }

    template <class... Args>
    std::optional<std::string> get_screen_flow_state(Args &&...) const
    {
        return std::nullopt;
    }

    template <class... Args>
    std::optional<gui::ViewStateValue> get_view_state(Args &&...) const
    {
        return std::nullopt;
    }

    template <class... Args>
    std::vector<gui::GuiDisplayInfo> list_displays(Args &&...) const
    {
        return {};
    }

    template <class... Args>
    std::vector<std::string> list_supported_fonts(Args &&...) const
    {
        return {};
    }

    template <class... Args>
    std::vector<std::string> list_supported_languages(Args &&...) const
    {
        return {};
    }

private:
    template <class T>
    static std::expected<T, std::string> unavailable()
    {
        return std::unexpected("JSON GUI is unavailable with the GSP backend");
    }
};

} // namespace esp_brookesia::system::core
