/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_gui_internal.h"
#if !ESP_BROOKESIA_LVGL_DISPLAY_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_lv_utils.hpp"
#include "esp_brookesia_lv_display.hpp"

namespace esp_brookesia::gui {

LvDisplay::LvDisplay(lv_disp_t *display, const LvDisplayData *data):
    _data(*data),
    _display(display)
{
}

LvDisplay::~LvDisplay()
{
}

bool LvDisplay::setTouchDevice(lv_indev_t *touch)
{
    ESP_UTILS_CHECK_NULL_RETURN(touch, false, "Invalid touch device");

    _touch = touch;

    return true;
}

bool LvDisplay::fontCalibrateMethod(StyleFont &target, const StyleSize *parent) const
{
    uint8_t size_px = 0;
    const lv_font_t *font_resource = (const lv_font_t *)target.font_resource;

    if (target.flags.enable_height) {
        goto process_height;
    }

    // Size
    ESP_UTILS_CHECK_VALUE_RETURN(
        target.size_px, StyleFont::FONT_SIZE_MIN, StyleFont::FONT_SIZE_MAX, false, "Invalid size"
    );
    // Font description
    if (font_resource == nullptr) {
        font_resource = getFontBySize(target.size_px);
        ESP_UTILS_CHECK_NULL_RETURN(font_resource, false, "Get default font failed");
        target.font_resource = font_resource;
        target.height = font_resource->line_height;
    }
    goto end;

process_height:
    // Height
    if (target.flags.enable_height_percent) {
        ESP_UTILS_CHECK_NULL_RETURN(parent, false, "Invalid parent size");
        ESP_UTILS_CHECK_VALUE_RETURN(target.height_percent, 1, 100, false, "Invalid height percent");
        target.height = (parent->height * target.height_percent) / 100;
    } else if (parent != nullptr) {
        ESP_UTILS_CHECK_VALUE_RETURN(target.height, 1, parent->height, false, "Invalid height");
    }

    // Font description & size
    font_resource = getFontByHeight(target.height, &size_px);
    ESP_UTILS_CHECK_NULL_RETURN(font_resource, false, "Get default font failed");
    target.font_resource = font_resource;
    target.size_px = size_px;

end:
    return true;
}

bool LvDisplay::updateByNewData(void)
{
    ESP_UTILS_LOGD("Update lvgl display by new data");

    // Debug styles
    for (size_t i = 0; i < _debug_styles.size(); i++) {
        lv_style_set_outline_width(&_debug_styles[i], _data.debug_styles[i].outline_width);
        lv_style_set_outline_color(
            &_debug_styles[i], lv_color_hex(_data.debug_styles[i].outline_color.color)
        );
        lv_style_set_outline_opa(&_debug_styles[i], _data.debug_styles[i].outline_color.opacity);
    }

    return true;
}

const lv_font_t *LvDisplay::getFontBySize(uint8_t size_px) const
{
    ESP_UTILS_CHECK_VALUE_RETURN(
        size_px, StyleFont::FONT_SIZE_MIN, StyleFont::FONT_SIZE_MAX, nullptr, "Invalid size"
    );

    auto it = _size_font_map.find(size_px);
    ESP_UTILS_CHECK_FALSE_RETURN(it != _size_font_map.end(), nullptr, "Font size(%d) is not found", size_px);

    return it->second;
}

const lv_font_t *LvDisplay::getFontByHeight(uint8_t height, uint8_t *size_px) const
{
    uint8_t ret_size = 0;

    auto lower = _height_font_map.lower_bound(height);
    if ((lower->first != height) && (lower != _height_font_map.begin())) {
        lower--;
    }
    ESP_UTILS_CHECK_FALSE_RETURN(lower != _height_font_map.end(), nullptr, "Font height(%d) is not found", height);

    if (size_px != nullptr) {
        for (auto &it : _size_font_map) {
            if (it.second == lower->second) {
                ret_size = it.first;
                break;
            }
        }
        ESP_UTILS_CHECK_FALSE_RETURN(ret_size != 0, nullptr, "Font size is not found");
        *size_px = ret_size;
    }

    return lower->second;
}

} // namespace esp_brookesia::gui
