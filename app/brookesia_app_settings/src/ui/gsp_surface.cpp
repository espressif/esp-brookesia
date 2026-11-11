/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <chrono>
#include <cstdio>
#include <string>

#include "private/settings_app_internal.hpp"

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
#   define GSP_BUNDLE_ENABLE_RAW_IDS 1
#   include "settings_gsp.h"
#   include "brookesia/gui_gsp/app_host.hpp"
#   include "esp_gsp.h"
#   include "ui/gsp_appearance.hpp"
#endif

namespace esp_brookesia::app::settings {

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP

namespace {

uint16_t scene_for_page(std::string_view page)
{
    if (page == detail::PAGE_HOME) {
        return GSP_SETTINGS_SCENE_SETTINGS_HOME;
    }
    if (page == detail::PAGE_DEVICE) {
        return GSP_SETTINGS_SCENE_SETTINGS_MY_DEVICE;
    }
    if (page == detail::PAGE_DISPLAY) {
        return GSP_SETTINGS_SCENE_SETTINGS_DISPLAY;
    }
    if (page == detail::PAGE_WIFI) {
        return GSP_SETTINGS_SCENE_SETTINGS_WIFI;
    }
    if (page == detail::PAGE_WIFI_CONNECT) {
        return GSP_SETTINGS_SCENE_SETTINGS_WIFI_CONNECT;
    }
    if (page == detail::PAGE_SOUND) {
        return GSP_SETTINGS_SCENE_SETTINGS_SOUND;
    }
    if (page == detail::PAGE_LANGUAGE) {
        return GSP_SETTINGS_SCENE_SETTINGS_LANGUAGE;
    }
    if (page == detail::PAGE_THEME) {
        return GSP_SETTINGS_SCENE_SETTINGS_THEME;
    }
    if (page == detail::PAGE_TIME_ZONE) {
        return GSP_SETTINGS_SCENE_SETTINGS_TIME_ZONE;
    }
    return UINT16_MAX;
}

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
    void set_appearance(std::string_view locale, std::string_view theme) override
    {
        locale_ = locale;
        theme_ = theme;
    }

    void set_suspended(bool suspended) override
    {
        suspended_ = suspended;
    }

    bool suspended() const override
    {
        return suspended_;
    }

    std::expected<void, std::string> set_home_summary(
        system::core::AppContext &, const HomeSummaryView &view
    ) override
    {
        if (suspended_ || page_ != detail::PAGE_HOME) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        const auto safe_text = [](const std::string & text) {
            return text.empty() ? " " : text.c_str();
        };
        if (esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_HOME_OBJ_KEY_VALUE_DEVICE, safe_text(view.device)
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_HOME_OBJ_KEY_VALUE_WIFI, safe_text(view.wifi)
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_HOME_OBJ_KEY_VALUE_DISPLAY, safe_text(view.display)
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_HOME_OBJ_KEY_VALUE_SOUND, safe_text(view.sound)
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_HOME_OBJ_KEY_VALUE_LANGUAGE, safe_text(view.language)
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_HOME_OBJ_KEY_VALUE_THEME, safe_text(view.theme)
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_HOME_OBJ_KEY_VALUE_TIME_ZONE, safe_text(view.time_zone)
                ) != ESP_GSP_OK) {
            return std::unexpected("Failed to update Settings home summaries");
        }
        return {};
    }

    std::expected<void, std::string> set_my_device(
        system::core::AppContext &, const MyDevicePageView &view
    ) override
    {
        if (suspended_ || page_ != detail::PAGE_DEVICE) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        if (esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_MY_DEVICE_OBJ_KEY_OS_NAME, view.system_name.c_str()
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_MY_DEVICE_OBJ_KEY_OS_VERSION, view.system_version.c_str()
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_MY_DEVICE_OBJ_KEY_DEVICE_VALUE, view.device_name.c_str()
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_MY_DEVICE_OBJ_KEY_PAGINATION, view.pagination.c_str()
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(
                    handle, GSP_SETTINGS_MY_DEVICE_OBJ_KEY_PREVIOUS, view.previous_enabled
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(
                    handle, GSP_SETTINGS_MY_DEVICE_OBJ_KEY_NEXT, view.next_enabled
                ) != ESP_GSP_OK) {
            return std::unexpected("Failed to update My Device summary controls");
        }

        static constexpr std::array<uint32_t, MY_DEVICE_GROUP_SLOTS> CARD_KEYS = {
            GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_CARD_0,
            GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_CARD_1,
            GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_CARD_2,
            GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_CARD_3,
        };
        static constexpr std::array<uint32_t, MY_DEVICE_GROUP_SLOTS> TITLE_KEYS = {
            GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_TITLE_0,
            GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_TITLE_1,
            GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_TITLE_2,
            GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_TITLE_3,
        };
        static constexpr std::array<std::array<uint32_t, MY_DEVICE_GROUP_LINES>, MY_DEVICE_GROUP_SLOTS> LINE_KEYS = {{
                {
                    GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_LINE_0_0,
                    GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_LINE_0_1,
                    GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_LINE_0_2,
                },
                {
                    GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_LINE_1_0,
                    GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_LINE_1_1,
                    GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_LINE_1_2,
                },
                {
                    GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_LINE_2_0,
                    GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_LINE_2_1,
                    GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_LINE_2_2,
                },
                {
                    GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_LINE_3_0,
                    GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_LINE_3_1,
                    GSP_SETTINGS_MY_DEVICE_OBJ_KEY_GROUP_LINE_3_2,
                },
            }
        };

        for (size_t slot = 0; slot < MY_DEVICE_GROUP_SLOTS; ++slot) {
            const auto &group = view.groups[slot];
            const char *title = (group.visible && !group.title.empty()) ? group.title.c_str() : " ";
            if (esp_gsp_component_set_visible(handle, CARD_KEYS[slot], group.visible) != ESP_GSP_OK ||
                    esp_gsp_component_set_text(handle, TITLE_KEYS[slot], title) != ESP_GSP_OK ||
                    esp_gsp_component_set_visible(handle, TITLE_KEYS[slot], group.visible) != ESP_GSP_OK) {
                return std::unexpected("Failed to update My Device hardware group card");
            }
            for (size_t line = 0; line < MY_DEVICE_GROUP_LINES; ++line) {
                const bool line_visible = group.visible && (line < group.line_count) && !group.lines[line].empty();
                const char *line_text = line_visible ? group.lines[line].c_str() : " ";
                if (esp_gsp_component_set_text(handle, LINE_KEYS[slot][line], line_text) != ESP_GSP_OK ||
                        esp_gsp_component_set_visible(handle, LINE_KEYS[slot][line], line_visible) != ESP_GSP_OK) {
                    return std::unexpected("Failed to update My Device hardware group line");
                }
            }
        }
        return {};
    }

    std::expected<void, std::string> set_time_zone(
        system::core::AppContext &, const TimeZonePageView &view
    ) override
    {
        if (suspended_ || page_ != detail::PAGE_TIME_ZONE) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        if (esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_TIME_ZONE_OBJ_KEY_CURRENT_VALUE, view.current_timezone.c_str()
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_TIME_ZONE_OBJ_KEY_STATE_VALUE, view.sntp_state.c_str()
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_STATUS, view.status.c_str()
                ) != ESP_GSP_OK) {
            return std::unexpected("Failed to update Time zone status card");
        }
        static constexpr std::array<uint32_t, TIME_ZONE_OPTION_COUNT> NAME_KEYS = {
            GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_NAME_0,
            GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_NAME_1,
            GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_NAME_2,
            GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_NAME_3,
            GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_NAME_4,
            GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_NAME_5,
        };
        static constexpr std::array<uint32_t, TIME_ZONE_OPTION_COUNT> VALUE_KEYS = {
            GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_VALUE_0,
            GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_VALUE_1,
            GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_VALUE_2,
            GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_VALUE_3,
            GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_VALUE_4,
            GSP_SETTINGS_TIME_ZONE_OBJ_KEY_TZ_VALUE_5,
        };
        for (size_t i = 0; i < TIME_ZONE_OPTION_COUNT; ++i) {
            if (esp_gsp_component_set_text(handle, NAME_KEYS[i], view.labels[i].c_str()) != ESP_GSP_OK ||
                    esp_gsp_component_set_text(handle, VALUE_KEYS[i], view.values[i].c_str()) != ESP_GSP_OK) {
                return std::unexpected("Failed to update Time zone option");
            }
        }
        return {};
    }

    std::expected<void, std::string> set_sound(
        system::core::AppContext &, const SoundPageView &view
    ) override
    {
        if (suspended_ || page_ != detail::PAGE_SOUND) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        const auto level = view.enabled ? std::to_string(view.volume) + "%" : "--";
        const auto mute = detail::localized_text(locale_, view.muted ? "gsp.muted" : "gsp.unmuted");
        if (esp_gsp_component_set_value(handle, GSP_SETTINGS_SOUND_OBJ_KEY_VOLUME_SLIDER, view.volume) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(handle, GSP_SETTINGS_SOUND_OBJ_KEY_VOLUME_SLIDER, view.enabled) != ESP_GSP_OK ||
                esp_gsp_component_set_text(handle, GSP_SETTINGS_SOUND_OBJ_KEY_VOLUME_VALUE, level.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_text(handle, GSP_SETTINGS_SOUND_OBJ_KEY_MUTE_TOGGLE, mute.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(handle, GSP_SETTINGS_SOUND_OBJ_KEY_MUTE_TOGGLE, view.enabled) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(handle, GSP_SETTINGS_SOUND_OBJ_KEY_TEST_SOUND, view.test_enabled) != ESP_GSP_OK ||
                esp_gsp_component_set_text(handle, GSP_SETTINGS_SOUND_OBJ_KEY_SOUND_STATUS, view.status.c_str()) != ESP_GSP_OK) {
            return std::unexpected("Failed to update sound controls");
        }
        return {};
    }

    std::expected<void, std::string> set_preferences(
        system::core::AppContext &, const PreferencePageView &view
    ) override
    {
        if (suspended_ || (page_ != detail::PAGE_LANGUAGE && page_ != detail::PAGE_THEME)) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        const bool language = page_ == detail::PAGE_LANGUAGE;
        const std::array<uint32_t, 2> options = language ?
                                                std::array<uint32_t, 2> {GSP_SETTINGS_LANGUAGE_OBJ_KEY_OPTION_0, GSP_SETTINGS_LANGUAGE_OBJ_KEY_OPTION_1} :
                                                std::array<uint32_t, 2> {GSP_SETTINGS_THEME_OBJ_KEY_OPTION_0, GSP_SETTINGS_THEME_OBJ_KEY_OPTION_1};
        const std::array<uint32_t, 2> values = language ?
                                               std::array<uint32_t, 2> {GSP_SETTINGS_LANGUAGE_OBJ_KEY_VALUE_0, GSP_SETTINGS_LANGUAGE_OBJ_KEY_VALUE_1} :
                                               std::array<uint32_t, 2> {GSP_SETTINGS_THEME_OBJ_KEY_VALUE_0, GSP_SETTINGS_THEME_OBJ_KEY_VALUE_1};
        for (size_t index = 0; index < options.size(); ++index) {
            const char *value_str = view.values[index].empty() ? " " : view.values[index].c_str();
            if (esp_gsp_component_set_text(handle, options[index], view.labels[index].c_str()) != ESP_GSP_OK ||
                    esp_gsp_component_set_text(handle, values[index], value_str) != ESP_GSP_OK) {
                BROOKESIA_LOGW("Failed to update preference choice %1%", index);
            }
        }
        const auto status = language ? GSP_SETTINGS_LANGUAGE_OBJ_KEY_STATUS : GSP_SETTINGS_THEME_OBJ_KEY_STATUS;
        const auto restart = language ? GSP_SETTINGS_LANGUAGE_OBJ_KEY_RESTART_NOW : GSP_SETTINGS_THEME_OBJ_KEY_RESTART_NOW;
        const auto later = language ? GSP_SETTINGS_LANGUAGE_OBJ_KEY_LATER : GSP_SETTINGS_THEME_OBJ_KEY_LATER;
        const char *status_str = view.status.empty() ? " " : view.status.c_str();
        if (esp_gsp_component_set_text(handle, status, status_str) != ESP_GSP_OK ||
                esp_gsp_component_set_text(
                    handle, restart, detail::localized_text(locale_, "restart_now").c_str()
                ) != ESP_GSP_OK ||
                esp_gsp_component_set_text(
                    handle, later, detail::localized_text(locale_, "later").c_str()
                ) != ESP_GSP_OK) {
            BROOKESIA_LOGW("Failed to update preference text");
        }
        if (esp_gsp_component_set_visible(handle, restart, view.show_restart) != ESP_GSP_OK ||
                esp_gsp_component_set_visible(handle, later, view.show_restart) != ESP_GSP_OK) {
            return std::unexpected("Failed to update preference confirmation");
        }
        return {};
    }

    std::expected<void, std::string> set_wifi(
        system::core::AppContext &, const WifiPageView &view
    ) override
    {
        if (suspended_ || page_ != detail::PAGE_WIFI) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        const auto set_text = [handle](uint32_t key, const std::string & text) {
            return esp_gsp_component_set_text(handle, key, text.c_str()) == ESP_GSP_OK;
        };
        const auto set_enabled = [handle](uint32_t key, bool enabled) {
            return esp_gsp_component_set_enabled(handle, key, enabled) == ESP_GSP_OK;
        };
        if (!set_text(GSP_SETTINGS_WIFI_OBJ_KEY_WIFI_STATUS, view.status) ||
                !set_text(GSP_SETTINGS_WIFI_OBJ_KEY_CONNECTION_STATUS, view.connection) ||
                !set_text(GSP_SETTINGS_WIFI_OBJ_KEY_PAGINATION, view.pagination) ||
                !set_text(GSP_SETTINGS_WIFI_OBJ_KEY_CONNECTION_NOTICE, view.selection_notice) ||
                !set_text(GSP_SETTINGS_WIFI_OBJ_KEY_SAVED_NETWORKS,
                          detail::localized_text(locale_, "wifi.saved_list")) ||
                !set_text(GSP_SETTINGS_WIFI_OBJ_KEY_NEARBY_NETWORKS,
                          detail::localized_text(locale_, "wifi.nearby_list")) ||
                !set_enabled(GSP_SETTINGS_WIFI_OBJ_KEY_SAVED_NETWORKS, view.list_switch_enabled && !view.saved_list) ||
                !set_enabled(GSP_SETTINGS_WIFI_OBJ_KEY_NEARBY_NETWORKS, view.list_switch_enabled && view.saved_list) ||
                !set_text(GSP_SETTINGS_WIFI_OBJ_KEY_WIFI_TOGGLE, detail::localized_text(locale_, view.enabled ? "gsp.wifi_on" : "gsp.wifi_off")) ||
                !set_enabled(GSP_SETTINGS_WIFI_OBJ_KEY_WIFI_TOGGLE, view.toggle_enabled) ||
                !set_enabled(GSP_SETTINGS_WIFI_OBJ_KEY_REFRESH, view.refresh_enabled) ||
                !set_enabled(GSP_SETTINGS_WIFI_OBJ_KEY_FORGET, view.forget_enabled) ||
                !set_enabled(GSP_SETTINGS_WIFI_OBJ_KEY_DISCONNECT, view.disconnect_enabled) ||
                !set_enabled(GSP_SETTINGS_WIFI_OBJ_KEY_PREVIOUS, view.previous_enabled) ||
                !set_enabled(GSP_SETTINGS_WIFI_OBJ_KEY_NEXT, view.next_enabled)) {
            return std::unexpected("Failed to update Wi-Fi controls");
        }
        static constexpr std::array<uint32_t, WIFI_PAGE_ROW_COUNT> ssid_keys = {
            GSP_SETTINGS_WIFI_OBJ_KEY_SSID_0, GSP_SETTINGS_WIFI_OBJ_KEY_SSID_1,
            GSP_SETTINGS_WIFI_OBJ_KEY_SSID_2, GSP_SETTINGS_WIFI_OBJ_KEY_SSID_3, GSP_SETTINGS_WIFI_OBJ_KEY_SSID_4,
        };
        static constexpr std::array<uint32_t, WIFI_PAGE_ROW_COUNT> detail_keys = {
            GSP_SETTINGS_WIFI_OBJ_KEY_DETAIL_0, GSP_SETTINGS_WIFI_OBJ_KEY_DETAIL_1,
            GSP_SETTINGS_WIFI_OBJ_KEY_DETAIL_2, GSP_SETTINGS_WIFI_OBJ_KEY_DETAIL_3, GSP_SETTINGS_WIFI_OBJ_KEY_DETAIL_4,
        };
        static constexpr std::array<uint32_t, WIFI_PAGE_ROW_COUNT> row_keys = {
            GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_0, GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_1,
            GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_2, GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_3,
            GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_4,
        };
        static constexpr std::array<uint32_t, WIFI_PAGE_ROW_COUNT> row_visibility_keys = {
            GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_0_GROUP, GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_1_GROUP,
            GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_2_GROUP, GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_3_GROUP,
            GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_4_GROUP,
        };
        for (size_t row = 0; row < WIFI_PAGE_ROW_COUNT; ++row) {
            if (!set_text(ssid_keys[row], view.rows[row].ssid) ||
                    !set_text(detail_keys[row], view.rows[row].detail)) {
                return std::unexpected("Failed to update Wi-Fi network labels: " + std::to_string(row));
            }
            if (esp_gsp_component_set_visible(
                        handle, row_visibility_keys[row], !view.rows[row].ssid.empty()
                    ) != ESP_GSP_OK) {
                return std::unexpected("Failed to update Wi-Fi network row visibility");
            }
            const auto enabled = esp_gsp_component_set_enabled(handle, row_keys[row], view.rows[row].enabled);
            if (enabled != ESP_GSP_OK) {
                return std::unexpected("Failed to enable Wi-Fi network row " + std::to_string(row) +
                                       ": " + std::to_string(static_cast<int>(enabled)));
            }
        }
        return {};
    }

    std::expected<void, std::string> set_wifi_connect(
        system::core::AppContext &, const WifiConnectView &view
    ) override
    {
        wifi_connect_view_ = view;
        if (suspended_ || page_ != detail::PAGE_WIFI_CONNECT) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        if (esp_gsp_component_set_text(handle, GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_SSID,
                                       view.ssid.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_text(handle, GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_DETAIL,
                                           view.detail.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_text(handle, GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_PASSWORD_VALUE,
                                           view.password_summary.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_text(handle, GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_STATUS,
                                           view.status.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(handle, GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_EDIT_PASSWORD,
                                              view.password_enabled) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(handle, GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_CONNECT,
                                              view.connect_enabled) != ESP_GSP_OK) {
            return std::unexpected("Failed to update the Wi-Fi connection page");
        }
        return {};
    }

    std::expected<void, std::string> show_page(
        system::core::AppContext &,
        std::string_view page
    ) override
    {
        page_ = page;
        if (suspended_) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        const auto scene = scene_for_page(page);
        if (scene == UINT16_MAX) {
            return std::unexpected("Unsupported settings page");
        }
        const auto settle_started_at = std::chrono::steady_clock::now();
        if (esp_gsp_goto_scene(handle, scene, ESP_GSP_NO_TRANSITION) != ESP_GSP_OK) {
            return std::unexpected("Failed to show the Settings page");
        }
        const auto flushed = esp_gsp_flush(handle, 500);
        const auto settle_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - settle_started_at
                               ).count();
        BROOKESIA_LOGI(
            "Settings page settle elapsed_ms(%1%) scene(%2%) flush(%3%)",
            settle_ms, static_cast<unsigned>(scene), static_cast<int>(flushed)
        );
        if (flushed != ESP_GSP_OK) {
            BROOKESIA_LOGW("Failed to settle the Settings page: %1%", static_cast<int>(flushed));
            return {};
        }
        auto appearance = apply_settings_appearance(handle, scene, locale_, theme_);
        if (!appearance) {
            BROOKESIA_LOGW("Failed to update Settings page labels: %1%", appearance.error());
        }
        return {};
    }

    std::expected<void, std::string> set_header(
        system::core::AppContext &,
        std::string_view title,
        bool show_back
    ) override
    {
        if (suspended_) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        (void)show_back;
        const auto title_key = page_ == detail::PAGE_DEVICE ? GSP_SETTINGS_MY_DEVICE_OBJ_KEY_HEADER_TITLE :
                               page_ == detail::PAGE_WIFI_CONNECT ? GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_HEADER_TITLE :
                               page_ == detail::PAGE_WIFI ? GSP_SETTINGS_WIFI_OBJ_KEY_HEADER_TITLE :
                               page_ == detail::PAGE_SOUND ? GSP_SETTINGS_SOUND_OBJ_KEY_HEADER_TITLE :
                               page_ == detail::PAGE_LANGUAGE ? GSP_SETTINGS_LANGUAGE_OBJ_KEY_HEADER_TITLE :
                               page_ == detail::PAGE_THEME ? GSP_SETTINGS_THEME_OBJ_KEY_HEADER_TITLE :
                               page_ == detail::PAGE_TIME_ZONE ? GSP_SETTINGS_TIME_ZONE_OBJ_KEY_HEADER_TITLE :
                               page_ == detail::PAGE_DISPLAY ? GSP_SETTINGS_DISPLAY_OBJ_KEY_HEADER_TITLE :
                               GSP_SETTINGS_HOME_OBJ_KEY_HEADER_TITLE;
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
        if (suspended_ || page_ != detail::PAGE_DISPLAY) {
            return {};
        }
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

private:
    std::string page_;
    std::string locale_ = "en";
    std::string theme_ = "light";
    WifiConnectView wifi_connect_view_;
    bool suspended_ = false;
};

std::unique_ptr<SettingsSurface> make_gsp_settings_surface()
{
    return std::make_unique<GspSettingsSurface>();
}

esp_gsp_config_t settings_dark_config();

std::span<const std::byte> SettingsApp::gui_bundle() const
{
    const auto config = *static_cast<const esp_gsp_config_t *>(gui_runtime_config());
    return {reinterpret_cast<const std::byte *>(config.bundle), config.bundle_size};
}

const void *SettingsApp::gui_runtime_config() const
{
    static const auto light = gsp_settings_config();
    static const auto dark = []() {
        auto config = settings_dark_config();
        // Both palettes use the same font and object layout.
        config.ttf = light.ttf;
        config.ttf_size = light.ttf_size;
        return config;
    }();
    // App permissions are available after installation, before the first GUI load.
    const auto theme = impl_->installed_context != nullptr ?
                       impl_->installed_context->gui().get_theme_language().theme_id : current_theme_id_;
    return theme == detail::THEME_DARK ? &dark : &light;
}

std::expected<void, std::string> SettingsApp::refresh_gsp_home_summaries(system::core::AppContext &context)
{
    if (!settings_uses_gsp_backend() || current_page_ != detail::PAGE_HOME) {
        return {};
    }
    if (cached_device_name_.empty()) {
        (void)refresh_my_device_state(context);
    }
    bind_sntp_service();
    if (sntp_service_binding_.is_valid() && detail::SNTPHelper::is_running()) {
        if (auto tz = detail::SNTPHelper::call_function_sync<std::string>(
                          detail::SNTPHelper::FunctionId::GetTimezone,
                          service::helper::Timeout(detail::SNTP_SERVICE_TIMEOUT_MS)
                      ); tz) {
            time_zone_ui_state_.timezone = *tz;
        }
    }
    std::string wifi_summary = detail::localized_text(current_locale_, "wifi.off_short");
    bind_wifi_service();
    if (wifi_service_binding_.is_valid() && detail::WifiHelper::is_running()) {
        if (auto state = detail::WifiHelper::call_function_sync<std::string>(
                             detail::WifiHelper::FunctionId::GetGeneralState,
                             service::helper::Timeout(detail::WIFI_SERVICE_TIMEOUT_MS)
                         ); state) {
            if (*state == "Connected") {
                if (auto ssid = detail::WifiHelper::call_function_sync<std::string>(
                                    detail::WifiHelper::FunctionId::GetConnectedSsid,
                                    service::helper::Timeout(detail::WIFI_SERVICE_TIMEOUT_MS)
                                ); ssid && !ssid->empty()) {
                    wifi_summary = *ssid;
                } else {
                    wifi_summary = detail::localized_text(current_locale_, "wifi.connected");
                }
            } else if (detail::is_wifi_started_state(*state)) {
                wifi_summary = detail::localized_text(current_locale_, "wifi.not_connected");
            }
        }
    } else {
        wifi_summary = detail::make_localized_unavailable_text(current_locale_);
    }

    HomeSummaryView view;
    view.device = cached_device_name_.empty() ?
                  detail::make_localized_unavailable_text(current_locale_) :
                  cached_device_name_;
    view.wifi = std::move(wifi_summary);
    view.display = (device_ui_state_.display_service_ready && device_ui_state_.display_backlight_supported) ?
                   detail::make_percent_text(device_ui_state_.brightness) :
                   detail::make_localized_unavailable_text(current_locale_);
    view.sound = (device_ui_state_.audio_service_ready && device_ui_state_.audio_player_supported) ?
                 (device_ui_state_.muted ? detail::make_localized_muted_text(current_locale_) :
                  detail::make_percent_text(device_ui_state_.volume)) :
                 detail::make_localized_unavailable_text(current_locale_);
    view.language = detail::get_language_summary_name(current_locale_);
    view.theme = detail::localized_text(
                     current_locale_, current_theme_id_ == detail::THEME_DARK ? "dark" : "light"
                 );
    view.time_zone = time_zone_ui_state_.timezone.empty() ?
                     detail::make_localized_unavailable_text(current_locale_) :
                     detail::get_time_zone_summary_name(current_locale_, time_zone_ui_state_.timezone);
    return surface_->set_home_summary(context, view);
}

std::expected<void, std::string> SettingsApp::start_gsp_pages(system::core::AppContext &context)
{
    context_ = &context;
    const auto active = context.gui().get_theme_language();
    current_locale_ = detail::normalize_locale(active.language);
    current_theme_id_ = active.theme_id == detail::THEME_DARK ? detail::THEME_DARK : detail::THEME_LIGHT;
    surface_->set_suspended(false);
    surface_->set_appearance(current_locale_, current_theme_id_);
    current_page_ = detail::PAGE_HOME;
    my_device_page_ = 0;
    cached_device_name_.clear();
    cached_hardware_groups_.clear();
    pending_language_locale_.clear();
    if (const auto stored = context.gui().get_stored_language_preference(); stored && *stored != current_locale_ &&
            (*stored == detail::LOCALE_EN || *stored == detail::LOCALE_ZH_CN)) {
        pending_language_locale_ = *stored;
    }
    pending_theme_id_.clear();
    if (const auto stored = context.gui().get_stored_theme_preference(); stored && *stored != current_theme_id_ &&
            (*stored == detail::THEME_LIGHT || *stored == detail::THEME_DARK)) {
        pending_theme_id_ = *stored;
    }
    preference_error_.clear();
    restart_prompt_kind_ = RestartPromptKind::None;
    restart_in_progress_ = false;
    restart_iface_ = hal::acquire_first_interface<hal::system::GeneralIface>();
    device_ui_state_ = DeviceUiState{};
    load_backlight_state();
    auto audio = load_audio_state();
    if (!audio) {
        shutdown_brightness_callbacks();
        return audio;
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
    (void)refresh_gsp_home_summaries(context);
    if (auto handle = current_handle(); handle != nullptr) {
        (void)esp_gsp_set_swipe_enabled(handle, false);
    }
    auto sntp_timer = context.timer().start_periodic(detail::SNTP_UI_TIMER_NAME, detail::SNTP_UI_POLL_INTERVAL_MS);
    if (!sntp_timer) {
        shutdown_brightness_callbacks();
        return std::unexpected(sntp_timer.error());
    }
    sntp_ui_timer_id_ = *sntp_timer;
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
    if (scene != scene_for_page(current_page_)) {
        return {};
    }
    const auto open_page = [&](std::string_view page) -> std::expected<void, std::string> {
        if (page == detail::PAGE_THEME)
        {
            theme_from_display_ = current_page_ == detail::PAGE_DISPLAY;
        }
        if (page == detail::PAGE_DEVICE)
        {
            my_device_page_ = 0;
        }
        if (page == detail::PAGE_SOUND)
        {
            auto loaded = load_audio_state();
            if (!loaded) {
                return loaded;
            }
            if (!sound_ui_mailbox_) {
                disconnect_device_events();
                subscribe_device_events();
            }
        }
        auto shown = surface_->show_page(context, page);
        if (!shown)
        {
            return shown;
        }
        current_page_ = page;
        preference_error_.clear();
        restart_prompt_kind_ = RestartPromptKind::None;
        auto header = refresh_header(context);
        if (!header)
        {
            return header;
        }
        if (page == detail::PAGE_HOME)
        {
            return refresh_gsp_home_summaries(context);
        }
        if (page == detail::PAGE_DEVICE)
        {
            return refresh_my_device_state(context);
        }
        if (page == detail::PAGE_SOUND)
        {
            return refresh_sound_state(context);
        }
        if (page == detail::PAGE_LANGUAGE)
        {
            return refresh_language_state(context);
        }
        if (page == detail::PAGE_THEME)
        {
            return refresh_theme_state(context);
        }
        if (page == detail::PAGE_TIME_ZONE)
        {
            return refresh_time_zone_state(context);
        }
        if (page == detail::PAGE_DISPLAY)
        {
            return refresh_display_state(context);
        }
        return {};
    };
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_HOME) {
        if (id == GSP_SETTINGS_HOME_ACT_ID_OPEN_DEVICE) {
            return open_page(detail::PAGE_DEVICE);
        }
        if (id == GSP_SETTINGS_HOME_ACT_ID_OPEN_SOUND) {
            return open_page(detail::PAGE_SOUND);
        }
        if (id == GSP_SETTINGS_HOME_ACT_ID_OPEN_LANGUAGE) {
            return open_page(detail::PAGE_LANGUAGE);
        }
        if (id == GSP_SETTINGS_HOME_ACT_ID_OPEN_THEME) {
            return open_page(detail::PAGE_THEME);
        }
        if (id == GSP_SETTINGS_HOME_ACT_ID_OPEN_TIME_ZONE) {
            return open_page(detail::PAGE_TIME_ZONE);
        }
    }
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_MY_DEVICE) {
        if (id == GSP_SETTINGS_MY_DEVICE_ACT_ID_HEADER_BACK) {
            return open_page(detail::PAGE_HOME);
        }
        if (id == GSP_SETTINGS_MY_DEVICE_ACT_ID_PREVIOUS && my_device_page_ > 0) {
            --my_device_page_;
            return refresh_my_device_state(context);
        }
        if (id == GSP_SETTINGS_MY_DEVICE_ACT_ID_NEXT) {
            ++my_device_page_;
            return refresh_my_device_state(context);
        }
        return {};
    }
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_TIME_ZONE) {
        if (id == GSP_SETTINGS_TIME_ZONE_ACT_ID_HEADER_BACK) {
            return open_page(detail::PAGE_HOME);
        }
        static constexpr std::array<unsigned, TIME_ZONE_OPTION_COUNT> TZ_SELECT_ACTIONS = {
            GSP_SETTINGS_TIME_ZONE_ACT_ID_SELECT_0,
            GSP_SETTINGS_TIME_ZONE_ACT_ID_SELECT_1,
            GSP_SETTINGS_TIME_ZONE_ACT_ID_SELECT_2,
            GSP_SETTINGS_TIME_ZONE_ACT_ID_SELECT_3,
            GSP_SETTINGS_TIME_ZONE_ACT_ID_SELECT_4,
            GSP_SETTINGS_TIME_ZONE_ACT_ID_SELECT_5,
        };
        for (size_t i = 0; i < TZ_SELECT_ACTIONS.size() && i < detail::TIME_ZONE_OPTIONS.size(); ++i) {
            if (id == TZ_SELECT_ACTIONS[i]) {
                return set_time_zone(context, detail::TIME_ZONE_OPTIONS[i].timezone);
            }
        }
        return {};
    }
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_DISPLAY && id == GSP_SETTINGS_DISPLAY_ACT_ID_OPEN_THEME) {
        return open_page(detail::PAGE_THEME);
    }
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_SOUND) {
        if (id == GSP_SETTINGS_SOUND_ACT_ID_HEADER_BACK) {
            return open_page(detail::PAGE_HOME);
        }
        if (id == GSP_SETTINGS_SOUND_ACT_ID_VOLUME) {
            set_volume(context, detail::clamp_percent(arg));
        }
        if (id == GSP_SETTINGS_SOUND_ACT_ID_MUTE) {
            set_mute(context, !device_ui_state_.muted);
        }
        if (id == GSP_SETTINGS_SOUND_ACT_ID_TEST_SOUND) {
            return play_test_sound(context);
        }
        return {};
    }
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_LANGUAGE || scene == GSP_SETTINGS_SCENE_SETTINGS_THEME) {
        const bool language = scene == GSP_SETTINGS_SCENE_SETTINGS_LANGUAGE;
        const auto back = language ? GSP_SETTINGS_LANGUAGE_ACT_ID_HEADER_BACK : GSP_SETTINGS_THEME_ACT_ID_HEADER_BACK;
        const auto first = language ? GSP_SETTINGS_LANGUAGE_ACT_ID_SELECT_0 : GSP_SETTINGS_THEME_ACT_ID_SELECT_0;
        const auto second = language ? GSP_SETTINGS_LANGUAGE_ACT_ID_SELECT_1 : GSP_SETTINGS_THEME_ACT_ID_SELECT_1;
        const auto restart = language ? GSP_SETTINGS_LANGUAGE_ACT_ID_RESTART_NOW : GSP_SETTINGS_THEME_ACT_ID_RESTART_NOW;
        const auto later = language ? GSP_SETTINGS_LANGUAGE_ACT_ID_LATER : GSP_SETTINGS_THEME_ACT_ID_LATER;
        if (id == back) {
            return open_page(!language && theme_from_display_ ? detail::PAGE_DISPLAY : detail::PAGE_HOME);
        }
        if (id == first || id == second) {
            return language ? schedule_language_switch(context, id == first ? detail::LOCALE_EN : detail::LOCALE_ZH_CN) :
                   schedule_theme_switch(context, id == first ? detail::THEME_LIGHT : detail::THEME_DARK);
        }
        if (id == restart || id == later) {
            system::core::MessageDialogResult result;
            result.reason = system::core::MessageDialogCloseReason::Button;
            result.button_index = id == restart ? 0 : 1;
            handle_restart_dialog_result(context, restart_prompt_kind_, result);
        }
        return {};
    }
    if ((scene == GSP_SETTINGS_SCENE_SETTINGS_HOME) && (id == GSP_SETTINGS_HOME_ACT_ID_OPEN_WIFI)) {
        auto shown = surface_->show_page(context, detail::PAGE_WIFI);
        if (!shown) {
            return shown;
        }
        current_page_ = detail::PAGE_WIFI;
        return open_wifi_page(context);
    }
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_WIFI) {
        if (id == GSP_SETTINGS_WIFI_ACT_ID_HEADER_BACK) {
            close_wifi_page(context);
            return open_page(detail::PAGE_HOME);
        }
        if (id == GSP_SETTINGS_WIFI_ACT_ID_WIFI_TOGGLE) {
            return toggle_wifi_page(context);
        }
        if (id == GSP_SETTINGS_WIFI_ACT_ID_REFRESH) {
            return scan_wifi_page(context);
        }
        if (id == GSP_SETTINGS_WIFI_ACT_ID_SAVED_NETWORKS || id == GSP_SETTINGS_WIFI_ACT_ID_NEARBY_NETWORKS) {
            if (!wifi_ui_state_.service_ready || !wifi_ui_state_.enabled || wifi_ui_state_.scanning ||
                    wifi_ui_state_.connecting || wifi_page_.connect_pending || wifi_page_.toggle_target ||
                    wifi_page_.selection) {
                return {};
            }
            wifi_page_.saved_list = id == GSP_SETTINGS_WIFI_ACT_ID_SAVED_NETWORKS;
            wifi_page_.forget_ssid.clear();
            wifi_page_.error.clear();
            return render_wifi_page(context);
        }
        if (id == GSP_SETTINGS_WIFI_ACT_ID_FORGET) {
            return forget_wifi_page_network(context);
        }
        if (id == GSP_SETTINGS_WIFI_ACT_ID_DISCONNECT) {
            return disconnect_current_wifi(context);
        }
        static constexpr std::array<unsigned, WIFI_PAGE_ROW_COUNT> selection_actions = {
            GSP_SETTINGS_WIFI_ACT_ID_SELECT_0, GSP_SETTINGS_WIFI_ACT_ID_SELECT_1,
            GSP_SETTINGS_WIFI_ACT_ID_SELECT_2, GSP_SETTINGS_WIFI_ACT_ID_SELECT_3, GSP_SETTINGS_WIFI_ACT_ID_SELECT_4,
        };
        for (size_t row = 0; row < selection_actions.size(); ++row) {
            if (id == selection_actions[row]) {
                return select_wifi_page_network(context, row);
            }
        }
        auto &page = wifi_page_.saved_list ? saved_wifi_page_ : available_wifi_page_;
        if (id == GSP_SETTINGS_WIFI_ACT_ID_PREVIOUS && page > 0) {
            --page;
        } else if (id == GSP_SETTINGS_WIFI_ACT_ID_NEXT) {
            ++page;
        }
        return render_wifi_page(context);
    }
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_WIFI_CONNECT) {
        if (id == GSP_SETTINGS_WIFI_CONNECT_ACT_ID_HEADER_BACK) {
            return back_wifi_connect(context);
        }
        if (id == GSP_SETTINGS_WIFI_CONNECT_ACT_ID_EDIT_PASSWORD) {
            auto shown = show_wifi_password_keyboard(context);
            if (!shown) {
                BROOKESIA_LOGW("Failed to show Settings Wi-Fi password keyboard: %1%", shown.error());
                wifi_page_.error = detail::localized_text(current_locale_, "wifi.keyboard_unavailable");
                return refresh_wifi_connect_state(context);
            }
            return {};
        }
        if (id == GSP_SETTINGS_WIFI_CONNECT_ACT_ID_CONNECT) {
            return submit_wifi_connect(context);
        }
        return {};
    }
    if ((scene == GSP_SETTINGS_SCENE_SETTINGS_HOME) && (id == GSP_SETTINGS_HOME_ACT_ID_HEADER_EXIT)) {
        return context.system_service().stop_app(context.app_id());
    }
    if ((scene == GSP_SETTINGS_SCENE_SETTINGS_HOME) && (id == GSP_SETTINGS_HOME_ACT_ID_OPEN_DISPLAY)) {
        load_backlight_state();
        return open_page(detail::PAGE_DISPLAY);
    }
    if ((scene == GSP_SETTINGS_SCENE_SETTINGS_DISPLAY) && (id == GSP_SETTINGS_DISPLAY_ACT_ID_HEADER_BACK)) {
        return open_page(detail::PAGE_HOME);
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

std::expected<void, std::string> SettingsApp::refresh_gsp_home_summaries(system::core::AppContext &)
{
    return {};
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

std::expected<void, std::string> SettingsApp::on_pause(system::core::AppContext &)
{
    if (surface_) {
        surface_->set_suspended(true);
    }
    return {};
}

std::expected<void, std::string> SettingsApp::on_resume(system::core::AppContext &context)
{
    if (surface_) {
        surface_->set_suspended(false);
    }
    if (!settings_uses_gsp_backend()) {
        return {};
    }
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto handle = current_handle(); handle != nullptr) {
        (void)esp_gsp_set_swipe_enabled(handle, false);
    }
    auto shown = surface_->show_page(context, current_page_);
    if (!shown) {
        return shown;
    }
    auto header = refresh_header(context);
    if (!header) {
        return header;
    }
    if (current_page_ == detail::PAGE_HOME) {
        return refresh_gsp_home_summaries(context);
    }
    if (current_page_ == detail::PAGE_DEVICE) {
        return refresh_my_device_state(context);
    }
    if (current_page_ == detail::PAGE_DISPLAY) {
        return refresh_display_state(context);
    }
    if (current_page_ == detail::PAGE_WIFI) {
        return render_wifi_page(context);
    }
    if (current_page_ == detail::PAGE_WIFI_CONNECT) {
        return refresh_wifi_connect_state(context);
    }
    if (current_page_ == detail::PAGE_SOUND) {
        return refresh_sound_state(context);
    }
    if (current_page_ == detail::PAGE_LANGUAGE) {
        return refresh_language_state(context);
    }
    if (current_page_ == detail::PAGE_THEME) {
        return refresh_theme_state(context);
    }
    if (current_page_ == detail::PAGE_TIME_ZONE) {
        return refresh_time_zone_state(context);
    }
#else
    (void)context;
#endif
    return {};
}

} // namespace esp_brookesia::app::settings
