/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_brookesia.hpp"
#include "esp_brookesia_app_settings_ui.hpp"
#include "esp_brookesia_app_settings_manager.hpp"

namespace esp_brookesia::apps {

struct SettingsStylesheetData {
    const char *name;
    gui::StyleSize screen_size;
    SettingsUI_Data ui;
    SettingsManagerData manager;
};

} // namespace esp_brookesia::apps
