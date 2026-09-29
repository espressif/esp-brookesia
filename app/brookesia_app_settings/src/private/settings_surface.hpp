/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>

#include "brookesia/system_core/macro_configs.h"
#include "brookesia/system_core/app/context.hpp"

namespace esp_brookesia::app::settings {

/**
 * @brief Screen updates used while adjusting brightness.
 *
 * LVGL and GSP each implement this. The rest of Settings does not
 * call the widgets itself.
 */
class SettingsSurface {
public:
    virtual ~SettingsSurface() = default;

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
