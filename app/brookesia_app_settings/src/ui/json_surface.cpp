/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string>
#include <vector>

#include "private/settings_app_internal.hpp"

namespace esp_brookesia::app::settings {

namespace {

class JsonSettingsSurface: public SettingsSurface {
public:
    std::expected<void, std::string> set_wifi(
        system::core::AppContext &, const WifiPageView &
    ) override
    {
        return std::unexpected("JSON Wi-Fi uses its screen bindings");
    }

    std::expected<void, std::string> set_wifi_connect(
        system::core::AppContext &context, const WifiConnectView &view
    ) override
    {
        std::vector<gui::BindingValueUpdate> updates;
        detail::add_binding_update(updates, detail::WIFI_CONNECT_SSID_PATH, "ssid", view.ssid);
        detail::add_binding_update(updates, detail::WIFI_CONNECT_DETAIL_PATH, "detail", view.detail);
        detail::add_binding_update(
            updates, detail::WIFI_CONNECT_PASSWORD_VALUE_PATH, "passwordValue", view.password_summary
        );
        detail::add_binding_update(updates, detail::WIFI_CONNECT_STATUS_PATH, "status", view.status);
        detail::add_binding_update(
            updates, detail::WIFI_CONNECT_BUTTON_PATH, "disabled", view.connect_enabled ? "false" : "true"
        );
        detail::add_binding_update(
            updates, detail::WIFI_CONNECT_BUTTON_PATH, "opacity",
            std::to_string(view.connect_enabled ? detail::ENABLED_OPACITY : detail::DISABLED_OPACITY)
        );
        return context.gui().set_binding_values(updates);
    }

    std::expected<void, std::string> show_page(
        system::core::AppContext &context,
        std::string_view page
    ) override
    {
        std::string_view action;
        if (page == detail::PAGE_HOME) {
            action = detail::ACTION_OPEN_HOME;
        } else if (page == detail::PAGE_DISPLAY) {
            action = "settings.open.display";
        } else if (page == detail::PAGE_WIFI_CONNECT) {
            action = detail::ACTION_OPEN_WIFI_CONNECT;
        } else if (page == detail::PAGE_WIFI) {
            action = detail::ACTION_BACK_WIFI_CONNECT;
        } else {
            return std::unexpected("Unsupported settings page");
        }
        return context.gui().trigger_screen_flow(detail::CONTENT_FLOW_ID, action);
    }

    std::expected<void, std::string> set_header(
        system::core::AppContext &context,
        std::string_view title,
        bool show_back
    ) override
    {
        std::vector<gui::BindingValueUpdate> updates;
        detail::add_binding_update(
            updates, detail::HEADER_BACK_PATH, "commonProps.hidden", show_back ? "false" : "true"
        );
        detail::add_binding_update(updates, detail::HEADER_TITLE_PATH, "labelProps.text", std::string(title));
        return context.gui().set_binding_values(updates);
    }

    std::expected<void, std::string> set_brightness(
        system::core::AppContext &context,
        int value,
        bool enabled,
        std::string_view status,
        std::string_view caption
    ) override
    {
        std::vector<gui::BindingValueUpdate> updates;
        detail::add_control_state_updates(
            updates, detail::DISPLAY_BRIGHTNESS_ROW_PATH, detail::DISPLAY_BRIGHTNESS_SLIDER_PATH, enabled
        );
        detail::add_binding_update(
            updates, detail::DISPLAY_BRIGHTNESS_SLIDER_PATH, "value", std::to_string(value)
        );
        detail::add_binding_update(
            updates,
            "/display/page/brightness_caption",
            "labelProps.text",
            std::string(enabled ? caption : status)
        );
        return context.gui().set_binding_values(updates);
    }
};

} // namespace

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
std::unique_ptr<SettingsSurface> make_gsp_settings_surface();
#endif

std::unique_ptr<SettingsSurface> make_settings_surface()
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    return make_gsp_settings_surface();
#else
    return std::make_unique<JsonSettingsSurface>();
#endif
}

} // namespace esp_brookesia::app::settings
