/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_conf_internal.h"
#if !ESP_BROOKESIA_CONF_LVGL_CONTAINER_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_utils.h"
#include "esp_brookesia_lv_container.hpp"

namespace esp_brookesia::gui {

LvContainer::LvContainer(const LvObject *parent):
    LvObject((parent != nullptr) ? lv_obj_create(parent->getNativeHandle()) : nullptr)
{
    ESP_BROOKESIA_LOG_TRACE_ENTER_WITH_THIS();
    ESP_BROOKESIA_LOGD("Param: parent(0x%p)", parent);

    ESP_BROOKESIA_CHECK_FALSE_EXIT(isValid(), "Failed to create container");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(
        setStyleAttribute(
            StyleSize::RECT(StyleSize::LENGTH_AUTO, StyleSize::LENGTH_AUTO)
        ), "Set style attribute failed"
    );
    ESP_BROOKESIA_CHECK_FALSE_EXIT(
        setStyleAttribute(
            StyleColorItem::STYLE_COLOR_ITEM_BACKGROUND, StyleColor::COLOR_WITH_OPACITY(0, 0)
        ), "Set style attribute failed"
    );
    ESP_BROOKESIA_CHECK_FALSE_EXIT(
        setStyleAttribute(
            StyleWidthItem::STYLE_WIDTH_ITEM_BORDER, 0
        ), "Set style attribute failed"
    );
    ESP_BROOKESIA_CHECK_FALSE_EXIT(
        setStyleAttribute(
            StyleWidthItem::STYLE_WIDTH_ITEM_OUTLINE, 0
        ), "Set style attribute failed"
    );
    ESP_BROOKESIA_CHECK_FALSE_EXIT(
        setStyleAttribute(StyleGap{0, 0, 0, 0, 0, 0}), "Set style attribute failed"
    );

    ESP_BROOKESIA_LOG_TRACE_EXIT_WITH_THIS();
}

} // namespace esp_brookesia::gui
