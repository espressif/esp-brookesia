/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <limits>
#include <cmath>
#include "esp_ui_gesture.hpp"

#if !ESP_UI_LOG_ENABLE_DEBUG_WIDGETS_GESTURE
#undef ESP_UI_LOGD
#define ESP_UI_LOGD(...)
#endif

using namespace std;

#define ESP_UI_GESTURE_INFO_INIT()                \
    {                                             \
        .direction = ESP_UI_GESTURE_DIR_NONE,     \
        .start_area = ESP_UI_GESTURE_AREA_CENTER, \
        .stop_area = ESP_UI_GESTURE_AREA_CENTER,  \
        .start_x = -1,                            \
        .start_y = -1,                            \
        .stop_x = -1,                             \
        .stop_y = -1,                             \
        .duration_ms = 0,                         \
        .distance_px = 0,                         \
        .flags = {                                \
            .slow_speed = 0,                      \
            .short_duration = 0,                  \
        },                                        \
    }

ESP_UI_Gesture::ESP_UI_Gesture(ESP_UI_Core &core, const ESP_UI_GestureData_t &data):
    _core(core),
    _data(data),
    _touch_device(nullptr),
    _enable_mask_object(false),
    _direction_tan_threshold(0),
    _touch_start_tick(0),
    _detect_timer(nullptr),
    _event_mask_obj(nullptr),
    _press_event_code(LV_EVENT_ALL),
    _pressing_event_code(LV_EVENT_ALL),
    _release_event_code(LV_EVENT_ALL),
    _info((ESP_UI_GestureInfo_t)ESP_UI_GESTURE_INFO_INIT()),
    _event_data((ESP_UI_GestureInfo_t)ESP_UI_GESTURE_INFO_INIT())
{
}

ESP_UI_Gesture::~ESP_UI_Gesture()
{
    ESP_UI_LOGD("Destroy(0x%p)", this);
    if (!del()) {
        ESP_UI_LOGE("Delete failed");
    }
}

bool ESP_UI_Gesture::begin(lv_obj_t *parent)
{
    ESP_UI_LvTimer_t detect_timer = nullptr;
    ESP_UI_LvObj_t event_mask_obj = nullptr;
    lv_event_code_t press_event_code = LV_EVENT_ALL;
    lv_event_code_t pressing_event_code = LV_EVENT_ALL;
    lv_event_code_t release_event_code = LV_EVENT_ALL;

    ESP_UI_LOGD("Begin(0x%p)", this);
    ESP_UI_CHECK_NULL_RETURN(_core.getTouchDevice(), false, "Invalid core touch device");

    /* Create objects */
    detect_timer = ESP_UI_LV_TIMER(onTouchDetectTimerCallback, _data.detect_period_ms, this);
    ESP_UI_CHECK_NULL_RETURN(detect_timer, false, "Create detect timer failed");
    event_mask_obj = ESP_UI_LV_OBJ(obj, parent);
    ESP_UI_CHECK_NULL_RETURN(event_mask_obj, false, "Create event & mask object failed");
    press_event_code = _core.getFreeEventCode();
    ESP_UI_CHECK_FALSE_RETURN(esp_ui_core_utils_check_event_code_valid(press_event_code), false,
                              "Invalid press event code");
    pressing_event_code = _core.getFreeEventCode();
    ESP_UI_CHECK_FALSE_RETURN(esp_ui_core_utils_check_event_code_valid(pressing_event_code), false,
                              "Invalid pressing event code");
    release_event_code = _core.getFreeEventCode();
    ESP_UI_CHECK_FALSE_RETURN(esp_ui_core_utils_check_event_code_valid(release_event_code), false,
                              "Invalid release event code");

    /* Setup objects */
    lv_obj_add_style(event_mask_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_add_flag(event_mask_obj.get(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(event_mask_obj.get());

    // Save objects
    _touch_device = _core.getTouchDevice();
    _detect_timer = detect_timer;
    _event_mask_obj = event_mask_obj;
    _press_event_code = press_event_code;
    _pressing_event_code = pressing_event_code;
    _release_event_code = release_event_code;

    // Update the object style
    ESP_UI_CHECK_FALSE_GOTO(updateByNewData(), err, "Update failed");

    return true;

err:
    ESP_UI_CHECK_FALSE_RETURN(del(), false, "Delete gesture failed");

    return false;
}

bool ESP_UI_Gesture::del(void)
{
    ESP_UI_LOGD("Delete(0x%p)", this);

    _direction_tan_threshold = 0;
    _touch_start_tick = 0;
    _detect_timer.reset();
    resetGestureInfo();
    _event_mask_obj.reset();

    return true;
}

bool ESP_UI_Gesture::enableMaskObject(bool enable)
{
    ESP_UI_LOGD("Enable mask object(%d)", enable);

    if (_enable_mask_object == enable) {
        return true;
    }

    if (!enable) {
        ESP_UI_CHECK_FALSE_RETURN(setMaskObjectVisible(false), false, "Hide mask object failed");
    }
    _enable_mask_object = enable;

    return true;
}

bool ESP_UI_Gesture::readTouchPoint(int &x, int &y) const
{
    lv_point_t point = {};

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (_touch_device->proc.state != LV_INDEV_STATE_PR) {
        return false;
    }

    lv_indev_get_point(_touch_device, &point);
    if ((point.x >= _core.getCoreData().screen_size.width) || (point.y >= _core.getCoreData().screen_size.height)) {
        return false;
    }

    x = point.x;
    y = point.y;

    return true;
}

bool ESP_UI_Gesture::calibrateData(const ESP_UI_CoreData_t &core_data, ESP_UI_GestureData_t &data)
{
    uint16_t parent_w = core_data.screen_size.width;
    uint16_t parent_h = core_data.screen_size.height;

    ESP_UI_LOGD("Calibrate data");

    ESP_UI_CHECK_FALSE_RETURN(data.detect_period_ms > 0, false, "Invalid detect period");
    ESP_UI_CHECK_VALUE_RETURN(data.threshold.direction_vertical, 1, parent_h, false,
                              "Invalid vertical direction threshold");
    ESP_UI_CHECK_VALUE_RETURN(data.threshold.direction_horizon, 1, parent_w, false,
                              "Invalid horizon direction threshold");
    ESP_UI_CHECK_VALUE_RETURN(data.threshold.direction_angle, 1, 89, false, "Invalid direction angle threshold");
    ESP_UI_CHECK_VALUE_RETURN(data.threshold.top_edge, 1, parent_h, false, "Invalid top edge threshold");
    ESP_UI_CHECK_VALUE_RETURN(data.threshold.bottom_edge, 1, parent_h, false, "Invalid bottom edge threshold");
    ESP_UI_CHECK_VALUE_RETURN(data.threshold.left_edge, 1, parent_w, false, "Invalid left edge threshold");
    ESP_UI_CHECK_VALUE_RETURN(data.threshold.right_edge, 1, parent_w, false, "Invalid right edge threshold");
    ESP_UI_CHECK_FALSE_RETURN(data.threshold.speed_slow_px_per_ms > 0, false, "Invalid speed slow threshold");
    ESP_UI_CHECK_FALSE_RETURN(data.threshold.duration_short_ms > 0, false, "Invalid duration short threshold");

    return true;
}

bool ESP_UI_Gesture::setMaskObjectVisible(bool visible) const
{
    ESP_UI_LOGD("Set mask object visible(%d)", visible);
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!_enable_mask_object) {
        return true;
    }

    if (visible) {
        lv_obj_move_foreground(_event_mask_obj.get());
        lv_obj_clear_flag(_event_mask_obj.get(), LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_event_mask_obj.get(), LV_OBJ_FLAG_HIDDEN);
    }

    return true;
}

void ESP_UI_Gesture::resetGestureInfo(void)
{
    ESP_UI_GestureInfo_t reset_info = (ESP_UI_GestureInfo_t)ESP_UI_GESTURE_INFO_INIT();
    _info = reset_info;
}

bool ESP_UI_Gesture::updateByNewData(void)
{
    ESP_UI_LOGD("Update(0x%p)", this);
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Timer
    lv_timer_set_period(_detect_timer.get(), _data.detect_period_ms);
    // Mask
    lv_obj_set_size(_event_mask_obj.get(), _core.getCoreData().screen_size.width, _core.getCoreData().screen_size.height);
    // Data
    _direction_tan_threshold = tan((int)_data.threshold.direction_angle * M_PI / 180);

    return true;
}

void ESP_UI_Gesture::onDataUpdateEventCallback(lv_event_t *event)
{
    ESP_UI_Gesture *gesture = nullptr;

    ESP_UI_LOGD("Data update event callback");
    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event object");

    gesture = (ESP_UI_Gesture *)lv_event_get_user_data(event);
    ESP_UI_CHECK_NULL_EXIT(gesture, "Invalid gesture object");

    ESP_UI_CHECK_FALSE_EXIT(gesture->updateByNewData(), "Update gesture object style failed");
}

void ESP_UI_Gesture::onTouchDetectTimerCallback(struct _lv_timer_t *t)
{
    bool touched = false;
    int distance_x = 0;
    int distance_y = 0;
    float distance_tan = numeric_limits<float>::infinity();
    lv_event_code_t event_code = LV_EVENT_ALL;

    ESP_UI_Gesture *gesture = (ESP_UI_Gesture *)t->user_data;
    const ESP_UI_GestureData_t &data = gesture->_data;
    const uint16_t &display_w = gesture->_core.getCoreData().screen_size.width;
    const uint16_t &display_h = gesture->_core.getCoreData().screen_size.height;
    const float &distance_tan_threshold = gesture->_direction_tan_threshold;
    ESP_UI_GestureInfo_t &info = gesture->_info;

    ESP_UI_CHECK_NULL_EXIT(gesture, "Invalid gesture");
    // Check if touched and save the last touch point
    touched = gesture->readTouchPoint(info.stop_x, info.stop_y);

    // Process the stop area
    info.stop_area = ESP_UI_GESTURE_AREA_CENTER;
    info.stop_area |= (info.stop_y < data.threshold.top_edge) ? ESP_UI_GESTURE_AREA_TOP_EDGE : 0;
    info.stop_area |= ((display_h - info.stop_y) < data.threshold.bottom_edge) ? ESP_UI_GESTURE_AREA_BOTTOM_EDGE : 0;
    info.stop_area |= (info.stop_x < data.threshold.left_edge) ? ESP_UI_GESTURE_AREA_LEFT_EDGE : 0;
    info.stop_area |= ((display_w - info.stop_x) < data.threshold.right_edge) ? ESP_UI_GESTURE_AREA_RIGHT_EDGE : 0;

    // If not touched before and now, just ignore and return
    if (!gesture->checkGestureStart() && !touched) {
        return;
    }

    // If not touched before but touched now, it means the gesture is started
    if (!gesture->checkGestureStart() && touched) {
        // Save the first touch point
        gesture->_touch_start_tick = lv_tick_get();
        info.start_x = info.stop_x;
        info.start_y = info.stop_y;

        // Process the start area
        info.start_area = ESP_UI_GESTURE_AREA_CENTER;
        info.start_area |= (info.start_y < data.threshold.top_edge) ? ESP_UI_GESTURE_AREA_TOP_EDGE : 0;
        info.start_area |= ((display_h - info.start_y) < data.threshold.bottom_edge) ? ESP_UI_GESTURE_AREA_BOTTOM_EDGE : 0;
        info.start_area |= (info.start_x < data.threshold.left_edge) ? ESP_UI_GESTURE_AREA_LEFT_EDGE : 0;
        info.start_area |= ((display_w - info.start_x) < data.threshold.right_edge) ? ESP_UI_GESTURE_AREA_RIGHT_EDGE : 0;

        // Set the press event code
        event_code = gesture->_press_event_code;
        ESP_UI_LOGD("Gesture send press event");

        goto event_process;
    }

    // Process the duration
    info.duration_ms = lv_tick_elaps(gesture->_touch_start_tick);
    info.flags.short_duration = (info.duration_ms < data.threshold.duration_short_ms);

    // Set the event code according to the touch status
    if (touched) {
        event_code = gesture->_pressing_event_code;
        ESP_UI_LOGD("Gesture send pressing event");
    } else {
        event_code = gesture->_release_event_code;
        ESP_UI_LOGD("Gesture send release event");
    }

    // If not touched now but touched before, it means the gesture is finished
    distance_x = info.stop_x - info.start_x;
    distance_y = info.stop_y - info.start_y;
    if ((distance_x == 0) && (distance_y == 0)) {
        // If the distance is too small, just ignore and go to the end
        goto event_process;
    }

    // Process the distance and speed
    info.distance_px = (float)sqrt(distance_x * distance_x + distance_y * distance_y);
    info.speed_px_per_ms = (info.duration_ms > 0) ? (info.distance_px / info.duration_ms) : numeric_limits<float>::infinity();
    info.flags.slow_speed = (info.speed_px_per_ms < data.threshold.speed_slow_px_per_ms);

    /* Process the direction */
    // Calculate the tan value of the gesture
    distance_tan = (distance_x == 0) ? numeric_limits<float>::infinity() : (float)distance_y / distance_x;
    // Check if the tan absolute value is large enoughd
    // if so, it means the gesture is up or down, otherwise, it's left or right
    if ((distance_tan == numeric_limits<float>::infinity()) || (distance_tan > distance_tan_threshold) ||
            (distance_tan < -distance_tan_threshold)) {
        // Check the distance in y axis
        if (distance_y > data.threshold.direction_vertical) {
            info.direction = ESP_UI_GESTURE_DIR_DOWN;
        } else if (distance_y < -data.threshold.direction_vertical) {
            info.direction = ESP_UI_GESTURE_DIR_UP;
        }
    } else {
        // Check the distance in x axis
        if (distance_x > data.threshold.direction_horizon) {
            info.direction = ESP_UI_GESTURE_DIR_RIGHT;
        } else if (distance_x < -data.threshold.direction_horizon) {
            info.direction = ESP_UI_GESTURE_DIR_LEFT;
        }
    }

event_process:
    if (gesture->checkGestureStart()) {
        ESP_UI_LOGD(
            "\npoint(%d,%d->%d,%d), area(%d->%d), dir(%d), distance(%.2f), angle(%d), duration(%dms), speed(%.2f),"
            "event(%d)", info.start_x, info.start_y, info.stop_x, info.stop_y, info.start_area, info.stop_area,
            (int)info.direction, info.distance_px, (int)(atan(distance_tan) * -180 / M_PI), (int)info.duration_ms,
            info.speed_px_per_ms, (int)event_code
        );
    }

    if ((event_code == gesture->_pressing_event_code) && (info.start_area != ESP_UI_GESTURE_AREA_CENTER)) {
        ESP_UI_CHECK_FALSE_EXIT(gesture->setMaskObjectVisible(true), "Show mask object failed");
    }

    gesture->_event_data = info;
    lv_event_send(gesture->_event_mask_obj.get(), event_code, (void *)&gesture->_event_data);

    if (event_code == gesture->_release_event_code) {
        if (info.start_area != ESP_UI_GESTURE_AREA_CENTER) {
            ESP_UI_CHECK_FALSE_EXIT(gesture->setMaskObjectVisible(false), "Hide mask object failed");
        }
        gesture->resetGestureInfo();
    }
}
