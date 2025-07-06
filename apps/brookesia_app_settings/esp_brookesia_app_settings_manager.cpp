/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstdio>
#include <algorithm>
#include <chrono>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_pthread.h"
#include "esp_task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "esp_mac.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "app_sntp.h"
#include "private/esp_brookesia_app_settings_utils.hpp"
#include "assets/esp_brookesia_app_settings_assets.h"
#include "esp_brookesia_app_settings_manager.hpp"

#define WLAN_OPERATION_THREAD_NAME              "wlan_operation"
#define WLAN_OPERATION_THREAD_STACK_SIZE        (6 * 1024)
#define WLAN_OPERATION_THREAD_STACK_CAPS_EXT    (true)

#define WLAN_UI_THREAD_NAME                     "wlan_ui"
#define WLAN_UI_THREAD_STACK_SIZE               (8 * 1024)
#define WLAN_UI_THREAD_STACK_CAPS_EXT           (true)

#define WLAN_CONNECT_THREAD_NAME                "wlan_connect"
#define WLAN_CONNECT_THREAD_STACK_SIZE          (6 * 1024)
#define WLAN_CONNECT_THREAD_STACK_CAPS_EXT      (true)

#define WLAN_TIME_SYNC_THREAD_NAME              "wlan_time_sync"
#define WLAN_TIME_SYNC_THREAD_STACK_SIZE        (6 * 1024)
#define WLAN_TIME_SYNC_THREAD_STACK_CAPS_EXT    (true)

// UI screens
#define UI_SCREEN_BACK_MAP() \
    { \
        {UI_Screen::SETTINGS,          UI_Screen::HOME}, \
        {UI_Screen::MEDIA_SOUND,       UI_Screen::SETTINGS}, \
        {UI_Screen::MEDIA_DISPLAY,     UI_Screen::SETTINGS}, \
        {UI_Screen::WIRELESS_WLAN,     UI_Screen::SETTINGS}, \
        {UI_Screen::WLAN_VERIFICATION, UI_Screen::WIRELESS_WLAN}, \
        {UI_Screen::MORE_ABOUT,        UI_Screen::SETTINGS}, \
    }
// UI screen: Settings
#define UI_SCREEN_SETTINGS_WIRELESS_LABEL_TEXT_ON       "On"
#define UI_SCREEN_SETTINGS_WIRELESS_LABEL_TEXT_OFF      "Off"
// // UI screen: WLAN
// #define UI_SCREEN_WLAN_SCAN_TIMER_INTERVAL_MS           (20000)
#define UI_SCREEN_ABOUT_SYSTEM_FIRMWARE_NAME            "EchoEar"
// UI screen: About
#define _VERSION_STR(x, y, z)                           "V" # x "." # y "." # z
#define VERSION_STR(x, y, z)                           _VERSION_STR(x, y, z)
#define UI_SCREEN_ABOUT_SYSTEM_OS_NAME                  "FreeRTOS"
#define UI_SCREEN_ABOUT_SYSTEM_UI_NAME                  "ESP-Brookesia & LVGL"
#define UI_SCREEN_ABOUT_SYSTEM_UI_BROOKESIA_VERSION     VERSION_STR(ESP_BROOKESIA_VER_MAJOR, ESP_BROOKESIA_VER_MINOR ,\
                                                                    ESP_BROOKESIA_VER_PATCH)
#define UI_SCREEN_ABOUT_SYSTEM_UI_LVGL_VERSION          VERSION_STR(LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, \
                                                                    LVGL_VERSION_PATCH)
#define UI_SCREEN_ABOUT_DEVICE_MANUFACTURER             "Espressif"
#define UI_SCREEN_ABOUT_SYSTEM_OS_VERSION               tskKERNEL_VERSION_NUMBER
#define UI_SCREEN_ABOUT_DEVICE_CHIP                     CONFIG_IDF_TARGET

// WLAN
#define WLAN_SW_FLAG_DEFAULT            (0)
#define WLAN_SCAN_ENABLE_DEBUG_LOG      (0)
#define WLAN_INIT_MODE_DEFAULT          WIFI_MODE_STA
#define WLAN_CONFIG_MODE_DEFAULT        WIFI_IF_STA
#define WLAN_CONNECT_RETRY_MAX          (5)
#define WLAN_SCAN_CONNECT_AP_DELAY_MS   (100)
#define WLAN_DISCONNECT_HIDE_TIME_MS    (3000)
#define WLAN_INIT_WAIT_TIMEOUT_MS       (5000)
#define WLAN_START_WAIT_TIMEOUT_MS      (1000)
#define WLAN_STOP_WAIT_TIMEOUT_MS       (1000)
#define WLAN_CONNECT_WAIT_TIMEOUT_MS    (5000)
#define WLAN_DISCONNECT_WAIT_TIMEOUT_MS (5000)
#define WLAN_SCAN_START_WAIT_TIMEOUT_MS (5000)
#define WLAN_SCAN_STOP_WAIT_TIMEOUT_MS  (1000)

using namespace esp_brookesia::speaker;
using namespace esp_brookesia::services;
using namespace esp_utils;

namespace esp_brookesia::speaker_apps {

const std::unordered_map<wifi_event_t, std::string> SettingsManager::_wlan_event_str = {
    {WIFI_EVENT_SCAN_DONE, "WIFI_EVENT_SCAN_DONE"},
    {WIFI_EVENT_STA_START, "WIFI_EVENT_STA_START"},
    {WIFI_EVENT_STA_STOP, "WIFI_EVENT_STA_STOP"},
    {WIFI_EVENT_STA_CONNECTED, "WIFI_EVENT_STA_CONNECTED"},
    {WIFI_EVENT_STA_DISCONNECTED, "WIFI_EVENT_STA_DISCONNECTED"},
};

const std::unordered_map<SettingsManager::WlanGeneraState, std::string> SettingsManager::_wlan_general_state_str = {
    {WlanGeneraState::DEINIT, "DEINIT"},
    {WlanGeneraState::INIT, "INIT"},
    {WlanGeneraState::_START, "_START"},
    {WlanGeneraState::STARTING, "STARTING"},
    {WlanGeneraState::STARTED, "STARTED"},
    {WlanGeneraState::_STOP, "_STOP"},
    {WlanGeneraState::STOPPING, "STOPPING"},
    {WlanGeneraState::STOPPED, "STOPPED"},
    {WlanGeneraState::_CONNECT, "_CONNECT"},
    {WlanGeneraState::CONNECTING, "CONNECTING"},
    {WlanGeneraState::CONNECTED, "CONNECTED"},
    {WlanGeneraState::_DISCONNECT, "_DISCONNECT"},
    {WlanGeneraState::DISCONNECTING, "DISCONNECTING"},
    {WlanGeneraState::DISCONNECTED, "DISCONNECTED"},
};

const std::unordered_map<SettingsManager::WlanScanState, std::string> SettingsManager::_wlan_scan_state_str = {
    {WlanScanState::_SCAN_START, "_SCAN_START"},
    {WlanScanState::SCANNING, "SCANNING"},
    {WlanScanState::SCAN_DONE, "SCAN_DONE"},
    {WlanScanState::SCAN_STOPPED, "STOPPED"},
};

const std::unordered_map<SettingsManager::WlanOperation, std::string> SettingsManager::_wlan_operation_str = {
    {SettingsManager::WlanOperation::NONE, "NONE"},
    {SettingsManager::WlanOperation::INIT, "INIT"},
    {SettingsManager::WlanOperation::DEINIT, "DEINIT"},
    {SettingsManager::WlanOperation::START, "START"},
    {SettingsManager::WlanOperation::STOP, "STOP"},
    {SettingsManager::WlanOperation::SCAN_START, "SCAN_START"},
    {SettingsManager::WlanOperation::SCAN_STOP, "SCAN_STOP"},
    {SettingsManager::WlanOperation::CONNECT, "CONNECT"},
    {SettingsManager::WlanOperation::DISCONNECT, "DISCONNECT"},
};

SettingsManager::SettingsManager(speaker::App &app_in, SettingsUI &ui_in, const SettingsManagerData &data_in):
    app(app_in),
    ui(ui_in),
    data(data_in),
    _ui_current_screen(UI_Screen::HOME),
    _ui_screen_back_map(UI_SCREEN_BACK_MAP()),
    _is_wlan_operation_stopped(true),
    _wlan_event_id(WIFI_EVENT_WIFI_READY),
    _wlan_sta_netif(nullptr),
    _wlan_event_handler_instance(nullptr)
{
}

SettingsManager::~SettingsManager()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);

    if (!checkClosed()) {
        ESP_UTILS_CHECK_FALSE_EXIT(processClose(), "Close failed");
    }
}

bool SettingsManager::processInit()
{
    ESP_UTILS_CHECK_FALSE_RETURN(initWlan(), false, "Init WLAN failed");

    StorageNVS::Value wlan_sw_flag;
    int wlan_sw_flag_int = WLAN_SW_FLAG_DEFAULT;
    if (StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_WLAN_SWITCH, wlan_sw_flag)) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            std::holds_alternative<int>(wlan_sw_flag), false, "Invalid WLAN switch flag type"
        );
        wlan_sw_flag_int = std::get<int>(wlan_sw_flag);
    } else {
        ESP_UTILS_LOGW("WLAN switch flag not found in NVS, set to default value(%d)", static_cast<int>(wlan_sw_flag_int));
        ESP_UTILS_CHECK_FALSE_RETURN(
            StorageNVS::requestInstance().setLocalParam(SETTINGS_NVS_KEY_WLAN_SWITCH, wlan_sw_flag_int), false,
            "Failed to set WLAN switch flag"
        );
    }

    WlanOperation target_operation = WlanOperation::NONE;
    ESP_UTILS_CHECK_FALSE_RETURN(
        app.getSystem()->display.getQuickSettings().setWifiIconState(
            wlan_sw_flag_int ? QuickSettings::WifiState::DISCONNECTED : QuickSettings::WifiState::CLOSED
        ), false, "Set WLAN icon state failed"
    );
    // Force WLAN operation later since the Wlan init may take some time
    target_operation = wlan_sw_flag_int ? WlanOperation::START : WlanOperation::STOP;
    boost::thread([this, target_operation]() {
        // std::this_thread::sleep_for(boost::chrono::milliseconds(WLAN_INIT_WAIT_TIMEOUT_MS));
        if (!forceWlanOperation(target_operation, 0)) {
            ESP_UTILS_LOGE("Force WLAN operation(%d) failed", static_cast<int>(target_operation));
        }
    }).detach();

    StorageNVS::Value wlan_ssid;
    std::string wlan_ssid_str = WLAN_DEFAULT_SSID;
    if (!StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_WLAN_SSID, wlan_ssid)) {
        ESP_UTILS_LOGW("WLAN SSID not found in NVS, set to default value(%s)", wlan_ssid_str.c_str());
        ESP_UTILS_CHECK_FALSE_RETURN(
            StorageNVS::requestInstance().setLocalParam(SETTINGS_NVS_KEY_WLAN_SSID, wlan_ssid_str, !wlan_ssid_str.empty()),
            false, "Failed to set WLAN SSID"
        );
    }

    StorageNVS::Value wlan_password;
    std::string wlan_password_str = WLAN_DEFAULT_PWD;
    if (!StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_WLAN_PASSWORD, wlan_password)) {
        ESP_UTILS_LOGW("WLAN password not found in NVS, set to default value(%s)", wlan_password_str.c_str());
        ESP_UTILS_CHECK_FALSE_RETURN(
            StorageNVS::requestInstance().setLocalParam(SETTINGS_NVS_KEY_WLAN_PASSWORD, wlan_password_str,
                    !wlan_password_str.empty()), false, "Failed to set WLAN password"
        );
    }

    return true;
}

bool SettingsManager::processRun()
{
    ESP_UTILS_LOGD("Process run");
    ESP_UTILS_CHECK_FALSE_RETURN(checkClosed(), false, "Already running");

    ESP_UTILS_CHECK_FALSE_GOTO(processRunUI_ScreenSettings(), err, "Process run UI screen settings failed");
    ESP_UTILS_CHECK_FALSE_GOTO(processRunUI_ScreenWlan(), err, "Process run UI screen WLAN failed");
    ESP_UTILS_CHECK_FALSE_GOTO(processRunUI_ScreenWlanVerification(), err, "Process run UI screen WLAN connect failed");
    ESP_UTILS_CHECK_FALSE_GOTO(processRunUI_ScreenAbout(), err, "Process run UI screen about failed");
    ESP_UTILS_CHECK_FALSE_GOTO(processRunUI_ScreenSound(), err, "Process run UI screen sound failed");
    ESP_UTILS_CHECK_FALSE_GOTO(processRunUI_ScreenDisplay(), err, "Process run UI screen display failed");

    _ui_current_screen = UI_Screen::SETTINGS;
    _is_ui_initialized = true;

    ESP_UTILS_CHECK_FALSE_GOTO(
        updateUI_ScreenWlanAvailable(false), err, "Update UI screen WLAN available failed"
    );
    ESP_UTILS_CHECK_FALSE_GOTO(
        updateUI_ScreenWlanConnected(false), err, "Update UI screen WLAN connected failed"
    );

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(processClose(), false, "Process close failed");

    return false;
}

bool SettingsManager::processBack()
{
    ESP_UTILS_LOGD("Process back");

    lv_obj_t *back_screen = nullptr;
    UI_Screen back_ui = UI_Screen::HOME;

    std::tie(back_ui, back_screen) = getUI_BackScreenObject(_ui_current_screen);
    ESP_UTILS_CHECK_FALSE_RETURN(processUI_ScreenChange(back_ui, back_screen), false, "Process UI screen change failed");

    if (back_ui == UI_Screen::HOME) {
        return false;
    }

    return true;
}

bool SettingsManager::processClose()
{
    ESP_UTILS_LOGD("Process close");
    ESP_UTILS_CHECK_FALSE_RETURN(!checkClosed(), false, "Already closed");

    _is_ui_initialized = false;

    if (!processCloseUI_ScreenSound()) {
        ESP_UTILS_LOGE("Process close UI screen sound failed");
    }
    if (!processCloseUI_ScreenDisplay()) {
        ESP_UTILS_LOGE("Process close UI screen display failed");
    }
    if (!processCloseUI_ScreenAbout()) {
        ESP_UTILS_LOGE("Process close UI screen about failed");
    }
    if (!processCloseUI_ScreenWlan()) {
        ESP_UTILS_LOGE("Process close UI screen WLAN failed");
    }
    if (!processCloseUI_ScreenWlanVerification()) {
        ESP_UTILS_LOGE("Process close UI screen WLAN connect failed");
    }
    if (!processCloseUI_ScreenSettings()) {
        ESP_UTILS_LOGE("Process close UI screen settings failed");
    }

    _ui_current_screen = UI_Screen::HOME;
    _ui_screen_object_map.clear();

    return true;
}

bool SettingsManager::processRunUI_ScreenSettings()
{
    ESP_UTILS_LOGD("Process run UI screen settings");

    // Wireless: WLAN
    SettingsUI_WidgetCell *wlan_cell = ui.screen_settings.getCell(
                                           static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::WIRELESS),
                                           static_cast<int>(SettingsUI_ScreenSettingsCellIndex::WIRELESS_WLAN)
                                       );
    ESP_UTILS_CHECK_NULL_GOTO(wlan_cell, err, "Get cell WLAN failed");
    _ui_screen_object_map[wlan_cell->getEventObject()] = {
        UI_Screen::WIRELESS_WLAN, ui.screen_wlan.getScreenObject()
    };
    ESP_UTILS_CHECK_FALSE_GOTO(
        app.getCore()->getCoreEvent()->registerEvent(
            wlan_cell->getEventObject(), onScreenSettingsCellClickEventHandler, wlan_cell->getClickEventID(), this
        ), err, "Register event failed"
    );
    {
        StorageNVS::Value wlan_sw_flag;
        ESP_UTILS_CHECK_FALSE_RETURN(
            StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_WLAN_SWITCH, wlan_sw_flag), false,
            "Get WLAN switch flag failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            std::holds_alternative<int>(wlan_sw_flag), false, "Invalid WLAN switch flag type"
        );
        auto &wlan_sw_flag_int = std::get<int>(wlan_sw_flag);
        ESP_UTILS_CHECK_FALSE_RETURN(
            wlan_cell->updateRightMainLabel(
                wlan_sw_flag_int ? UI_SCREEN_SETTINGS_WIRELESS_LABEL_TEXT_ON : UI_SCREEN_SETTINGS_WIRELESS_LABEL_TEXT_OFF
            ), false, "Update right main label failed"
        );
    }

    // Media: Sound
    {
        SettingsUI_WidgetCell *sound_cell = ui.screen_settings.getCell(
                                                static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MEDIA),
                                                static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MEDIA_SOUND)
                                            );
        ESP_UTILS_CHECK_NULL_GOTO(sound_cell, err, "Get cell sound failed");
        _ui_screen_object_map[sound_cell->getEventObject()] = {
            UI_Screen::MEDIA_SOUND, ui.screen_sound.getScreenObject()
        };
        ESP_UTILS_CHECK_FALSE_GOTO(
            app.getCore()->getCoreEvent()->registerEvent(
                sound_cell->getEventObject(), onScreenSettingsCellClickEventHandler, sound_cell->getClickEventID(), this
            ), err, "Register event failed"
        );
    }

    // Media: Display
    {
        SettingsUI_WidgetCell *display_cell = ui.screen_settings.getCell(
                static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MEDIA),
                static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MEDIA_DISPLAY)
                                              );
        ESP_UTILS_CHECK_NULL_GOTO(display_cell, err, "Get cell display failed");
        _ui_screen_object_map[display_cell->getEventObject()] = {
            UI_Screen::MEDIA_DISPLAY, ui.screen_display.getScreenObject()
        };
        ESP_UTILS_CHECK_FALSE_GOTO(
            app.getCore()->getCoreEvent()->registerEvent(
                display_cell->getEventObject(), onScreenSettingsCellClickEventHandler, display_cell->getClickEventID(), this
            ), err, "Register event failed"
        );
    }

    // More: About
    {
        SettingsUI_WidgetCell *about_cell = ui.screen_settings.getCell(
                                                static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MORE),
                                                static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MORE_ABOUT)
                                            );
        ESP_UTILS_CHECK_NULL_GOTO(about_cell, err, "Get cell about failed");
        _ui_screen_object_map[about_cell->getEventObject()] = {
            UI_Screen::MORE_ABOUT, ui.screen_about.getScreenObject()
        };
        ESP_UTILS_CHECK_FALSE_GOTO(
            app.getCore()->getCoreEvent()->registerEvent(
                about_cell->getEventObject(), onScreenSettingsCellClickEventHandler, about_cell->getClickEventID(), this
            ), err, "Register event failed"
        );
    }

    // More: Restart
    {
        SettingsUI_WidgetCell *restart_cell = ui.screen_settings.getCell(
                static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MORE),
                static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MORE_RESTORE)
                                              );
        ESP_UTILS_CHECK_NULL_GOTO(restart_cell, err, "Get cell restart failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            app.getCore()->getCoreEvent()->registerEvent(
        restart_cell->getEventObject(), [](const ESP_Brookesia_CoreEvent::HandlerData & data) {
            ESP_UTILS_LOGW("Erase NVS flash and restart system");
            ESP_UTILS_CHECK_FALSE_RETURN(StorageNVS::requestInstance().eraseNVS(-1), false, "Erase NVS failed");
            esp_restart();
            return true;
        }, restart_cell->getClickEventID(), nullptr
            ), err, "Register event failed"
        );
    }

    // More: Developer mode
    {
        SettingsUI_WidgetCell *developer_mode_cell = ui.screen_settings.getCell(
                    static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MORE),
                    static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MORE_DEVELOPER_MODE)
                );
        ESP_UTILS_CHECK_NULL_GOTO(developer_mode_cell, err, "Get cell developer mode failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            app.getCore()->getCoreEvent()->registerEvent(
        developer_mode_cell->getEventObject(), [](const ESP_Brookesia_CoreEvent::HandlerData & data) {
            auto manager = static_cast<SettingsManager *>(data.user_data);

            ESP_UTILS_CHECK_NULL_RETURN(manager, false, "Manager is null");
            ESP_UTILS_CHECK_FALSE_RETURN(
                manager->event_signal(EventType::EnterDeveloperMode, std::monostate()), false,
                "Enter developer mode failed"
            );

            return true;
        }, developer_mode_cell->getClickEventID(), this
            ), err, "Register event failed"
        );
    }

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(processCloseUI_ScreenSettings(), false, "Process close UI screen settings failed");

    return false;
}

bool SettingsManager::processRunUI_ScreenWlan()
{
    ESP_UTILS_LOGD("Process run UI screen WLAN");

    // Process screen header
    ESP_UTILS_CHECK_FALSE_RETURN(
        app.getCore()->getCoreEvent()->registerEvent(
            ui.screen_wlan.getEventObject(), onScreenNavigationClickEventHandler,
            ui.screen_wlan.getNavigaitionClickEventID(), this
        ), false, "Register navigation click event failed"
    );

    // Process WLAN switch
    lv_obj_t *wlan_sw = ui.screen_wlan.getElementObject(
                            static_cast<int>(SettingsUI_ScreenWlanContainerIndex::CONTROL),
                            static_cast<int>(SettingsUI_ScreenWlanCellIndex::CONTROL_SW),
                            SettingsUI_WidgetCellElement::RIGHT_SWITCH
                        );
    ESP_UTILS_CHECK_NULL_GOTO(wlan_sw, err, "Get WLAN switch failed");
    {
        StorageNVS::Value wlan_sw_flag;
        ESP_UTILS_CHECK_FALSE_RETURN(
            StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_WLAN_SWITCH, wlan_sw_flag), false,
            "Get WLAN switch flag failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            std::holds_alternative<int>(wlan_sw_flag), false, "Invalid WLAN switch flag type"
        );
        auto &wlan_sw_flag_int = std::get<int>(wlan_sw_flag);
        if (wlan_sw_flag_int) {
            lv_obj_add_state(wlan_sw, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(wlan_sw, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(wlan_sw, onUI_ScreenWlanControlSwitchChangeEvent, LV_EVENT_VALUE_CHANGED, this);
    }

    // Process WLAN available list
    {
        auto gesture = app.getSystem()->manager.getGesture();
        ESP_UTILS_CHECK_NULL_GOTO(gesture, err, "Get gesture failed");
        lv_obj_add_event_cb(
            gesture->getEventObj(), onUI_ScreenWlanGestureEvent, gesture->getPressingEventCode(), this
        );
        lv_obj_add_event_cb(
            gesture->getEventObj(), onUI_ScreenWlanGestureEvent, gesture->getReleaseEventCode(), this
        );
    }

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(processCloseUI_ScreenWlan(), false, "Process close UI screen WLAN failed");

    return false;
}

bool SettingsManager::processRunUI_ScreenWlanVerification()
{
    ESP_UTILS_LOGD("Process run UI screen WLAN verification");

    // Register Navigation click event
    ESP_UTILS_CHECK_FALSE_GOTO(
        app.getCore()->getCoreEvent()->registerEvent(
            ui.screen_wlan_verification.getEventObject(), onScreenNavigationClickEventHandler,
            ui.screen_wlan_verification.getNavigaitionClickEventID(), this
        ), err, "Register navigation click event failed"
    );

    // Register keyboard confirm event
    ui.screen_wlan_verification.on_keyboard_confirm_signal.connect([this](std::pair<std::string_view, std::string_view> ssid_with_pwd) {
        ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

        ESP_UTILS_CHECK_FALSE_EXIT(
            processOnUI_ScreenWlanVerificationKeyboardConfirmEvent(ssid_with_pwd),
            "Process on UI screen WLAN connect keyboard confirm event failed"
        );

        ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    });

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(processCloseUI_ScreenWlanVerification(), false, "Process close UI screen WLAN connect failed");

    return false;
}

bool SettingsManager::processRunUI_ScreenAbout()
{
    ESP_UTILS_LOGD("Process run UI screen about");

    char str_buffer[128] = {0};

    ESP_UTILS_CHECK_FALSE_GOTO(
        app.getCore()->getCoreEvent()->registerEvent(
            ui.screen_about.getEventObject(), onScreenNavigationClickEventHandler,
            ui.screen_about.getNavigaitionClickEventID(), this
        ), err, "Register navigation click event failed"
    );

    {
        auto cell_firmware = ui.screen_about.getCell(
                                 static_cast<int>(SettingsUI_ScreenAboutContainerIndex::SYSTEM),
                                 static_cast<int>(SettingsUI_ScreenAboutCellIndex::SYSTEM_FIRMWARE_VERSION)
                             );
        ESP_UTILS_CHECK_NULL_GOTO(cell_firmware, err, "Get cell firmware failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_firmware->updateRightMainLabel(CONFIG_APP_PROJECT_VER), err,
            "Cell firmware update failed"
        );
    }
    {
        auto cell_os_name = ui.screen_about.getCell(
                                static_cast<int>(SettingsUI_ScreenAboutContainerIndex::SYSTEM),
                                static_cast<int>(SettingsUI_ScreenAboutCellIndex::SYSTEM_OS_NAME)
                            );
        ESP_UTILS_CHECK_NULL_GOTO(cell_os_name, err, "Get cell OS name failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_os_name->updateRightMainLabel(UI_SCREEN_ABOUT_SYSTEM_OS_NAME), err, "Cell OS name update failed"
        );
    }
    {
        auto cell_os_version = ui.screen_about.getCell(
                                   static_cast<int>(SettingsUI_ScreenAboutContainerIndex::SYSTEM),
                                   static_cast<int>(SettingsUI_ScreenAboutCellIndex::SYSTEM_OS_VERSION)
                               );
        ESP_UTILS_CHECK_NULL_GOTO(cell_os_version, err, "Get cell OS version failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_os_version->updateRightMainLabel(UI_SCREEN_ABOUT_SYSTEM_OS_VERSION), err, "Cell OS version update failed"
        );
    }
    {
        auto cell_ui_name = ui.screen_about.getCell(
                                static_cast<int>(SettingsUI_ScreenAboutContainerIndex::SYSTEM),
                                static_cast<int>(SettingsUI_ScreenAboutCellIndex::SYSTEM_UI_NAME)
                            );
        ESP_UTILS_CHECK_NULL_GOTO(cell_ui_name, err, "Get cell UI name failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_ui_name->updateRightMainLabel(UI_SCREEN_ABOUT_SYSTEM_UI_NAME), err,
            "Cell UI name update failed"
        );
    }
    {
        auto cell_ui_version = ui.screen_about.getCell(
                                   static_cast<int>(SettingsUI_ScreenAboutContainerIndex::SYSTEM),
                                   static_cast<int>(SettingsUI_ScreenAboutCellIndex::SYSTEM_UI_VERSION)
                               );
        ESP_UTILS_CHECK_NULL_GOTO(cell_ui_version, err, "Get cell UI version failed");
        snprintf(
            str_buffer, sizeof(str_buffer) - 1, "%s & %s",
            UI_SCREEN_ABOUT_SYSTEM_UI_BROOKESIA_VERSION, UI_SCREEN_ABOUT_SYSTEM_UI_LVGL_VERSION
        );
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_ui_version->updateRightMainLabel(str_buffer), err, "Cell UI version update failed"
        );
    }

    {
        auto cell_manufacturer = ui.screen_about.getCell(
                                     static_cast<int>(SettingsUI_ScreenAboutContainerIndex::DEVICE),
                                     static_cast<int>(SettingsUI_ScreenAboutCellIndex::DEVICE_MANUFACTURER)
                                 );
        ESP_UTILS_CHECK_NULL_GOTO(cell_manufacturer, err, "Get cell manufacturer failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_manufacturer->updateRightMainLabel(UI_SCREEN_ABOUT_DEVICE_MANUFACTURER), err,
            "Cell manufacturer update failed"
        );

        auto cell_board = ui.screen_about.getCell(
                              static_cast<int>(SettingsUI_ScreenAboutContainerIndex::DEVICE),
                              static_cast<int>(SettingsUI_ScreenAboutCellIndex::DEVICE_NAME)
                          );
        ESP_UTILS_CHECK_NULL_GOTO(cell_board, err, "Get cell board failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_board->updateRightMainLabel(data.about.device_board_name), err, "Cell board update failed"
        );
    }

    {
        snprintf(
            str_buffer, sizeof(str_buffer) - 1, "%dx%d", app.getCoreActiveData().screen_size.width,
            app.getCoreActiveData().screen_size.height
        );
        auto cell_resolution = ui.screen_about.getCell(
                                   static_cast<int>(SettingsUI_ScreenAboutContainerIndex::DEVICE),
                                   static_cast<int>(SettingsUI_ScreenAboutCellIndex::DEVICE_RESOLUTION)
                               );
        ESP_UTILS_CHECK_NULL_GOTO(cell_resolution, err, "Get cell resolution failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_resolution->updateRightMainLabel(str_buffer), err, "Cell resolution update failed"
        );
    }

    {
        uint32_t flash_size;
        ESP_UTILS_CHECK_FALSE_RETURN(
            esp_flash_get_size(NULL, &flash_size) == ESP_OK, false, "Get flash size failed"
        );
        snprintf(str_buffer, sizeof(str_buffer) - 1, "%dMB", static_cast<int>((flash_size / (1024 * 1024))));
        auto cell_flash_size = ui.screen_about.getCell(
                                   static_cast<int>(SettingsUI_ScreenAboutContainerIndex::DEVICE),
                                   static_cast<int>(SettingsUI_ScreenAboutCellIndex::DEVICE_FLASH_SIZE)
                               );
        ESP_UTILS_CHECK_NULL_GOTO(cell_flash_size, err, "Get cell flash size failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_flash_size->updateRightMainLabel(str_buffer), err, "Cell flash size update failed"
        );
    }

    {
        auto cell_ram_size = ui.screen_about.getCell(
                                 static_cast<int>(SettingsUI_ScreenAboutContainerIndex::DEVICE),
                                 static_cast<int>(SettingsUI_ScreenAboutCellIndex::DEVICE_RAM_SIZE)
                             );
        ESP_UTILS_CHECK_NULL_GOTO(cell_ram_size, err, "Get cell RAM size failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_ram_size->updateRightMainLabel(data.about.device_ram_main), err, "Cell RAM size update failed"
        );
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_ram_size->updateRightMinorLabel(data.about.device_ram_minor), err, "Cell RAM size update failed"
        );
    }

    {
        auto cell_chip_name = ui.screen_about.getCell(
                                  static_cast<int>(SettingsUI_ScreenAboutContainerIndex::CHIP),
                                  static_cast<int>(SettingsUI_ScreenAboutCellIndex::CHIP_NAME)
                              );
        ESP_UTILS_CHECK_NULL_GOTO(cell_chip_name, err, "Get cell chip name failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_chip_name->updateRightMainLabel(UI_SCREEN_ABOUT_DEVICE_CHIP), err, "Cell chip name update failed"
        );
    }

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    {
        unsigned major_rev = chip_info.revision / 100;
        unsigned minor_rev = chip_info.revision % 100;
        snprintf(str_buffer, sizeof(str_buffer) - 1, "V%d.%d", major_rev, minor_rev);
        auto cell_chip_revision = ui.screen_about.getCell(
                                      static_cast<int>(SettingsUI_ScreenAboutContainerIndex::CHIP),
                                      static_cast<int>(SettingsUI_ScreenAboutCellIndex::CHIP_VERSION)
                                  );
        ESP_UTILS_CHECK_NULL_GOTO(cell_chip_revision, err, "Get cell chip revision failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_chip_revision->updateRightMainLabel(str_buffer), err, "Cell chip revision update failed"
        );
    }

    {
        uint8_t mac[6] = {0};
        ESP_UTILS_CHECK_FALSE_GOTO(esp_efuse_mac_get_default(mac) == ESP_OK, err, "Get MAC address failed");

        snprintf(str_buffer, sizeof(str_buffer) - 1, "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
                );
        auto cell_chip_mac = ui.screen_about.getCell(
                                 static_cast<int>(SettingsUI_ScreenAboutContainerIndex::CHIP),
                                 static_cast<int>(SettingsUI_ScreenAboutCellIndex::CHIP_MAC)
                             );
        ESP_UTILS_CHECK_NULL_GOTO(cell_chip_mac, err, "Get cell chip MAC address failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_chip_mac->updateRightMainLabel(str_buffer), err, "Cell chip MAC address update failed"
        );
    }

    {
        snprintf(str_buffer, sizeof(str_buffer) - 1, "%d CPU cores", chip_info.cores);
        auto cell_chip_features = ui.screen_about.getCell(
                                      static_cast<int>(SettingsUI_ScreenAboutContainerIndex::CHIP),
                                      static_cast<int>(SettingsUI_ScreenAboutCellIndex::CHIP_FEATURES)
                                  );
        ESP_UTILS_CHECK_NULL_GOTO(cell_chip_features, err, "Get cell chip features failed");
        ESP_UTILS_CHECK_FALSE_GOTO(
            cell_chip_features->updateRightMainLabel(str_buffer), err, "Cell chip features update failed"
        );
    }

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(processCloseUI_ScreenAbout(), false, "Process close UI screen about failed");

    return false;
}

bool SettingsManager::updateUI_ScreenWlanConnected(bool use_current, WlanGeneraState current_state)
{
    ESP_UTILS_LOGD(
        "Update UI screen WLAN connected (%d, state: %s)", use_current,
        use_current ? getWlanGeneralStateStr(current_state) : getWlanGeneralStateStr(_wlan_general_state)
    );

    auto checkIsWlanGeneralState = [this, current_state, use_current](WlanGeneraState state) -> bool {
        return use_current ? ((current_state & state) == state) : (this->checkIsWlanGeneralState(state));
    };

    SettingsUI_ScreenWlan::WlanData data = {};
    if (!checkIsWlanGeneralState(WlanGeneraState::_CONNECT)) {
        if (checkIsWlanGeneralState(WlanGeneraState::_DISCONNECT)) {
            ESP_UTILS_LOGD(
                "WLAN is not connected, show disconnect and hide after %d ms", WLAN_DISCONNECT_HIDE_TIME_MS
            );
            ESP_UTILS_CHECK_FALSE_RETURN(
                ui.screen_wlan.updateConnectedState(SettingsUI_ScreenWlan::ConnectState::DISCONNECT), false,
                "Update WLAN connect state failed"
            );
            boost::thread([this]() {
                boost::this_thread::sleep_for(boost::chrono::milliseconds(WLAN_DISCONNECT_HIDE_TIME_MS));

                if (_is_ui_initialized && !this->checkIsWlanGeneralState(WlanGeneraState::_CONNECT)) {
                    app.getCore()->lockLv();
                    if (!ui.screen_wlan.setConnectedVisible(false)) {
                        ESP_UTILS_LOGE("Set WLAN connect visible failed");
                    }
                    app.getCore()->unlockLv();
                }
            }).detach();
        } else {
            ESP_UTILS_LOGD("Hide WLAN connect");
            ESP_UTILS_CHECK_FALSE_RETURN(
                ui.screen_wlan.setConnectedVisible(false), false, "Set WLAN connect visible failed"
            );
        }

        return true;
    }

    SettingsUI_ScreenWlan::ConnectState state = SettingsUI_ScreenWlan::ConnectState::DISCONNECT;
    ESP_UTILS_LOGD("Show WLAN connect");

    if (checkIsWlanGeneralState(WlanGeneraState::CONNECTED)) {
        wifi_ap_record_t ap_info = {};
        ESP_UTILS_CHECK_FALSE_RETURN(esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK, false, "Get AP info failed");
        data = getWlanDataFromApInfo(ap_info);
        _wlan_connected_info.first = data;
        state = SettingsUI_ScreenWlan::ConnectState::CONNECTED;
    } else if (checkIsWlanGeneralState(WlanGeneraState::CONNECTING)) {
        data = _wlan_connecting_info.first;
        state = SettingsUI_ScreenWlan::ConnectState::CONNECTING;
    }
    ESP_UTILS_CHECK_FALSE_RETURN(
        ui.screen_wlan.updateConnectedData(data), false, "Update WLAN connect data failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        ui.screen_wlan.updateConnectedState(state), false, "Update WLAN connect state failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        ui.screen_wlan.setConnectedVisible(true), false, "Set WLAN connect visible failed"
    );

    return true;
}

bool SettingsManager::updateUI_ScreenWlanAvailable(bool use_current, WlanGeneraState current_state)
{
    ESP_UTILS_LOGI(
        "Update UI screen WLAN available (%d, state: %s)", use_current,
        use_current ? getWlanGeneralStateStr(current_state) : getWlanGeneralStateStr(_wlan_general_state)
    );

    auto checkIsWlanGeneralState = [this, current_state, use_current](WlanGeneraState state) -> bool {
        return use_current ? ((current_state & state) == state) : (this->checkIsWlanGeneralState(state));
    };

    if (checkIsWlanGeneralState(WlanGeneraState::_CONNECT)) {
        if (!_ui_wlan_available_data.empty()) {
            _ui_wlan_available_data.erase(
                std::remove_if(
                    _ui_wlan_available_data.begin(), _ui_wlan_available_data.end(),
            [this](const SettingsUI_ScreenWlan::WlanData & data) {
                return (data.ssid == _wlan_connecting_info.first.ssid) || (data.ssid == _wlan_connected_info.first.ssid);
            }
                )
            );
        }
    } else if (!checkIsWlanGeneralState(WlanGeneraState::_START)) {
        _ui_wlan_available_data.clear();
    }
    ESP_UTILS_CHECK_FALSE_RETURN(
        ui.screen_wlan.updateAvailableData(_ui_wlan_available_data, onUI_ScreenWlanAvailableCellClickEventHander, this),
        false, "Update WLAN available data failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        ui.screen_wlan.setAvailableVisible(checkIsWlanGeneralState(WlanGeneraState::_START)), false,
        "Set WLAN available visible failed"
    );

    return true;
}


bool SettingsManager::scrollUI_ScreenWlanConnectedToView()
{
    ESP_UTILS_LOGD("Show WLAN connected");

    ESP_UTILS_CHECK_FALSE_RETURN(
        ui.screen_wlan.scrollConnectedToView(), false, "Scroll WLAN connect to view failed"
    );

    return true;
}

bool SettingsManager::onUI_ScreenWlanAvailableCellClickEventHander(const ESP_Brookesia_CoreEvent::HandlerData &data)
{
    ESP_UTILS_LOGD("On UI screen WLAN available cell clicked event handler");

    SettingsManager *manager = (SettingsManager *)data.user_data;
    ESP_UTILS_CHECK_NULL_RETURN(manager, false, "Invalid manager");

    ESP_UTILS_CHECK_FALSE_RETURN(
        manager->processOnUI_ScreenWlanAvailableCellClickEvent(data), false,
        "Process on UI screen WLAN available cell clicked event failed"
    );

    return true;
}

void SettingsManager::onUI_ScreenWlanGestureEvent(lv_event_t *e)
{
    ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");
    // ESP_UTILS_LOGD("On UI screen WLAN gesture press event");

    SettingsManager *manager = (SettingsManager *)lv_event_get_user_data(e);
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid app pointer");

    if (manager->checkClosed()) {
        return;
    }

    ESP_UTILS_CHECK_FALSE_EXIT(
        manager->processOnUI_ScreenWlanGestureEvent(e),
    );
}

void SettingsManager::onUI_ScreenWlanControlSwitchChangeEvent(lv_event_t *e)
{
    ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");
    ESP_UTILS_LOGD("On UI screen WLAN control switch value changed event");

    SettingsManager *manager = (SettingsManager *)lv_event_get_user_data(e);
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid app pointer");

    ESP_UTILS_CHECK_FALSE_EXIT(
        manager->processOnUI_ScreenWlanControlSwitchChangeEvent(e),
        "Process on UI screen WLAN control switch value changed event failed"
    );
}

SettingsUI_ScreenBase *SettingsManager::getUI_Screen(const UI_Screen &ui_screen)
{
    switch (ui_screen) {
    case UI_Screen::SETTINGS:
        return &ui.screen_settings;
    case UI_Screen::MEDIA_SOUND:
        return &ui.screen_sound;
    case UI_Screen::MEDIA_DISPLAY:
        return &ui.screen_display;
    case UI_Screen::WIRELESS_WLAN:
        return &ui.screen_wlan;
    case UI_Screen::MORE_ABOUT:
        return &ui.screen_about;
    default:
        break;
    }

    return nullptr;
}

std::pair<SettingsManager::UI_Screen, lv_obj_t *> SettingsManager::getUI_BackScreenObject(const UI_Screen &ui_screen)
{
    auto screen_back_it = _ui_screen_back_map.find(ui_screen);
    if (screen_back_it == _ui_screen_back_map.end()) {
        return std::make_pair(UI_Screen::HOME, nullptr);
    }

    lv_obj_t *back_screen = nullptr;
    switch (screen_back_it->second) {
    case UI_Screen::HOME:
        back_screen = nullptr;
        break;
    case UI_Screen::SETTINGS:
        back_screen = ui.screen_settings.getScreenObject();
        break;
    case UI_Screen::MEDIA_SOUND:
        back_screen = ui.screen_sound.getScreenObject();
        break;
    case UI_Screen::MEDIA_DISPLAY:
        back_screen = ui.screen_display.getScreenObject();
        break;
    case UI_Screen::WIRELESS_WLAN:
        back_screen = ui.screen_wlan.getScreenObject();
        break;
    case UI_Screen::MORE_ABOUT:
        back_screen = ui.screen_about.getScreenObject();
        break;
    default:
        ESP_UTILS_LOGE("Invalid screen");
        break;
    }

    return std::make_pair(screen_back_it->second, back_screen);
}

bool SettingsManager::onScreenSettingsCellClickEventHandler(const ESP_Brookesia_CoreEvent::HandlerData &data)
{
    ESP_UTILS_CHECK_NULL_RETURN(data.object, false, "Invalid object");
    ESP_UTILS_CHECK_NULL_RETURN(data.user_data, false, "Invalid user data");
    ESP_UTILS_LOGD("On screen settings cell clicked event handler");

    SettingsManager *manager = static_cast<SettingsManager *>(data.user_data);
    auto ui_screen_object_it = manager->_ui_screen_object_map.find(static_cast<lv_obj_t *>(data.object));
    ESP_UTILS_CHECK_FALSE_RETURN(ui_screen_object_it != manager->_ui_screen_object_map.end(), false,
                                 "Invalid screen");

    ESP_UTILS_CHECK_FALSE_RETURN(
        manager->processUI_ScreenChange(ui_screen_object_it->second.first, ui_screen_object_it->second.second),
        false, "Process UI screen change failed"
    );

    return true;
}


bool SettingsManager::processCloseUI_ScreenSettings()
{
    // Container Wireless
    SettingsUI_WidgetCell *wlan_cell = ui.screen_settings.getCell(
                                           static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::WIRELESS),
                                           static_cast<int>(SettingsUI_ScreenSettingsCellIndex::WIRELESS_WLAN)
                                       );
    ESP_UTILS_CHECK_NULL_RETURN(wlan_cell, false, "Get cell WLAN failed");
    app.getCore()->getCoreEvent()->unregisterEvent(wlan_cell->getEventObject(), onScreenSettingsCellClickEventHandler,
            wlan_cell->getClickEventID());

    // Container More
    SettingsUI_WidgetCell *about_cell = ui.screen_settings.getCell(
                                            static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MORE),
                                            static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MORE_ABOUT)
                                        );
    ESP_UTILS_CHECK_NULL_RETURN(about_cell, false, "Get cell about failed");
    app.getCore()->getCoreEvent()->unregisterEvent(about_cell->getEventObject(), onScreenSettingsCellClickEventHandler,
            about_cell->getClickEventID());

    return true;
}

bool SettingsManager::processRunUI_ScreenSound()
{
    ESP_UTILS_LOGD("Process run UI screen sound");

    ESP_UTILS_CHECK_FALSE_GOTO(
        app.getCore()->getCoreEvent()->registerEvent(
            ui.screen_sound.getEventObject(), onScreenNavigationClickEventHandler,
            ui.screen_sound.getNavigaitionClickEventID(), this
        ), err, "Register navigation click event failed"
    );

    {
        auto volume_slider = ui.screen_sound.getElementObject(
                                 static_cast<int>(SettingsUI_ScreenSoundContainerIndex::VOLUME),
                                 static_cast<int>(SettingsUI_ScreenSoundCellIndex::VOLUME_SLIDER),
                                 SettingsUI_WidgetCellElement::CENTER_SLIDER
                             );
        ESP_UTILS_CHECK_NULL_GOTO(volume_slider, err, "Get cell volume slider failed");

        int volume = 0;
        if (!getSoundVolume(volume)) {
            ESP_UTILS_LOGE("Get volume failed, skip");
            return true;
        }
        if (!setSoundVolume(volume, &volume)) {
            ESP_UTILS_LOGE("Set sound volume failed, skip");
            return true;
        }

        lv_obj_add_event_cb(volume_slider, onUI_ScreenSoundVolumeSliderValueChangeEvent, LV_EVENT_VALUE_CHANGED, this);
    }

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(
        processCloseUI_ScreenSound(), false, "Process close UI screen sound failed"
    );

    return true;
}

bool SettingsManager::processOnUI_ScreenSoundVolumeSliderValueChangeEvent(lv_event_t *e)
{
    ESP_UTILS_LOGD("Process on UI screen sound volume slider value change event");

    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    ESP_UTILS_CHECK_NULL_RETURN(slider, false, "Invalid slider");

    int target_value = lv_slider_get_value(slider);
    int out_volume = 0;

    if (!setSoundVolume(target_value, &out_volume)) {
        ESP_UTILS_LOGE("Set sound volume failed, set slider back to %d", static_cast<int>(out_volume));
        lv_slider_set_value(slider, out_volume, LV_ANIM_OFF);
    }

    return true;
}

void SettingsManager::onUI_ScreenSoundVolumeSliderValueChangeEvent(lv_event_t *e)
{
    ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");
    ESP_UTILS_LOGD("On UI screen sound volume slider value change event");

    SettingsManager *manager = (SettingsManager *)lv_event_get_user_data(e);
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid app pointer");

    ESP_UTILS_CHECK_FALSE_EXIT(
        manager->processOnUI_ScreenSoundVolumeSliderValueChangeEvent(e),
        "Process on UI screen sound volume slider value change event failed"
    );
}

bool SettingsManager::processCloseUI_ScreenSound()
{
    app.getCore()->getCoreEvent()->unregisterEvent(
        ui.screen_sound.getEventObject(), onScreenNavigationClickEventHandler,
        ui.screen_sound.getNavigaitionClickEventID()
    );

    return true;
}

bool SettingsManager::processRunUI_ScreenDisplay()
{
    ESP_UTILS_LOGD("Process run UI screen display");

    ESP_UTILS_CHECK_FALSE_GOTO(
        app.getCore()->getCoreEvent()->registerEvent(
            ui.screen_display.getEventObject(), onScreenNavigationClickEventHandler,
            ui.screen_display.getNavigaitionClickEventID(), this
        ), err, "Register navigation click event failed"
    );

    {
        auto brightness_slider = ui.screen_display.getElementObject(
                                     static_cast<int>(SettingsUI_ScreenDisplayContainerIndex::BRIGHTNESS),
                                     static_cast<int>(SettingsUI_ScreenDisplayCellIndex::BRIGHTNESS_SLIDER),
                                     SettingsUI_WidgetCellElement::CENTER_SLIDER
                                 );
        ESP_UTILS_CHECK_NULL_GOTO(brightness_slider, err, "Get cell display slider failed");

        int brightness = 0;
        if (!getDisplayBrightness(brightness)) {
            ESP_UTILS_LOGE("Get brightness failed, skip");
            return true;
        }
        if (!setDisplayBrightness(brightness, &brightness)) {
            ESP_UTILS_LOGE("Set brightness failed, skip");
            return true;
        }

        lv_obj_add_event_cb(
            brightness_slider, onUI_ScreenDisplayBrightnessSliderValueChangeEvent, LV_EVENT_VALUE_CHANGED, this
        );
    }

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(
        processCloseUI_ScreenDisplay(), false, "Process close UI screen display failed"
    );

    return true;
}

bool SettingsManager::processOnUI_ScreenDisplayBrightnessSliderValueChangeEvent(lv_event_t *e)
{
    ESP_UTILS_LOGD("Process on UI screen display brightness slider value change event");

    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    ESP_UTILS_CHECK_NULL_RETURN(slider, false, "Invalid slider");

    int target_value = lv_slider_get_value(slider);
    int out_brightness = 0;

    if (!setDisplayBrightness(target_value, &out_brightness)) {
        ESP_UTILS_LOGE("Set brightness failed, set slider back to %d", static_cast<int>(out_brightness));
        lv_slider_set_value(slider, out_brightness, LV_ANIM_OFF);
    }

    return true;
}

void SettingsManager::onUI_ScreenDisplayBrightnessSliderValueChangeEvent(lv_event_t *e)
{
    ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");
    ESP_UTILS_LOGD("On UI screen display brightness slider value change event");

    SettingsManager *manager = (SettingsManager *)lv_event_get_user_data(e);
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid app pointer");

    ESP_UTILS_CHECK_FALSE_EXIT(
        manager->processOnUI_ScreenDisplayBrightnessSliderValueChangeEvent(e),
        "Process on UI screen display_brightness slider value change event failed"
    );
}

bool SettingsManager::processCloseUI_ScreenDisplay()
{
    app.getCore()->getCoreEvent()->unregisterEvent(
        ui.screen_display.getEventObject(), onScreenNavigationClickEventHandler,
        ui.screen_display.getNavigaitionClickEventID()
    );

    return true;
}

bool SettingsManager::processCloseUI_ScreenAbout()
{
    app.getCore()->getCoreEvent()->unregisterEvent(
        ui.screen_about.getEventObject(), onScreenNavigationClickEventHandler,
        ui.screen_about.getNavigaitionClickEventID()
    );

    return true;
}

// SettingsManager::WlanGeneraState operator|(
//     SettingsManager::WlanGeneraState lhs, SettingsManager::WlanGeneraState rhs
// )
// {
//     using T = std::underlying_type_t<SettingsManager::WlanGeneraState>;
//     return static_cast<SettingsManager::WlanGeneraState>(static_cast<T>(lhs) | static_cast<T>(rhs));
// }

// SettingsManager::WlanGeneraState operator&(
//     SettingsManager::WlanGeneraState lhs, SettingsManager::WlanGeneraState rhs
// )
// {
//     using T = std::underlying_type_t<SettingsManager::WlanGeneraState>;
//     return static_cast<SettingsManager::WlanGeneraState>(static_cast<T>(lhs) & static_cast<T>(rhs));
// }

// SettingsManager::WlanGeneraState &operator|=(
//     SettingsManager::WlanGeneraState &lhs, SettingsManager::WlanGeneraState rhs
// )
// {
//     using T = std::underlying_type_t<SettingsManager::WlanGeneraState>;
//     lhs = static_cast<SettingsManager::WlanGeneraState>(static_cast<T>(lhs) | static_cast<T>(rhs));
//     return lhs;
// }

// SettingsManager::WlanGeneraState &operator&=(
//     SettingsManager::WlanGeneraState &lhs, SettingsManager::WlanGeneraState rhs
// )
// {
//     using T = std::underlying_type_t<SettingsManager::WlanGeneraState>;
//     lhs = static_cast<SettingsManager::WlanGeneraState>(static_cast<T>(lhs) & static_cast<T>(rhs));
//     return lhs;
// }

bool SettingsManager::processUI_ScreenChange(const UI_Screen &ui_screen, lv_obj_t *ui_screen_object)
{
    ESP_UTILS_LOGD(
        "Process UI screen change(%d -> %d)", static_cast<int>(_ui_current_screen), static_cast<int>(ui_screen)
    );

    if (_ui_current_screen == ui_screen) {
        ESP_UTILS_LOGW("Same screen, ignore");
        return true;
    }

    UI_Screen last_screen = _ui_current_screen;
    if (ui_screen_object != nullptr) {
        lv_scr_load(ui_screen_object);
    }

    _ui_current_screen = ui_screen;

    auto screen = getUI_Screen(ui_screen);
    if (screen != nullptr) {
        lv_obj_scroll_to_y(screen->getObject(SettingsUI_ScreenBaseObject::CONTENT_OBJECT), 0, LV_ANIM_OFF);
    }

    if (checkIsWlanGeneralState(WlanGeneraState::_START) && (last_screen != UI_Screen::WLAN_VERIFICATION) &&
            (_ui_current_screen == UI_Screen::WIRELESS_WLAN)) {
        toggleWlanScanTimer(true, true);
    }

    return true;
}

bool SettingsManager::onScreenNavigationClickEventHandler(const ESP_Brookesia_CoreEvent::HandlerData &data)
{
    ESP_UTILS_LOGD("Navigation click event");
    ESP_UTILS_CHECK_NULL_RETURN(data.user_data, false, "Invalid user data");

    SettingsManager *manager = static_cast<SettingsManager *>(data.user_data);
    ESP_UTILS_CHECK_FALSE_RETURN(manager->processBack(), false, "Process back failed");

    return true;
}

bool SettingsManager::setSoundVolume(int in_volume, int *out_volume)
{
    ESP_UTILS_LOGD("Set sound volume(%d)", static_cast<int>(in_volume));

    ESP_UTILS_CHECK_FALSE_RETURN(
        event_signal(EventType::SetSoundVolume, in_volume), false, "Set media sound volume failed"
    );

    int ret_volume = in_volume;
    ESP_UTILS_CHECK_FALSE_RETURN(
        event_signal(EventType::GetSoundVolume, std::ref(ret_volume)), false, "Get media sound volume failed"
    );
    if (ret_volume != in_volume) {
        ESP_UTILS_LOGW("Mismatch volume, target(%d), actual(%d)", in_volume, ret_volume);
    }

    if (out_volume != nullptr) {
        *out_volume = ret_volume;
    }

    if (ui.checkInitialized()) {
        auto volume_slider = ui.screen_sound.getElementObject(
                                 static_cast<int>(SettingsUI_ScreenSoundContainerIndex::VOLUME),
                                 static_cast<int>(SettingsUI_ScreenSoundCellIndex::VOLUME_SLIDER),
                                 SettingsUI_WidgetCellElement::CENTER_SLIDER
                             );
        ESP_UTILS_CHECK_NULL_RETURN(volume_slider, false, "Get cell volume slider failed");
        lv_slider_set_value(volume_slider, ret_volume, LV_ANIM_OFF);
    }

    return true;
}

bool SettingsManager::getSoundVolume(int &volume)
{
    int ret_volume = 0;
    ESP_UTILS_CHECK_FALSE_RETURN(
        event_signal(EventType::GetSoundVolume, std::ref(ret_volume)), false, "Get media sound volume failed"
    );

    ESP_UTILS_CHECK_FALSE_RETURN(ret_volume >= 0, false, "Invalid volume");
    volume = ret_volume;

    return true;
}

bool SettingsManager::setDisplayBrightness(int in_brightness, int *out_brightness)
{
    ESP_UTILS_LOGD("Set display brightness(%d)", static_cast<int>(in_brightness));

    ESP_UTILS_CHECK_FALSE_RETURN(
        event_signal(EventType::SetDisplayBrightness, in_brightness), false, "Set media display brightness failed"
    );

    int ret_brightness = in_brightness;
    ESP_UTILS_CHECK_FALSE_RETURN(
        event_signal(EventType::GetDisplayBrightness, std::ref(ret_brightness)), false, "Get media display brightness failed"
    );
    if (ret_brightness != in_brightness) {
        ESP_UTILS_LOGW("Mismatch brightness, target(%d), actual(%d)", in_brightness, ret_brightness);
    }

    if (out_brightness != nullptr) {
        *out_brightness = ret_brightness;
    }

    if (ui.checkInitialized()) {
        auto brightness_slider = ui.screen_display.getElementObject(
                                     static_cast<int>(SettingsUI_ScreenDisplayContainerIndex::BRIGHTNESS),
                                     static_cast<int>(SettingsUI_ScreenDisplayCellIndex::BRIGHTNESS_SLIDER),
                                     SettingsUI_WidgetCellElement::CENTER_SLIDER
                                 );
        ESP_UTILS_CHECK_NULL_RETURN(brightness_slider, false, "Get cell display slider failed");
        lv_slider_set_value(brightness_slider, ret_brightness, LV_ANIM_OFF);
    }

    return true;
}

bool SettingsManager::getDisplayBrightness(int &brightness)
{
    int ret_brightness = 0;
    ESP_UTILS_CHECK_FALSE_RETURN(
        event_signal(EventType::GetDisplayBrightness, std::ref(ret_brightness)), false, "Get media display brightness failed"
    );

    ESP_UTILS_CHECK_FALSE_RETURN(ret_brightness >= 0, false, "Invalid brightness");
    brightness = ret_brightness;

    return true;
}

bool SettingsManager::initWlan()
{
    ESP_UTILS_LOGD("Initialize WLAN");
    ESP_UTILS_CHECK_FALSE_RETURN(checkIsWlanGeneralState(WlanGeneraState::DEINIT), false, "WLAN already initialized");

    {
        esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
            .name  = WLAN_OPERATION_THREAD_NAME,
            .stack_size = WLAN_OPERATION_THREAD_STACK_SIZE,
            .stack_in_ext = WLAN_OPERATION_THREAD_STACK_CAPS_EXT,
        });
        _wlan_operation_thread = boost::thread(onWlanOperationThread, this);
    }

    {
        esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
            .name  = WLAN_UI_THREAD_NAME,
            .stack_size = WLAN_UI_THREAD_STACK_SIZE,
            .stack_in_ext = WLAN_UI_THREAD_STACK_CAPS_EXT,
        });
        _wlan_ui_thread = boost::thread(onWlanUI_Thread, this);
    }

    _wlan_update_timer = ESP_BROOKESIA_LV_TIMER(onWlanScanTimer, data.wlan.scan_interval_ms, this);
    ESP_UTILS_CHECK_NULL_RETURN(_wlan_update_timer, false, "Create WLAN update timer failed");

    ESP_UTILS_CHECK_FALSE_RETURN(
        forceWlanOperation(WlanOperation::INIT, 0), false, "Force WLAN operation init failed"
    );

    return true;
}

bool SettingsManager::deinitWlan()
{
    ESP_UTILS_LOGD("Deinitialize WLAN");

    ESP_UTILS_CHECK_FALSE_RETURN(
        forceWlanOperation(WlanOperation::DEINIT, 0), false, "Force WLAN operation deinit failed"
    );

    _wlan_update_timer = nullptr;
    _wlan_operation_thread.join();
    _wlan_ui_thread.join();

    return true;
}

bool SettingsManager::processCloseWlan()
{
    ESP_UTILS_LOGD("Close WLAN");

    return true;
}

bool SettingsManager::toggleWlanScanTimer(bool is_start, bool is_once)
{
    ESP_UTILS_CHECK_NULL_RETURN(_wlan_update_timer, false, "Invalid WLAN scan timer");
    ESP_UTILS_LOGD("Toggle UI screen WLAN scan timer(%s)", is_start ? "start" : "stop");

    if (is_start) {
        lv_timer_resume(_wlan_update_timer.get());
        lv_timer_ready(_wlan_update_timer.get());
    } else {
        lv_timer_pause(_wlan_update_timer.get());
        lv_timer_reset(_wlan_update_timer.get());
    }
    _wlan_scan_timer_once = is_once;

    return true;
}

bool SettingsManager::processOnWlanScanTimer(lv_timer_t *t)
{
    ESP_UTILS_CHECK_NULL_RETURN(t, false, "Invalid timer");
    ESP_UTILS_LOGD("On WLAN update timer");

    if (_wlan_scan_timer_once) {
        _wlan_scan_timer_once = false;
        ESP_UTILS_CHECK_FALSE_RETURN(
            toggleWlanScanTimer(false), false, "Toggle WLAN scan timer failed"
        );
    }

    if (!checkIsWlanGeneralState(WlanGeneraState::STARTED) ||
            (checkIsWlanGeneralState(WlanGeneraState::_CONNECT) && (_ui_current_screen != UI_Screen::WIRELESS_WLAN))) {
        ESP_UTILS_LOGD("Ignore scan start");
        return true;
    }

    if (_is_ui_initialized && !_is_wlan_retry_connecting) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            updateUI_ScreenWlanConnected(false), false, "Update UI screen WLAN connected failed"
        );
    }

    if (!_is_wlan_retry_connecting && !tryWlanOperation(WlanOperation::SCAN_START, 0)) {
        ESP_UTILS_LOGW("Try WLAN operation scan start failed");
    }

    return true;
}

bool SettingsManager::triggerWlanOperation(WlanOperation operation, int timeout_ms)
{
    ESP_UTILS_LOGI(
        "Trigger WLAN operation(%s) (general state:%s, scan state:%s)",
        getWlanOperationStr(operation),
        getWlanGeneralStateStr(_wlan_general_state.load()),
        getWlanScanStateStr(_wlan_scan_state.load())
    );

    {
        std::lock_guard<std::mutex> lock(_wlan_operation_start_mutex);
        _wlan_operation_queue.push(operation);
        _wlan_operation_start_cv.notify_all();
    }

    if (timeout_ms > 0) {
        ESP_UTILS_LOGD("Wait for operation finish with timeout(%d)", (int)timeout_ms);
        std::unique_lock<std::mutex> ulock(_wlan_operation_stop_mutex);
        auto status = _wlan_operation_stop_cv.wait_for(ulock, std::chrono::milliseconds(timeout_ms), [this] {
            return _is_wlan_operation_stopped.load();
        });
        ESP_UTILS_CHECK_FALSE_RETURN(status, false, "Wait for operation finish timeout");
        ESP_UTILS_LOGD("Wait operation finish");
    }

    return true;
}

bool SettingsManager::forceWlanOperation(WlanOperation operation, int timeout_ms)
{
    ESP_UTILS_LOGI(
        "Force WLAN operation(%s) in %d ms (general state:%s, scan state:%s)",
        getWlanOperationStr(operation), static_cast<int>(timeout_ms),
        getWlanGeneralStateStr(_wlan_general_state.load()),
        getWlanScanStateStr(_wlan_scan_state.load())
    );
    if (operation == WlanOperation::STOP) {
        ESP_UTILS_LOGD("Force WLAN operation stop");
    }

    switch (operation) {
    case WlanOperation::INIT:
        // Ignore if WLAN is initialized
        if (checkIsWlanGeneralState(WlanGeneraState::INIT)) {
            ESP_UTILS_LOGD("Ignore init");
            break;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::INIT, timeout_ms), false, "Trigger WLAN operation init failed"
        );
        break;
    case WlanOperation::DEINIT:
        // Ignore if WLAN is not initialized
        if (!checkIsWlanGeneralState(WlanGeneraState::INIT)) {
            ESP_UTILS_LOGD("Ignore deinit");
            break;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::DEINIT, timeout_ms), false, "Trigger WLAN operation deinit failed"
        );
        break;
    case WlanOperation::START:
        // Ignore if WLAN is started
        if (checkIsWlanGeneralState(WlanGeneraState::_START)) {
            ESP_UTILS_LOGD("Ignore start");
            break;
        }
        // Force WLAN operation initialize first
        ESP_UTILS_CHECK_FALSE_RETURN(
            forceWlanOperation(WlanOperation::INIT, WLAN_INIT_WAIT_TIMEOUT_MS), false,
            "Force WLAN operation init failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::START, timeout_ms), false, "Trigger WLAN operation start failed"
        );
        break;
    case WlanOperation::STOP:
        // Ignore if WLAN is started
        if (!checkIsWlanGeneralState(WlanGeneraState::_START)) {
            ESP_UTILS_LOGD("Ignore stop");
            break;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::STOP, timeout_ms), false, "Trigger WLAN operation stop failed"
        );
        break;
    case WlanOperation::CONNECT:
        // Force WLAN operation scan stop first
        ESP_UTILS_CHECK_FALSE_RETURN(
            forceWlanOperation(WlanOperation::SCAN_STOP, WLAN_SCAN_STOP_WAIT_TIMEOUT_MS), false,
            "Force WLAN scan stop operation failed"
        );
        // Force WLAN operation disconnect first, record this state to skip the upcoming disconnection event
        if (checkIsWlanGeneralState(WlanGeneraState::_CONNECT)) {
            ESP_UTILS_LOGD("Connection already established, force disconnect first");
            _is_wlan_force_connecting = true;
            ESP_UTILS_CHECK_FALSE_RETURN(
                forceWlanOperation(WlanOperation::DISCONNECT, WLAN_DISCONNECT_WAIT_TIMEOUT_MS), false,
                "Force WLAN operation disconnect failed"
            );
        }
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::CONNECT, timeout_ms), false, "Trigger WLAN operation connect failed"
        );
        break;
    case WlanOperation::DISCONNECT:
        // Ignore if WLAN is not connected
        if (!checkIsWlanGeneralState(WlanGeneraState::_CONNECT)) {
            ESP_UTILS_LOGD("Ignore disconnect");
            break;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::DISCONNECT, timeout_ms), false, "Trigger WLAN operation disconnect failed"
        );
        break;
    case WlanOperation::SCAN_START:
        // Ignore if WLAN is scanning
        if (checkIsWlanScanState(WlanScanState::SCANNING)) {
            ESP_UTILS_LOGD("Ignore scan start");
            break;
        }
        // Check if WLAN is started, start first
        ESP_UTILS_CHECK_FALSE_RETURN(
            forceWlanOperation(WlanOperation::START, WLAN_START_WAIT_TIMEOUT_MS), false,
            "Force WLAN start operation failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::SCAN_START, timeout_ms), false, "Trigger WLAN operation scan start failed"
        );
        break;
    case WlanOperation::SCAN_STOP:
        // Ignore if WLAN is stopped
        if (!checkIsWlanScanState(WlanScanState::_SCAN_START)) {
            ESP_UTILS_LOGD("Ignore scan stop");
            break;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::SCAN_STOP, timeout_ms), false, "Trigger WLAN operation scan stop failed"
        );
        break;
    default:
        ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid WLAN operation(%d)", static_cast<int>(operation));
        break;
    }

    return true;
}

bool SettingsManager::tryWlanOperation(WlanOperation operation, int timeout_ms)
{
    ESP_UTILS_LOGD(
        "Try WLAN operation(%s) in %d ms (general state:%s, scan state:%s)",
        getWlanOperationStr(operation), static_cast<int>(timeout_ms),
        getWlanGeneralStateStr(_wlan_general_state.load()),
        getWlanScanStateStr(_wlan_scan_state.load())
    );

    switch (operation) {
    case WlanOperation::INIT:
        // Check if WLAN is initialized
        if (checkIsWlanGeneralState(WlanGeneraState::INIT)) {
            ESP_UTILS_LOGD("Ignore init");
            break;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::INIT, timeout_ms), false, "Trigger WLAN operation init failed"
        );
        break;
    case WlanOperation::DEINIT:
        // Check if WLAN is not initialized
        if (checkIsWlanGeneralState(WlanGeneraState::DEINIT)) {
            ESP_UTILS_LOGD("Ignore deinit");
            break;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::DEINIT, timeout_ms), false, "Trigger WLAN operation deinit failed"
        );
        break;
    case WlanOperation::START:
        // Ignore if WLAN is started
        if (checkIsWlanGeneralState(WlanGeneraState::_START)) {
            ESP_UTILS_LOGD("Ignore start");
            break;
        }
        // Try WLAN operation initialize first
        ESP_UTILS_CHECK_FALSE_RETURN(
            tryWlanOperation(WlanOperation::INIT, WLAN_INIT_WAIT_TIMEOUT_MS), false, "Try WLAN operation init failed"
        );
        // Trigger WLAN operation start
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::START, timeout_ms), false, "Trigger WLAN operation start failed"
        );
        break;
    case WlanOperation::STOP:
        // Ignore if WLAN is stopped
        if (!checkIsWlanGeneralState(WlanGeneraState::_START)) {
            ESP_UTILS_LOGD("Ignore stop");
            break;
        }
        // Try WLAN operation initialize first
        ESP_UTILS_CHECK_FALSE_RETURN(
            tryWlanOperation(WlanOperation::INIT, WLAN_INIT_WAIT_TIMEOUT_MS), false, "Try WLAN operation init failed"
        );
        // Trigger WLAN operation start
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::STOP, timeout_ms), false, "Trigger WLAN operation stop failed"
        );
        break;
    case WlanOperation::CONNECT:
        // Ignore if WLAN is connecting or connected
        if (checkIsWlanGeneralState(WlanGeneraState::_CONNECT)) {
            ESP_UTILS_LOGW("Ignore connect");
            break;
        }
        // Try WLAN operation start first
        ESP_UTILS_CHECK_FALSE_RETURN(
            tryWlanOperation(WlanOperation::START, WLAN_START_WAIT_TIMEOUT_MS), false, "Try WLAN operation start failed"
        );
        // Trigger WLAN operation connect
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::CONNECT, timeout_ms), false, "Trigger WLAN operation connect failed"
        );
        break;
    case WlanOperation::DISCONNECT:
        // Ignore if WLAN is disconnected
        if (!checkIsWlanGeneralState(WlanGeneraState::_CONNECT)) {
            ESP_UTILS_LOGD("Ignore disconnected");
            break;
        }
        // Try WLAN operation start first
        ESP_UTILS_CHECK_FALSE_RETURN(
            tryWlanOperation(WlanOperation::START, WLAN_START_WAIT_TIMEOUT_MS), false, "Try WLAN operation start failed"
        );
        // Trigger WLAN operation disconnect
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::DISCONNECT, timeout_ms), false, "Trigger WLAN operation disconnect failed"
        );
        break;
    case WlanOperation::SCAN_START:
        // Ignore if WLAN is scanning
        if (checkIsWlanScanState(WlanScanState::SCANNING)) {
            ESP_UTILS_LOGD("Ignore scan start");
            break;
        }
        // Check if WLAN is connecting
        if (checkIsWlanGeneralState(WlanGeneraState::CONNECTING)) {
            ESP_UTILS_LOGW("Connecting");
            return false;
        }
        // Try WLAN operation start first
        ESP_UTILS_CHECK_FALSE_RETURN(
            tryWlanOperation(WlanOperation::START, WLAN_START_WAIT_TIMEOUT_MS), false, "Try WLAN operation start failed"
        );
        // Trigger WLAN operation scan start
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::SCAN_START, timeout_ms), false, "Trigger WLAN operation scan start failed"
        );
        break;
    case WlanOperation::SCAN_STOP:
        // Ignore if WLAN is stopped
        if (checkIsWlanScanState(WlanScanState::SCAN_STOPPED)) {
            ESP_UTILS_LOGD("Ignore scan stop");
            break;
        }
        // Try WLAN operation start first
        ESP_UTILS_CHECK_FALSE_RETURN(
            tryWlanOperation(WlanOperation::START, WLAN_START_WAIT_TIMEOUT_MS), false, "Try WLAN operation start failed"
        );
        // Trigger WLAN operation scan stop
        ESP_UTILS_CHECK_FALSE_RETURN(
            triggerWlanOperation(WlanOperation::SCAN_STOP, timeout_ms), false, "Trigger WLAN operation scan stop failed"
        );
        break;
    default:
        ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid WLAN operation(%d)", static_cast<int>(operation));
        break;
    }

    return true;
}

bool SettingsManager::doWlanOperationInit()
{
    ESP_UTILS_LOGD("Do WLAN operation init");

    if (checkIsWlanGeneralState(WlanGeneraState::INIT)) {
        ESP_UTILS_LOGD("Ignore init");
        return true;
    }

    bool ret = true;
    esp_err_t err = esp_netif_init();
    ESP_UTILS_CHECK_FALSE_GOTO(err == ESP_OK, end, "Init netif failed");
    err = esp_event_loop_create_default();
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_UTILS_LOGW("Default event loop already created");
    } else {
        ESP_UTILS_CHECK_FALSE_GOTO(err == ESP_OK, end, "Create default event loop failed");
    }
    _wlan_sta_netif = esp_netif_create_default_wifi_sta();
    ESP_UTILS_CHECK_NULL_GOTO(_wlan_sta_netif, end, "Create default STA netif failed");
    {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&cfg);
        ESP_UTILS_CHECK_FALSE_GOTO(err == ESP_OK, end, "Initialize WLAN failed");
    }
    err = esp_event_handler_instance_register(
              WIFI_EVENT, static_cast<int32_t>(ESP_EVENT_ANY_ID), onWlanEventHandler, this,
              &_wlan_event_handler_instance
          );
    ESP_UTILS_CHECK_FALSE_GOTO(err == ESP_OK, end, "Register WLAN event handler failed");
    err = esp_wifi_set_mode(WLAN_INIT_MODE_DEFAULT);
    ESP_UTILS_CHECK_FALSE_GOTO(err == ESP_OK, end, "Set WLAN mode failed");

end:
    if (err != ESP_OK) {
        ret = false;
        ESP_UTILS_LOGE("Failed reason(%s)", esp_err_to_name(err));
        doWlanOperationDeinit();
    }

    if (ret) {
        _wlan_general_state = WlanGeneraState::INIT;
    }

    return ret;
}

bool SettingsManager::doWlanOperationDeinit()
{
    ESP_UTILS_LOGD("Do WLAN operation deinit");
    bool ret = true;

    if (checkIsWlanGeneralState(WlanGeneraState::INIT)) {
        ESP_UTILS_LOGD("Ignore deinit");
        return true;
    }

    esp_err_t err = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, _wlan_event_handler_instance);
    if (err != ESP_OK) {
        ESP_UTILS_LOGE("Unregister WLAN event handler failed(%s)", esp_err_to_name(err));
    }
    err = esp_wifi_deinit();
    if (err != ESP_OK) {
        ESP_UTILS_LOGE("Deinitialize WLAN failed(%s)", esp_err_to_name(err));
    }
    if (_wlan_sta_netif != nullptr) {
        esp_netif_destroy_default_wifi(_wlan_sta_netif);
        _wlan_sta_netif = nullptr;
    }
    /* Not supported now */
    // err = esp_netif_deinit();
    if (err != ESP_OK) {
        ret = false;
        ESP_UTILS_LOGE("Failed reason(%s)", esp_err_to_name(err));
    }

    _wlan_general_state = WlanGeneraState::DEINIT;

    return ret;
}

bool SettingsManager::doWlanOperationStart()
{
    ESP_UTILS_LOGD("Do WLAN operation start");

    if (checkIsWlanGeneralState(WlanGeneraState::_START)) {
        ESP_UTILS_LOGD("Ignore start");
        return true;
    }

    bool ret = true;

    esp_err_t err = esp_wifi_start();
    ESP_UTILS_CHECK_FALSE_GOTO(err == ESP_OK, end, "Set WLAN mode failed");

end:
    if (err != ESP_OK) {
        ret = false;
        ESP_UTILS_LOGE("Failed reason(%s)", esp_err_to_name(err));
    }

    if (ret && ((_wlan_general_state & WlanGeneraState::STARTED) != WlanGeneraState::STARTED)) {
        _wlan_general_state = WlanGeneraState::STARTING;
    }

    return ret;
}

bool SettingsManager::doWlanOperationStop()
{
    ESP_UTILS_LOGD("Do WLAN operation stop");

    if (checkIsWlanGeneralState(WlanGeneraState::_STOP)) {
        ESP_UTILS_LOGD("Ignore start");
        return true;
    }

    bool ret = true;

    esp_err_t err = esp_wifi_stop();
    ESP_UTILS_CHECK_FALSE_GOTO(err == ESP_OK, end, "Stop WLAN failed");

end:
    if (err != ESP_OK) {
        ret = false;
        ESP_UTILS_LOGE("Failed reason(%s)", esp_err_to_name(err));
    }

    if (ret && ((_wlan_general_state & WlanGeneraState::STOPPED) != WlanGeneraState::STOPPED)) {
        _wlan_general_state = WlanGeneraState::STOPPING;
    }

    return ret;
}

bool SettingsManager::doWlanOperationConnect()
{
    ESP_UTILS_LOGD("Do WLAN operation connect");

    if (checkIsWlanGeneralState(WlanGeneraState::_CONNECT)) {
        ESP_UTILS_LOGD("Ignore start");
        return true;
    }
    ESP_UTILS_CHECK_FALSE_RETURN(
        !_wlan_connecting_info.first.ssid.empty(), false, "Invalid WLAN connect info ssid"
    );

    bool ret = true;
    strncpy(
        reinterpret_cast<char *>(_wlan_config.sta.ssid), _wlan_connecting_info.first.ssid.c_str(),
        sizeof(_wlan_config.sta.ssid) - 1
    );
    if (!_wlan_connecting_info.second.empty()) {
        strncpy(
            reinterpret_cast<char *>(_wlan_config.sta.password), _wlan_connecting_info.second.c_str(),
            sizeof(_wlan_config.sta.password) - 1
        );
    } else {
        _wlan_config.sta.password[0] = '\0';
    }
    ESP_UTILS_LOGD("Try to connect WLAN(%s: %s)", _wlan_config.sta.ssid, _wlan_config.sta.password);
    esp_err_t err = esp_wifi_set_config(WLAN_CONFIG_MODE_DEFAULT, &_wlan_config);
    ESP_UTILS_CHECK_FALSE_GOTO(err == ESP_OK, end, "Config WLAN failed");
    err = esp_wifi_connect();
    ESP_UTILS_CHECK_FALSE_GOTO(err == ESP_OK, end, "Connect WLAN failed");

end:
    if (err != ESP_OK) {
        ret = false;
        ESP_UTILS_LOGE("Failed reason(%s)", esp_err_to_name(err));
    }

    if (ret && ((_wlan_general_state & WlanGeneraState::CONNECTED) != WlanGeneraState::CONNECTED)) {
        _wlan_general_state = WlanGeneraState::CONNECTING;
    }

    return ret;
}

bool SettingsManager::doWlanOperationDisconnect()
{
    ESP_UTILS_LOGD("Do WLAN operation disconnect");

    if (checkIsWlanGeneralState(WlanGeneraState::_DISCONNECT)) {
        ESP_UTILS_LOGD("Ignore start");
        return true;
    }

    bool ret = true;

    esp_err_t err = esp_wifi_disconnect();
    ESP_UTILS_CHECK_FALSE_GOTO(err == ESP_OK, end, "Disconnect WLAN failed");

end:
    if (err != ESP_OK) {
        ret = false;
        ESP_UTILS_LOGE("Failed reason(%s)", esp_err_to_name(err));
    }

    if (ret && ((_wlan_general_state & WlanGeneraState::DISCONNECTED) != WlanGeneraState::DISCONNECTED)) {
        _wlan_general_state = WlanGeneraState::DISCONNECTING;
    }

    return ret;
}

bool SettingsManager::doWlanOperationScanStart()
{
    ESP_UTILS_LOGD("Do WLAN operation scan start");

    if (checkIsWlanScanState(WlanScanState::SCANNING)) {
        ESP_UTILS_LOGD("Ignore start");
        return true;
    }

    bool ret = true;

    esp_err_t err = esp_wifi_scan_start(nullptr, false);
    ESP_UTILS_CHECK_FALSE_GOTO(err == ESP_OK, end, "Disconnect WLAN failed");

end:
    if (err != ESP_OK) {
        ret = false;
        ESP_UTILS_LOGE("Failed reason(%s)", esp_err_to_name(err));
    }

    if (ret) {
        _wlan_scan_state = WlanScanState::SCANNING;
    }

    return ret;
}

bool SettingsManager::doWlanOperationScanStop()
{
    ESP_UTILS_LOGD("Do WLAN operation scan stop");

    if (checkIsWlanScanState(WlanScanState::SCAN_STOPPED)) {
        ESP_UTILS_LOGD("Ignore stop");
        return true;
    }

    bool ret = true;

    esp_err_t err = esp_wifi_scan_stop();
    ESP_UTILS_CHECK_FALSE_GOTO(err == ESP_OK, end, "Stop WLAN scan failed");

end:
    if (err != ESP_OK) {
        ret = false;
        ESP_UTILS_LOGE("Failed reason(%s)", esp_err_to_name(err));
    }

    if (ret) {
        _wlan_scan_state = WlanScanState::SCAN_STOPPED;
    }

    return ret;
}

bool SettingsManager::processOnWlanOperationThread()
{
    std::unique_lock<std::mutex> start_lock(_wlan_operation_start_mutex);
    _wlan_operation_start_cv.wait(start_lock, [this] { return !_wlan_operation_queue.empty(); });
    _is_wlan_operation_stopped = false;

    WlanOperation operation = _wlan_operation_queue.front();
    _wlan_operation_queue.pop();
    ESP_UTILS_LOGI("Process on wlan operation(%s) start", getWlanOperationStr(operation));

    bool ret = true;
    int timeout_ms = 0;
    std::vector<WlanGeneraState> target_general_state;
    std::vector<WlanScanState> target_scan_state;

    switch (operation) {
    case WlanOperation::INIT:
        ESP_UTILS_CHECK_FALSE_GOTO(ret = doWlanOperationInit(), end, "Do WLAN operation init failed");
        break;
    case WlanOperation::DEINIT:
        ESP_UTILS_CHECK_FALSE_GOTO(ret = doWlanOperationDeinit(), end, "Do WLAN operation deinit failed");
        break;
    case WlanOperation::START:
        ESP_UTILS_CHECK_FALSE_GOTO(ret = doWlanOperationStart(), end, "Do WLAN operation start failed");
        target_general_state.emplace_back(WlanGeneraState::STARTED);
        timeout_ms = WLAN_START_WAIT_TIMEOUT_MS;
        break;
    case WlanOperation::STOP:
        ESP_UTILS_CHECK_FALSE_GOTO(ret = doWlanOperationStop(), end, "Do WLAN operation stop failed");
        target_general_state.emplace_back(WlanGeneraState::STOPPED);
        timeout_ms = WLAN_STOP_WAIT_TIMEOUT_MS;
        break;
    case WlanOperation::CONNECT:
        ESP_UTILS_CHECK_FALSE_GOTO(ret = doWlanOperationConnect(), end, "Do WLAN operation connect failed");
        target_general_state.emplace_back(WlanGeneraState::CONNECTED);
        target_general_state.emplace_back(WlanGeneraState::DISCONNECTED);
        timeout_ms = WLAN_CONNECT_WAIT_TIMEOUT_MS;
        break;
    case WlanOperation::DISCONNECT:
        ESP_UTILS_CHECK_FALSE_GOTO(ret = doWlanOperationDisconnect(), end, "Do WLAN operation disconnect failed");
        target_general_state.emplace_back(WlanGeneraState::DISCONNECTED);
        timeout_ms = WLAN_DISCONNECT_WAIT_TIMEOUT_MS;
        break;
    case WlanOperation::SCAN_START:
        ESP_UTILS_CHECK_FALSE_GOTO(ret = doWlanOperationScanStart(), end, "Do WLAN operation scan start failed");
        target_scan_state.emplace_back(WlanScanState::SCAN_DONE);
        timeout_ms = WLAN_SCAN_START_WAIT_TIMEOUT_MS;
        break;
    case WlanOperation::SCAN_STOP:
        ESP_UTILS_CHECK_FALSE_GOTO(ret = doWlanOperationScanStop(), end, "Do WLAN operation scan stop failed");
        target_scan_state.emplace_back(WlanScanState::SCAN_STOPPED);
        timeout_ms = WLAN_SCAN_STOP_WAIT_TIMEOUT_MS;
        break;
    default:
        ESP_UTILS_CHECK_FALSE_GOTO(ret = false, end, "Invalid WLAN operation");
        break;
    }

    if (target_general_state.size() > 0) {
        ret = waitForWlanGeneralState(target_general_state, timeout_ms);
    } else if (target_scan_state.size() > 0) {
        ret = waitForWlanScanState(target_scan_state, timeout_ms);
    }
    _wlan_prev_operation = operation;

end:
    _is_wlan_operation_stopped = true;
    std::unique_lock<std::mutex> stop_lock(_wlan_operation_stop_mutex);
    _wlan_operation_stop_cv.notify_all();
    ESP_UTILS_LOGD("Process on WLAN operation(%s) done", getWlanOperationStr(operation));

    return ret;
}

bool SettingsManager::processOnWlanUI_Thread()
{
    std::unique_lock<std::mutex> lock(_wlan_event_mutex);
    _wlan_event_cv.wait(lock, [this] {
        return _is_wlan_event_updated;
    });
    _is_wlan_event_updated = false;

    int wlan_event_id = _wlan_event_id;
    lock.unlock();
    ESP_UTILS_LOGI(
        "Process on wlan UI thread start (event: %s)", getWlanEventStr(static_cast<wifi_event_t>(_wlan_event_id.load()))
    );

    _wlan_event_id_prev = wlan_event_id;

    bool ret = true;
    auto system = app.getSystem();
    auto &quick_settings = system->display.getQuickSettings();
    // Use temp variable to avoid `_ui_wlan_available_data` being modified outside lvgl task
    decltype(_ui_wlan_available_data) temp_available_data;

    // Process non-UI
    switch (wlan_event_id) {
    case WIFI_EVENT_SCAN_DONE: {
        bool psk_flag = false;
        uint16_t number = data.wlan.scan_ap_count_max;
        uint16_t ap_count = 0;
        SettingsUI_ScreenWlan::SignalLevel signal_level = SettingsUI_ScreenWlan::SignalLevel::WEAK;

        std::shared_ptr<wifi_ap_record_t[]> ap_info(new wifi_ap_record_t[number]);
        ESP_UTILS_CHECK_NULL_RETURN(ap_info, false, "Allocate AP info failed");

        ESP_UTILS_CHECK_FALSE_RETURN(esp_wifi_scan_get_ap_num(&ap_count) == ESP_OK, false, "Get AP number failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            esp_wifi_scan_get_ap_records(&number, ap_info.get()) == ESP_OK, false, "Get AP records failed"
        );

        ESP_UTILS_LOGD("Get AP count: %d", std::min(number, ap_count));

        for (uint16_t i = 0; (i < ap_count) && (i < number); i++) {
#if WLAN_SCAN_ENABLE_DEBUG_LOG
            printf("SSID: \t\t%s\n", ap_info[i].ssid);
            printf("RSSI: \t\t%d\n", ap_info[i].rssi);
            printf("Channel: \t\t%d\n", ap_info[i].primary);
            printf("Locked: %s\n", psk_flag ? "yes" : "no");
            printf("Signal Level: %d\n\n", static_cast<int>(signal_level));
#endif
            if (checkIsWlanGeneralState(WlanGeneraState::_CONNECT)) {
                // Skip connected AP
                if (strcmp((const char *)ap_info[i].ssid, _wlan_connected_info.first.ssid.c_str()) == 0) {
                    ESP_UTILS_LOGD("Skip connecting or connected AP(%s)", _wlan_connected_info.first.ssid.c_str());
                    continue;
                }
            } else if (checkIsWlanGeneralState(WlanGeneraState::_START)) {
                // Check if default AP is in scan list
                StorageNVS::Value default_connect_ssid;
                StorageNVS::Value default_connect_pwd;
                ESP_UTILS_CHECK_FALSE_RETURN(
                    StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_WLAN_SSID, default_connect_ssid), false,
                    "Get default connect SSID failed"
                );
                ESP_UTILS_CHECK_FALSE_RETURN(
                    StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_WLAN_PASSWORD, default_connect_pwd), false,
                    "Get default connect PWD failed"
                );
                ESP_UTILS_CHECK_FALSE_RETURN(
                    std::holds_alternative<std::string>(default_connect_ssid), false, "Invalid default connect SSID type"
                );
                ESP_UTILS_CHECK_FALSE_RETURN(
                    std::holds_alternative<std::string>(default_connect_pwd), false, "Invalid default connect PWD type"
                );
                auto &default_connect_ssid_str = std::get<std::string>(default_connect_ssid);
                auto &default_connect_pwd_str = std::get<std::string>(default_connect_pwd);

                if ((default_connect_ssid_str.size() > 0) &&
                        strcmp((const char *)ap_info[i].ssid, default_connect_ssid_str.c_str()) == 0) {
                    ESP_UTILS_LOGD("Connect to default AP(%s)", default_connect_ssid_str.c_str());
                    _wlan_connecting_info.first = getWlanDataFromApInfo(ap_info[i]);
                    _wlan_connecting_info.second = default_connect_pwd_str;

                    // Create a thread to connect to default AP, avoid blocking UI
                    {
                        esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
                            .name = WLAN_CONNECT_THREAD_NAME,
                            .stack_size = WLAN_CONNECT_THREAD_STACK_SIZE,
                            .stack_in_ext = WLAN_CONNECT_THREAD_STACK_CAPS_EXT,
                        });
                        boost::thread([this]() {
                            boost::this_thread::sleep_for(boost::chrono::milliseconds(WLAN_SCAN_CONNECT_AP_DELAY_MS));
                            ESP_UTILS_LOGI("Connect to default AP(%s)", _wlan_connecting_info.first.ssid.c_str());
                            ESP_UTILS_CHECK_FALSE_EXIT(
                                forceWlanOperation(WlanOperation::CONNECT, 0), "Force WLAN operation connect failed"
                            );
                            if (_is_ui_initialized) {
                                app.getCore()->lockLv();
                                // Update connecting WLAN data
                                if (!checkIsWlanGeneralState(WlanGeneraState::CONNECTED)) {
                                    if (!updateUI_ScreenWlanConnected(true, WlanGeneraState::CONNECTING)) {
                                        ESP_UTILS_LOGE("Update UI screen WLAN connected failed");
                                    }
                                }
                                // Update available WLAN data
                                if (!checkIsWlanGeneralState(WlanGeneraState::CONNECTED)) {
                                    if (!updateUI_ScreenWlanAvailable(true, WlanGeneraState::CONNECTING)) {
                                        ESP_UTILS_LOGE("Update UI screen WLAN available failed");
                                    }
                                }
                                app.getCore()->unlockLv();
                            }
                        }).detach();
                    }
                }
            }
            psk_flag = (ap_info[i].authmode != WIFI_AUTH_OPEN);
            if (ap_info[i].rssi <= -70) {
                signal_level = SettingsUI_ScreenWlan::SignalLevel::WEAK;
            } else if (ap_info[i].rssi <= -50) {
                signal_level = SettingsUI_ScreenWlan::SignalLevel::MODERATE;
            } else {
                signal_level = SettingsUI_ScreenWlan::SignalLevel::GOOD;
            }
            temp_available_data.push_back(
                SettingsUI_ScreenWlan::WlanData{std::string((char *)ap_info[i].ssid), psk_flag, signal_level}
            );
        }
        break;
    }
    case WIFI_EVENT_STA_CONNECTED: {
        StorageNVS::Value last_ssid;
        StorageNVS::Value last_pwd;
        ESP_UTILS_CHECK_FALSE_RETURN(
            StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_WLAN_SSID, last_ssid), false, "Get last SSID failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_WLAN_PASSWORD, last_pwd), false, "Get last PWD failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            std::holds_alternative<std::string>(last_ssid), false, "Invalid last SSID type"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            std::holds_alternative<std::string>(last_pwd), false, "Invalid last PWD type"
        );
        auto &last_ssid_str = std::get<std::string>(last_ssid);
        auto &last_pwd_str = std::get<std::string>(last_pwd);

        std::string current_ssid((char *)_wlan_config.sta.ssid);
        std::string current_pwd((char *)_wlan_config.sta.password);
        if ((last_ssid_str != current_ssid) || (last_pwd_str != current_pwd)) {
            ESP_UTILS_CHECK_FALSE_RETURN(
                StorageNVS::requestInstance().setLocalParam(SETTINGS_NVS_KEY_WLAN_SSID, current_ssid), false,
                "Set last SSID failed"
            );
            ESP_UTILS_CHECK_FALSE_RETURN(
                StorageNVS::requestInstance().setLocalParam(SETTINGS_NVS_KEY_WLAN_PASSWORD, current_pwd), false,
                "Set last PWD failed"
            );
        }
        break;
    }
    default:
        break;
    }

    app.getCore()->lockLv();

    // Process system UI
    switch (wlan_event_id) {
    case WIFI_EVENT_SCAN_DONE:
        _ui_wlan_available_data = std::move(temp_available_data);
        break;
    case WIFI_EVENT_STA_START:
        // Show status bar WLAN icon
        quick_settings.setWifiIconState(QuickSettings::WifiState::DISCONNECTED);
        ESP_UTILS_CHECK_FALSE_GOTO(
            toggleWlanScanTimer(true), end, "Toggle WLAN scan timer failed"
        );
        break;
    case WIFI_EVENT_STA_STOP:
        ESP_UTILS_CHECK_FALSE_GOTO(
            quick_settings.setWifiIconState(QuickSettings::WifiState::CLOSED),
            end, "Set WLAN icon state failed"
        );
        ESP_UTILS_CHECK_FALSE_GOTO(
            toggleWlanScanTimer(false), end, "Toggle WLAN scan timer failed"
        );
        break;
    case WIFI_EVENT_STA_CONNECTED: {
        ESP_UTILS_CHECK_FALSE_GOTO(
            toggleWlanScanTimer(_ui_current_screen == UI_Screen::WIRELESS_WLAN, true), end, "Toggle WLAN scan timer failed"
        );
        wifi_ap_record_t ap_info = {};
        ESP_UTILS_CHECK_FALSE_GOTO(esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK, end, "Get AP info failed");
        auto data = getWlanDataFromApInfo(ap_info);
        ESP_UTILS_LOGI("Connected to AP(%s, %d)", data.ssid.c_str(), data.signal_level);
        ESP_UTILS_CHECK_FALSE_GOTO(
            quick_settings.setWifiIconState(static_cast<QuickSettings::WifiState>(data.signal_level + 1)), end,
            "Set WLAN icon state failed"
        );
        if (!isTimeSync()) {
            if (!_wlan_time_sync_thread.joinable()) {
                esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
                    .name = WLAN_TIME_SYNC_THREAD_NAME,
                    .stack_size = WLAN_TIME_SYNC_THREAD_STACK_SIZE,
                    .stack_in_ext = WLAN_TIME_SYNC_THREAD_STACK_CAPS_EXT,
                });
                _wlan_time_sync_thread = boost::thread([this]() {
                    ESP_UTILS_LOGD("Update time start");
                    app_sntp_init();
                    ESP_UTILS_LOGD("Update time end");
                });
            } else {
                ESP_UTILS_LOGD("Update time thread is running");
            }
        } else {
            ESP_UTILS_LOGD("Time is synchronized");
        }
        break;
    }
    case WIFI_EVENT_STA_DISCONNECTED: {
        ESP_UTILS_CHECK_FALSE_GOTO(
            quick_settings.setWifiIconState(QuickSettings::WifiState::DISCONNECTED),
            end, "Set WLAN icon state failed"
        );
        break;
    }
    default:
        break;
    }

    if (!_is_ui_initialized) {
        ESP_UTILS_LOGD("Skip APP UI update when not initialized");
        goto end;
    }

    // Process APP UI
    switch (wlan_event_id) {
    case WIFI_EVENT_STA_START:
        [[fallthrough]];
    case WIFI_EVENT_STA_STOP:
        ESP_UTILS_CHECK_FALSE_GOTO(
            updateUI_ScreenWlanAvailable(false), end, "Update UI screen WLAN available failed"
        );
        ESP_UTILS_CHECK_FALSE_GOTO(
            updateUI_ScreenWlanConnected(false), end, "Update UI screen WLAN connected failed"
        );
        break;
    case WIFI_EVENT_STA_CONNECTED: {
        ESP_UTILS_CHECK_FALSE_GOTO(
            updateUI_ScreenWlanConnected(false), end, "Update UI screen WLAN connected failed"
        );
        break;
    }
    case WIFI_EVENT_STA_DISCONNECTED: {
        if (_is_wlan_force_connecting) {
            _is_wlan_force_connecting = false;
            ESP_UTILS_LOGD("Ignore disconnect event when force connecting");
            break;
        }
        if (_is_wlan_retry_connecting) {
            ESP_UTILS_LOGD("Ignore disconnect event when retry connecting");
            break;
        }
        // Clear connected WLAN data
        _wlan_connected_info = {};
        ESP_UTILS_CHECK_FALSE_GOTO(
            updateUI_ScreenWlanConnected(false), end, "Update UI screen WLAN connected failed"
        );
        break;
    }
    case WIFI_EVENT_SCAN_DONE:
        ESP_UTILS_CHECK_FALSE_GOTO(
            updateUI_ScreenWlanAvailable(false), end, "Update UI screen WLAN available failed"
        );
        break;
    default:
        ESP_UTILS_LOGD("Ignore WLAN event(%d)", wlan_event_id);
        break;
    }

end:
    app.getCore()->unlockLv();

    ESP_UTILS_LOGD("Process on wlan UI thread stop");

    return ret;
}

const char *SettingsManager::getWlanGeneralStateStr(WlanGeneraState state)
{
    const char *unknown_str = "UNKNOWN";
    ESP_UTILS_CHECK_FALSE_RETURN(
        _wlan_general_state_str.find(state) != _wlan_general_state_str.end(), unknown_str, "Unknown WLAN general state"
    );

    return _wlan_general_state_str.at(state).c_str();
}

const char *SettingsManager::getWlanScanStateStr(WlanScanState state)
{
    const char *unknown_str = "UNKNOWN";
    ESP_UTILS_CHECK_FALSE_RETURN(
        _wlan_scan_state_str.find(state) != _wlan_scan_state_str.end(), unknown_str, "Unknown WLAN scan state"
    );

    return _wlan_scan_state_str.at(state).c_str();
}

const char *SettingsManager::getWlanOperationStr(WlanOperation operation)
{
    const char *unknown_str = "UNKNOWN";
    ESP_UTILS_CHECK_FALSE_RETURN(
        _wlan_operation_str.find(operation) != _wlan_operation_str.end(), unknown_str,
        "Unknown WLAN operation state"
    );

    return _wlan_operation_str.at(operation).c_str();
}

void SettingsManager::onWlanScanTimer(lv_timer_t *t)
{
    ESP_UTILS_CHECK_NULL_EXIT(t, "Invalid timer");
    ESP_UTILS_LOGD("On WLAN scan timer");

    SettingsManager *manager = (SettingsManager *)t->user_data;
    ESP_UTILS_CHECK_FALSE_EXIT(manager->processOnWlanScanTimer(t), "Process on WLAN update timer failed");
}

void SettingsManager::onWlanOperationThread(SettingsManager *manager)
{
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
    ESP_UTILS_LOGD("On WLAN operation thread start");

    while (true) {
        if (!manager->processOnWlanOperationThread()) {
            ESP_UTILS_LOGE("Process on WLAN operation thread failed");
        }
        if (manager->checkIsWlanGeneralState(WlanGeneraState::DEINIT)) {
            break;
        }
    }

    ESP_UTILS_LOGD("On WLAN operation thread end");
}

void SettingsManager::onWlanUI_Thread(SettingsManager *manager)
{
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
    ESP_UTILS_LOGD("On WLAN UI thread start");

    while (true) {
        if (!manager->processOnWlanUI_Thread()) {
            ESP_UTILS_LOGE("Process on WLAN UI thread failed");
        }
        if (manager->checkIsWlanGeneralState(WlanGeneraState::DEINIT)) {
            break;
        }
    }

    ESP_UTILS_LOGD("On WLAN UI thread end");
}

bool SettingsManager::processCloseUI_ScreenWlan()
{
    app.getCore()->getCoreEvent()->unregisterEvent(
        ui.screen_wlan.getEventObject(), onScreenNavigationClickEventHandler, ui.screen_wlan.getNavigaitionClickEventID()
    );

    // Avoid enter gesture event when App is closed
    auto gesture = app.getSystem()->manager.getGesture();
    if (gesture != nullptr) {
        if (!lv_obj_remove_event_cb(gesture->getEventObj(), onUI_ScreenWlanGestureEvent)) {
            ESP_UTILS_LOGE("Remove gesture event failed");
        }
    }

    return true;
}

bool SettingsManager::processCloseUI_ScreenWlanVerification()
{
    app.getCore()->getCoreEvent()->unregisterEvent(
        ui.screen_wlan_verification.getEventObject(), onScreenNavigationClickEventHandler,
        ui.screen_wlan_verification.getNavigaitionClickEventID()
    );

    return true;
}

bool SettingsManager::processOnUI_ScreenWlanVerificationKeyboardConfirmEvent(std::pair<std::string_view, std::string_view> ssid_with_pwd)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Process on UI screen WLAN connect keyboard confirm event handler");
    ESP_UTILS_LOGI("SSID: %s, PWD: %s", ssid_with_pwd.first.data(), ssid_with_pwd.second.data());

    auto [ssid, pwd] = ssid_with_pwd;
    if (!ssid.empty()) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            ssid == _wlan_connecting_info.first.ssid, false, "Mismatch SSID(%s, %s)", ssid.data(),
            _wlan_connecting_info.first.ssid.data()
        );
    } else {
        ssid = _wlan_connecting_info.first.ssid;
    }

    // Back to WLAN screen
    ESP_UTILS_CHECK_FALSE_RETURN(
        processBack(), false, "Process back failed"
    );

    // Update Available WLAN data
    ESP_UTILS_CHECK_FALSE_RETURN(
        updateUI_ScreenWlanAvailable(true, WlanGeneraState::CONNECTING), false, "Update UI screen WLAN available failed"
    );

    // Update connecting WLAN data
    ESP_UTILS_CHECK_FALSE_RETURN(
        updateUI_ScreenWlanConnected(true, WlanGeneraState::CONNECTING), false, "Update UI screen WLAN connected failed"
    );

    ESP_UTILS_CHECK_FALSE_RETURN(
        scrollUI_ScreenWlanConnectedToView(), false, "Show UI screen WLAN connected failed"
    );

    _wlan_connecting_info.second = pwd;
    ESP_UTILS_LOGI("Connecting to Wlan %s (pwd: %s)", ssid.data(), pwd.data());
    boost::thread([this]() {
        ESP_UTILS_CHECK_FALSE_EXIT(
            forceWlanOperation(WlanOperation::CONNECT, 0), "Force WLAN operation connect failed"
        );
    }).detach();

    return true;
}

bool SettingsManager::processOnUI_ScreenWlanControlSwitchChangeEvent(lv_event_t *e)
{
    ESP_UTILS_LOGD("Process on UI screen WLAN control switch value changed event");

    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    ESP_UTILS_CHECK_NULL_RETURN(sw, false, "Get switch failed");

    lv_state_t state = lv_obj_get_state(sw);

    bool wlan_sw_flag = state & LV_STATE_CHECKED;
    // Show/Hide status bar WLAN icon
    app.getSystem()->display.getQuickSettings().setWifiIconState(
        wlan_sw_flag ? QuickSettings::WifiState::DISCONNECTED : QuickSettings::WifiState::CLOSED
    );

    auto wlan_cell = ui.screen_settings.getCell(
                         static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::WIRELESS),
                         static_cast<int>(SettingsUI_ScreenSettingsCellIndex::WIRELESS_WLAN)
                     );
    ESP_UTILS_CHECK_NULL_RETURN(wlan_cell, false, "Get cell WLAN failed");
    ESP_UTILS_CHECK_FALSE_RETURN(
        wlan_cell->updateRightMainLabel(
            wlan_sw_flag ? UI_SCREEN_SETTINGS_WIRELESS_LABEL_TEXT_ON : UI_SCREEN_SETTINGS_WIRELESS_LABEL_TEXT_OFF
        ), false, "Update right main label failed"
    );

    WlanGeneraState target_state = wlan_sw_flag ? WlanGeneraState::STARTING : WlanGeneraState::STOPPING;
    ESP_UTILS_CHECK_FALSE_RETURN(
        updateUI_ScreenWlanConnected(true, target_state), false, "Update UI screen WLAN connected failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        updateUI_ScreenWlanAvailable(true, target_state), false, "Update UI screen WLAN available failed"
    );

    boost::thread([this, wlan_sw_flag]() {
        ESP_UTILS_CHECK_FALSE_EXIT(
            forceWlanOperation(wlan_sw_flag ? WlanOperation::START : WlanOperation::STOP, WLAN_START_WAIT_TIMEOUT_MS),
        );

        ESP_UTILS_CHECK_FALSE_EXIT(
            StorageNVS::requestInstance().setLocalParam(SETTINGS_NVS_KEY_WLAN_SWITCH, static_cast<int>(wlan_sw_flag)),
            "Set WLAN switch flag failed"
        );
    }).detach();

    return true;
}

bool SettingsManager::processOnUI_ScreenWlanAvailableCellClickEvent(const ESP_Brookesia_CoreEvent::HandlerData &data)
{
    ESP_UTILS_LOGD("Process on UI screen WLAN available cell clicked event");

    SettingsUI_WidgetCell *cell = (SettingsUI_WidgetCell *)data.object;
    ESP_UTILS_CHECK_NULL_RETURN(cell, false, "Invalid cell");

    int cell_index = ui.screen_wlan.getCellContainer(
                         static_cast<int>(SettingsUI_ScreenWlanContainerIndex::AVAILABLE)
                     ) ->getCellIndex(cell);
    ESP_UTILS_CHECK_FALSE_RETURN(cell_index >= 0, false, "Get cell index failed");
    ESP_UTILS_LOGD("Cell index: %d", cell_index);

    _wlan_connecting_info.first = _ui_wlan_available_data[cell_index];
    ESP_UTILS_LOGD("Connect to Wlan %s", _wlan_connecting_info.first.ssid.c_str());

    if (_wlan_connecting_info.first.is_locked) {
        // Stop WLAN scan timer first
        ESP_UTILS_CHECK_FALSE_RETURN(toggleWlanScanTimer(false), false, "Toggle WLAN scan timer failed");

        // If WLAN is locked, show verification screen
        lv_obj_t *label_screen_title = ui.screen_wlan_verification.getObject(SettingsUI_ScreenBaseObject::HEADER_TITLE_LABEL);
        if (label_screen_title != nullptr) {
            lv_label_set_text_fmt(label_screen_title, "%s", _wlan_connecting_info.first.ssid.c_str());
        }

        lv_obj_t *label_password_edit = ui.screen_wlan_verification.getElementObject(
                                            static_cast<int>(SettingsUI_ScreenWlanVerificationContainerIndex::PASSWORD),
                                            static_cast<int>(SettingsUI_ScreenWlanVerificationCellIndex::PASSWORD_EDIT),
                                            SettingsUI_WidgetCellElement::LEFT_TEXT_EDIT
                                        );
        ESP_UTILS_CHECK_NULL_RETURN(label_password_edit, false, "Get password edit failed");
        lv_textarea_set_text(label_password_edit, "");

        ESP_UTILS_CHECK_FALSE_RETURN(
            processUI_ScreenChange(UI_Screen::WLAN_VERIFICATION, ui.screen_wlan_verification.getScreenObject()),
            false, "Process UI screen change failed"
        );
    } else {
        // Update connecting WLAN data
        ESP_UTILS_CHECK_FALSE_RETURN(
            updateUI_ScreenWlanConnected(true, WlanGeneraState::CONNECTING), false,
            "Update UI screen WLAN connected failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            scrollUI_ScreenWlanConnectedToView(), false, "Show UI screen WLAN connected failed"
        );
        // Update available WLAN data
        ESP_UTILS_CHECK_FALSE_RETURN(
            updateUI_ScreenWlanAvailable(true, WlanGeneraState::CONNECTING), false,
            "Update UI screen WLAN connected failed"
        );
        // Force connect to WLAN
        _wlan_connecting_info.second = "";
        ESP_UTILS_CHECK_FALSE_RETURN(
            forceWlanOperation(WlanOperation::CONNECT, 0), false, "Force WLAN operation connect failed"
        );
    }

    return true;
}

bool SettingsManager::processOnUI_ScreenWlanGestureEvent(lv_event_t *e)
{
    ESP_UTILS_CHECK_NULL_RETURN(e, false, "Invalid event");
    // ESP_UTILS_LOGD("Process on UI screen WLAN gesture press event");

    if (_ui_current_screen != UI_Screen::WIRELESS_WLAN) {
        return true;
    }

    lv_event_code_t code = lv_event_get_code(e);
    GestureInfo *gesture_info = (GestureInfo *)lv_event_get_param(e);
    auto gesture = app.getSystem()->manager.getGesture();

    if (code == gesture->getPressingEventCode()) {
        if (_ui_wlan_available_clickable && (gesture_info->direction != GESTURE_DIR_NONE)) {
            ESP_UTILS_CHECK_FALSE_RETURN(
                ui.screen_wlan.setAvaliableClickable(false), false, "Set available clickable failed"
            );
            _ui_wlan_available_clickable = false;
        }
    }

    if (!_ui_wlan_available_clickable && (code == gesture->getReleaseEventCode())) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            ui.screen_wlan.setAvaliableClickable(true), false, "Set available clickable failed"
        );
        _ui_wlan_available_clickable = true;
    }

    return true;
}

bool SettingsManager::processOnWlanEventHandler(int event_id, void *event_data)
{
    ESP_UTILS_LOGI("Process WLAN event(%s) start", getWlanEventStr(static_cast<wifi_event_t>(event_id)));

    std::unique_lock<std::mutex> lock(_wlan_event_mutex);

    switch (event_id) {
    case WIFI_EVENT_STA_START:
        _wlan_general_state = WlanGeneraState::STARTED;
        _wlan_scan_state = WlanScanState::SCAN_STOPPED;
        break;
    case WIFI_EVENT_STA_STOP:
        _wlan_general_state = WlanGeneraState::STOPPED;
        _wlan_scan_state = WlanScanState::SCAN_STOPPED;
        break;
    case WIFI_EVENT_STA_CONNECTED:
        _wlan_general_state = WlanGeneraState::CONNECTED;
        _wlan_connect_retry_count = 0;
        _is_wlan_retry_connecting = false;
        break;
    case WIFI_EVENT_STA_DISCONNECTED: {
        wifi_event_sta_disconnected_t *data = (wifi_event_sta_disconnected_t *)event_data;
        ESP_UTILS_LOGD("Disconnect! (ssid: %s, reason: %d)", data->ssid, data->reason);
        bool need_check_retry = (
                                    !_is_wlan_force_connecting &&
                                    checkIsWlanGeneralState(WlanGeneraState::CONNECTING) &&
                                    (data->reason != WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) &&
                                    (data->reason != WIFI_REASON_AUTH_FAIL)
                                );
        _wlan_general_state = WlanGeneraState::DISCONNECTED;
        if (need_check_retry) {
            if (++_wlan_connect_retry_count <= WLAN_CONNECT_RETRY_MAX) {
                ESP_UTILS_LOGD(
                    "Retry connect to WLAN (%s %d/%d)", _wlan_connecting_info.first.ssid.c_str(),
                    _wlan_connect_retry_count, WLAN_CONNECT_RETRY_MAX
                );
                _is_wlan_retry_connecting = true;
            } else {
                ESP_UTILS_LOGD("Retry connect to WLAN failed");
                _is_wlan_retry_connecting = false;
                _wlan_connect_retry_count = 0;
            }
        } else {
            toggleWlanScanTimer(true);
        }
        break;
    }
    case WIFI_EVENT_SCAN_DONE:
        if (_wlan_scan_state == WlanScanState::SCANNING) {
            _wlan_scan_state = WlanScanState::SCAN_DONE;
        } else {
            _wlan_scan_state = WlanScanState::SCAN_STOPPED;
        }
        break;
    default:
        ESP_UTILS_LOGD("Ignore WLAN event(%d)", static_cast<int>(event_id));
        goto end;
    }

    if (event_id == WIFI_EVENT_SCAN_DONE) {
        ESP_UTILS_LOGI("Set WLAN scan state(%s)", getWlanScanStateStr(_wlan_scan_state));
    } else {
        ESP_UTILS_LOGI("Set WLAN general state(%s)", getWlanGeneralStateStr(_wlan_general_state));
    }

    _wlan_event_id = event_id;
    _is_wlan_event_updated = true;
    lock.unlock();
    _wlan_event_cv.notify_all();

    if (_is_wlan_retry_connecting) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            forceWlanOperation(WlanOperation::CONNECT, 0), false, "Force WLAN operation connect failed"
        );
    }

end:
    ESP_UTILS_LOGD("Process WLAN event end");

    return true;
}

bool SettingsManager::waitForWlanGeneralState(const std::vector<WlanGeneraState> &state, int timeout_ms)
{
    std::string state_str = "";
    for (auto &s : state) {
        state_str += " ";
        state_str += getWlanGeneralStateStr(s);
    }

    ESP_UTILS_LOGD("Wait for WLAN general state(%s) in %d ms", state_str.c_str(), static_cast<int>(timeout_ms));
    ESP_UTILS_LOGD("Current general state: %s", getWlanGeneralStateStr(_wlan_general_state));
    for (auto &s : state) {
        if (checkIsWlanGeneralState(s)) {
            ESP_UTILS_LOGD("General state(%s) is already", getWlanGeneralStateStr(s));
            return true;
        }
    }

    std::unique_lock<std::mutex> lock(_wlan_event_mutex);
    bool status = _wlan_event_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this, state] {
        for (auto &s : state)
        {
            if (checkIsWlanGeneralState(s)) {
                return true;
            }
        }
        return false;
    });
    ESP_UTILS_CHECK_FALSE_RETURN(status, false, "Wait timeout");
    ESP_UTILS_LOGD("Wait state finish");

    return status;
}

bool SettingsManager::waitForWlanScanState(const std::vector<WlanScanState> &state, int timeout_ms)
{
    std::string state_str = "";
    for (auto &s : state) {
        state_str += " ";
        state_str += getWlanScanStateStr(s);
    }

    ESP_UTILS_LOGD("Wait for WLAN scan state(%s) in %d ms", state_str.c_str(), static_cast<int>(timeout_ms));
    for (auto &s : state) {
        if (checkIsWlanScanState(s)) {
            ESP_UTILS_LOGD("Scan state(%s) is already", getWlanScanStateStr(s));
            return true;
        }
    }

    std::unique_lock<std::mutex> lock(_wlan_event_mutex);
    bool status = _wlan_event_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this, state] {
        for (auto &s : state)
        {
            if (checkIsWlanScanState(s)) {
                return true;
            }
        }
        return false;
    });
    ESP_UTILS_CHECK_FALSE_RETURN(status, false, "Wait timeout");
    ESP_UTILS_LOGD("Wait state finish");

    return true;
}

const char *SettingsManager::getWlanEventStr(wifi_event_t event)
{
    auto it = _wlan_event_str.find(event);
    if (it != _wlan_event_str.end()) {
        return it->second.c_str();
    }

    return "UNKNOWN";
}

void SettingsManager::onWlanEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_UTILS_CHECK_FALSE_EXIT(event_base == WIFI_EVENT, "Invalid event");
    SettingsManager *manager = (SettingsManager *)arg;
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
    ESP_UTILS_LOGD("On WLAN event handler");

    ESP_UTILS_CHECK_FALSE_EXIT(
        manager->processOnWlanEventHandler(static_cast<int>(event_id), event_data), "Process WLAN event failed"
    );
}

SettingsUI_ScreenWlan::WlanData SettingsManager::getWlanDataFromApInfo(wifi_ap_record_t &ap_info)
{
    SettingsUI_ScreenWlan::SignalLevel signal_level = SettingsUI_ScreenWlan::SignalLevel::WEAK;
    if (ap_info.rssi <= -70) {
        signal_level = SettingsUI_ScreenWlan::SignalLevel::WEAK;
    } else if (ap_info.rssi <= -50) {
        signal_level = SettingsUI_ScreenWlan::SignalLevel::MODERATE;
    } else {
        signal_level = SettingsUI_ScreenWlan::SignalLevel::GOOD;
    }

    return {
        std::string((char *)ap_info.ssid), (ap_info.authmode != WIFI_AUTH_OPEN) &&(ap_info.authmode != WIFI_AUTH_OWE),
        signal_level
    };
}

bool SettingsManager::isTimeSync()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    return (timeinfo.tm_year > (2020 - 1900));
}

} // namespace esp_brookesia::speaker_apps
