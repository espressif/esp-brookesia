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

void invalidate_if_available(lv_obj_t *object)
{
    if (object != nullptr) {
        lv_obj_invalidate(object);
    }
}

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

bool request_refresh_now()
{
#if BROOKESIA_GUI_LVGL_HAS_ESP_FONT_BACKEND
    if (!esp_lv_adapter_is_initialized()) {
        return false;
    }
    auto *display = lv_display_get_default();
    if (display == nullptr) {
        return false;
    }
    if (esp_lv_adapter_lock(0) != ESP_OK) {
        return false;
    }

    invalidate_if_available(lv_display_get_screen_active(display));
    invalidate_if_available(lv_display_get_layer_bottom(display));
    invalidate_if_available(lv_display_get_layer_top(display));
    invalidate_if_available(lv_display_get_layer_sys(display));
    esp_lv_adapter_unlock();
    (void)esp_lv_adapter_report_activity();
    return true;
#else
    return false;
#endif
}

} // namespace esp_brookesia::gui::lvgl
