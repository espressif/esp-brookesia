/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ui/gsp_appearance.hpp"

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP

#include <initializer_list>
#include "settings_gsp.h"

namespace esp_brookesia::app::settings::detail {
std::string localized_text(std::string_view locale, std::string_view key);
}

namespace esp_brookesia::app::settings {

namespace {

struct Label {
    uint32_t object;
    std::string_view key;
};

std::expected<void, std::string> apply_labels(
    esp_gsp_handle_t handle, std::string_view locale, std::initializer_list<Label> labels
)
{
    for (const auto &label : labels) {
        const auto text = detail::localized_text(locale, label.key);
        if (esp_gsp_component_set_text(handle, label.object, text.c_str()) != ESP_GSP_OK) {
            return std::unexpected("Failed to localize Settings label: " + std::string(label.key));
        }
    }
    return {};
}

} // namespace

std::expected<void, std::string> apply_settings_appearance(
    esp_gsp_handle_t handle, uint16_t scene, std::string_view locale, std::string_view theme
)
{
    if (handle == nullptr) {
        return std::unexpected("GSP Settings appearance requires a running session");
    }
    (void)theme;
    if (locale.empty()) {
        locale = "en";
    }
    switch (scene) {
    case GSP_SETTINGS_SCENE_SETTINGS_HOME:
        return apply_labels(handle, locale, {
            {GSP_SETTINGS_HOME_OBJ_KEY_HEADER_TITLE, "settings_home"},
            {GSP_SETTINGS_HOME_OBJ_KEY_HEADER_EXIT, "gsp.exit"},
            {GSP_SETTINGS_HOME_OBJ_KEY_NAME_DEVICE, "my_device"},
            {GSP_SETTINGS_HOME_OBJ_KEY_NAME_WIFI, "wifi"},
            {GSP_SETTINGS_HOME_OBJ_KEY_NAME_DISPLAY, "gsp.display"},
            {GSP_SETTINGS_HOME_OBJ_KEY_NAME_SOUND, "sound"},
            {GSP_SETTINGS_HOME_OBJ_KEY_NAME_LANGUAGE, "language"},
            {GSP_SETTINGS_HOME_OBJ_KEY_NAME_THEME, "gsp.theme"},
            {GSP_SETTINGS_HOME_OBJ_KEY_NAME_TIME_ZONE, "time_zone"},
        });
    case GSP_SETTINGS_SCENE_SETTINGS_MY_DEVICE:
        return apply_labels(handle, locale, {
            {GSP_SETTINGS_MY_DEVICE_OBJ_KEY_HEADER_BACK, "gsp.back"},
            {GSP_SETTINGS_MY_DEVICE_OBJ_KEY_HEADER_TITLE, "my_device"},
            {GSP_SETTINGS_MY_DEVICE_OBJ_KEY_DEVICE_LABEL, "gsp.device_name"},
            {GSP_SETTINGS_MY_DEVICE_OBJ_KEY_PREVIOUS, "gsp.previous"},
            {GSP_SETTINGS_MY_DEVICE_OBJ_KEY_NEXT, "gsp.next"},
        });
    case GSP_SETTINGS_SCENE_SETTINGS_DISPLAY:
        return apply_labels(handle, locale, {
            {GSP_SETTINGS_DISPLAY_OBJ_KEY_HEADER_BACK, "gsp.back"},
            {GSP_SETTINGS_DISPLAY_OBJ_KEY_HEADER_TITLE, "display"},
            {GSP_SETTINGS_DISPLAY_OBJ_KEY_THEME_CAPTION, "gsp.theme"},
            {GSP_SETTINGS_DISPLAY_OBJ_KEY_THEME_NAME, "gsp.theme"},
            {GSP_SETTINGS_DISPLAY_OBJ_KEY_BRIGHTNESS_CAPTION, "gsp.brightness_level"},
            {GSP_SETTINGS_DISPLAY_OBJ_KEY_SCREEN_BRIGHTNESS, "gsp.screen_brightness"},
        });
    case GSP_SETTINGS_SCENE_SETTINGS_WIFI:
        return apply_labels(handle, locale, {
            {GSP_SETTINGS_WIFI_OBJ_KEY_HEADER_BACK, "gsp.back"},
            {GSP_SETTINGS_WIFI_OBJ_KEY_HEADER_TITLE, "wifi"},
            {GSP_SETTINGS_WIFI_OBJ_KEY_REFRESH, "wifi_refresh"},
            {GSP_SETTINGS_WIFI_OBJ_KEY_PREVIOUS, "gsp.previous"},
            {GSP_SETTINGS_WIFI_OBJ_KEY_NEXT, "gsp.next"},
            {GSP_SETTINGS_WIFI_OBJ_KEY_SAVED_NETWORKS, "wifi.saved_list"},
            {GSP_SETTINGS_WIFI_OBJ_KEY_NEARBY_NETWORKS, "wifi.nearby_list"},
            {GSP_SETTINGS_WIFI_OBJ_KEY_FORGET, "gsp.forget"},
            {GSP_SETTINGS_WIFI_OBJ_KEY_DISCONNECT, "gsp.disconnect"},
        });
    case GSP_SETTINGS_SCENE_SETTINGS_WIFI_CONNECT:
        return apply_labels(handle, locale, {
            {GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_HEADER_BACK, "gsp.back"},
            {GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_HEADER_TITLE, "wifi_connect"},
            {GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_PASSWORD_CAPTION, "gsp.network_password"},
            {GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_EDIT_PASSWORD, "gsp.edit_password"},
            {GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_CONNECT, "gsp.connect"},
            {GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_HINT, "gsp.password_hint"},
        });
    case GSP_SETTINGS_SCENE_SETTINGS_SOUND:
        return apply_labels(handle, locale, {
            {GSP_SETTINGS_SOUND_OBJ_KEY_HEADER_BACK, "gsp.back"},
            {GSP_SETTINGS_SOUND_OBJ_KEY_HEADER_TITLE, "sound"},
            {GSP_SETTINGS_SOUND_OBJ_KEY_VOLUME_CAPTION, "gsp.volume"},
            {GSP_SETTINGS_SOUND_OBJ_KEY_MUTE_LABEL, "gsp.mute"},
            {GSP_SETTINGS_SOUND_OBJ_KEY_TEST_SOUND, "gsp.test_sound"},
        });
    case GSP_SETTINGS_SCENE_SETTINGS_LANGUAGE:
        return apply_labels(handle, locale, {
            {GSP_SETTINGS_LANGUAGE_OBJ_KEY_HEADER_BACK, "gsp.back"},
            {GSP_SETTINGS_LANGUAGE_OBJ_KEY_HEADER_TITLE, "language"},
            {GSP_SETTINGS_LANGUAGE_OBJ_KEY_OPTIONS_CAPTION, "language"},
            {GSP_SETTINGS_LANGUAGE_OBJ_KEY_OPTION_0, "language.display.en"},
            {GSP_SETTINGS_LANGUAGE_OBJ_KEY_OPTION_1, "language.display.zh_CN"},
            {GSP_SETTINGS_LANGUAGE_OBJ_KEY_RESTART_NOW, "restart_now"},
            {GSP_SETTINGS_LANGUAGE_OBJ_KEY_LATER, "later"},
        });
    case GSP_SETTINGS_SCENE_SETTINGS_THEME:
        return apply_labels(handle, locale, {
            {GSP_SETTINGS_THEME_OBJ_KEY_HEADER_BACK, "gsp.back"},
            {GSP_SETTINGS_THEME_OBJ_KEY_HEADER_TITLE, "gsp.theme"},
            {GSP_SETTINGS_THEME_OBJ_KEY_OPTIONS_CAPTION, "gsp.theme"},
            {GSP_SETTINGS_THEME_OBJ_KEY_OPTION_0, "gsp.light_mode"},
            {GSP_SETTINGS_THEME_OBJ_KEY_OPTION_1, "gsp.dark_mode"},
            {GSP_SETTINGS_THEME_OBJ_KEY_RESTART_NOW, "restart_now"},
            {GSP_SETTINGS_THEME_OBJ_KEY_LATER, "later"},
        });
    case GSP_SETTINGS_SCENE_SETTINGS_TIME_ZONE:
        return apply_labels(handle, locale, {
            {GSP_SETTINGS_TIME_ZONE_OBJ_KEY_HEADER_BACK, "gsp.back"},
            {GSP_SETTINGS_TIME_ZONE_OBJ_KEY_HEADER_TITLE, "time_zone"},
            {GSP_SETTINGS_TIME_ZONE_OBJ_KEY_CURRENT_CAPTION, "gsp.current_time_zone"},
            {GSP_SETTINGS_TIME_ZONE_OBJ_KEY_STATE_CAPTION, "gsp.sntp_state"},
            {GSP_SETTINGS_TIME_ZONE_OBJ_KEY_OPTIONS_CAPTION, "gsp.common_time_zones"},
            {GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_STATUS, "gsp.time_zone_hint"},
        });
    default:
        return std::unexpected("Unsupported Settings GSP scene");
    }
}

} // namespace esp_brookesia::app::settings

#endif
