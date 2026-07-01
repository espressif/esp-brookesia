/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "brookesia/gui_lvgl/backend.hpp"
#include "private/types.hpp"
#include "port/private/threading.hpp"

#include <cstdint>

namespace esp_brookesia::gui::lvgl {

namespace {

thread_local uint32_t thread_lock_depth = 0;

} // namespace

void lock_thread()
{
    if (thread_lock_depth++ > 0) {
        return;
    }

#if BROOKESIA_GUI_LVGL_HAS_ESP_FONT_BACKEND
    esp_lv_adapter_lock(-1);
#endif
}

void unlock_thread()
{
    if (thread_lock_depth == 0) {
        return;
    }
    if (--thread_lock_depth > 0) {
        return;
    }

#if BROOKESIA_GUI_LVGL_HAS_ESP_FONT_BACKEND
    esp_lv_adapter_unlock();
#endif
}

bool is_timer_managed_by_port()
{
#if BROOKESIA_GUI_LVGL_HAS_ESP_FONT_BACKEND
    return esp_lv_adapter_is_initialized();
#else
    return false;
#endif
}

void set_timer_managed_by_port(bool managed)
{
    (void)managed;
}

} // namespace esp_brookesia::gui::lvgl
