/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "style/esp_brookesia_gui_style.hpp"
#include "widgets/keyboard/esp_brookesia_keyboard.hpp"

namespace esp_brookesia::systems::speaker {

constexpr KeyboardData STYLESHEET_360_360_DARK_KEYBOARD_DATA = {
    .main{
        .size = gui::StyleSize::RECT_PERCENT(100, 60),
        .align{
            .type = gui::STYLE_ALIGN_TYPE_BOTTOM_MID,
        },
        // .background_color = gui::StyleColor::COLOR(0x38393A),
        .background_color = gui::StyleColor::COLOR(0x000000),
    },
    .keyboard{
        .size = gui::StyleSize::RECT_PERCENT(94, 85),
        .align{
            .type = gui::STYLE_ALIGN_TYPE_BOTTOM_MID,
            .offset_y = -30,
        },
        .button_text_font = gui::StyleFont::SIZE(24),
        .normal_button_inactive_background_color = gui::StyleColor::COLOR_WITH_OPACITY(0x000000, 0),
        .normal_button_inactive_text_color = gui::StyleColor::COLOR(0xFFFFFF),
        .normal_button_active_background_color = gui::StyleColor::COLOR_WITH_OPACITY(0xFFFFFF, 128),
        .normal_button_active_text_color = gui::StyleColor::COLOR(0xFFFFFF),
        .special_button_inactive_background_color = gui::StyleColor::COLOR_WITH_OPACITY(0x000000, 0),
        .special_button_inactive_text_color = gui::StyleColor::COLOR(0xFFFFFF),
        .special_button_active_background_color = gui::StyleColor::COLOR_WITH_OPACITY(0xFFFFFF, 128),
        .special_button_active_text_color = gui::StyleColor::COLOR(0xFFFFFF),
        .ok_button_enabled_background_color = gui::StyleColor::COLOR(0xFF3034),
        .ok_button_enabled_text_color = gui::StyleColor::COLOR(0xFFFFFF),
        .ok_button_disabled_background_color = gui::StyleColor::COLOR_WITH_OPACITY(0xFF3034, 80),
        .ok_button_disabled_text_color = gui::StyleColor::COLOR(0xFFFFFF),
        .ok_button_active_background_color = gui::StyleColor::COLOR_WITH_OPACITY(0xFFFFFF, 128),
        .ok_button_active_text_color = gui::StyleColor::COLOR(0xFFFFFF),
    },
};

} // namespace esp_brookesia::systems::speaker
