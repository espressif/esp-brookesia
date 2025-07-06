/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <optional>
#include <memory>
#include "lvgl.h"
#include "style/esp_brookesia_gui_style.hpp"

namespace esp_brookesia::gui {

constexpr uint8_t LV_DISPLAY_DEBUG_STYLES_NUM = 6;

struct LvDisplayFonts {
    uint8_t fonts_num = 0;
    StyleFont fonts[StyleFont::FONT_SIZE_NUM] {};
};

struct LvDisplayDebugStyles {
    uint8_t outline_width{0};
    StyleColor outline_color{};
};

struct LvDisplayData {
    StyleSize screen_size{};
    std::array<LvDisplayFonts, StyleFont::FONT_SIZE_NUM> fonts{};
    std::array<LvDisplayDebugStyles, LV_DISPLAY_DEBUG_STYLES_NUM> debug_styles{};
};

class LvDisplay {
public:
    LvDisplay(lv_disp_t *display, const LvDisplayData *data);
    ~LvDisplay();

    bool setTouchDevice(lv_indev_t *touch);

    bool showContainerBorder(void);
    bool hideContainerBorder(void);
    bool fontCalibrateMethod(StyleFont &target, const StyleSize *parent) const;

private:
    bool updateByNewData(void);
    const lv_font_t *getFontBySize(uint8_t size) const;
    const lv_font_t *getFontByHeight(uint8_t height, uint8_t *size_px) const;

    LvDisplayData _data;

    lv_disp_t  *_display = nullptr;
    lv_indev_t *_touch = nullptr;

    uint8_t _debug_style_index = 0;
    std::array<lv_style_t, LV_DISPLAY_DEBUG_STYLES_NUM> _debug_styles;
    std::map<uint8_t, const lv_font_t *> _size_font_map;
    std::map<uint8_t, const lv_font_t *> _height_font_map;
};

using LvDisplayPtr = std::unique_ptr<LvDisplay>;

} // namespace esp_brookesia::gui
