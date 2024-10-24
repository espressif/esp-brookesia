/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <limits>
#include <cmath>
#include "esp_brookesia_gesture.hpp"

#if !ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_GESTURE
#undef ESP_BROOKESIA_LOGD
#define ESP_BROOKESIA_LOGD(...)
#endif

using namespace std;

#define ESP_BROOKESIA_GESTURE_INFO_INIT()                \
    {                                             \
        .direction = ESP_BROOKESIA_GESTURE_DIR_NONE,     \
        .start_area = ESP_BROOKESIA_GESTURE_AREA_CENTER, \
        .stop_area = ESP_BROOKESIA_GESTURE_AREA_CENTER,  \
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

ESP_Brookesia_Gesture::ESP_Brookesia_Gesture(ESP_Brookesia_Core &core_in, const ESP_Brookesia_GestureData_t &data_in):
    core(core_in),
    data(data_in),
    _touch_device(nullptr),
    _flags{},
    _direction_tan_threshold(0),
    _indicator_bar_min_lengths{},
    _indicator_bar_max_lengths{},
    _touch_start_tick(0),
    _detect_timer(nullptr),
    _event_mask_obj(nullptr),
    _indicator_bars{},
    _indicator_bar_anim_var{},
    _indicator_bar_scale_back_anims{},
    _indicator_bar_scale_factors{},
    _press_event_code(LV_EVENT_ALL),
    _pressing_event_code(LV_EVENT_ALL),
    _release_event_code(LV_EVENT_ALL),
    _info((ESP_Brookesia_GestureInfo_t)ESP_BROOKESIA_GESTURE_INFO_INIT()),
    _event_data((ESP_Brookesia_GestureInfo_t)ESP_BROOKESIA_GESTURE_INFO_INIT())
{
}

ESP_Brookesia_Gesture::~ESP_Brookesia_Gesture()
{
    ESP_BROOKESIA_LOGD("Destroy(0x%p)", this);
    if (!del()) {
        ESP_BROOKESIA_LOGE("Delete failed");
    }
}

bool ESP_Brookesia_Gesture::begin(lv_obj_t *parent)
{
    ESP_Brookesia_LvTimer_t detect_timer = nullptr;
    ESP_Brookesia_LvObj_t event_mask_obj = nullptr;
    array<ESP_Brookesia_LvObj_t, ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX> indicator_bars = {};
    array<ESP_Brookesia_LvAnim_t, ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX> indicator_bar_scale_back_anims = {};
    lv_event_code_t press_event_code = LV_EVENT_ALL;
    lv_event_code_t pressing_event_code = LV_EVENT_ALL;
    lv_event_code_t release_event_code = LV_EVENT_ALL;

    ESP_BROOKESIA_LOGD("Begin(0x%p)", this);
    ESP_BROOKESIA_CHECK_NULL_RETURN(core.getTouchDevice(), false, "Invalid core touch device");

    /* Create objects */
    detect_timer = ESP_BROOKESIA_LV_TIMER(onTouchDetectTimerCallback, data.detect_period_ms, this);
    ESP_BROOKESIA_CHECK_NULL_RETURN(detect_timer, false, "Create detect timer failed");
    event_mask_obj = ESP_BROOKESIA_LV_OBJ(obj, parent);
    ESP_BROOKESIA_CHECK_NULL_RETURN(event_mask_obj, false, "Create event & mask object failed");
    press_event_code = core.getFreeEventCode();
    ESP_BROOKESIA_CHECK_FALSE_RETURN(esp_brookesia_core_utils_check_event_code_valid(press_event_code), false,
                                     "Invalid press event code");
    pressing_event_code = core.getFreeEventCode();
    ESP_BROOKESIA_CHECK_FALSE_RETURN(esp_brookesia_core_utils_check_event_code_valid(pressing_event_code), false,
                                     "Invalid pressing event code");
    release_event_code = core.getFreeEventCode();
    ESP_BROOKESIA_CHECK_FALSE_RETURN(esp_brookesia_core_utils_check_event_code_valid(release_event_code), false,
                                     "Invalid release event code");
    for (int i = 0; i < ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX; i++) {
        indicator_bars[i] = ESP_BROOKESIA_LV_OBJ(bar, parent);
        ESP_BROOKESIA_CHECK_NULL_RETURN(indicator_bars[i], false, "Create indicator bar failed");
        indicator_bar_scale_back_anims[i] = ESP_BROOKESIA_LV_ANIM();
        ESP_BROOKESIA_CHECK_NULL_RETURN(indicator_bar_scale_back_anims[i], false, "Create indicator bar animation failed");
        _indicator_bar_anim_var[i] = {
            .gesture = this,
            .type = (ESP_Brookesia_GestureIndicatorBarType_t)i,
        };
    }

    /* Setup objects */
    // Event mask
    lv_obj_add_style(event_mask_obj.get(), core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_add_flag(event_mask_obj.get(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(event_mask_obj.get());
    // Indicator bar
    for (int i = 0; i < ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX; i++) {
        // Bar
        lv_obj_add_style(indicator_bars[i].get(), core.getCoreHome().getCoreContainerStyle(), 0);
        lv_obj_clear_flag(indicator_bars[i].get(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(indicator_bars[i].get(), LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_range(indicator_bars[i].get(), 0, 100);
        lv_bar_set_start_value(indicator_bars[i].get(), 0, LV_ANIM_OFF);
        lv_bar_set_value(indicator_bars[i].get(), 100, LV_ANIM_OFF);
        // Animation
        lv_anim_set_user_data(indicator_bar_scale_back_anims[i].get(), reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_anim_set_var(indicator_bar_scale_back_anims[i].get(), &_indicator_bar_anim_var[i]);
        lv_anim_set_early_apply(indicator_bar_scale_back_anims[i].get(), false);
        lv_anim_set_exec_cb(indicator_bar_scale_back_anims[i].get(), onIndicatorBarScaleBackAnimationExecuteCallback);
        lv_anim_set_ready_cb(indicator_bar_scale_back_anims[i].get(), onIndicatorBarScaleBackAnimationReadyCallback);
    }

    // Save objects
    _touch_device = core.getTouchDevice();
    _detect_timer = detect_timer;
    _event_mask_obj = event_mask_obj;
    _press_event_code = press_event_code;
    _pressing_event_code = pressing_event_code;
    _release_event_code = release_event_code;
    _indicator_bars = indicator_bars;
    _indicator_bar_scale_back_anims = indicator_bar_scale_back_anims;

    // Update the object style
    ESP_BROOKESIA_CHECK_FALSE_GOTO(updateByNewData(), err, "Update failed");

    return true;

err:
    ESP_BROOKESIA_CHECK_FALSE_RETURN(del(), false, "Delete gesture failed");

    return false;
}

bool ESP_Brookesia_Gesture::del(void)
{
    ESP_BROOKESIA_LOGD("Delete(0x%p)", this);

    _direction_tan_threshold = 0;
    _touch_start_tick = 0;
    _detect_timer.reset();
    resetGestureInfo();
    _event_mask_obj.reset();
    for (int i = 0; i < ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX; i++) {
        _indicator_bar_scale_back_anims[i].reset();
    }

    return true;
}

bool ESP_Brookesia_Gesture::readTouchPoint(int &x, int &y) const
{
    lv_point_t point = {};

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (_touch_device->proc.state != LV_INDEV_STATE_PR) {
        return false;
    }

    lv_indev_get_point(_touch_device, &point);
    if ((point.x >= core.getCoreData().screen_size.width) || (point.y >= core.getCoreData().screen_size.height)) {
        return false;
    }

    x = point.x;
    y = point.y;

    return true;
}

bool ESP_Brookesia_Gesture::checkMaskVisible(void) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    return !lv_obj_has_flag(_event_mask_obj.get(), LV_OBJ_FLAG_HIDDEN);
}

bool ESP_Brookesia_Gesture::checkIndicatorBarVisible(ESP_Brookesia_GestureIndicatorBarType_t type) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(type < ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX, false, "Invalid indicator bar type");

    return !lv_obj_has_flag(_indicator_bars[type].get(), LV_OBJ_FLAG_HIDDEN);
}

int ESP_Brookesia_Gesture::getIndicatorBarLength(ESP_Brookesia_GestureIndicatorBarType_t type) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), -1, "Not initialized");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(type < ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX, -1, "Invalid indicator bar type");

    lv_obj_update_layout(_indicator_bars[type].get());
    lv_obj_refresh_self_size(_indicator_bars[type].get());

    if (type == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_LEFT || type == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_RIGHT) {
        return lv_obj_get_height(_indicator_bars[type].get());
    } else if (type == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_BOTTOM) {
        return lv_obj_get_width(_indicator_bars[type].get());
    }

    return -1;
}

bool ESP_Brookesia_Gesture::calibrateData(const ESP_Brookesia_StyleSize_t &screen_size, const ESP_Brookesia_CoreHome &home,
        ESP_Brookesia_GestureData_t &data)
{
    uint16_t parent_w = 0;
    uint16_t parent_h = 0;
    const ESP_Brookesia_StyleSize_t *parent_size = nullptr;

    ESP_BROOKESIA_LOGD("Calibrate data");

    // Threshold
    parent_size = &screen_size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    ESP_BROOKESIA_CHECK_FALSE_RETURN(data.detect_period_ms > 0, false, "Invalid detect period");
    ESP_BROOKESIA_CHECK_VALUE_RETURN(data.threshold.direction_vertical, 1, parent_h, false,
                                     "Invalid vertical direction threshold");
    ESP_BROOKESIA_CHECK_VALUE_RETURN(data.threshold.direction_horizon, 1, parent_w, false,
                                     "Invalid horizon direction threshold");
    ESP_BROOKESIA_CHECK_VALUE_RETURN(data.threshold.direction_angle, 1, 89, false, "Invalid direction angle threshold");
    ESP_BROOKESIA_CHECK_VALUE_RETURN(data.threshold.horizontal_edge, 1, parent_w, false, "Invalid left edge threshold");
    ESP_BROOKESIA_CHECK_VALUE_RETURN(data.threshold.vertical_edge, 1, parent_h, false, "Invalid top edge threshold");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(data.threshold.speed_slow_px_per_ms > 0, false, "Invalid speed slow threshold");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(data.threshold.duration_short_ms > 0, false, "Invalid duration short threshold");
    // Left/Right indicator bar
    for (int i = 0; i < ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX; i++) {
        if (!data.flags.enable_indicator_bars[i]) {
            continue;
        }
        ESP_BROOKESIA_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(screen_size, data.indicator_bars[i].main.size_max), false,
                                         "Calibrate indicator bar main size max failed");
        ESP_BROOKESIA_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(screen_size, data.indicator_bars[i].main.size_min, true),
                                         false, "Calibrate indicator bar main size min failed");
        switch (i) {
        case ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_LEFT:
        case ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_RIGHT:
            parent_size = &data.indicator_bars[i].main.size_min;
            parent_w = parent_size->width;
            ESP_BROOKESIA_CHECK_VALUE_RETURN(data.indicator_bars[i].main.layout_pad_all, 0, parent_w / 2, false,
                                             "Invalid indicator bar main layout pad all");
            break;
        case ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_BOTTOM:
            parent_size = &data.indicator_bars[i].main.size_min;
            parent_h = parent_size->height;
            ESP_BROOKESIA_CHECK_VALUE_RETURN(data.indicator_bars[i].main.layout_pad_all, 0, parent_h / 2, false,
                                             "Invalid indicator bar main layout pad all");
            break;
        default:
            break;
        }
    }

    return true;
}

bool ESP_Brookesia_Gesture::setMaskObjectVisible(bool visible) const
{
    ESP_BROOKESIA_LOGD("Set mask object visible(%d)", visible);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (visible) {
        lv_indev_reset(_touch_device, NULL);
        lv_obj_move_foreground(_event_mask_obj.get());
        lv_obj_clear_flag(_event_mask_obj.get(), LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_event_mask_obj.get(), LV_OBJ_FLAG_HIDDEN);
    }

    return true;
}

bool ESP_Brookesia_Gesture::setIndicatorBarLength(ESP_Brookesia_GestureIndicatorBarType_t type, uint16_t length) const
{
    ESP_BROOKESIA_LOGD("Set indicator bar(%d) length(%d)", type, length);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(type < ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX, -1, "Invalid indicator bar type");

    if (!data.flags.enable_indicator_bars[type]) {
        return true;
    }

    const ESP_Brookesia_GestureIndicatorBarData_t &bar_data = data.indicator_bars[type];
    if (type == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_LEFT || type == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_RIGHT) {
        length = max(min(length, bar_data.main.size_max.height), bar_data.main.size_min.height);
        lv_obj_set_height(_indicator_bars[type].get(), length);
    } else if (type == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_BOTTOM) {
        length = max(min(length, bar_data.main.size_max.width), bar_data.main.size_min.width);
        lv_obj_set_width(_indicator_bars[type].get(), length);
    }

    return true;
}

bool ESP_Brookesia_Gesture::setIndicatorBarLengthByOffset(ESP_Brookesia_GestureIndicatorBarType_t type, int offset) const
{
    ESP_BROOKESIA_LOGD("Set indicator bar(%d) length by offset(%d)", type, offset);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(type < ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX, -1, "Invalid indicator bar type");

    int target_len = 0;
    uint16_t max_len = 0;
    float erase_len_ratio = 0;

    if (!data.flags.enable_indicator_bars[type]) {
        return true;
    }

    switch (type) {
    case ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_LEFT:
    case ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_RIGHT:
        offset = max(0, min(offset, (int)data.threshold.direction_horizon));
        max_len = data.indicator_bars[type].main.size_max.height;
        break;
    case ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_BOTTOM:
        offset = max(0, min(offset, (int)data.threshold.direction_vertical));
        max_len = data.indicator_bars[type].main.size_max.width;
        break;
    default:
        ESP_BROOKESIA_CHECK_FALSE_RETURN(false, -1, "Invalid type");
    }
    erase_len_ratio = (offset * _indicator_bar_scale_factors[type]) / (float)max_len;
    target_len =  max_len * (1 - erase_len_ratio);

    const ESP_Brookesia_GestureIndicatorBarData_t &bar_data = data.indicator_bars[type];
    if (type == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_LEFT || type == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_RIGHT) {
        target_len = max(target_len, (int)bar_data.main.size_min.height);
        lv_obj_set_height(_indicator_bars[type].get(), target_len);
    } else if (type == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_BOTTOM) {
        target_len = max(target_len, (int)bar_data.main.size_min.width);
        lv_obj_set_width(_indicator_bars[type].get(), target_len);
    }

    return true;
}

bool ESP_Brookesia_Gesture::setIndicatorBarVisible(ESP_Brookesia_GestureIndicatorBarType_t type, bool visible)
{
    ESP_BROOKESIA_LOGD("Set indicator bar(%d) visible(%d)", type, visible);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!data.flags.enable_indicator_bars[type]) {
        return true;
    }

    if (visible) {
        lv_obj_move_foreground(_indicator_bars[type].get());
        lv_obj_clear_flag(_indicator_bars[type].get(), LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_indicator_bars[type].get(), LV_OBJ_FLAG_HIDDEN);
        ESP_BROOKESIA_CHECK_FALSE_RETURN(setIndicatorBarLength(type, _indicator_bar_max_lengths[type]), false,
                                         "Set indicator bar length failed");
    }

    return true;
}

bool ESP_Brookesia_Gesture::controlIndicatorBarScaleBackAnim(ESP_Brookesia_GestureIndicatorBarType_t type, bool start)
{
    int length = 0;

    ESP_BROOKESIA_LOGD("Control indicator bar(%d) scale back animation(%d)", type, start);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!data.flags.enable_indicator_bars[type]) {
        return true;
    }

    length = getIndicatorBarLength(type);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(length >= 0, false, "Get indicator bar length failed");

    if (start) {
        if (_flags.is_indicator_bar_scale_back_anim_running[type]) {
            return true;
        }
        if (length == _indicator_bar_max_lengths[type]) {
            if (type != ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_BOTTOM) {
                ESP_BROOKESIA_CHECK_FALSE_RETURN(
                    setIndicatorBarVisible(type, false), false, "Set indicator bar visible failed"
                );
            }
            return true;
        }
        lv_anim_set_values(_indicator_bar_scale_back_anims[type].get(), length, _indicator_bar_max_lengths[type]);
        ESP_BROOKESIA_CHECK_NULL_RETURN(lv_anim_start(_indicator_bar_scale_back_anims[type].get()), false,
                                        "Start animation failed");
        _flags.is_indicator_bar_scale_back_anim_running[type] = true;
    } else {
        if (_flags.is_indicator_bar_scale_back_anim_running[type]) {
            ESP_BROOKESIA_CHECK_FALSE_RETURN(
                lv_anim_del(_indicator_bar_scale_back_anims[type]->var, _indicator_bar_scale_back_anims[type]->exec_cb),
                false, "Delete animation failed"
            );
            _flags.is_indicator_bar_scale_back_anim_running[type] = false;
        }
    }

    return true;
}

void ESP_Brookesia_Gesture::resetGestureInfo(void)
{
    ESP_Brookesia_GestureInfo_t reset_info = (ESP_Brookesia_GestureInfo_t)ESP_BROOKESIA_GESTURE_INFO_INIT();
    _info = reset_info;
}

bool ESP_Brookesia_Gesture::updateByNewData(void)
{
    ESP_BROOKESIA_LOGD("Update(0x%p)", this);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    uint16_t bar_range = 0;
    int align_x_offset = 0;
    int align_y_offset = 0;
    lv_align_t align = LV_ALIGN_DEFAULT;
    // Timer
    lv_timer_set_period(_detect_timer.get(), data.detect_period_ms);
    // Mask
    lv_obj_set_size(_event_mask_obj.get(), core.getCoreData().screen_size.width, core.getCoreData().screen_size.height);
    // Indicator bar
    for (int i = 0; i < ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX; i++) {
        const ESP_Brookesia_GestureIndicatorBarData_t &bar_data = data.indicator_bars[i];
        // Main
        lv_obj_set_size(_indicator_bars[i].get(), bar_data.main.size_max.width, bar_data.main.size_max.height);
        lv_obj_set_style_radius(_indicator_bars[i].get(), bar_data.main.radius, 0);
        lv_obj_set_style_pad_all(_indicator_bars[i].get(), bar_data.main.layout_pad_all, 0);
        lv_obj_set_style_bg_color(_indicator_bars[i].get(), lv_color_hex(bar_data.main.color.color), 0);
        lv_obj_set_style_bg_opa(_indicator_bars[i].get(), bar_data.main.color.opacity, 0);
        // Indicator
        lv_obj_set_style_radius(_indicator_bars[i].get(), bar_data.indicator.radius, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(_indicator_bars[i].get(), lv_color_hex(bar_data.indicator.color.color),
                                  LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(_indicator_bars[i].get(), bar_data.indicator.color.opacity, LV_PART_INDICATOR);
        lv_anim_set_path_cb(_indicator_bar_scale_back_anims[i].get(),
                            esp_brookesia_core_utils_get_anim_path_cb(bar_data.animation.scale_back_path_type));
        lv_anim_set_time(_indicator_bar_scale_back_anims[i].get(), bar_data.animation.scale_back_time_ms);
        // Others
        if (i == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_LEFT) {
            align = LV_ALIGN_LEFT_MID;
            align_x_offset = max(data.threshold.horizontal_edge - bar_data.main.size_max.width, 0);
            align_y_offset = 0;
            _indicator_bar_min_lengths[i] = bar_data.main.size_min.height;
            _indicator_bar_max_lengths[i] = bar_data.main.size_max.height;
            bar_range = data.threshold.direction_horizon;
        } else if (i == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_RIGHT) {
            align = LV_ALIGN_RIGHT_MID;
            align_x_offset = min(-data.threshold.horizontal_edge + bar_data.main.size_max.width, 0);
            align_y_offset = 0;
            _indicator_bar_min_lengths[i] = bar_data.main.size_min.height;
            _indicator_bar_max_lengths[i] = bar_data.main.size_max.height;
            bar_range = data.threshold.direction_horizon;
        } else if (i == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_BOTTOM) {
            align = LV_ALIGN_BOTTOM_MID;
            align_x_offset = 0;
            align_y_offset = min(-data.threshold.vertical_edge + bar_data.main.size_max.height, 0);
            _indicator_bar_min_lengths[i] = bar_data.main.size_min.width;
            _indicator_bar_max_lengths[i] = bar_data.main.size_max.width;
            bar_range = data.threshold.direction_vertical;
        }
        ESP_BROOKESIA_CHECK_FALSE_RETURN(bar_range > 0, false, "Invalid bar range");
        _indicator_bar_scale_factors[i] =  (_indicator_bar_max_lengths[i] - _indicator_bar_min_lengths[i]) /
                                           (float)bar_range;
        lv_obj_align(_indicator_bars[i].get(), align, align_x_offset, align_y_offset);
    }
    // Data
    _direction_tan_threshold = tan((int)data.threshold.direction_angle * M_PI / 180);

    return true;
}

void ESP_Brookesia_Gesture::onDataUpdateEventCallback(lv_event_t *event)
{
    ESP_Brookesia_Gesture *gesture = nullptr;

    ESP_BROOKESIA_LOGD("Data update event callback");
    ESP_BROOKESIA_CHECK_NULL_EXIT(event, "Invalid event object");

    gesture = (ESP_Brookesia_Gesture *)lv_event_get_user_data(event);
    ESP_BROOKESIA_CHECK_NULL_EXIT(gesture, "Invalid gesture object");

    ESP_BROOKESIA_CHECK_FALSE_EXIT(gesture->updateByNewData(), "Update gesture object style failed");
}

void ESP_Brookesia_Gesture::onTouchDetectTimerCallback(struct _lv_timer_t *t)
{
    bool touched = false;
    int distance_x = 0;
    int distance_y = 0;
    float distance_tan = numeric_limits<float>::infinity();
    lv_event_code_t event_code = LV_EVENT_ALL;

    ESP_Brookesia_Gesture *gesture = (ESP_Brookesia_Gesture *)t->user_data;
    ESP_BROOKESIA_CHECK_NULL_EXIT(gesture, "Invalid gesture");

    const ESP_Brookesia_GestureData_t &data = gesture->data;
    const uint16_t &display_w = gesture->core.getCoreData().screen_size.width;
    const uint16_t &display_h = gesture->core.getCoreData().screen_size.height;
    const float &distance_tan_threshold = gesture->_direction_tan_threshold;
    ESP_Brookesia_GestureInfo_t &info = gesture->_info;

    // Check if touched and save the last touch point
    touched = gesture->readTouchPoint(info.stop_x, info.stop_y);

    // Process the stop area
    info.stop_area = ESP_BROOKESIA_GESTURE_AREA_CENTER;
    info.stop_area |= (info.stop_y < data.threshold.vertical_edge) ? ESP_BROOKESIA_GESTURE_AREA_TOP_EDGE : 0;
    info.stop_area |= ((display_h - info.stop_y) < data.threshold.vertical_edge) ? ESP_BROOKESIA_GESTURE_AREA_BOTTOM_EDGE : 0;
    info.stop_area |= (info.stop_x < data.threshold.horizontal_edge) ? ESP_BROOKESIA_GESTURE_AREA_LEFT_EDGE : 0;
    info.stop_area |= ((display_w - info.stop_x) < data.threshold.horizontal_edge) ? ESP_BROOKESIA_GESTURE_AREA_RIGHT_EDGE : 0;

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
        info.start_area = ESP_BROOKESIA_GESTURE_AREA_CENTER;
        info.start_area |= (info.start_y < data.threshold.vertical_edge) ? ESP_BROOKESIA_GESTURE_AREA_TOP_EDGE : 0;
        info.start_area |= ((display_h - info.start_y) < data.threshold.vertical_edge) ? ESP_BROOKESIA_GESTURE_AREA_BOTTOM_EDGE : 0;
        info.start_area |= (info.start_x < data.threshold.horizontal_edge) ? ESP_BROOKESIA_GESTURE_AREA_LEFT_EDGE : 0;
        info.start_area |= ((display_w - info.start_x) < data.threshold.horizontal_edge) ? ESP_BROOKESIA_GESTURE_AREA_RIGHT_EDGE : 0;

        // Set the press event code
        event_code = gesture->_press_event_code;
        ESP_BROOKESIA_LOGD("Gesture send press event");

        goto event_process;
    }

    // Process the duration
    info.duration_ms = lv_tick_elaps(gesture->_touch_start_tick);
    info.flags.short_duration = (info.duration_ms < data.threshold.duration_short_ms);

    // Set the event code according to the touch status
    if (touched) {
        event_code = gesture->_pressing_event_code;
        ESP_BROOKESIA_LOGD("Gesture send pressing event");
    } else {
        event_code = gesture->_release_event_code;
        ESP_BROOKESIA_LOGD("Gesture send release event");
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
    info.speed_px_per_ms = (info.duration_ms > 0) ? (info.distance_px / info.duration_ms) :
                           numeric_limits<float>::infinity();
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
            info.direction = ESP_BROOKESIA_GESTURE_DIR_DOWN;
        } else if (distance_y < -data.threshold.direction_vertical) {
            info.direction = ESP_BROOKESIA_GESTURE_DIR_UP;
        }
    } else {
        // Check the distance in x axis
        if (distance_x > data.threshold.direction_horizon) {
            info.direction = ESP_BROOKESIA_GESTURE_DIR_RIGHT;
        } else if (distance_x < -data.threshold.direction_horizon) {
            info.direction = ESP_BROOKESIA_GESTURE_DIR_LEFT;
        }
    }

event_process:
    if (gesture->checkGestureStart()) {
        ESP_BROOKESIA_LOGD(
            "\n\tpoint(%d,%d->%d,%d), area(%d->%d), dir(%d), distance(%.2f), angle(%d), duration(%dms), speed(%.2f),"
            "event(%d)", info.start_x, info.start_y, info.stop_x, info.stop_y, info.start_area, info.stop_area,
            (int)info.direction, info.distance_px, (int)(atan(distance_tan) * -180 / M_PI), (int)info.duration_ms,
            info.speed_px_per_ms, (int)event_code
        );
    }

    gesture->_event_data = info;
    lv_event_send(gesture->_event_mask_obj.get(), event_code, (void *)&gesture->_event_data);
    if (event_code == gesture->_release_event_code) {
        gesture->resetGestureInfo();
    }
}

void ESP_Brookesia_Gesture::onIndicatorBarScaleBackAnimationExecuteCallback(void *var, int32_t value)
{
    auto anim_var = static_cast<IndicatorBarAnimVar_t *>(var);
    ESP_Brookesia_Gesture *gesture = nullptr;
    ESP_Brookesia_GestureIndicatorBarType_t type = ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX;

    ESP_BROOKESIA_CHECK_NULL_EXIT(anim_var, "Invalid var");

    gesture = anim_var->gesture;
    ESP_BROOKESIA_CHECK_NULL_EXIT(gesture, "Invalid gesture");
    type = anim_var->type;
    ESP_BROOKESIA_CHECK_FALSE_EXIT(type < ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX, "Invalid indicator bar type");

    if (type == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_LEFT || type == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_RIGHT) {
        lv_obj_set_height(gesture->_indicator_bars[type].get(), value);
    } else if (type == ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_BOTTOM) {
        lv_obj_set_width(gesture->_indicator_bars[type].get(), value);
    }
}

void ESP_Brookesia_Gesture::onIndicatorBarScaleBackAnimationReadyCallback(lv_anim_t *anim)
{
    ESP_Brookesia_Gesture *gesture = nullptr;
    ESP_Brookesia_GestureIndicatorBarType_t type = ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX;

    ESP_BROOKESIA_LOGD("Indicator bar scale back animation ready callback");
    ESP_BROOKESIA_CHECK_NULL_EXIT(anim, "Invalid anim");

    auto anim_var = static_cast<IndicatorBarAnimVar_t *>(anim->var);
    ESP_BROOKESIA_CHECK_NULL_EXIT(anim_var, "Invalid user data");
    gesture = anim_var->gesture;
    ESP_BROOKESIA_CHECK_NULL_EXIT(gesture, "Invalid gesture");
    type = anim_var->type;
    ESP_BROOKESIA_CHECK_FALSE_EXIT(type < ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_MAX, "Invalid indicator bar type");

    gesture->_flags.is_indicator_bar_scale_back_anim_running[type] = false;
    // If the animation is finished, hide the indicator bar (except the bottom one)
    if (type != ESP_BROOKESIA_GESTURE_INDICATOR_BAR_TYPE_BOTTOM) {
        ESP_BROOKESIA_CHECK_FALSE_EXIT(gesture->setIndicatorBarVisible(type, false), "Hide indicator bar failed");
    }
}
