/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

namespace esp_brookesia::gui::lvgl {

bool is_timer_managed_by_port();
void set_timer_managed_by_port(bool managed);

} // namespace esp_brookesia::gui::lvgl
