/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstdio>
#include <string>

#include "private/settings_app_internal.hpp"

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
#   define GSP_BUNDLE_ENABLE_RAW_IDS 1
#   include "settings_gsp.h"
#   include "brookesia/gui_gsp/app_host.hpp"
#   include "esp_gsp.h"
#endif

namespace esp_brookesia::app::settings {

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP

namespace {

esp_gsp_handle_t current_handle()
{
    auto *host = gui::gsp::AppHost::current();
    if (host == nullptr) {
        return nullptr;
    }
    return host->handle();
}

std::expected<void, std::string> require_handle(esp_gsp_handle_t &handle)
{
    handle = current_handle();
    if (handle == nullptr) {
        return std::unexpected("GSP session is not running");
    }
    return {};
}

} // namespace

class GspSettingsSurface: public SettingsSurface {
public:
    std::expected<void, std::string> show_page(
        system::core::AppContext &,
        std::string_view page
    ) override
    {
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        uint16_t scene = 0;
        if (page == detail::PAGE_HOME) {
            scene = GSP_SETTINGS_SCENE_SETTINGS_HOME;
        } else if (page == detail::PAGE_DISPLAY) {
            scene = GSP_SETTINGS_SCENE_SETTINGS_DISPLAY;
        } else {
            return std::unexpected("Unsupported settings page");
        }
        if (esp_gsp_goto_scene(handle, scene, ESP_GSP_NO_TRANSITION) != ESP_GSP_OK) {
            return std::unexpected("Failed to show the Settings page");
        }
        const auto flushed = esp_gsp_flush(handle, 500);
        if (flushed != ESP_GSP_OK) {
            return std::unexpected("Failed to settle the Settings page: " + std::to_string(static_cast<int>(flushed)));
        }
        return {};
    }

    std::expected<void, std::string> set_header(
        system::core::AppContext &,
        std::string_view title,
        bool show_back
    ) override
    {
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        const auto title_key = show_back ? GSP_SETTINGS_DISPLAY_OBJ_KEY_HEADER_TITLE
                               : GSP_SETTINGS_HOME_OBJ_KEY_HEADER_TITLE;
        if (esp_gsp_component_set_text(handle, title_key, std::string(title).c_str()) != ESP_GSP_OK) {
            return std::unexpected("Failed to update the Settings title");
        }
        return {};
    }

    std::expected<void, std::string> set_brightness(
        system::core::AppContext &,
        int value,
        bool enabled,
        std::string_view status,
        std::string_view
    ) override
    {
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        const auto slider_result = esp_gsp_component_set_value(
                                       handle, GSP_SETTINGS_DISPLAY_OBJ_KEY_BRIGHTNESS_SLIDER, value
                                   );
        if (slider_result != ESP_GSP_OK) {
            return std::unexpected(
                       "Failed to update the brightness slider: " + std::to_string(static_cast<int>(slider_result))
                   );
        }
        if (esp_gsp_component_set_enabled(
                    handle, GSP_SETTINGS_DISPLAY_OBJ_KEY_BRIGHTNESS_SLIDER, enabled
                ) != ESP_GSP_OK) {
            return std::unexpected("Failed to update the brightness slider state");
        }
        char value_text[16];
        if (enabled) {
            std::snprintf(value_text, sizeof(value_text), "%d%%", value);
        } else {
            std::snprintf(value_text, sizeof(value_text), "--");
        }
        if (esp_gsp_component_set_text(handle, GSP_SETTINGS_DISPLAY_OBJ_KEY_BRIGHTNESS_VALUE, value_text) != ESP_GSP_OK) {
            return std::unexpected("Failed to update the brightness value");
        }
        if (esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_DISPLAY_OBJ_KEY_BRIGHTNESS_STATUS, std::string(status).c_str()
                ) != ESP_GSP_OK) {
            return std::unexpected("Failed to update the brightness status");
        }
        return {};
    }
};

std::unique_ptr<SettingsSurface> make_gsp_settings_surface()
{
    return std::make_unique<GspSettingsSurface>();
}

std::span<const std::byte> SettingsApp::gui_bundle() const
{
    const auto config = gsp_settings_config();
    return {reinterpret_cast<const std::byte *>(config.bundle), config.bundle_size};
}

const void *SettingsApp::gui_runtime_config() const
{
    static esp_gsp_config_t config;
    config = gsp_settings_config();
    return &config;
}

std::expected<void, std::string> SettingsApp::start_gsp_pages(system::core::AppContext &context)
{
    context_ = &context;
    current_locale_ = detail::LOCALE_EN;
    current_page_ = detail::PAGE_HOME;
    current_theme_id_.clear();
    device_ui_state_ = DeviceUiState{};
    arm_ui_epoch();

    auto loaded = load_backlight_state();
    if (!loaded) {
        shutdown_brightness_callbacks();
        return loaded;
    }
    subscribe_device_events();
    auto shown = surface_->show_page(context, detail::PAGE_HOME);
    if (!shown) {
        shutdown_brightness_callbacks();
        return shown;
    }
    auto header = refresh_header(context);
    if (!header) {
        shutdown_brightness_callbacks();
        return header;
    }
    if (auto handle = current_handle(); handle != nullptr) {
        (void)esp_gsp_set_swipe_enabled(handle, false);
    }
    BROOKESIA_LOGI("Settings GSP pages ready");
    return {};
}

std::expected<void, std::string> SettingsApp::handle_gsp_action(
    system::core::AppContext &context,
    std::string_view action
)
{
    if (action.starts_with("scene:")) {
        return {};
    }
    unsigned scene = 0;
    unsigned id = 0;
    unsigned arg = 0;
    const std::string text(action);
    if (std::sscanf(text.c_str(), "%u:%u:%u", &scene, &id, &arg) != 3) {
        return std::unexpected("Unrecognized GSP action");
    }
    if ((scene == GSP_SETTINGS_SCENE_SETTINGS_HOME) && (id == GSP_SETTINGS_HOME_ACT_ID_HEADER_EXIT)) {
        return context.system_service().stop_app(context.app_id());
    }
    if ((scene == GSP_SETTINGS_SCENE_SETTINGS_HOME) && (id == GSP_SETTINGS_HOME_ACT_ID_OPEN_DISPLAY)) {
        current_page_ = detail::PAGE_DISPLAY;
        auto loaded = load_backlight_state();
        if (!loaded) {
            return loaded;
        }
        auto shown = surface_->show_page(context, detail::PAGE_DISPLAY);
        if (!shown) {
            return shown;
        }
        auto header = refresh_header(context);
        if (!header) {
            return header;
        }
        return refresh_display_state(context);
    }
    if ((scene == GSP_SETTINGS_SCENE_SETTINGS_DISPLAY) && (id == GSP_SETTINGS_DISPLAY_ACT_ID_HEADER_BACK)) {
        current_page_ = detail::PAGE_HOME;
        auto shown = surface_->show_page(context, detail::PAGE_HOME);
        if (!shown) {
            return shown;
        }
        return refresh_header(context);
    }
    if ((scene == GSP_SETTINGS_SCENE_SETTINGS_DISPLAY) && (id == GSP_SETTINGS_DISPLAY_ACT_ID_BRIGHTNESS)) {
        set_brightness(context, detail::clamp_percent(static_cast<double>(arg)));
        return {};
    }
    BROOKESIA_LOGI("Ignore unsupported Settings GSP action %1%", id);
    return {};
}

#else

std::span<const std::byte> SettingsApp::gui_bundle() const
{
    return {};
}

const void *SettingsApp::gui_runtime_config() const
{
    return nullptr;
}

std::expected<void, std::string> SettingsApp::start_gsp_pages(system::core::AppContext &)
{
    return std::unexpected("GSP backend is not enabled");
}

std::expected<void, std::string> SettingsApp::handle_gsp_action(system::core::AppContext &, std::string_view)
{
    return std::unexpected("GSP backend is not enabled");
}

#endif

} // namespace esp_brookesia::app::settings
