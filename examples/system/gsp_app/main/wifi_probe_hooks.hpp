/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <string_view>

namespace esp_brookesia::gsp_app::wifi_probe_hooks {

enum class Fault {
    None,
    Empty,
    CountFailure,
    ScanFailure,
    UnicodeAps,
};

enum class Status {
    Scanning,
    Empty,
    Failed,
    Nearby,
};

struct WifiDisplaySnapshot {
    unsigned status_updates = 0;
    unsigned connection_updates = 0;
    unsigned target_connecting_updates = 0;
    bool wifi_off = false;
    bool status_connecting = false;
    bool connection_connecting = false;
};

bool arm(Fault fault);
void disarm();
unsigned injection_count(Fault fault);
unsigned status_count(Status status);
unsigned unicode_row_count(size_t row);
bool cleanup_succeeded();
bool clear_driver_scan_list();
void track_target_ssid(std::string_view ssid);
void clear_target_ssid();
void reset_target_row();
int matching_target_row();
unsigned connected_target_count();
WifiDisplaySnapshot wifi_display_snapshot();

} // namespace esp_brookesia::gsp_app::wifi_probe_hooks
