/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "style/esp_brookesia_gui_style.hpp"
#include "widgets/quick_settings/esp_brookesia_speaker_quick_settings.hpp"

namespace esp_brookesia::systems::speaker {

constexpr QuickSettingsData STYLESHEET_360_360_DARK_QUICK_SETTINGS_DATA = {
    .main{
        .size = gui::StyleSize::RECT_PERCENT(100, 100),
        .align{
            .type = gui::STYLE_ALIGN_TYPE_TOP_MID,
            .offset_x = 0,
            .offset_y = -(360 - 20),
        },
    },
    .animation{
        .path_type = gui::StyleAnimation::AnimationPathType::ANIM_PATH_TYPE_EASE_OUT,
        .speed_px_in_s = 400,
    }
};

} // namespace esp_brookesia::systems::speaker
