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

namespace esp_brookesia::systems::speaker {

#define BATTERY_ICON_COLOR_CHARGING  lv_color_hex(0x00FF00)
#define BATTERY_ICON_COLOR_LEVEL_1   lv_color_hex(0xFF0000)
#define BATTERY_ICON_COLOR_LEVEL_2   lv_color_hex(0xFFFF00)
#define BATTERY_ICON_COLOR_LEVEL_3   lv_color_hex(0xFFFFFF)
#define BATTERY_ICON_COLOR_LEVEL_4   lv_color_hex(0xFFFFFF)

constexpr int QUICK_SETTINGS_BATTERY_PERCENT_MIN = 0;
constexpr int QUICK_SETTINGS_BATTERY_PERCENT_MAX = 100;
constexpr int QUICK_SETTINGS_VOLUME_PERCENT_MIN = 0;
constexpr int QUICK_SETTINGS_VOLUME_PERCENT_MAX = 90;
constexpr int QUICK_SETTINGS_BRIGHTNESS_PERCENT_MIN = 10;
constexpr int QUICK_SETTINGS_BRIGHTNESS_PERCENT_MAX = 100;
constexpr int QUICK_SETTINGS_MEMORY_SRAM_PERCENT_MIN = 0;
constexpr int QUICK_SETTINGS_MEMORY_SRAM_PERCENT_MAX = 100;
constexpr int QUICK_SETTINGS_MEMORY_PSRAM_PERCENT_MIN = 0;
constexpr int QUICK_SETTINGS_MEMORY_PSRAM_PERCENT_MAX = 100;

QuickSettings::QuickSettings(base::Context &core, const QuickSettingsData &data):
    _system_context(core),
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

    _wifi_button = std::make_unique<gui::LvObject>(ui_comp_get_child(
                       _main_object->getNativeHandle(),
                       UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERBUTTONS_CONTAINERBUTTONSWIFI_CONTAINERBUTTONSWIFIICON
                   ), false);
    _wifi_button->addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        auto wifi_button = static_cast<lv_obj_t *>(lv_event_get_target(e));
        ESP_UTILS_CHECK_NULL_EXIT(wifi_button, "Invalid target");

        if (!quick_settings->_flags.is_wifi_button_long_pressed) {
            quick_settings->_event_signal({EventType::WifiButtonClicked});
        } else {
            quick_settings->_flags.is_wifi_button_long_pressed = false;
            // Avoid the button state is checked when long pressed
            if (lv_obj_has_state(wifi_button, LV_STATE_CHECKED)) {
                lv_obj_remove_state(wifi_button, LV_STATE_CHECKED);
            } else {
                lv_obj_add_state(wifi_button, LV_STATE_CHECKED);
            }
        }
    }, LV_EVENT_CLICKED, this);
    _wifi_button->addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        quick_settings->_event_signal({EventType::WifiButtonLongPressed});
        quick_settings->_flags.is_wifi_button_long_pressed = true;
    }, LV_EVENT_LONG_PRESSED, this);

    _volume_button = std::make_unique<gui::LvObject>(ui_comp_get_child(
                         _main_object->getNativeHandle(),
                         UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERBUTTONS_CONTAINERBUTTONSVOLUME_CONTAINERBUTTONSVOLUMEICON
                     ), false);
    _volume_button->addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        if (!quick_settings->_flags.is_volume_button_long_pressed) {
            quick_settings->_event_signal({EventType::VolumeButtonClicked});
        } else {
            quick_settings->_flags.is_volume_button_long_pressed = false;
        }
    }, LV_EVENT_CLICKED, this);
    _volume_button->addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        quick_settings->_event_signal({EventType::VolumeButtonLongPressed});
        quick_settings->_flags.is_volume_button_long_pressed = true;
    }, LV_EVENT_LONG_PRESSED, this);

    _brightness_button = std::make_unique<gui::LvObject>(ui_comp_get_child(
                             _main_object->getNativeHandle(),
                             UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERBUTTONS_CONTAINERBUTTONSBRIGHTNESS_CONTAINERBUTTONSBRIGHTNESSICON
                         ), false);
    _brightness_button->addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        if (!quick_settings->_flags.is_brightness_button_long_pressed) {
            quick_settings->_event_signal({EventType::BrightnessButtonClicked});
        } else {
            quick_settings->_flags.is_brightness_button_long_pressed = false;
        }
    }, LV_EVENT_CLICKED, this);
    _brightness_button->addEventCallback([](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: e(0x%p)", e);
        ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

        auto quick_settings = static_cast<QuickSettings *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(quick_settings, "Invalid user data");

        quick_settings->_event_signal({EventType::BrightnessButtonLongPressed});
        quick_settings->_flags.is_brightness_button_long_pressed = true;
    }, LV_EVENT_LONG_PRESSED, this);

    _animation = std::make_unique<gui::LvAnimation>();
    ESP_UTILS_CHECK_NULL_RETURN(_animation, false, "Failed to create animation");
    ESP_UTILS_CHECK_FALSE_GOTO(updateByNewData(), err, "Update by new data failed");

    ESP_UTILS_CHECK_FALSE_RETURN(setWifiIconState(WifiState::CLOSED), false, "Set wifi icon state failed");
    ESP_UTILS_CHECK_FALSE_RETURN(setBatteryPercent(true, 100), false, "Set battery percent failed");

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
    _wifi_button = nullptr;
    _volume_button = nullptr;
    _brightness_button = nullptr;
    _animation = nullptr;

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

boost::signals2::connection QuickSettings::connectEventSignal(EventSignalSlot slot)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    return _event_signal.connect(slot);
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

    const lv_image_dsc_t *image_src = nullptr;
    switch (state) {
    case WifiState::CLOSED:
        lv_obj_add_flag(wifi_icon, LV_OBJ_FLAG_HIDDEN);
        break;
    case WifiState::DISCONNECTED:
        image_src = &speaker_image_middle_quick_settings_wifi_close_20_20;
        break;
    case WifiState::SIGNAL_1:
        image_src = &speaker_image_middle_quick_settings_wifi_level1_20_20;
        break;
    case WifiState::SIGNAL_2:
        image_src = &speaker_image_middle_quick_settings_wifi_level2_20_20;
        break;
    case WifiState::SIGNAL_3:
        image_src = &speaker_image_middle_quick_settings_wifi_level3_20_20;
        break;
    default:
        break;
    }

    if (image_src != nullptr) {
        lv_image_set_src(wifi_icon, image_src);
        lv_obj_remove_flag(wifi_icon, LV_OBJ_FLAG_HIDDEN);
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool QuickSettings::setBatteryPercent(bool charge_flag, int percent)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: charge_flag(%d), percent(%d)", charge_flag, percent);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    percent = std::clamp(percent, QUICK_SETTINGS_BATTERY_PERCENT_MIN, QUICK_SETTINGS_BATTERY_PERCENT_MAX);
    auto battery_label = ui_comp_get_child(
                             _main_object->getNativeHandle(),
                             UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERSTATUS_CONTAINERSTATUSINTERNAL_CONTAINERSTATUSINTERNALTOP_CONTAINERSTATUSINTERNALRIGHT_LABELSTATUSINTERNALRIGHTBATTERYPERCENT
                         );
    ESP_UTILS_CHECK_NULL_RETURN(battery_label, false, "Invalid battery_label");
    lv_label_set_text_fmt(battery_label, "%d%%", percent);

    auto battery_icon = ui_comp_get_child(
                            _main_object->getNativeHandle(),
                            UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERSTATUS_CONTAINERSTATUSINTERNAL_CONTAINERSTATUSINTERNALTOP_CONTAINERSTATUSINTERNALRIGHT_IMAGESTATUSINTERNALRIGHTBATTERY
                        );
    ESP_UTILS_CHECK_NULL_RETURN(battery_icon, false, "Invalid battery_icon");
    const lv_image_dsc_t *image_src = nullptr;
    lv_color_t color = BATTERY_ICON_COLOR_CHARGING;
    const lv_image_dsc_t *_images[] = {
        &speaker_image_middle_quick_settings_battery_level1_20_20,
        &speaker_image_middle_quick_settings_battery_level2_20_20,
        &speaker_image_middle_quick_settings_battery_level3_20_20,
        &speaker_image_middle_quick_settings_battery_level4_20_20,
    };
    if (charge_flag) {
        image_src = &speaker_image_middle_quick_settings_battery_charge_20_20;
        color = BATTERY_ICON_COLOR_CHARGING;
    } else {
        if (percent >= 75) {
            image_src = _images[3];
            color = BATTERY_ICON_COLOR_LEVEL_4;
        } else if (percent >= 50) {
            image_src = _images[2];
            color = BATTERY_ICON_COLOR_LEVEL_3;
        } else if (percent >= 20) {
            image_src = _images[1];
            color = BATTERY_ICON_COLOR_LEVEL_2;
        } else {
            image_src = _images[0];
            color = BATTERY_ICON_COLOR_LEVEL_1;
        }
    }

    lv_image_set_src(battery_icon, image_src);
    lv_obj_set_style_image_recolor(battery_icon, color, 0);
    lv_obj_set_style_image_recolor_opa(battery_icon, LV_OPA_COVER, 0);

    return true;
}

bool QuickSettings::setVolume(VolumeLevel level)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: level(%d)", level);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    const lv_image_dsc_t *image_src = nullptr;
    switch (level) {
    case VolumeLevel::MUTE:
        image_src = &speaker_image_middle_quick_settings_volume_off_48_48;
        break;
    case VolumeLevel::LEVEL_1:
        image_src = &speaker_image_middle_quick_settings_volume_low_48_48;
        break;
    case VolumeLevel::LEVEL_2:
        image_src = &speaker_image_middle_quick_settings_volume_medium_48_48;
        break;
    case VolumeLevel::LEVEL_3:
        image_src = &speaker_image_middle_quick_settings_volume_high_48_48;
        break;
    default:
        break;
    }
    ESP_UTILS_CHECK_NULL_RETURN(image_src, false, "Invalid image_src");

    auto volume_icon = ui_comp_get_child(
                           _main_object->getNativeHandle(),
                           UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERBUTTONS_CONTAINERBUTTONSVOLUME_CONTAINERBUTTONSVOLUMEICON
                       );
    ESP_UTILS_CHECK_NULL_RETURN(volume_icon, false, "Invalid volume_icon");
    lv_obj_set_style_bg_image_src(volume_icon, image_src, 0);
    _volume_level = level;

    return true;
}

bool QuickSettings::setVolume(int percent)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: percent(%d)", percent);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    VolumeLevel level = VolumeLevel::MUTE;
    if (percent > 0) {
        percent = std::clamp(percent, QUICK_SETTINGS_VOLUME_PERCENT_MIN + 1, QUICK_SETTINGS_VOLUME_PERCENT_MAX);
        auto level_interval =
            ((QUICK_SETTINGS_VOLUME_PERCENT_MAX - QUICK_SETTINGS_VOLUME_PERCENT_MIN) / static_cast<int>(VolumeLevel::MAX));
        level = static_cast<VolumeLevel>((percent - QUICK_SETTINGS_VOLUME_PERCENT_MIN + level_interval - 1) / level_interval - 1);
        if (level >= VolumeLevel::MAX) {
            level = VolumeLevel::LEVEL_3;
        }
    }
    ESP_UTILS_CHECK_FALSE_RETURN(setVolume(level), false, "Set volume failed");

    return true;
}

int QuickSettings::getVolumePercent(void) const
{
    if (_volume_level == VolumeLevel::MUTE) {
        return 0;
    } else if (_volume_level == VolumeLevel::LEVEL_3) {
        return QUICK_SETTINGS_VOLUME_PERCENT_MAX;
    }

    auto level_interval =
        ((QUICK_SETTINGS_VOLUME_PERCENT_MAX - QUICK_SETTINGS_VOLUME_PERCENT_MIN) / static_cast<int>(VolumeLevel::MAX));
    return (static_cast<int>(_volume_level) + 1) * level_interval + QUICK_SETTINGS_VOLUME_PERCENT_MIN;
}

bool QuickSettings::setBrightness(BrightnessLevel level)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: level(%d)", level);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    const lv_image_dsc_t *image_src = nullptr;
    switch (level) {
    case BrightnessLevel::LEVEL_1:
        image_src = &speaker_image_middle_quick_settings_brightness_low_48_48;
        break;
    case BrightnessLevel::LEVEL_2:
        image_src = &speaker_image_middle_quick_settings_brightness_medium_48_48;
        break;
    case BrightnessLevel::LEVEL_3:
        image_src = &speaker_image_middle_quick_settings_brightness_high_48_48;
        break;
    default:
        break;
    }
    ESP_UTILS_CHECK_NULL_RETURN(image_src, false, "Invalid image_src");

    auto brightness_icon = ui_comp_get_child(
                               _main_object->getNativeHandle(),
                               UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERBUTTONS_CONTAINERBUTTONSBRIGHTNESS_CONTAINERBUTTONSBRIGHTNESSICON
                           );
    ESP_UTILS_CHECK_NULL_RETURN(brightness_icon, false, "Invalid brightness_icon");
    lv_obj_set_style_bg_image_src(brightness_icon, image_src, 0);
    _brightness_level = level;

    return true;
}

bool QuickSettings::setBrightness(int percent)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: percent(%d)", percent);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    percent = std::clamp(percent, QUICK_SETTINGS_BRIGHTNESS_PERCENT_MIN, QUICK_SETTINGS_BRIGHTNESS_PERCENT_MAX);
    auto level_interval =
        ((QUICK_SETTINGS_BRIGHTNESS_PERCENT_MAX - QUICK_SETTINGS_BRIGHTNESS_PERCENT_MIN) / static_cast<int>(BrightnessLevel::MAX));
    auto level = static_cast<BrightnessLevel>((percent - QUICK_SETTINGS_BRIGHTNESS_PERCENT_MIN) / level_interval);
    if (level >= BrightnessLevel::MAX) {
        level = BrightnessLevel::LEVEL_3;
    }
    ESP_UTILS_CHECK_FALSE_RETURN(setBrightness(level), false, "Set brightness failed");

    return true;
}

int QuickSettings::getBrightnessPercent(void) const
{
    if (_brightness_level == BrightnessLevel::LEVEL_3) {
        return QUICK_SETTINGS_BRIGHTNESS_PERCENT_MAX;
    }

    auto level_interval =
        ((QUICK_SETTINGS_BRIGHTNESS_PERCENT_MAX - QUICK_SETTINGS_BRIGHTNESS_PERCENT_MIN) / static_cast<int>(BrightnessLevel::MAX));
    return (static_cast<int>(_brightness_level) + 1) * level_interval + QUICK_SETTINGS_BRIGHTNESS_PERCENT_MIN;
}

bool QuickSettings::setMemorySRAM(int percent)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: percent(%d)", percent);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    auto memory_sram_bar = ui_comp_get_child(
                               _main_object->getNativeHandle(),
                               UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERMEMORY_CONTAINERMEMORYINTERNAL_CONTAINERMEMORYINTERNALSRAM_BARMEMORYINTERNALSRAMBAR
                           );
    ESP_UTILS_CHECK_NULL_RETURN(memory_sram_bar, false, "Invalid memory_sram_bar");

    percent = std::clamp(percent, QUICK_SETTINGS_MEMORY_SRAM_PERCENT_MIN, QUICK_SETTINGS_MEMORY_SRAM_PERCENT_MAX);
    lv_bar_set_value(memory_sram_bar, percent, LV_ANIM_OFF);
    return true;
}

bool QuickSettings::setMemoryPSRAM(int percent)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: percent(%d)", percent);
    ESP_UTILS_CHECK_FALSE_RETURN(isBegun(), false, "Not begun");

    auto memory_psram_bar = ui_comp_get_child(
                                _main_object->getNativeHandle(),
                                UI_COMP_CONTAINERQUICKSETTINGS_CONTAINERMEMORY_CONTAINERMEMORYINTERNAL_CONTAINERMEMORYINTERNALPSRAM_BARMEMORYINTERNALPSRAMBAR
                            );
    ESP_UTILS_CHECK_NULL_RETURN(memory_psram_bar, false, "Invalid memory_psram_bar");

    percent = std::clamp(percent, QUICK_SETTINGS_MEMORY_PSRAM_PERCENT_MIN, QUICK_SETTINGS_MEMORY_PSRAM_PERCENT_MAX);
    lv_bar_set_value(memory_psram_bar, percent, LV_ANIM_OFF);
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
    const gui::StyleSize &screen_size, const base::Display &display, QuickSettingsData &data
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

} // namespace esp_brookesia::systems::speaker
