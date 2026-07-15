/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <string>

namespace esp_brookesia::app::matter_controller {

struct ControllerDevice {
    std::string name_key;
    std::string display_name;
    std::string device_type;
    std::string card_template;
    std::string tag_key;
    std::string room_key;
    bool powered_on = false;
    int brightness = 100;
    int cct = 40;
    int hue = 30;
    int saturation = 100;
    std::string status_key;
    uint64_t node_id = 0;
    uint16_t endpoint_id = 0;
    bool reachable = true;
    bool is_commissioned = true;
    bool supports_onoff = true;
    bool supports_level = false;
    bool supports_color_temp = false;
    bool supports_hue = false;

    bool operator==(const ControllerDevice &) const = default;
};

struct ControllerRoom {
    std::string name_key;

    bool operator==(const ControllerRoom &) const = default;
};

} // namespace esp_brookesia::app::matter_controller
