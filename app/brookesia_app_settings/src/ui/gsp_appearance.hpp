/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#ifdef ESP_PLATFORM
#   include "sdkconfig.h"
#endif

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
#   include "esp_gsp.h"

namespace esp_brookesia::app::settings {

std::expected<void, std::string> apply_settings_appearance(
    esp_gsp_handle_t handle, uint16_t scene, std::string_view locale, std::string_view theme
);

} // namespace esp_brookesia::app::settings
#endif
