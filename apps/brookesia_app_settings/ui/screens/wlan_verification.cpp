/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "private/esp_brookesia_app_settings_utils.hpp"
#include "wlan_verification.hpp"

#define DEFAULT_KEYBOARD_MODE LV_KEYBOARD_MODE_TEXT_LOWER

using namespace std;
using namespace esp_brookesia::systems::speaker;

namespace esp_brookesia::apps {

#define CELL_ELEMENT_CONF_PASSWORD() \
    { \
        SettingsUI_WidgetCellElement::MAIN \
        | SettingsUI_WidgetCellElement::LEFT_TEXT_EDIT, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_ELEMENT_CONF_ADVANCED() \
    { \
        SettingsUI_WidgetCellElement::MAIN \
        | SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL \
        | SettingsUI_WidgetCellElement::RIGHT_MAIN_LABEL, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_CONTAINER_MAP() \
    { \
        { \
            SettingsUI_ScreenWlanVerificationContainerIndex::PASSWORD, { \
                {}, \
                { \
                    { SettingsUI_ScreenWlanVerificationCellIndex::PASSWORD_EDIT, CELL_ELEMENT_CONF_PASSWORD() }, \
                }, \
            } \
        }, \
        { \
            SettingsUI_ScreenWlanVerificationContainerIndex::ADVANCED, { \
                {}, \
                { \
                    { SettingsUI_ScreenWlanVerificationCellIndex::ADVANCED_PROXY, CELL_ELEMENT_CONF_ADVANCED() }, \
                    { SettingsUI_ScreenWlanVerificationCellIndex::ADVANCED_IP, CELL_ELEMENT_CONF_ADVANCED() }, \
                }, \
            }, \
        },\
    }

#define TEXT_EDIT_SEND_CONFIRM_EVENT_LEN_MIN    8

SettingsUI_ScreenWlanVerification::SettingsUI_ScreenWlanVerification(
    App &ui_app, const SettingsUI_ScreenBaseData &base_data,
    const SettingsUI_ScreenWlanVerificationData &main_data
):
    SettingsUI_ScreenBase(ui_app, base_data, SettingsUI_ScreenBaseType::CHILD),
    data(main_data)
{
}

SettingsUI_ScreenWlanVerification::~SettingsUI_ScreenWlanVerification()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);

    ESP_UTILS_CHECK_FALSE_EXIT(del(), "Delete failed");
}

bool SettingsUI_ScreenWlanVerification::begin()
{
    ESP_UTILS_LOGD("Begin(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(SettingsUI_ScreenBase::begin(" ", "Cancel"), false, "Screen base begin failed");

    _cell_container_map = CELL_CONTAINER_MAP();
    ESP_UTILS_CHECK_FALSE_GOTO(processCellContainerMapInit(), err, "Process cell container map init failed");

    {
        // Screen
        lv_obj_add_event_cb(getScreenObject(), onScreenLoadEventCallback, LV_EVENT_SCREEN_LOADED, this);
        lv_obj_add_event_cb(getScreenObject(), onScreenUnloadEventCallback, LV_EVENT_SCREEN_UNLOADED, this);
        // Text edit
        auto text_edit_cell = getCell(
                                  static_cast<int>(SettingsUI_ScreenWlanVerificationContainerIndex::PASSWORD),
                                  static_cast<int>(SettingsUI_ScreenWlanVerificationCellIndex::PASSWORD_EDIT)
                              );
        ESP_UTILS_CHECK_NULL_GOTO(text_edit_cell, err, "Get text edit cell failed");
        lv_obj_t *text_edit = text_edit_cell->getElementObject(SettingsUI_WidgetCellElement::LEFT_TEXT_EDIT);
        ESP_UTILS_CHECK_NULL_GOTO(text_edit, err, "Get text edit failed");
        lv_textarea_set_password_mode(text_edit, true);
        lv_textarea_set_one_line(text_edit, true);
        lv_obj_add_event_cb(text_edit, onTextEditValueChangeEventCallback, LV_EVENT_VALUE_CHANGED, this);
        // Keyboard
        auto &keyboard = app.getSystem()->getDisplay().getKeyboard();
        keyboard.on_keyboard_value_changed_signal.connect([this](const std::string_view & text) {
            ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

            ESP_UTILS_CHECK_FALSE_EXIT(
                processOnKeyboardValueChangedEventCallback(text), "Process keyboard value changed event callback failed"
            );

            ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
        });
        // keyboard.on_keyboard_draw_task_signal.connect([this](lv_event_t *e) {
        //     ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

        //     ESP_UTILS_CHECK_FALSE_EXIT(
        //         processOnKeyboardDrawTaskEventCallback(e), "Process keyboard draw task event callback failed"
        //     );

        //     ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
        // });
        // Gesture
        lv_obj_t *gesture_object = app.getSystem()->getManager().getGesture()->getEventObj();
        ESP_UTILS_CHECK_NULL_GOTO(gesture_object, err, "Get gesture object failed");
        lv_event_code_t gesture_press_code = app.getSystem()->getManager().getGesture()->getPressEventCode();
        lv_event_code_t gesture_release_code = app.getSystem()->getManager().getGesture()->getReleaseEventCode();
        lv_obj_add_event_cb(gesture_object, onGestureEventCallback, gesture_press_code, this);
        lv_obj_add_event_cb(gesture_object, onGestureEventCallback, gesture_release_code, this);

        // _keyboard = keyboard;
        // _last_keyboard_mode = lv_keyboard_get_mode(_keyboard.get());
    }

    ESP_UTILS_CHECK_FALSE_GOTO(processDataUpdate(), err, "Process data update failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool SettingsUI_ScreenWlanVerification::del()
{
    ESP_UTILS_LOGD("Delete(0x%p)", this);
    if (!checkInitialized()) {
        return true;
    }

    bool ret = true;
    if (!SettingsUI_ScreenBase::del()) {
        ret  = false;
        ESP_UTILS_LOGE("Screen base delete failed");
    }

    // Avoid enter gesture event when App is closed
    auto gesture = app.getSystem()->getManager().getGesture();
    if ((gesture != nullptr) && (gesture->getEventObj() != nullptr)) {
        if (!lv_obj_remove_event_cb(gesture->getEventObj(), onGestureEventCallback)) {
            ESP_UTILS_LOGE("Remove gesture event callback failed");
        }
    }

    _flags = {};
    _cell_container_map.clear();

    return ret;
}

bool SettingsUI_ScreenWlanVerification::processDataUpdate()
{
    ESP_UTILS_LOGD("Process data update");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(SettingsUI_ScreenBase::processDataUpdate(), false, "Process base data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processCellContainerMapUpdate(), false, "Process cell container map update failed");

    return true;
}

bool SettingsUI_ScreenWlanVerification::resetScreen()
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_LOGD("Reset screen");

    return true;
}

bool SettingsUI_ScreenWlanVerification::setKeyboardVisible(bool visible)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_LOGD("Set keyboard visible(%d)", (int)visible);

    lv_obj_t *text_edit = getElementObject(
                              static_cast<int>(SettingsUI_ScreenWlanVerificationContainerIndex::PASSWORD),
                              static_cast<int>(SettingsUI_ScreenWlanVerificationCellIndex::PASSWORD_EDIT),
                              SettingsUI_WidgetCellElement::LEFT_TEXT_EDIT
                          );
    ESP_UTILS_CHECK_NULL_RETURN(text_edit, false, "Get text edit failed");

    auto &keyboard = app.getSystem()->getDisplay().getKeyboard();
    if (visible) {
        keyboard.setVisible(true);
        lv_obj_add_state(text_edit, LV_STATE_FOCUSED);
    } else {
        keyboard.setVisible(false);
        keyboard.setMode(DEFAULT_KEYBOARD_MODE);
        lv_obj_clear_state(text_edit, LV_STATE_FOCUSED);
    }

    return true;
}

bool SettingsUI_ScreenWlanVerification::calibrateData(
    const gui::StyleSize &parent_size, const systems::base::Display &display,
    SettingsUI_ScreenWlanVerificationData &data
)
{
    ESP_UTILS_LOGD("Calibrate data");

    const gui::StyleSize *compare_size = nullptr;

    // Keyboard
    compare_size = &parent_size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*compare_size, data.keyboard.size), false,
                                 "Invalid keyboard size");
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreFont(compare_size, data.keyboard.text_font), false,
                                 "Invalid keyboard text font");

    return true;
}

bool SettingsUI_ScreenWlanVerification::processCellContainerMapInit()
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_LOGD("Process cell container map init");

    if (!SettingsUI_ScreenBase::processCellContainerMapInit<SettingsUI_ScreenWlanVerificationContainerIndex,
            SettingsUI_ScreenWlanVerificationCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map init failed");
        return false;
    }

    return true;
}

bool SettingsUI_ScreenWlanVerification::processOnKeyboardValueChangedEventCallback(std::string_view text)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    lv_obj_t *text_edit = getElementObject(
                              static_cast<int>(SettingsUI_ScreenWlanVerificationContainerIndex::PASSWORD),
                              static_cast<int>(SettingsUI_ScreenWlanVerificationCellIndex::PASSWORD_EDIT),
                              SettingsUI_WidgetCellElement::LEFT_TEXT_EDIT
                          );
    ESP_UTILS_CHECK_NULL_RETURN(text_edit, false, "Get text edit failed");

    auto &keyboard = app.getSystem()->getDisplay().getKeyboard();
    std::string pwd = lv_textarea_get_text(text_edit);
    keyboard.setOkEnabled(pwd.length() >= TEXT_EDIT_SEND_CONFIRM_EVENT_LEN_MIN);

    if (text != LV_SYMBOL_OK) {
        return true;
    }
    if (pwd.length() < TEXT_EDIT_SEND_CONFIRM_EVENT_LEN_MIN) {
        ESP_UTILS_LOGW("Password length is less than %d", TEXT_EDIT_SEND_CONFIRM_EVENT_LEN_MIN);
        return true;
    }

    string ssid = "";
    if (SettingsUI_ScreenBase::data.flags.enable_header_title) {
        ssid = getSSID_Text();
        ESP_UTILS_CHECK_FALSE_RETURN(!ssid.empty(), false, "Get SSID text failed");
    }

    on_keyboard_confirm_signal(std::pair<std::string_view, std::string_view> {ssid, pwd});

    return true;
}

// bool SettingsUI_ScreenWlanVerification::processOnKeyboardDrawTaskEventCallback(lv_event_t *e)
// {
//     ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

//     // auto &keyboard = app.getSystem()->getDisplay().getKeyboard();
//     // int keyboard_mode = (int)keyboard.get;

//     // lv_draw_task_t *draw_task = lv_event_get_draw_task(e);
//     // lv_draw_dsc_base_t *base_dsc = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);

//     // auto symbol_button_id_it = _keyboard_symbol_button_ids.find(keyboard_mode);
//     // ESP_UTILS_CHECK_FALSE_RETURN(
//     //     symbol_button_id_it != _keyboard_symbol_button_ids.end(), false, "Find normal key id set failed"
//     // );
//     // auto symbol_button_id = symbol_button_id_it->second;
//     // auto symbol_id_it = std::find(symbol_button_id.begin(), symbol_button_id.end(), dsc->id1);

//     // /* Change the font for symbol buttons */
//     // lv_draw_label_dsc_t * label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
//     // if ((label_draw_dsc != nullptr) && (symbol_id_it != symbol_button_id.end())) {
//     //     ESP_UTILS_CHECK_FALSE_RETURN(
//     //         esp_brookesia_core_utils_get_internal_font_by_size(data.keyboard.text_font.size_px, &font), false,
//     //         "Get internal font by size failed"
//     //     );
//     //     label_draw_dsc->font = font;
//     // }

//     // auto special_button_id_it = _keyboard_special_button_ids.find(keyboard_mode);
//     // ESP_UTILS_CHECK_FALSE_RETURN(
//     //     special_button_id_it != _keyboard_special_button_ids.end(), false, "Find special key id set failed"
//     // );
//     // auto special_button_id = special_button_id_it->second;
//     // auto special_id_it = std::find(special_button_id.begin(), special_button_id.end(), dsc->id1);

//     // /* Change the color for special buttons */
//     // lv_draw_fill_dsc_t * fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
//     // if ((fill_draw_dsc != nullptr) && (special_id_it != special_button_id.end())) {
//     //     if (((keyboard_mode == LV_CUSTOM_KEYBOARD_MODE_NUMBER) && (dsc->id1 == KEYBOARD_NUM_MODE_OK_BTN_ID)) ||
//     //             ((keyboard_mode != LV_CUSTOM_KEYBOARD_MODE_NUMBER) && (dsc->id1 == KEYBOARD_NON_NUM_MODE_OK_BTN_ID))) {
//     //         fill_draw_dsc->color = _flags.keyboard_ok_enabled ?
//     //                                     lv_color_hex(data.keyboard.ok_button_enabled_background_color.color) :
//     //                                     lv_color_hex(data.keyboard.ok_button_disabled_background_color.color);
//     //         fill_draw_dsc->opa = _flags.keyboard_ok_enabled ?
//     //                                 data.keyboard.ok_button_enabled_background_color.opacity :
//     //                                 data.keyboard.ok_button_disabled_background_color.opacity;
//     //     } else {
//     //         fill_draw_dsc->color = lv_color_hex(data.keyboard.special_button_background_color.color);
//     //         fill_draw_dsc->opa = data.keyboard.special_button_background_color.opacity;
//     //     }
//     // }

//     return true;
// }

bool SettingsUI_ScreenWlanVerification::processOnTextEditValueChangeEventCallback(lv_event_t *e)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    // ESP_UTILS_LOGD("Process on text edit value change event callback");

    lv_obj_t *text_edit = (lv_obj_t *)lv_event_get_target(e);
    ESP_UTILS_CHECK_NULL_RETURN(text_edit, false, "Get target failed");

    std::string text = lv_textarea_get_text(text_edit);
    if (text.length() >= TEXT_EDIT_SEND_CONFIRM_EVENT_LEN_MIN) {
        _flags.keyboard_ok_enabled = true;
    } else {
        _flags.keyboard_ok_enabled = false;
    }

    return true;
}

bool SettingsUI_ScreenWlanVerification::processOnGestureEventCallback(lv_event_t *e)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    // ESP_UTILS_LOGD("Process on gesture event callback");

    if (!_flags.gesture_enabled || _flags.is_screen_loaded_gesture) {
        if (_flags.is_screen_loaded_gesture) {
            _flags.is_screen_loaded_gesture = false;
        }
        return true;
    }

    lv_obj_t *text_edit = getElementObject(
                              static_cast<int>(SettingsUI_ScreenWlanVerificationContainerIndex::PASSWORD),
                              static_cast<int>(SettingsUI_ScreenWlanVerificationCellIndex::PASSWORD_EDIT),
                              SettingsUI_WidgetCellElement::LEFT_TEXT_EDIT
                          );
    lv_obj_update_layout(text_edit);

    lv_area_t text_edit_area = {
        .x1 = text_edit->coords.x1,
        .y1 = text_edit->coords.y1,
        .x2 = text_edit->coords.x2,
        .y2 = text_edit->coords.y2,
    };
    lv_area_t keyboard_area = {};
    auto &keyboard = app.getSystem()->getDisplay().getKeyboard();
    ESP_UTILS_CHECK_FALSE_RETURN(keyboard.getArea(keyboard_area), false, "Get keyboard area failed");

    GestureInfo *gesture_info = (GestureInfo *)lv_event_get_param(e);
    lv_point_t gesture_point = {(lv_coord_t)gesture_info->start_x, (lv_coord_t)gesture_info->start_y};
    bool touch_on_text_edit = _lv_area_is_point_on(
                                  &text_edit_area, &gesture_point, lv_obj_get_style_radius(text_edit, 0)
                              );
    bool touch_on_keyboard = keyboard.isVisible() && lv_area_is_point_on(&keyboard_area, &gesture_point, 0);

    lv_event_code_t code = lv_event_get_code(e);
    lv_event_code_t gesture_press_code = app.getSystem()->getManager().getGesture()->getPressEventCode();
    lv_event_code_t gesture_release_code = app.getSystem()->getManager().getGesture()->getReleaseEventCode();
    if (code == gesture_press_code) {
        _flags.text_edit_pressed = touch_on_text_edit;
    } else if (code == gesture_release_code) {
        if (_flags.text_edit_pressed || touch_on_keyboard) {
            ESP_UTILS_CHECK_FALSE_RETURN(setKeyboardVisible(true), false, "Show keyboard failed");
        } else if (!_flags.text_edit_pressed && !touch_on_keyboard) {
            ESP_UTILS_CHECK_FALSE_RETURN(setKeyboardVisible(false), false, "Hide keyboard failed");
        }
    }

    return true;
}

bool SettingsUI_ScreenWlanVerification::processOnScreenLoadEventCallback(lv_event_t *e)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    // ESP_UTILS_LOGD("Process on gesture event callback");


    auto text_edit_cell = getCell(
                              static_cast<int>(SettingsUI_ScreenWlanVerificationContainerIndex::PASSWORD),
                              static_cast<int>(SettingsUI_ScreenWlanVerificationCellIndex::PASSWORD_EDIT)
                          );
    ESP_UTILS_CHECK_NULL_RETURN(text_edit_cell, false, "Get text edit cell failed");
    auto text_edit = text_edit_cell->getElementObject(SettingsUI_WidgetCellElement::LEFT_TEXT_EDIT);
    ESP_UTILS_CHECK_NULL_RETURN(text_edit, false, "Get text edit failed");

    // Keyboard
    ESP_UTILS_CHECK_FALSE_RETURN(setKeyboardVisible(true), false, "Set keyboard visible failed");
    auto &keyboard = app.getSystem()->getDisplay().getKeyboard();
    ESP_UTILS_CHECK_FALSE_RETURN(keyboard.setMode(DEFAULT_KEYBOARD_MODE), false, "Set keyboard mode failed");
    ESP_UTILS_CHECK_FALSE_RETURN(keyboard.setOkEnabled(false), false, "Set keyboard ok enabled failed");
    ESP_UTILS_CHECK_FALSE_RETURN(keyboard.setTextEdit(text_edit), false, "Set text edit failed");

    // Text edit
    lv_textarea_delete_char(text_edit);

    _flags = {};
    _flags.gesture_enabled = true;
    _flags.is_screen_loaded_gesture = true;

    return true;
}

bool SettingsUI_ScreenWlanVerification::processOnScreenUnloadEventCallback(lv_event_t *e)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    // ESP_UTILS_LOGD("Process on gesture event callback");

    auto &keyboard = app.getSystem()->getDisplay().getKeyboard();
    ESP_UTILS_CHECK_FALSE_RETURN(keyboard.setVisible(false), false, "Hide keyboard failed");

    _flags.gesture_enabled = false;
    _flags.is_screen_loaded_gesture = false;

    return true;
}

bool SettingsUI_ScreenWlanVerification::processCellContainerMapUpdate()
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_LOGD("Process cell container map update");

    auto &password_container_conf = _cell_container_map[SettingsUI_ScreenWlanVerificationContainerIndex::PASSWORD].first;
    password_container_conf = data.container_confs[(int)SettingsUI_ScreenWlanVerificationContainerIndex::PASSWORD];
    auto &password_cell_confs = _cell_container_map[SettingsUI_ScreenWlanVerificationContainerIndex::PASSWORD].second;
    for (int i = static_cast<int>(SettingsUI_ScreenWlanVerificationCellIndex::PASSWORD_EDIT);
            i <= static_cast<int>(SettingsUI_ScreenWlanVerificationCellIndex::PASSWORD_EDIT); i++) {
        password_cell_confs[static_cast<SettingsUI_ScreenWlanVerificationCellIndex>(i)].second = data.cell_confs[i];
    }

    auto &adv_settings_container_conf = _cell_container_map[SettingsUI_ScreenWlanVerificationContainerIndex::ADVANCED].first;
    adv_settings_container_conf = data.container_confs[(int)SettingsUI_ScreenWlanVerificationContainerIndex::ADVANCED];
    auto &adv_settings_cell_confs = _cell_container_map[SettingsUI_ScreenWlanVerificationContainerIndex::ADVANCED].second;
    for (int i = static_cast<int>(SettingsUI_ScreenWlanVerificationCellIndex::ADVANCED_PROXY);
            i <= static_cast<int>(SettingsUI_ScreenWlanVerificationCellIndex::ADVANCED_IP); i++) {
        adv_settings_cell_confs[static_cast<SettingsUI_ScreenWlanVerificationCellIndex>(i)].second = data.cell_confs[i];
    }

    if (!SettingsUI_ScreenBase::processCellContainerMapUpdate<SettingsUI_ScreenWlanVerificationContainerIndex,
            SettingsUI_ScreenWlanVerificationCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map uipdate failed");
        return false;
    }

    return true;
}

std::string SettingsUI_ScreenWlanVerification::getSSID_Text() const
{
    lv_obj_t *label = getObject(SettingsUI_ScreenBaseObject::HEADER_TITLE_LABEL);
    ESP_UTILS_CHECK_FALSE_RETURN(lv_obj_is_valid(label), "", "Get screen title label failed");

    return lv_label_get_text(label);
}

void SettingsUI_ScreenWlanVerification::onTextEditValueChangeEventCallback(lv_event_t *e)
{
    ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

    SettingsUI_ScreenWlanVerification *screen = static_cast<SettingsUI_ScreenWlanVerification *>(lv_event_get_user_data(e));
    ESP_UTILS_CHECK_NULL_EXIT(screen, "Get screen failed");
    // ESP_UTILS_LOGD("On keyboard all event callback");

    ESP_UTILS_CHECK_FALSE_EXIT(
        screen->processOnTextEditValueChangeEventCallback(e), "Process on keyboard all event callback failed"
    );
}

// void SettingsUI_ScreenWlanVerification::onKeyboardAllEventCallback(lv_event_t *e)
// {
//     ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

//     SettingsUI_ScreenWlanVerification *screen = static_cast<SettingsUI_ScreenWlanVerification *>(lv_event_get_user_data(e));
//     ESP_UTILS_CHECK_NULL_EXIT(screen, "Get screen failed");
//     // ESP_UTILS_LOGD("On keyboard all event callback");

//     ESP_UTILS_CHECK_FALSE_EXIT(
//         screen->processOnKeyboardAllEventCallback(e), "Process on keyboard all event callback failed"
//     );
// }

void SettingsUI_ScreenWlanVerification::onGestureEventCallback(lv_event_t *e)
{
    ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

    SettingsUI_ScreenWlanVerification *screen = static_cast<SettingsUI_ScreenWlanVerification *>(lv_event_get_user_data(e));
    ESP_UTILS_CHECK_NULL_EXIT(screen, "Get screen failed");
    // ESP_UTILS_LOGD("On keyboard all event callback");

    if (!screen->checkInitialized()) {
        return;
    }

    ESP_UTILS_CHECK_FALSE_EXIT(
        screen->processOnGestureEventCallback(e), "Process on gesture event callback failed"
    );
}

void SettingsUI_ScreenWlanVerification::onScreenLoadEventCallback(lv_event_t *e)
{
    ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

    SettingsUI_ScreenWlanVerification *screen = static_cast<SettingsUI_ScreenWlanVerification *>(lv_event_get_user_data(e));
    ESP_UTILS_CHECK_NULL_EXIT(screen, "Get screen failed");
    // ESP_UTILS_LOGD("On keyboard all event callback");

    ESP_UTILS_CHECK_FALSE_EXIT(
        screen->processOnScreenLoadEventCallback(e), "Process on gesture event callback failed"
    );
}

void SettingsUI_ScreenWlanVerification::onScreenUnloadEventCallback(lv_event_t *e)
{
    ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

    SettingsUI_ScreenWlanVerification *screen = static_cast<SettingsUI_ScreenWlanVerification *>(lv_event_get_user_data(e));
    ESP_UTILS_CHECK_NULL_EXIT(screen, "Get screen failed");
    // ESP_UTILS_LOGD("On keyboard all event callback");

    ESP_UTILS_CHECK_FALSE_EXIT(
        screen->processOnScreenUnloadEventCallback(e), "Process on gesture event callback failed"
    );
}

} // namespace esp_brookesia::apps
