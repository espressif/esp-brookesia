/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <functional>

namespace esp_brookesia::gui::wasm {

using GuiTask = std::function<void()>;

bool post_gui_task(GuiTask task);
void drain_gui_tasks(size_t max_task_count = 128);
void set_gui_task_blocked(bool blocked);
bool is_gui_task_blocked();
bool is_gui_task_draining();

} // namespace esp_brookesia::gui::wasm
