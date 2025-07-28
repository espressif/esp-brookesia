/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <memory>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_SPEAKER_QUICK_SETTINGS_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "speaker/private/esp_brookesia_speaker_utils.hpp"
#include "style/esp_brookesia_gui_style.hpp"
#include "esp_brookesia_speaker_quick_settings.hpp"
#include "squareline/ui_comp/ui_comp.h"
#include "ui/ui_comp_quicksettings.h"

namespace esp_brookesia::speaker {

QuickSettings::QuickSettings(ESP_Brookesia_Core &core, const QuickSettingsData &data):
    _core(core),
    _data(data)
{
}

QuickSettings::~QuickSettings(void)
{
}

bool QuickSettings::begin(const gui::LvObject &parent)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(!isBegun(), false, "Already begun");
    ESP_UTILS_LOGD("Param: parent(0x%p)", &parent);

    _main_object = std::make_unique<gui::LvObject>(ui_ContainerQuickSettings_create(parent.getNativeHandle()));
    ESP_UTILS_CHECK_NULL_RETURN(_main_object, false, "Failed to create main object");
    _main_object->setStyleAttribute(gui::STYLE_FLAG_CLIP_CORNER, true);

    auto settings_button = gui::LvObject(ui_comp_get_child(
            _main_object->getNativeHandle(),
            UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERBUTTONS_CONTAINERBUTTONSSETTINGS_CONTAINERBUTTONSSETTINGSICON
                                         ), false);
    settings_button.addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        if (!quick_settings->_flags.is_settings_button_long_clicked) {
            quick_settings->on_event_signal(EventType::SettingsButtonClicked);
        } else {
            quick_settings->_flags.is_settings_button_long_clicked = false;
        }
    }, LV_EVENT_CLICKED, this);

    auto wifi_button = gui::LvObject(ui_comp_get_child(
                                         _main_object->getNativeHandle(),
                                         UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERBUTTONS_CONTAINERBUTTONSWIFI_CONTAINERBUTTONSWIFIICON
                                     ), false);
    wifi_button.addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        if (!quick_settings->_flags.is_wifi_button_long_pressed) {
            quick_settings->on_event_signal(EventType::WifiButtonClicked);
        } else {
            quick_settings->_flags.is_wifi_button_long_pressed = false;
        }
    }, LV_EVENT_CLICKED, this);
    wifi_button.addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        quick_settings->on_event_signal(EventType::WifiButtonLongPressed);
        quick_settings->_flags.is_wifi_button_long_pressed = true;
    }, LV_EVENT_LONG_PRESSED, this);

    auto volume_button = gui::LvObject(ui_comp_get_child(
                                           _main_object->getNativeHandle(),
                                           UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERBUTTONS_CONTAINERBUTTONSVOLUME_CONTAINERBUTTONSVOLUMEICON
                                       ), false);
    volume_button.addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        if (!quick_settings->_flags.is_volume_button_long_pressed) {
            quick_settings->on_event_signal(EventType::VolumeButtonClicked);
        } else {
            quick_settings->_flags.is_volume_button_long_pressed = false;
        }
    }, LV_EVENT_CLICKED, this);
    volume_button.addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        quick_settings->on_event_signal(EventType::VolumeButtonLongPressed);
        quick_settings->_flags.is_volume_button_long_pressed = true;
    }, LV_EVENT_LONG_PRESSED, this);

    auto brightness_button = gui::LvObject(ui_comp_get_child(
            _main_object->getNativeHandle(),
            UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERBUTTONS_CONTAINERBUTTONSBRIGHTNESS_CONTAINERBUTTONSBRIGHTNESSICON
                                           ), false);
    brightness_button.addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        if (!quick_settings->_flags.is_brightness_button_long_pressed) {
            quick_settings->on_event_signal(EventType::BrightnessButtonClicked);
        } else {
            quick_settings->_flags.is_brightness_button_long_pressed = false;
        }
    }, LV_EVENT_CLICKED, this);
    brightness_button.addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        quick_settings->on_event_signal(EventType::BrightnessButtonLongPressed);
        quick_settings->_flags.is_brightness_button_long_pressed = true;
    }, LV_EVENT_LONG_PRESSED, this);

    auto lock_button = gui::LvObject(ui_comp_get_child(
                                         _main_object->getNativeHandle(),
                                         UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERBUTTONS_CONTAINERBUTTONSLOCK_CONTAINERBUTTONSLOCKICON
                                     ), false);
    lock_button.addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        quick_settings->on_event_signal(EventType::LockButtonClicked);
    }, LV_EVENT_CLICKED, this);

    _animation = std::make_unique<gui::LvAnimation>();
    ESP_UTILS_CHECK_NULL_RETURN(_animation, false, "Failed to create animation");
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

bool QuickSettings::del(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    _main_object = nullptr;
    _animation = nullptr;

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool QuickSettings::setClockFormat(ClockFormat format)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    if (_clock_format == format) {
        ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
        return true;
    }

    _clock_format = format;
    ESP_UTILS_CHECK_FALSE_RETURN(setClockTime(_hour, _minute), false, "Set clock time failed");

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool QuickSettings::setClockTime(int hour, int minute)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    auto clock = ui_comp_get_child(
                     _main_object->getNativeHandle(),
                     UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERSTATUS_CONTAINERSTATUSINTERNAL_CONTAINERSTATUSINTERNALTOP_LABELSTATUSINTERNALLEFT
                 );
    ESP_UTILS_CHECK_NULL_RETURN(clock, false, "Invalid clock");

    hour = std::clamp(hour, 0, 23);
    bool is_pm = hour >= 12;

    if (_clock_format == ClockFormat::FORMAT_12H) {
        hour = hour % 12;
        if (hour == 0) {
            hour = 12;
        }
    }
    _hour = hour;
    minute = std::clamp(minute, 0, 59);
    _minute = minute;

    if (_clock_format == ClockFormat::FORMAT_12H) {
        lv_label_set_text_fmt(clock, "%02d:%02d %s", _hour, _minute, is_pm ? "PM" : "AM");
    } else {
        lv_label_set_text_fmt(clock, "%02d:%02d", _hour, _minute);
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool QuickSettings::setWifiIconState(WifiState state)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    auto wifi_icon = ui_comp_get_child(
                         _main_object->getNativeHandle(),
                         UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERSTATUS_CONTAINERSTATUSINTERNAL_CONTAINERSTATUSINTERNALTOP_CONTAINERSTATUSINTERNALRIGHT_IMAGESTATUSINTERNALRIGHTWIFI
                     );
    ESP_UTILS_CHECK_NULL_RETURN(wifi_icon, false, "Invalid wifi_icon");

    switch (state) {
    case WifiState::CLOSED:
        lv_obj_add_flag(wifi_icon, LV_OBJ_FLAG_HIDDEN);
        break;
    case WifiState::DISCONNECTED:
        lv_image_set_src(wifi_icon, &esp_brookesia_image_middle_status_bar_wifi_close_24_24);
        break;
    case WifiState::SIGNAL_1:
        lv_image_set_src(wifi_icon, &esp_brookesia_image_middle_status_bar_wifi_level1_24_24);
        break;
    case WifiState::SIGNAL_2:
        lv_image_set_src(wifi_icon, &esp_brookesia_image_middle_status_bar_wifi_level2_24_24);
        break;
    case WifiState::SIGNAL_3:
        lv_image_set_src(wifi_icon, &esp_brookesia_image_middle_status_bar_wifi_level3_24_24);
        break;
    default:
        break;
    }

    if (state != WifiState::CLOSED) {
        lv_obj_remove_flag(wifi_icon, LV_OBJ_FLAG_HIDDEN);
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool QuickSettings::setVisible(bool visible) const
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

bool QuickSettings::moveY_To(int pos) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: pos(%d)", pos);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    ESP_UTILS_CHECK_FALSE_RETURN(_main_object->setY(pos), false, "Set y failed");

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool QuickSettings::moveY_ToWithAnimation(int pos, bool is_visible_when_completed) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: pos(%d)", pos);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not initialized");

    // Get current position
    int current_y = 0;
    ESP_UTILS_CHECK_FALSE_RETURN(_main_object->getY(current_y), false, "Failed to get current Y coordinate");

    // Calculate distance
    float distance = std::abs(pos - current_y);

    // Calculate animation duration (milliseconds) based on speed
    int duration_ms = distance / _data.animation.speed_px_in_s * 1000;
    if (duration_ms <= 0) {
        duration_ms = 1; // Ensure at least 1 millisecond animation time
    }

    // Set animation properties
    gui::StyleAnimation anim_style = {
        .start_value = current_y,
        .end_value = pos,
        .duration_ms = duration_ms,
        .path_type = _data.animation.path_type
    };
    _animation->setStyleAttribute(anim_style);
    _animation->setVariableExecutionMethod(_main_object.get(), [](void *obj, int value) {
        static_cast<gui::LvObject *>(obj)->setY(value);
    });
    if (is_visible_when_completed) {
        _animation->setCompletedMethod([&](void *user_data) {
            ESP_UTILS_LOG_TRACE_GUARD();
            ESP_UTILS_CHECK_FALSE_EXIT(setVisible(true), "Set visible failed");
        });
    } else {
        _animation->setCompletedMethod([&](void *user_data) {
            ESP_UTILS_LOG_TRACE_GUARD();
            ESP_UTILS_CHECK_FALSE_EXIT(setVisible(false), "Set visible failed");
        });
    }

    // Start animation
    ESP_UTILS_CHECK_FALSE_RETURN(_animation->start(), false, "Failed to start animation");

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool QuickSettings::setAnimationCompletedMethod(gui::LvAnimation::CompletedMethod method) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: method(%p)", method);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    _animation->setCompletedMethod(method);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool QuickSettings::setScrollable(bool enable) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: enable(%d)", enable);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    ESP_UTILS_CHECK_FALSE_RETURN(
        _main_object->setStyleAttribute(gui::STYLE_FLAG_SCROLLABLE, enable), false, "Set scrollable failed"
    );

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool QuickSettings::scrollBack(void) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    ESP_UTILS_CHECK_FALSE_RETURN(_main_object->scrollY_To(0, false), false, "Scroll back failed");

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool QuickSettings::calibrateData(
    const ESP_Brookesia_StyleSize_t &screen_size, const ESP_Brookesia_CoreDisplay &display, QuickSettingsData &data
)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    /* Main */
    ESP_UTILS_CHECK_FALSE_RETURN(data.main.size.calibrate(screen_size), false, "Invalid main size");
    return true;
}

bool QuickSettings::updateByNewData(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    ESP_UTILS_CHECK_FALSE_RETURN(
        _main_object->setStyleAttribute(_data.main.size), false, "Set size failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        _main_object->setStyleAttribute(_data.main.align), false, "Set align failed"
    );

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

} // namespace esp_brookesia::speaker
