/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "style/esp_brookesia_gui_style.hpp"
#include "lvgl/esp_brookesia_lv_object.hpp"
#include "systems/base/esp_brookesia_base_context.hpp"
#include "lvgl/esp_brookesia_lv.hpp"
#include "boost/signals2.hpp"

namespace esp_brookesia::systems::speaker {

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
    enum class BatteryState {
        CHARGING = -1,
        LEVEL_1,
        LEVEL_2,
        LEVEL_3,
        LEVEL_4,
        MAX,
    };

    enum class EventType {
        WifiButtonClicked,
        WifiButtonLongPressed,
        VolumeButtonClicked,
        VolumeButtonLongPressed,
        BrightnessButtonClicked,
        BrightnessButtonLongPressed,
    };
    struct EventData {
        EventType type;
    };
    using EventSignal = boost::signals2::signal<void(EventData data)>;
    using EventSignalSlot = EventSignal::slot_type;

    enum class VolumeLevel {
        MUTE = -1,
        LEVEL_1,
        LEVEL_2,
        LEVEL_3,
        MAX,
    };

    enum class BrightnessLevel {
        LEVEL_1 = 0,
        LEVEL_2,
        LEVEL_3,
        MAX,
    };

    QuickSettings(base::Context &core, const QuickSettingsData &data);
    ~QuickSettings();

    bool begin(const gui::LvObject &parent);
    bool del(void);

    boost::signals2::connection connectEventSignal(EventSignalSlot slot);

    bool setClockFormat(ClockFormat format);
    bool setClockTime(int hour, int minute);

    bool setWifiIconState(WifiState state);

    bool setBatteryPercent(bool charge_flag, int percent);

    bool setVolume(VolumeLevel level);
    bool setVolume(int percent);
    VolumeLevel getVolumeLevel(void) const
    {
        return _volume_level;
    }
    int getVolumePercent(void) const;

    bool setBrightness(BrightnessLevel level);
    bool setBrightness(int percent);
    BrightnessLevel getBrightnessLevel(void) const
    {
        return _brightness_level;
    }
    int getBrightnessPercent(void) const;

    bool setMemorySRAM(int percent);
    bool setMemoryPSRAM(int percent);

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
    gui::LvObject *getWifiButton(void) const
    {
        return _wifi_button.get();
    }
    gui::LvObject *getVolumeButton(void) const
    {
        return _volume_button.get();
    }
    gui::LvObject *getBrightnessButton(void) const
    {
        return _brightness_button.get();
    }

    static bool calibrateData(
        const gui::StyleSize &screen_size, const base::Display &display, QuickSettingsData &data
    );

private:
    bool updateByNewData(void);

    base::Context &_system_context;
    const QuickSettingsData &_data;

    struct {
        int is_wifi_button_long_pressed: 1;
        int is_volume_button_long_pressed: 1;
        int is_brightness_button_long_pressed: 1;
    } _flags{};
    EventSignal _event_signal;

    int _hour = 0;
    int _minute = 0;
    ClockFormat _clock_format = ClockFormat::FORMAT_12H;

    VolumeLevel _volume_level = VolumeLevel::MUTE;
    BrightnessLevel _brightness_level = BrightnessLevel::LEVEL_1;

    gui::LvObjectUniquePtr _main_object{nullptr};
    gui::LvObjectUniquePtr _wifi_button{nullptr};
    gui::LvObjectUniquePtr _volume_button{nullptr};
    gui::LvObjectUniquePtr _brightness_button{nullptr};
    gui::LvAnimationUniquePtr _animation{nullptr};
};

} // namespace esp_brookesia::systems::speaker
