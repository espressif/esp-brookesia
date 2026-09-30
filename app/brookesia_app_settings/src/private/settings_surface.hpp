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
    std::string status;
    std::string connection;
    std::string pagination;
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

/**
 * @brief Present Settings state through the selected GUI backend.
 */
class SettingsSurface {
public:
    virtual ~SettingsSurface() = default;

    virtual std::expected<void, std::string> set_wifi(
        system::core::AppContext &context, const WifiPageView &view
    ) = 0;

    virtual std::expected<void, std::string> set_wifi_connect(
        system::core::AppContext &context, const WifiConnectView &view
    ) = 0;

    virtual std::expected<void, std::string> show_keyboard(
        system::core::AppContext &, system::core::KeyboardRequestId,
        const system::core::KeyboardRequestOptions &, system::core::KeyboardResultHandler
    )
    {
        return std::unexpected("This Settings surface does not host a keyboard");
    }

    virtual void hide_keyboard(system::core::AppContext &, system::core::KeyboardRequestId) {}

    virtual std::expected<bool, std::string> handle_keyboard_action(unsigned, unsigned, unsigned)
    {
        return false;
    }

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
