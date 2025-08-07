/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_brookesia.hpp"
#include "esp_brookesia_app_settings_ui.hpp"
#include "assets/esp_brookesia_app_settings_assets.h"

namespace esp_brookesia::apps {

constexpr SettingsUI_ScreenWlanSoftAPData SETTINGS_UI_360_360_SCREEN_WLAN_SOFTAP_DATA()
{
    return {
        .container_confs = {
            [(int)SettingsUI_ScreenWlanSoftAPContainerIndex::QRCODE] = {
                .title_text = "",
                .flags = {
                    .enable_title = 0,
                },
            },
        },
        .cell_confs = {},
        .qrcode_image = {
            .main_size = gui::StyleSize::SQUARE(100),
            .border_size = gui::StyleSize::SQUARE(10),
            .dark_color = gui::StyleColor::COLOR(0x000000),
            .light_color = gui::StyleColor::COLOR(0xFFFFFF),
        },
        .info_label = {
            .size = gui::StyleSize{
                .width = 280,
                .flags = {
                    .enable_height_auto = 1,
                },
            },
            .text_color = gui::StyleColor::COLOR(0xFFFFFF),
            .text_font = gui::StyleFont::SIZE(16),
        },
    };
}

} // namespace esp_brookesia::apps
