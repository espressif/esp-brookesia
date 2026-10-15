/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <expected>
#include <memory>
#include <string>
#include <string_view>

#include "brookesia/system_core/macro_configs.h"
#include "brookesia/system_core/app/context.hpp"

namespace esp_brookesia::app::settings {

inline constexpr size_t WIFI_PAGE_ROW_COUNT = 5;
inline constexpr size_t MY_DEVICE_GROUP_SLOTS = 4;
inline constexpr size_t MY_DEVICE_GROUP_LINES = 3;
inline constexpr size_t TIME_ZONE_OPTION_COUNT = 6;

struct HomeSummaryView {
    std::string device;
    std::string wifi;
    std::string display;
    std::string sound;
    std::string language;
    std::string theme;
    std::string time_zone;
};

struct MyDevicePageView {
    struct GroupCard {
        std::string title;
        std::array<std::string, MY_DEVICE_GROUP_LINES> lines{};
        size_t line_count = 0;
        bool visible = false;
    };
    std::string system_name;
    std::string system_version;
    std::string device_name;
    std::string pagination;
    bool previous_enabled = false;
    bool next_enabled = false;
    std::array<GroupCard, MY_DEVICE_GROUP_SLOTS> groups{};
};

struct TimeZonePageView {
    std::string current_timezone;
    std::string sntp_state;
    std::string status;
    std::array<std::string, TIME_ZONE_OPTION_COUNT> labels{};
    std::array<std::string, TIME_ZONE_OPTION_COUNT> values{};
};

struct WifiPageView {
    struct Row {
        std::string ssid;
        std::string detail;
        bool enabled = false;
    };
    bool enabled = false;
    bool toggle_enabled = false;
    bool refresh_enabled = false;
    bool previous_enabled = false;
    bool next_enabled = false;
    bool disconnect_enabled = false;
    bool forget_enabled = false;
    std::string status;
    std::string connection;
    std::string pagination;
    std::string selection_notice;
    bool saved_list = false;
    bool list_switch_enabled = false;
    std::array<Row, WIFI_PAGE_ROW_COUNT> rows;
};

struct WifiConnectView {
    std::string ssid;
    std::string detail;
    std::string password_summary;
    std::string status;
    bool connect_enabled = false;
    bool password_enabled = false;
};

struct SoundPageView {
    int volume = 0;
    bool muted = false;
    bool enabled = false;
    bool test_enabled = false;
    std::string status;
};

struct PreferencePageView {
    std::array<std::string, 2> labels;
    std::array<std::string, 2> values;
    std::string status;
    bool show_restart = false;
};

/**
 * @brief Present Settings state through the selected GUI backend.
 */
class SettingsSurface {
public:
    virtual ~SettingsSurface() = default;

    virtual void set_appearance(std::string_view, std::string_view) {}
    virtual void set_suspended(bool) {}
    virtual bool suspended() const
    {
        return false;
    }

    virtual std::expected<void, std::string> set_home_summary(
        system::core::AppContext &, const HomeSummaryView &
    )
    {
        return {};
    }

    virtual std::expected<void, std::string> set_my_device(
        system::core::AppContext &, const MyDevicePageView &
    )
    {
        return {};
    }

    virtual std::expected<void, std::string> set_time_zone(
        system::core::AppContext &, const TimeZonePageView &
    )
    {
        return {};
    }

    virtual std::expected<void, std::string> set_sound(
        system::core::AppContext &, const SoundPageView &
    )
    {
        return {};
    }

    virtual std::expected<void, std::string> set_preferences(
        system::core::AppContext &, const PreferencePageView &
    )
    {
        return {};
    }

    virtual std::expected<void, std::string> set_wifi(
        system::core::AppContext &context, const WifiPageView &view
    ) = 0;

    virtual std::expected<void, std::string> set_wifi_connect(
        system::core::AppContext &context, const WifiConnectView &view
    ) = 0;

    virtual std::expected<void, std::string> show_page(
        system::core::AppContext &context,
        std::string_view page
    ) = 0;
    virtual std::expected<void, std::string> set_header(
        system::core::AppContext &context,
        std::string_view title,
        bool show_back
    ) = 0;
    virtual std::expected<void, std::string> set_brightness(
        system::core::AppContext &context,
        int value,
        bool enabled,
        std::string_view status,
        std::string_view caption
    ) = 0;
};

std::unique_ptr<SettingsSurface> make_settings_surface();

constexpr bool settings_uses_gsp_backend()
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    return true;
#else
    return false;
#endif
}

} // namespace esp_brookesia::app::settings
