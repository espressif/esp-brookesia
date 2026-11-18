/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/gui_gsp/session.hpp"

namespace esp_brookesia::gsp_basic {

bool start_demo(gui::gsp::Session &session, const gui::gsp::DisplayLease &display);
bool restart_demo(gui::gsp::Session &session, const gui::gsp::DisplayLease &display);
void keep_alive();

} // namespace esp_brookesia::gsp_basic
