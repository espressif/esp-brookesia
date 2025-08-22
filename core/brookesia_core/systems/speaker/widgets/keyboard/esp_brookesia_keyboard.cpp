/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <memory>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_SPEAKER_KEYBOARD_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "speaker/private/esp_brookesia_speaker_utils.hpp"
#include "style/esp_brookesia_gui_style.hpp"
#include "esp_brookesia_keyboard.hpp"

namespace esp_brookesia::systems::speaker {

#define KEYBOARD_NUM_MODE_KEYBOARD_BTN_ID       3
#define KEYBOARD_NUM_MODE_OK_BTN_ID             7
#define KEYBOARD_NON_NUM_MODE_KEYBOARD_BTN_ID   35
#define KEYBOARD_NON_NUM_MODE_OK_BTN_ID         39
#define KEYBOARD_NON_NUM_MODE_NEW_LINE_BTN_ID   22

#define TEXT_EDIT_SEND_CONFIRM_EVENT_LEN_MIN    8

#define LV_KB_BTN(width)    LV_BUTTONMATRIX_CTRL_POPOVER | width
#define LV_KB_PHR(width)    (lv_buttonmatrix_ctrl_t)width
#define LV_KB_PHR_STR       "  "
#define LV_KB_SPACE_STR     "Space"
#define LV_KB_UPPER_STR     "ABC"
#define LV_KB_LOWER_STR     "abc"
#define LV_KB_NUMBER_STR    "123"
#define LV_KB_SPEC_STR      ",.?!"

static const char *const default_kb_map_lc[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    LV_KB_PHR_STR, "a", "s", "d", "f", "g", "h", "j", "k", "l", LV_KB_PHR_STR, "\n",
    LV_KB_PHR_STR, LV_KB_UPPER_STR, "z", "x", "c", "v", "b", "n", "m", LV_KB_PHR_STR, "\n",
    LV_KB_PHR_STR, LV_KB_NUMBER_STR, LV_KB_SPEC_STR, LV_KB_SPACE_STR, LV_SYMBOL_BACKSPACE, LV_KB_PHR_STR, "\n",
    LV_KB_PHR_STR, LV_SYMBOL_LEFT, LV_SYMBOL_OK, LV_SYMBOL_RIGHT, LV_KB_PHR_STR, ""
};
static const char *const default_kb_map_uc[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    LV_KB_PHR_STR, "A", "S", "D", "F", "G", "H", "J", "K", "L", LV_KB_PHR_STR, "\n",
    LV_KB_PHR_STR, LV_KB_LOWER_STR, "Z", "X", "C", "V", "B", "N", "M", LV_KB_PHR_STR, "\n",
    LV_KB_PHR_STR, LV_KB_NUMBER_STR, LV_KB_SPEC_STR, LV_KB_SPACE_STR, LV_SYMBOL_BACKSPACE, LV_KB_PHR_STR, "\n",
    LV_KB_PHR_STR, LV_SYMBOL_LEFT, LV_SYMBOL_OK, LV_SYMBOL_RIGHT, LV_KB_PHR_STR, ""
};
static const lv_buttonmatrix_ctrl_t default_kb_ctrl_map[] = {
    LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2),
    LV_KB_PHR(1), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_PHR(1),
    LV_KB_PHR(1), LV_KB_BTN(3), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_PHR(2),
    LV_KB_PHR(1), LV_KB_BTN(3), LV_KB_BTN(3), LV_KB_BTN(6), LV_KB_BTN(4), LV_KB_PHR(3),
    LV_KB_PHR(3), LV_KB_BTN(4), LV_KB_BTN(6), LV_KB_BTN(4), LV_KB_PHR(3)
};

static const char *const default_kb_map_spec[] = {
    "+", "|", "\\", "\"", "<", ">", "{", "}", "[", "]", "\n",
    LV_KB_PHR_STR, "~", "@", "#", "!", "%", "&", "*", "(", ")", LV_KB_PHR_STR, "\n",
    LV_KB_PHR_STR, LV_KB_LOWER_STR, "'", "/", "-", "_", ":", ";", "?", LV_KB_PHR_STR, "\n",
    LV_KB_PHR_STR, LV_KB_NUMBER_STR, ".", LV_KB_SPACE_STR, LV_SYMBOL_BACKSPACE, LV_KB_PHR_STR, "\n",
    LV_KB_PHR_STR, LV_SYMBOL_LEFT, LV_SYMBOL_OK, LV_SYMBOL_RIGHT, LV_KB_PHR_STR, ""
};
static const lv_buttonmatrix_ctrl_t default_kb_ctrl_spec_map[] = {
    LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2),
    LV_KB_PHR(1), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_PHR(1),
    LV_KB_PHR(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_PHR(2),
    LV_KB_PHR(2), LV_KB_BTN(3), LV_KB_BTN(2), LV_KB_BTN(6), LV_KB_BTN(4), LV_KB_PHR(3),
    LV_KB_PHR(3), LV_KB_BTN(4), LV_KB_BTN(6), LV_KB_BTN(4), LV_KB_PHR(3)
};

static const char *const default_kb_map_num[] = {
    LV_KB_PHR_STR, "1", "2", "3", LV_SYMBOL_BACKSPACE, LV_KB_PHR_STR, "\n",
    LV_KB_PHR_STR, "4", "5", "6", LV_KB_LOWER_STR, LV_KB_PHR_STR, "\n",
    LV_KB_PHR_STR, "7", "8", "9", LV_KB_SPEC_STR, LV_KB_PHR_STR, "\n",
    LV_KB_PHR_STR, LV_SYMBOL_LEFT, "0", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, LV_KB_PHR_STR, ""
};
static const lv_buttonmatrix_ctrl_t default_kb_ctrl_num_map[] = {
    LV_KB_PHR(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_PHR(2),
    LV_KB_PHR(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_PHR(2),
    LV_KB_PHR(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_PHR(2),
    LV_KB_PHR(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_BTN(2), LV_KB_PHR(2)
};

static const std::vector<std::string_view> keyboard_symbol_str = {
    LV_SYMBOL_BACKSPACE, LV_SYMBOL_LEFT, LV_SYMBOL_OK, LV_SYMBOL_RIGHT,
};
static const std::vector<std::string_view> keyboard_special_str = {
    LV_SYMBOL_BACKSPACE, LV_SYMBOL_LEFT, LV_SYMBOL_OK, LV_SYMBOL_RIGHT,
    LV_KB_SPACE_STR, LV_KB_UPPER_STR, LV_KB_LOWER_STR, LV_KB_NUMBER_STR, LV_KB_SPEC_STR,
};

Keyboard::Keyboard(base::Context &core, const KeyboardData &data):
    _system_context(core),
    _data(data)
{
}

Keyboard::~Keyboard(void)
{
}

bool Keyboard::begin(const gui::LvObject *parent)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(!isBegun(), false, "Already begun");
    ESP_UTILS_LOGD("Param: parent(0x%p)", parent);

    auto style = _system_context.getDisplay().getCoreContainerStyle();

    _main_object = std::make_unique<gui::LvContainer>(parent);
    ESP_UTILS_CHECK_NULL_RETURN(_main_object, false, "Failed to create main object");
    _main_object->setStyle(style);

    // _keyboard = std::make_unique<gui::LvObject>(lv_custom_keyboard_create(_main_object->getNativeHandle()));
    _keyboard = std::make_unique<gui::LvObject>(lv_keyboard_create(_main_object->getNativeHandle()));
    ESP_UTILS_CHECK_NULL_RETURN(_keyboard, false, "Failed to create keyboard");
    _keyboard->setStyleAttribute(gui::STYLE_FLAG_SEND_DRAW_TASK_EVENTS, true);
    _keyboard->setStyle(style);
    _keyboard->addEventCallback([](lv_event_t *e) -> void {
        // ESP_UTILS_LOG_TRACE_GUARD();

        // ESP_UTILS_LOGD("Param: e(%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto keyboard = (Keyboard *)lv_event_get_user_data(e);
        ESP_UTILS_CHECK_NULL_EXIT(keyboard, "Invalid keyboard");

        ESP_UTILS_CHECK_FALSE_EXIT(
            keyboard->processOnKeyboardValueChanged(e), "Process on keyboard value changed failed"
        );
    }, LV_EVENT_VALUE_CHANGED, this);
    _keyboard->addEventCallback([](lv_event_t *e) -> void {
        // ESP_UTILS_LOG_TRACE_GUARD();

        // ESP_UTILS_LOGD("Param: e(%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto keyboard = (Keyboard *)lv_event_get_user_data(e);
        ESP_UTILS_CHECK_NULL_EXIT(keyboard, "Invalid keyboard");

        ESP_UTILS_CHECK_FALSE_EXIT(
            keyboard->processOnKeyboardDrawTask(e), "Process on keyboard draw task failed"
        );
    }, LV_EVENT_DRAW_TASK_ADDED, this);
    lv_keyboard_set_map(
        _keyboard->getNativeHandle(), LV_KEYBOARD_MODE_TEXT_LOWER, default_kb_map_lc, default_kb_ctrl_map
    );
    lv_keyboard_set_map(
        _keyboard->getNativeHandle(), LV_KEYBOARD_MODE_TEXT_UPPER, default_kb_map_uc, default_kb_ctrl_map
    );
    lv_keyboard_set_map(
        _keyboard->getNativeHandle(), LV_KEYBOARD_MODE_SPECIAL, default_kb_map_spec, default_kb_ctrl_spec_map
    );
    lv_keyboard_set_map(
        _keyboard->getNativeHandle(), LV_KEYBOARD_MODE_NUMBER, default_kb_map_num, default_kb_ctrl_num_map
    );

    ESP_UTILS_CHECK_FALSE_GOTO(updateByNewData(), err, "Update by new data failed");

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;

err:
    if (!del()) {
        ESP_UTILS_LOGE("Failed to del");
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return false;
}

bool Keyboard::del(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    _main_object = nullptr;
    _keyboard = nullptr;

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Keyboard::setVisible(bool visible) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: visible(%d)", visible);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    ESP_UTILS_CHECK_FALSE_RETURN(
        _main_object->setStyleAttribute(gui::STYLE_FLAG_HIDDEN, !visible),
        false, "Set visible failed"
    );

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Keyboard::setMode(lv_keyboard_mode_t mode) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: mode(%d)", static_cast<int>(mode));

    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    lv_keyboard_set_mode(_keyboard->getNativeHandle(), mode);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Keyboard::processOnKeyboardValueChanged(lv_event_t *e)
{
    // ESP_UTILS_LOG_TRACE_GUARD();

    // ESP_UTILS_LOGD("Param: e(%p)", e);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    lv_event_code_t code = lv_event_get_code(e);
    ESP_UTILS_CHECK_FALSE_RETURN(code == LV_EVENT_VALUE_CHANGED, false, "Invalid event code");

    auto keyboard = _keyboard.get()->getNativeHandle();
    int current_keyboard_mode = static_cast<int>(lv_keyboard_get_mode(keyboard));
    int btn_id = lv_btnmatrix_get_selected_btn(keyboard);

    const char *text = lv_buttonmatrix_get_button_text(keyboard, btn_id);
    ESP_UTILS_CHECK_NULL_RETURN(text, false, "Invalid text");

    if (strcmp(text, LV_KB_NUMBER_STR) == 0) {
        lv_buttonmatrix_set_map(keyboard, default_kb_map_num);
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
    } else if (strcmp(text, LV_KB_SPEC_STR) == 0) {
        lv_buttonmatrix_set_map(keyboard, default_kb_map_spec);
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_SPECIAL);
    }

    {
        auto text_edit = lv_keyboard_get_textarea(keyboard);
        if (text_edit == nullptr) {
            goto end;
        }

        if (strcmp(text, LV_KB_SPACE_STR) == 0) {
            // Delete the "LV_KB_SPACE_STR" first
            for (int i = 0; i < strlen(LV_KB_SPACE_STR); i++) {
                lv_textarea_delete_char(text_edit);
            }
            // Add a space
            lv_textarea_add_text(text_edit, " ");
        } else if (strcmp(text, LV_KB_NUMBER_STR) == 0) {
            // Delete the "LV_KB_NUMBER_STR"
            for (int i = 0; i < strlen(LV_KB_NUMBER_STR); i++) {
                lv_textarea_delete_char(text_edit);
            }
        } else if (strcmp(text, LV_KB_SPEC_STR) == 0) {
            // Delete the "LV_KB_SPEC_STR"
            for (int i = 0; i < strlen(LV_KB_SPEC_STR); i++) {
                lv_textarea_delete_char(text_edit);
            }
        } else if ((strcmp(text, LV_KB_PHR_STR) == 0) && (current_keyboard_mode == _last_keyboard_mode)) {
            // Delete the "LV_KB_PHR_STR"
            for (int i = 0; i < strlen(LV_KB_PHR_STR); i++) {
                lv_textarea_delete_char(text_edit);
            }
        }
    }

end:
    if ((strcmp(text, LV_KB_PHR_STR) != 0) && ((strcmp(text, LV_SYMBOL_OK) != 0) || _is_keyboard_ok_enabled)) {
        on_keyboard_value_changed_signal(std::string_view(text));
    }
    _last_keyboard_mode = current_keyboard_mode;
    return true;
}

bool Keyboard::processOnKeyboardDrawTask(lv_event_t *e)
{
    // ESP_UTILS_LOG_TRACE_GUARD();

    // ESP_UTILS_LOGD("Param: e(%p)", e);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    lv_event_code_t code = lv_event_get_code(e);
    ESP_UTILS_CHECK_FALSE_RETURN(code == LV_EVENT_DRAW_TASK_ADDED, false, "Invalid event code");

    lv_draw_task_t *draw_task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t *base_dsc = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);

    if (base_dsc->part != LV_PART_ITEMS) {
        goto end;
    }

    {
        auto key_id = base_dsc->id1;
        bool pressed = false;
        auto keyboard = _keyboard.get()->getNativeHandle();
        if ((lv_buttonmatrix_get_selected_button(keyboard) == key_id) && lv_obj_has_state(keyboard, LV_STATE_PRESSED)) {
            pressed = true;
        }

        const char *text = lv_buttonmatrix_get_button_text(keyboard, key_id);
        // Change the color for normal and active buttons
        lv_draw_fill_dsc_t *fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
        if (fill_draw_dsc) {
            lv_color_t inactive_color = gui::toLvColor(_data.keyboard.normal_button_inactive_background_color.color);
            lv_opa_t inactive_opa = _data.keyboard.normal_button_inactive_background_color.opacity;
            lv_color_t active_color = gui::toLvColor(_data.keyboard.normal_button_active_background_color.color);
            lv_opa_t active_opa = _data.keyboard.normal_button_active_background_color.opacity;

            if (strcmp(text, LV_SYMBOL_OK) == 0) {
                inactive_color = gui::toLvColor(
                                     _is_keyboard_ok_enabled ? _data.keyboard.ok_button_enabled_background_color.color :
                                     _data.keyboard.ok_button_disabled_background_color.color
                                 );
                inactive_opa = _is_keyboard_ok_enabled ? _data.keyboard.ok_button_enabled_background_color.opacity :
                               _data.keyboard.ok_button_disabled_background_color.opacity;
                active_color = gui::toLvColor(_data.keyboard.ok_button_active_background_color.color);
                active_opa = _data.keyboard.ok_button_active_background_color.opacity;
            } else if (std::find(keyboard_special_str.begin(), keyboard_special_str.end(), text) != keyboard_special_str.end()) {
                inactive_color = gui::toLvColor(_data.keyboard.special_button_inactive_background_color.color);
                inactive_opa = _data.keyboard.special_button_inactive_background_color.opacity;
                active_color = gui::toLvColor(_data.keyboard.special_button_active_background_color.color);
                active_opa = _data.keyboard.special_button_active_background_color.opacity;
            }

            fill_draw_dsc->color = (pressed && (strcmp(text, LV_KB_PHR_STR) != 0)) ? active_color : inactive_color;
            fill_draw_dsc->opa = (pressed && (strcmp(text, LV_KB_PHR_STR) != 0)) ? active_opa : inactive_opa;
        }
        // Change the text font and color
        lv_draw_label_dsc_t *label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
        if (label_draw_dsc) {
            const lv_font_t *font = static_cast<const lv_font_t *>(_data.keyboard.button_text_font.font_resource);

            lv_color_t inactive_color = gui::toLvColor(_data.keyboard.normal_button_inactive_text_color.color);
            lv_opa_t inactive_opa = _data.keyboard.normal_button_inactive_text_color.opacity;
            lv_color_t active_color = gui::toLvColor(_data.keyboard.normal_button_active_text_color.color);
            lv_opa_t active_opa = _data.keyboard.normal_button_active_text_color.opacity;

            if (strcmp(text, LV_SYMBOL_OK) == 0) {
                inactive_color = gui::toLvColor(
                                     !_is_keyboard_ok_enabled ? _data.keyboard.ok_button_disabled_text_color.color :
                                     _data.keyboard.normal_button_inactive_text_color.color
                                 );
                inactive_opa = !_is_keyboard_ok_enabled ? _data.keyboard.ok_button_disabled_text_color.opacity :
                               _data.keyboard.normal_button_inactive_text_color.opacity;
                active_color = gui::toLvColor(_data.keyboard.ok_button_active_text_color.color);
                active_opa = _data.keyboard.ok_button_active_text_color.opacity;
            } else if (std::find(keyboard_special_str.begin(), keyboard_special_str.end(), text) != keyboard_special_str.end()) {
                inactive_color = gui::toLvColor(_data.keyboard.special_button_inactive_text_color.color);
                inactive_opa = _data.keyboard.special_button_inactive_text_color.opacity;
                active_color = gui::toLvColor(_data.keyboard.special_button_active_text_color.color);
                active_opa = _data.keyboard.special_button_active_text_color.opacity;
            }

            // Use the internal symbol font for the symbol buttons
            if (std::find(keyboard_symbol_str.begin(), keyboard_symbol_str.end(), text) != keyboard_symbol_str.end()) {
                ESP_UTILS_CHECK_FALSE_RETURN(
                    gui::getLvInternalFontBySize(_data.keyboard.button_text_font.size_px, &font), false, "Get font failed"
                );
            }

            label_draw_dsc->font = font;
            label_draw_dsc->color = pressed ? active_color : inactive_color;
            label_draw_dsc->opa = pressed ? active_opa : inactive_opa;
        }

        on_keyboard_draw_task_signal(e);
    }

end:
    return true;
}

bool Keyboard::setTextEdit(lv_obj_t *text_edit) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_CHECK_NULL_RETURN(text_edit, false, "Invalid text edit");
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    lv_keyboard_set_textarea(_keyboard->getNativeHandle(), text_edit);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Keyboard::setOkEnabled(bool enabled)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    _is_keyboard_ok_enabled = enabled;

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Keyboard::getArea(lv_area_t &area) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: area(%p)", &area);

    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    ESP_UTILS_CHECK_FALSE_RETURN(
        _main_object->getArea(area), false, "Get area failed"
    );

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Keyboard::getTextEdit(lv_obj_t *&text_edit) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: text_edit(%p)", text_edit);

    ESP_UTILS_CHECK_NULL_RETURN(text_edit, false, "Invalid text edit");
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    text_edit = lv_keyboard_get_textarea(_keyboard->getNativeHandle());

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Keyboard::calibrateData(
    const gui::StyleSize &screen_size, const base::Display &display, KeyboardData &data
)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    /* Main */
    ESP_UTILS_CHECK_FALSE_RETURN(data.main.size.calibrate(screen_size), false, "Invalid main size");

    /* Keyboard */
    ESP_UTILS_CHECK_FALSE_RETURN(data.keyboard.size.calibrate(data.main.size), false, "Invalid keyboard size");
    ESP_UTILS_CHECK_FALSE_RETURN(
        display.calibrateCoreFont(nullptr, data.keyboard.button_text_font),
        false, "Invalid keyboard button text font"
    );
    return true;
}

bool Keyboard::updateByNewData(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    /* Main */
    ESP_UTILS_CHECK_FALSE_RETURN(
        _main_object->setStyleAttribute(_data.main.size), false, "Set size failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        _main_object->setStyleAttribute(_data.main.align), false, "Set align failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        _main_object->setStyleAttribute(gui::StyleColorItem::STYLE_COLOR_ITEM_BACKGROUND, _data.main.background_color),
        false, "Set background color failed"
    );

    /* Keyboard */
    ESP_UTILS_CHECK_FALSE_RETURN(
        _keyboard->setStyleAttribute(_data.keyboard.size), false, "Set size failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        _keyboard->setStyleAttribute(_data.keyboard.align), false, "Set align failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        _keyboard->setStyleAttribute(_data.keyboard.button_text_font), false, "Set button text font failed"
    );

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

} // namespace esp_brookesia::systems::speaker
