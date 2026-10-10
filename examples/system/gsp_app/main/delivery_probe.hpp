/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/hal_interface/interfaces/display/panel.hpp"

namespace esp_brookesia::gsp_app {

/**
 * @brief Register the `GspProbe` service for USB-driven checks on a running host.
 *
 * `Tap`, `Drag` and `Component` act on the active GSP session. `Frame` returns at once
 * and prints the framebuffers to the console after `DelayMs`, once the USB session that
 * requested it has closed.
 */
bool start_delivery_probe(hal::display::PanelIface &panel);

} // namespace esp_brookesia::gsp_app
