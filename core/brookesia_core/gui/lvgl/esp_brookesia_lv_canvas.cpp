/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_gui_internal.h"
#if !ESP_BROOKESIA_LVGL_CANVAS_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_lv_utils.hpp"
#include "esp_brookesia_lv_canvas.hpp"

namespace esp_brookesia::gui {

LvCanvas::LvCanvas(const LvObject *parent):
    LvObject((parent != nullptr) ? (lv_canvas_create(parent->getNativeHandle())) : nullptr)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: parent(0x%p)", parent);

    ESP_UTILS_CHECK_FALSE_EXIT(isValid(), "Failed to create canvas");

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
}

bool LvCanvas::setBuffer(void *buffer, int width, int height)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: buffer(0x%p), width(%d), height(%d)", buffer, width, height);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");
    ESP_UTILS_CHECK_NULL_RETURN(buffer, false, "Invalid buffer");

    lv_canvas_set_buffer(getNativeHandle(), buffer, width, height, LV_COLOR_FORMAT_NATIVE);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();

    return true;
}

} // namespace esp_brookesia::gui
