/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string_view>
#include "lvgl/esp_brookesia_lv.hpp"
#include "systems/base/esp_brookesia_base_context.hpp"
#include "boost/signals2/signal.hpp"

namespace esp_brookesia::systems::speaker {

struct KeyboardData {
    struct {
        gui::StyleSize size;
        gui::StyleAlign align;
        gui::StyleColor background_color;
    } main;
    struct {
        gui::StyleSize size;
        gui::StyleAlign align;
        gui::StyleFont button_text_font;
        gui::StyleColor normal_button_inactive_background_color;
        gui::StyleColor normal_button_inactive_text_color;
        gui::StyleColor normal_button_active_background_color;
        gui::StyleColor normal_button_active_text_color;
        gui::StyleColor special_button_inactive_background_color;
        gui::StyleColor special_button_inactive_text_color;
        gui::StyleColor special_button_active_background_color;
        gui::StyleColor special_button_active_text_color;
        gui::StyleColor ok_button_enabled_background_color;
        gui::StyleColor ok_button_enabled_text_color;
        gui::StyleColor ok_button_disabled_background_color;
        gui::StyleColor ok_button_disabled_text_color;
        gui::StyleColor ok_button_active_background_color;
        gui::StyleColor ok_button_active_text_color;
    } keyboard;
};

class Keyboard {
public:
    using OnKeyboardValueChangedSignal = boost::signals2::signal<void(const std::string_view &text)>;
    using OnKeyboardValueChangedSignalSlot = OnKeyboardValueChangedSignal::slot_type;
    using OnKeyboardDrawTaskSignal = boost::signals2::signal<void(lv_event_t *e)>;
    using OnKeyboardDrawTaskSignalSlot = OnKeyboardDrawTaskSignal::slot_type;

    Keyboard(base::Context &core, const KeyboardData &data);
    ~Keyboard();

    Keyboard &operator=(const Keyboard &other) = delete;
    Keyboard &operator=(Keyboard &&other) = delete;

    bool begin(const gui::LvObject *parent);
    bool del(void);

    bool setVisible(bool visible) const;
    bool setMode(lv_keyboard_mode_t mode) const;
    bool setTextEdit(lv_obj_t *text_edit) const;
    bool setOkEnabled(bool enabled);

    bool isBegun(void) const
    {
        return (_main_object != nullptr);
    }
    bool isVisible(void) const
    {
        return (isBegun()) && !_main_object->hasFlags(gui::STYLE_FLAG_HIDDEN);
    }
    bool getArea(lv_area_t &area) const;
    bool getTextEdit(lv_obj_t *&text_edit) const;

    static bool calibrateData(
        const gui::StyleSize &screen_size, const base::Display &display, KeyboardData &data
    );

    OnKeyboardValueChangedSignal on_keyboard_value_changed_signal;
    OnKeyboardDrawTaskSignal on_keyboard_draw_task_signal;
private:
    bool updateByNewData(void);

    bool processOnKeyboardValueChanged(lv_event_t *e);
    bool processOnKeyboardDrawTask(lv_event_t *e);

    base::Context &_system_context;
    const KeyboardData &_data;

    bool _is_keyboard_ok_enabled = true;
    gui::LvContainerUniquePtr _main_object{nullptr};
    gui::LvObjectUniquePtr _keyboard{nullptr};
    int _last_keyboard_mode = static_cast<int>(LV_KEYBOARD_MODE_TEXT_LOWER);

    static const std::vector<std::string_view> _keyboard_symbol_str;
    static const std::vector<std::string_view> _keyboard_special_str;
};

} // namespace esp_brookesia::systems::speaker
