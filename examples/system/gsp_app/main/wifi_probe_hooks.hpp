/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>

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

bool arm(Fault fault);
void disarm();
unsigned injection_count(Fault fault);
unsigned status_count(Status status);
unsigned unicode_row_count(size_t row);
bool cleanup_succeeded();
bool clear_driver_scan_list();

} // namespace esp_brookesia::gsp_app::wifi_probe_hooks
