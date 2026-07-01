/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/types.hpp"
#include "brookesia/gui_lvgl/macro_configs.h"
#if !BROOKESIA_GUI_LVGL_FACTORY_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

namespace esp_brookesia::gui::lvgl {

void register_default_creators(BackendImpl &impl)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: impl(%1%)", static_cast<void *>(&impl));

    impl.creators.emplace(NodeType::Screen, lv_obj_create);
    impl.creators.emplace(NodeType::Container, lv_obj_create);
    impl.creators.emplace(NodeType::Label, lv_label_create);
    impl.creators.emplace(NodeType::Button, lv_button_create);
    impl.creators.emplace(NodeType::Image, lv_image_create);
    impl.creators.emplace(NodeType::FrameView, lv_image_create);
    impl.creators.emplace(NodeType::TextInput, lv_textarea_create);
    impl.creators.emplace(NodeType::Slider, lv_slider_create);
    impl.creators.emplace(NodeType::Switch, lv_switch_create);
    impl.creators.emplace(NodeType::Checkbox, lv_checkbox_create);
    impl.creators.emplace(NodeType::Dropdown, lv_dropdown_create);
    impl.creators.emplace(NodeType::ProgressBar, lv_bar_create);
    impl.creators.emplace(NodeType::Spinner, lv_spinner_create);
    impl.creators.emplace(NodeType::Arc, lv_arc_create);
    impl.creators.emplace(NodeType::Line, lv_line_create);
    impl.creators.emplace(NodeType::Table, lv_table_create);
    impl.creators.emplace(NodeType::Keyboard, lv_keyboard_create);
    impl.creators.emplace(NodeType::Canvas, lv_canvas_create);
}

} // namespace esp_brookesia::gui::lvgl
