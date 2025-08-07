/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string_view>
#include <array>
#include <vector>
#include <map>
#include "esp_brookesia.hpp"
#include "base.hpp"

namespace esp_brookesia::apps {

enum class SettingsUI_ScreenWlanVerificationContainerIndex {
    PASSWORD,
    ADVANCED,
    MAX,
};

enum class SettingsUI_ScreenWlanVerificationCellIndex {
    PASSWORD_EDIT,
    ADVANCED_PROXY,
    ADVANCED_IP,
    MAX,
};

struct SettingsUI_ScreenWlanVerificationData {
    SettingsUI_WidgetCellContainerConf container_confs[(int)SettingsUI_ScreenWlanVerificationContainerIndex::MAX];
    SettingsUI_WidgetCellConf cell_confs[(int)SettingsUI_ScreenWlanVerificationCellIndex::MAX];
    struct {
        ESP_Brookesia_StyleSize_t size;
        uint16_t align_bottom_offset;
        uint16_t top_pad;
        uint16_t bottom_pad;
        uint16_t left_pad;
        uint16_t right_pad;
        ESP_Brookesia_StyleColor_t main_background_color;
        ESP_Brookesia_StyleColor_t normal_button_background_color;
        ESP_Brookesia_StyleColor_t special_button_background_color;
        ESP_Brookesia_StyleColor_t ok_button_disabled_background_color;
        ESP_Brookesia_StyleColor_t ok_button_enabled_background_color;
        ESP_Brookesia_StyleFont_t text_font;
        ESP_Brookesia_StyleColor_t text_color;
    } keyboard;
};

using SettingsUI_ScreenWlanVerificationCellContainerMap =
    SettingsUI_ScreenBaseCellContainerMap<SettingsUI_ScreenWlanVerificationContainerIndex, SettingsUI_ScreenWlanVerificationCellIndex>;

class SettingsUI_ScreenWlanVerification: public SettingsUI_ScreenBase {
public:
    using OnKeyboardConfirmSignal = boost::signals2::signal<void(std::pair<std::string_view, std::string_view> ssid_with_pwd)>;
    using OnKeyboardConfirmSignalSlot = OnKeyboardConfirmSignal::slot_type;

    SettingsUI_ScreenWlanVerification(speaker::App &ui_app, const SettingsUI_ScreenBaseData &base_data,
                                      const SettingsUI_ScreenWlanVerificationData &main_data);
    ~SettingsUI_ScreenWlanVerification();

    bool begin();
    bool del();
    bool processDataUpdate();
    bool resetScreen();
    bool setKeyboardVisible(bool visible);

    static bool calibrateData(
        const ESP_Brookesia_StyleSize_t &parent_size, const ESP_Brookesia_CoreHome &home,
        SettingsUI_ScreenWlanVerificationData &data
    );

    const SettingsUI_ScreenWlanVerificationData &data;
    mutable OnKeyboardConfirmSignal on_keyboard_confirm_signal;

private:
    bool processCellContainerMapInit();
    bool processCellContainerMapUpdate();
    bool processOnTextEditValueChangeEventCallback(lv_event_t *e);
    bool processOnKeyboardValueChangedEventCallback(std::string_view text);
    // bool processOnKeyboardDrawTaskEventCallback(lv_event_t *e);
    bool processOnGestureEventCallback(lv_event_t *e);
    bool processOnScreenLoadEventCallback(lv_event_t *e);
    bool processOnScreenUnloadEventCallback(lv_event_t *e);
    std::string getSSID_Text() const;

    static void onTextEditValueChangeEventCallback(lv_event_t *e);
    // static void onKeyboardAllEventCallback(lv_event_t *e);
    static void onGestureEventCallback(lv_event_t *e);
    static void onScreenLoadEventCallback(lv_event_t *e);
    static void onScreenUnloadEventCallback(lv_event_t *e);

    struct {
        uint8_t text_edit_pressed: 1;
        uint8_t keyboard_ok_enabled: 1;
        uint8_t gesture_enabled: 1;
        uint8_t is_screen_loaded_gesture: 1;
    } _flags = {};
    SettingsUI_ScreenWlanVerificationCellContainerMap _cell_container_map;
};

} // namespace esp_brookesia::speaker
