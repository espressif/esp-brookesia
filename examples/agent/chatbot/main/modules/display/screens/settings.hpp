/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "lvgl.h"
#include "brookesia/lib_utils/state_base.hpp"
#include "brookesia/service_helper/device.hpp"
#include "brookesia/service_manager/event/registry.hpp"
#include "common.hpp"

class ScreenSettings : public esp_brookesia::lib_utils::StateBase {
public:
    ScreenSettings();
    ~ScreenSettings();

    bool on_enter(const std::string &from_state, const std::string &action) override;
    bool on_exit(const std::string &to_state, const std::string &action) override;

private:
    bool init_wifi();
    bool init_brightness();
    bool init_volume();
    bool init_agent();
    bool init_reset();

    bool enter_process_brightness();
    bool enter_process_volume();
    bool enter_process_agent();

    bool exit_process_agent();

    void refresh_brightness_ui() const;
    void refresh_volume_ui() const;

    bool is_backlight_available_ = false;
    bool is_volume_available_ = false;
    int current_brightness_ = 0;
    int current_volume_ = 0;

    lv_obj_t *init_screen_ = nullptr;
    std::string wifi_ssid_ = "";
    std::vector<std::string> agents_;
    std::string target_agent_ = "";
    esp_brookesia::service::EventRegistry::SignalConnection agent_general_event_connection_;
    esp_brookesia::service::EventRegistry::SignalConnection brightness_changed_connection_;
    esp_brookesia::service::EventRegistry::SignalConnection volume_changed_connection_;
    std::vector<esp_brookesia::service::EventRegistry::SignalConnection> service_connections_;
};
