/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "style/esp_brookesia_gui_style.hpp"
#include "lvgl/esp_brookesia_lv_object.hpp"
#include "systems/core/esp_brookesia_core.hpp"
#include "lvgl/esp_brookesia_lv.hpp"
#include "boost/signals2.hpp"

namespace esp_brookesia::speaker {

struct QuickSettingsData {
    struct {
        gui::StyleSize size;
        gui::StyleAlign align;
    } main;
    struct {
        gui::StyleAnimation::AnimationPathType path_type;
        int speed_px_in_s;
    } animation;
};

class QuickSettings {
public:
    enum class EventType {
        SettingsButtonClicked,
        WifiButtonClicked,
        WifiButtonLongPressed,
        BluetoothButtonClicked,
        BluetoothButtonLongPressed,
        VolumeButtonClicked,
        VolumeButtonLongPressed,
        BrightnessButtonClicked,
        BrightnessButtonLongPressed,
        LockButtonClicked,
    };
    enum class ClockFormat {
        FORMAT_12H,
        FORMAT_24H,
    };
    enum class WifiState {
        CLOSED,
        DISCONNECTED,
        SIGNAL_1,
        SIGNAL_2,
        SIGNAL_3,
    };

    using OnEventSignal = boost::signals2::signal<bool(EventType type)>;
    using OnEventSignalSlot = OnEventSignal::slot_type;

    QuickSettings(ESP_Brookesia_Core &core, const QuickSettingsData &data);
    ~QuickSettings();

    bool begin(const gui::LvObject &parent);
    bool del(void);

    bool setClockFormat(ClockFormat format);
    bool setClockTime(int hour, int minute);
    bool setWifiIconState(WifiState state);

    bool setVisible(bool visible) const;
    bool moveY_To(int pos) const;
    bool moveY_ToWithAnimation(int pos, bool is_visible_when_completed) const;
    bool setAnimationCompletedMethod(gui::LvAnimation::CompletedMethod method) const;
    bool setScrollable(bool enable) const;
    bool scrollBack(void) const;

    bool isBegun(void) const
    {
        return (_main_object != nullptr);
    }
    bool isVisible(void) const
    {
        return (isBegun()) && !_main_object->hasFlags(gui::STYLE_FLAG_HIDDEN);
    }
    bool isAnimationRunning(void) const
    {
        return _animation->isRunning();
    }

    static bool calibrateData(
        const ESP_Brookesia_StyleSize_t &screen_size, const ESP_Brookesia_CoreDisplay &display, QuickSettingsData &data
    );

    mutable OnEventSignal on_event_signal;

private:
    bool updateByNewData(void);

    ESP_Brookesia_Core &_core;
    const QuickSettingsData &_data;

    struct {
        int is_settings_button_long_clicked: 1;
        int is_wifi_button_long_pressed: 1;
        int is_volume_button_long_pressed: 1;
        int is_brightness_button_long_pressed: 1;
    } _flags{};

    int _hour = 0;
    int _minute = 0;
    ClockFormat _clock_format = ClockFormat::FORMAT_12H;

    gui::LvObjectUniquePtr _main_object{nullptr};
    gui::LvAnimationUniquePtr _animation{nullptr};
};

} // namespace esp_brookesia::speaker
