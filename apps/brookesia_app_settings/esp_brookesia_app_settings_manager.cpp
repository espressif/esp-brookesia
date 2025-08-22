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
#include "app_ap_conf.hpp"
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

#define ENTER_SCREEN_THREAD_NAME                "enter_screen"
#define ENTER_SCREEN_THREAD_STACK_SIZE          (4 * 1024)
#define ENTER_SCREEN_THREAD_STACK_CAPS_EXT      (true)

#define SAVE_WLAN_CONFIG_THREAD_NAME            "save_wlan_config"
#define SAVE_WLAN_CONFIG_THREAD_STACK_SIZE      (6 * 1024)
#define SAVE_WLAN_CONFIG_THREAD_STACK_CAPS_EXT  (true)

// UI screens
#define UI_SCREEN_BACK_MAP() \
    { \
        {UI_Screen::SETTINGS,          UI_Screen::HOME}, \
        {UI_Screen::MEDIA_SOUND,       UI_Screen::SETTINGS}, \
        {UI_Screen::MEDIA_DISPLAY,     UI_Screen::SETTINGS}, \
        {UI_Screen::WIRELESS_WLAN,     UI_Screen::SETTINGS}, \
        {UI_Screen::WLAN_VERIFICATION, UI_Screen::WIRELESS_WLAN}, \
        {UI_Screen::WLAN_SOFTAP,      UI_Screen::WIRELESS_WLAN}, \
        {UI_Screen::MORE_ABOUT,        UI_Screen::SETTINGS}, \
    }
// UI screen: Settings
#define UI_SCREEN_SETTINGS_WIRELESS_LABEL_TEXT_ON       "On"
#define UI_SCREEN_SETTINGS_WIRELESS_LABEL_TEXT_OFF      "Off"
// // UI screen: WLAN
// #define UI_SCREEN_WLAN_SCAN_TIMER_INTERVAL_MS           (20000)
#define UI_SCREEN_WLAN_SOFTAP_INFO_LABEL_TEXT() \
    "Option 1: Scan QRCode -> connect Wi-Fi in pop-up browser\n" \
    "Option 2: Join Wi-Fi '%s' -> visit '192.168.4.1' in browser"

// UI screen: About
#define _VERSION_STR(x, y, z)                           "V" # x "." # y "." # z
#define VERSION_STR(x, y, z)                           _VERSION_STR(x, y, z)
#define UI_SCREEN_ABOUT_SYSTEM_OS_NAME                  "FreeRTOS"
#define UI_SCREEN_ABOUT_SYSTEM_UI_NAME                  "ESP-Brookesia & LVGL"
#define UI_SCREEN_ABOUT_SYSTEM_UI_BROOKESIA_VERSION     VERSION_STR(BROOKESIA_CORE_VER_MAJOR, BROOKESIA_CORE_VER_MINOR ,\
                                                                    BROOKESIA_CORE_VER_PATCH)
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
#define WLAN_SCAN_CONNECT_AP_DELAY_MS   (200)
#define WLAN_DISCONNECT_HIDE_TIME_MS    (2000)
#define WLAN_INIT_WAIT_TIMEOUT_MS       (5000)
#define WLAN_START_WAIT_TIMEOUT_MS      (1000)
#define WLAN_STOP_WAIT_TIMEOUT_MS       (1000)
#define WLAN_CONNECT_WAIT_TIMEOUT_MS    (5000)
#define WLAN_DISCONNECT_WAIT_TIMEOUT_MS (5000)
#define WLAN_SCAN_START_WAIT_TIMEOUT_MS (5000)
#define WLAN_SCAN_STOP_WAIT_TIMEOUT_MS  (1000)

#define TOUCH_SW_FLAG_DEFAULT           (1)

#define NVS_ERASE_WAIT_TIMEOUT_MS       (1000)

using namespace esp_brookesia;
using namespace esp_brookesia::systems::speaker;
using namespace esp_brookesia::services;
using namespace esp_brookesia::gui;
using namespace esp_utils;

namespace esp_brookesia::apps {

const std::unordered_map<wifi_event_t, std::string> SettingsManager::_wlan_event_str = {
    {WIFI_EVENT_SCAN_DONE, "WIFI_EVENT_SCAN_DONE"},
    {WIFI_EVENT_STA_START, "WIFI_EVENT_STA_START"},
    {WIFI_EVENT_STA_STOP, "WIFI_EVENT_STA_STOP"},
    {WIFI_EVENT_STA_CONNECTED, "WIFI_EVENT_STA_CONNECTED"},
    {WIFI_EVENT_STA_DISCONNECTED, "WIFI_EVENT_STA_DISCONNECTED"},
};
const std::unordered_map<ip_event_t, std::string> SettingsManager::_ip_event_str = {
    {IP_EVENT_STA_GOT_IP, "IP_EVENT_STA_GOT_IP"},
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

SettingsManager::SettingsManager(App &app_in, SettingsUI &ui_in, const SettingsManagerData &data_in):
    app(app_in),
    ui(ui_in),
    data(data_in),
    _ui_current_screen(UI_Screen::HOME),
    _ui_screen_back_map(UI_SCREEN_BACK_MAP()),
    _is_wlan_operation_stopped(true),
    _wlan_event(WIFI_EVENT_WIFI_READY),
    _wlan_sta_netif(nullptr),
    _wlan_event_handler_instance(nullptr)
{
}

SettingsManager::~SettingsManager()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (!checkClosed()) {
        ESP_UTILS_CHECK_FALSE_EXIT(processClose(), "Close failed");
    }
}

bool SettingsManager::processInit()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(app_sntp_init(), false, "Init SNTP failed");

    ESP_UTILS_CHECK_FALSE_RETURN(initWlan(), false, "Init WLAN failed");

    auto &storage_service = StorageNVS::requestInstance();
    storage_service.connectEventSignal([this](const StorageNVS::Event & event) {
        if ((event.operation != StorageNVS::Operation::UpdateNVS) || (event.sender == this)) {
            ESP_UTILS_LOGD("Ignore event: operation(%d), sender(%p)", static_cast<int>(event.operation), event.sender);
            return;
        }

        StorageNVS::Value value;
        ESP_UTILS_CHECK_FALSE_EXIT(
            StorageNVS::requestInstance().getLocalParam(event.key, value), "Get NVS value failed"
        );

        if (event.key == Manager::SETTINGS_WLAN_SWITCH) {
            ESP_UTILS_CHECK_FALSE_EXIT(
                std::holds_alternative<int>(value), "Invalid WLAN switch flag type"
            );

            auto is_open = static_cast<bool>(std::get<int>(value));
            ESP_UTILS_CHECK_FALSE_EXIT(
                processStorageServiceEventSignalUpdateWlanSwitch(is_open), "Process WLAN switch flag updated failed"
            );
        } else if (event.key == Manager::SETTINGS_VOLUME) {
            ESP_UTILS_CHECK_FALSE_EXIT(
                std::holds_alternative<int>(value), "Invalid volume type"
            );

            auto volume = std::get<int>(value);
            ESP_UTILS_CHECK_FALSE_EXIT(
                processStorageServiceEventSignalUpdateVolume(volume), "Process volume updated failed"
            );
        } else if (event.key == Manager::SETTINGS_BRIGHTNESS) {
            ESP_UTILS_CHECK_FALSE_EXIT(
                std::holds_alternative<int>(value), "Invalid brightness type"
            );

            auto brightness = std::get<int>(value);
            ESP_UTILS_CHECK_FALSE_EXIT(
                processStorageServiceEventSignalUpdateBrightness(brightness), "Process brightness updated failed"
            );
        }
    });

    StorageNVS::Value wlan_sw_flag;
    int wlan_sw_flag_int = WLAN_SW_FLAG_DEFAULT;
    if (storage_service.getLocalParam(Manager::SETTINGS_WLAN_SWITCH, wlan_sw_flag)) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            std::holds_alternative<int>(wlan_sw_flag), false, "Invalid WLAN switch flag type"
        );
        wlan_sw_flag_int = std::get<int>(wlan_sw_flag);
    } else {
        ESP_UTILS_LOGW("WLAN switch flag not found in NVS, set to default value(%d)", static_cast<int>(wlan_sw_flag_int));
        ESP_UTILS_CHECK_FALSE_RETURN(
            storage_service.setLocalParam(Manager::SETTINGS_WLAN_SWITCH, wlan_sw_flag_int, this), false,
            "Failed to set WLAN switch flag"
        );
    }
    _is_wlan_sw_flag = wlan_sw_flag_int;

    StorageNVS::Value touch_sw_flag;
    int touch_sw_flag_int = TOUCH_SW_FLAG_DEFAULT;
    if (storage_service.getLocalParam(SETTINGS_NVS_KEY_TOUCH_SENSOR_SWITCH, touch_sw_flag)) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            std::holds_alternative<int>(touch_sw_flag), false, "Invalid touch switch flag type"
        );
        touch_sw_flag_int = std::get<int>(touch_sw_flag);
    } else {
        ESP_UTILS_LOGW("touch switch flag not found in NVS, set to default value(%d)", static_cast<int>(touch_sw_flag_int));
        ESP_UTILS_CHECK_FALSE_RETURN(
            storage_service.setLocalParam(SETTINGS_NVS_KEY_TOUCH_SENSOR_SWITCH, touch_sw_flag_int, this), false,
            "Failed to set touch switch flag"
        );
    }

    WlanOperation target_operation = WlanOperation::NONE;
    ESP_UTILS_CHECK_FALSE_RETURN(
        app.getSystem()->getDisplay().getQuickSettings().setWifiIconState(
            wlan_sw_flag_int ? QuickSettings::WifiState::DISCONNECTED : QuickSettings::WifiState::CLOSED
        ), false, "Set WLAN icon state failed"
    );
    // Force WLAN operation later since the Wlan init may take some time
    target_operation = wlan_sw_flag_int ? WlanOperation::START : WlanOperation::STOP;
    boost::thread([this, target_operation]() {
        ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

        // std::this_thread::sleep_for(boost::chrono::milliseconds(WLAN_INIT_WAIT_TIMEOUT_MS));
        if (!forceWlanOperation(target_operation, 0)) {
            ESP_UTILS_LOGE("Force WLAN operation(%d) failed", static_cast<int>(target_operation));
        }
    }).detach();

    StorageNVS::Value wlan_ssid;
    std::string wlan_ssid_str = WLAN_DEFAULT_SSID;
    if (!storage_service.getLocalParam(Manager::SETTINGS_WLAN_SSID, wlan_ssid)) {
        ESP_UTILS_LOGW("WLAN SSID not found in NVS, set to default value(%s)", wlan_ssid_str.c_str());
        ESP_UTILS_CHECK_FALSE_RETURN(
            storage_service.setLocalParam(Manager::SETTINGS_WLAN_SSID, wlan_ssid_str, this),
            false, "Failed to set WLAN SSID"
        );
    }

    StorageNVS::Value wlan_password;
    std::string wlan_password_str = WLAN_DEFAULT_PWD;
    if (!storage_service.getLocalParam(Manager::SETTINGS_WLAN_PASSWORD, wlan_password)) {
        ESP_UTILS_LOGW("WLAN password not found in NVS, set to default value(%s)", wlan_password_str.c_str());
        ESP_UTILS_CHECK_FALSE_RETURN(
            storage_service.setLocalParam(Manager::SETTINGS_WLAN_PASSWORD, wlan_password_str, this), false,
            "Failed to set WLAN password"
        );
    }

    app.getSystem()->registerAppEventCallback([](lv_event_t *event) {
        ESP_UTILS_LOG_TRACE_GUARD();

        auto manager = static_cast<SettingsManager *>(lv_event_get_user_data(event));
        ESP_UTILS_CHECK_NULL_EXIT(manager, "Manager is null");

        auto event_data = static_cast<systems::base::Context::AppEventData *>(lv_event_get_param(event));
        ESP_UTILS_CHECK_NULL_EXIT(event_data, "Event data is null");

        if ((event_data->type != systems::base::Context::AppEventType::OPERATION) || (event_data->id != manager->app.getId())) {
            return;
        }

        auto operation_data = static_cast<AppOperationData *>(event_data->data);
        ESP_UTILS_CHECK_NULL_EXIT(operation_data, "Operation data is null");

        ESP_UTILS_CHECK_FALSE_EXIT(manager->processAppEventOperation(*operation_data), "Process app event failed");
    }, this);

    return true;
}

bool SettingsManager::processRun()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(checkClosed(), false, "Already running");

    esp_utils::function_guard del_guard([this]() {
        ESP_UTILS_CHECK_FALSE_EXIT(processClose(), "Process close failed");
    });

    ESP_UTILS_CHECK_FALSE_RETURN(processRunUI_ScreenSettings(), false, "Process run UI screen settings failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processRunUI_ScreenWlan(), false, "Process run UI screen WLAN failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processRunUI_ScreenWlanVerification(), false, "Process run UI screen WLAN connect failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processRunUI_ScreenWlanSoftAP(), false, "Process run UI screen WLAN softap failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processRunUI_ScreenAbout(), false, "Process run UI screen about failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processRunUI_ScreenSound(), false, "Process run UI screen sound failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processRunUI_ScreenDisplay(), false, "Process run UI screen display failed");

    _ui_current_screen = UI_Screen::SETTINGS;
    _is_ui_initialized = true;

    del_guard.release();

    if (!updateUI_ScreenWlanAvailable(false)) {
        ESP_UTILS_LOGE("Update UI screen WLAN available failed");
    }
    if (!updateUI_ScreenWlanConnected(false)) {
        ESP_UTILS_LOGE("Update UI screen WLAN connected failed");
    }

    return true;
}

bool SettingsManager::processBack()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

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
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(!checkClosed(), false, "Already closed");

    bool is_success = true;

    _is_ui_initialized = false;

    if (!processCloseUI_ScreenWlan()) {
        ESP_UTILS_LOGE("Process close UI screen WLAN failed");
        is_success = false;
    }
    if (!processCloseUI_ScreenWlanVerification()) {
        ESP_UTILS_LOGE("Process close UI screen WLAN connect failed");
        is_success = false;
    }
    if (!processCloseUI_ScreenWlanSoftAP()) {
        ESP_UTILS_LOGE("Process close UI screen WLAN softap failed");
        is_success = false;
    }
    if (!processCloseUI_ScreenSettings()) {
        ESP_UTILS_LOGE("Process close UI screen settings failed");
        is_success = false;
    }
    if (!processCloseUI_ScreenSound()) {
        ESP_UTILS_LOGE("Process close UI screen sound failed");
        is_success = false;
    }
    if (!processCloseUI_ScreenDisplay()) {
        ESP_UTILS_LOGE("Process close UI screen display failed");
        is_success = false;
    }
    if (!processCloseUI_ScreenAbout()) {
        ESP_UTILS_LOGE("Process close UI screen about failed");
        is_success = false;
    }

    _ui_current_screen = UI_Screen::HOME;
    _ui_screen_object_map.clear();

    return is_success;
}

bool SettingsManager::processRunUI_ScreenSettings()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    esp_utils::function_guard del_guard([this]() {
        ESP_UTILS_CHECK_FALSE_EXIT(processCloseUI_ScreenSettings(), "Process close UI screen settings failed");
    });

    // Wireless: WLAN
    SettingsUI_WidgetCell *wlan_cell = ui.screen_settings.getCell(
                                           static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::WIRELESS),
                                           static_cast<int>(SettingsUI_ScreenSettingsCellIndex::WIRELESS_WLAN)
                                       );
    ESP_UTILS_CHECK_NULL_RETURN(wlan_cell, false, "Get cell WLAN failed");
    _ui_screen_object_map[wlan_cell->getEventObject()] = {
        UI_Screen::WIRELESS_WLAN, ui.screen_wlan.getScreenObject()
    };
    ESP_UTILS_CHECK_FALSE_RETURN(
        app.getSystemContext()->getEvent().registerEvent(
            wlan_cell->getEventObject(), onScreenSettingsCellClickEventHandler, wlan_cell->getClickEventID(), this
        ), false, "Register event failed"
    );
    {
        StorageNVS::Value wlan_sw_flag;
        ESP_UTILS_CHECK_FALSE_RETURN(
            StorageNVS::requestInstance().getLocalParam(Manager::SETTINGS_WLAN_SWITCH, wlan_sw_flag), false,
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
        ESP_UTILS_CHECK_NULL_RETURN(sound_cell, false, "Get cell sound failed");
        _ui_screen_object_map[sound_cell->getEventObject()] = {
            UI_Screen::MEDIA_SOUND, ui.screen_sound.getScreenObject()
        };
        ESP_UTILS_CHECK_FALSE_RETURN(
            app.getSystemContext()->getEvent().registerEvent(
                sound_cell->getEventObject(), onScreenSettingsCellClickEventHandler, sound_cell->getClickEventID(), this
            ), false, "Register event failed"
        );
    }

    // Media: Display
    {
        SettingsUI_WidgetCell *display_cell = ui.screen_settings.getCell(
                static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MEDIA),
                static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MEDIA_DISPLAY)
                                              );
        ESP_UTILS_CHECK_NULL_RETURN(display_cell, false, "Get cell display failed");
        _ui_screen_object_map[display_cell->getEventObject()] = {
            UI_Screen::MEDIA_DISPLAY, ui.screen_display.getScreenObject()
        };
        ESP_UTILS_CHECK_FALSE_RETURN(
            app.getSystemContext()->getEvent().registerEvent(
                display_cell->getEventObject(), onScreenSettingsCellClickEventHandler, display_cell->getClickEventID(), this
            ), false, "Register event failed"
        );
    }

    // Process touch sensor switch
    lv_obj_t *touch_sw = ui.screen_settings.getElementObject(
                             static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::INPUT),
                             static_cast<int>(SettingsUI_ScreenSettingsCellIndex::INPUT_TOUCH),
                             SettingsUI_WidgetCellElement::RIGHT_SWITCH
                         );
    ESP_UTILS_CHECK_NULL_RETURN(touch_sw, false, "Get Touch switch failed");
    StorageNVS::Value touch_sw_flag;
    ESP_UTILS_CHECK_FALSE_RETURN(
        StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_TOUCH_SENSOR_SWITCH, touch_sw_flag), false,
        "Get Touch switch flag failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        std::holds_alternative<int>(touch_sw_flag), false, "Invalid Touch switch flag type"
    );
    auto &touch_sw_flag_int = std::get<int>(touch_sw_flag);
    if (touch_sw_flag_int) {
        lv_obj_add_state(touch_sw, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(touch_sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(touch_sw, [] (lv_event_t *event) {
        auto obj = lv_event_get_target_obj(event);
        int s = lv_obj_has_state(obj, LV_STATE_CHECKED) ? 1 : 0;
        ESP_UTILS_CHECK_FALSE_EXIT(
            StorageNVS::requestInstance().setLocalParam(SETTINGS_NVS_KEY_TOUCH_SENSOR_SWITCH, s),
            "Get Touch switch flag failed"
        );
    }, LV_EVENT_VALUE_CHANGED, this);

    // More: About
    {
        SettingsUI_WidgetCell *about_cell = ui.screen_settings.getCell(
                                                static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MORE),
                                                static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MORE_ABOUT)
                                            );
        ESP_UTILS_CHECK_NULL_RETURN(about_cell, false, "Get cell about failed");
        _ui_screen_object_map[about_cell->getEventObject()] = {
            UI_Screen::MORE_ABOUT, ui.screen_about.getScreenObject()
        };
        ESP_UTILS_CHECK_FALSE_RETURN(
            app.getSystemContext()->getEvent().registerEvent(
                about_cell->getEventObject(), onScreenSettingsCellClickEventHandler, about_cell->getClickEventID(), this
            ), false, "Register event failed"
        );
    }

    // More: Restart
    {
        SettingsUI_WidgetCell *restart_cell = ui.screen_settings.getCell(
                static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MORE),
                static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MORE_RESTORE)
                                              );
        ESP_UTILS_CHECK_NULL_RETURN(restart_cell, false, "Get cell restart failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            app.getSystemContext()->getEvent().registerEvent(
        restart_cell->getEventObject(), [](const systems::base::Event::HandlerData & data) {
            ESP_UTILS_LOGW("Erase NVS flash");

            StorageNVS::EventFuture future;
            ESP_UTILS_CHECK_FALSE_RETURN(
                StorageNVS::requestInstance().eraseNVS(nullptr, &future), false, "Erase NVS failed"
            );
            auto status = future.wait_for(std::chrono::milliseconds(NVS_ERASE_WAIT_TIMEOUT_MS));
            ESP_UTILS_CHECK_FALSE_RETURN(status == std::future_status::ready, false, "Wait for erase NVS timeout");
            ESP_UTILS_CHECK_FALSE_RETURN(future.get(), false, "Erase NVS failed");

            ESP_UTILS_LOGW("Restart system");
            esp_restart();
            return true;
        }, restart_cell->getClickEventID(), nullptr
            ), false, "Register event failed"
        );
    }

    // More: Developer mode
    {
        SettingsUI_WidgetCell *developer_mode_cell = ui.screen_settings.getCell(
                    static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MORE),
                    static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MORE_DEVELOPER_MODE)
                );
        ESP_UTILS_CHECK_NULL_RETURN(developer_mode_cell, false, "Get cell developer mode failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            app.getSystemContext()->getEvent().registerEvent(
        developer_mode_cell->getEventObject(), [](const systems::base::Event::HandlerData & data) {
            auto manager = static_cast<SettingsManager *>(data.user_data);

            ESP_UTILS_CHECK_NULL_RETURN(manager, false, "Manager is null");
            ESP_UTILS_CHECK_FALSE_RETURN(
                manager->event_signal(EventType::EnterDeveloperMode, std::monostate()), false,
                "Enter developer mode failed"
            );

            return true;
        }, developer_mode_cell->getClickEventID(), this
            ), false, "Register event failed"
        );
    }

    del_guard.release();

    return true;
}

bool SettingsManager::processCloseUI_ScreenSettings()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    bool is_success = true;

    // Wireless: WLAN
    SettingsUI_WidgetCell *wlan_cell = ui.screen_settings.getCell(
                                           static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::WIRELESS),
                                           static_cast<int>(SettingsUI_ScreenSettingsCellIndex::WIRELESS_WLAN)
                                       );
    if (wlan_cell) {
        app.getSystemContext()->getEvent().unregisterEvent(
            wlan_cell->getEventObject(), onScreenSettingsCellClickEventHandler, wlan_cell->getClickEventID()
        );
    } else {
        ESP_UTILS_LOGE("Get cell WLAN failed");
        is_success = false;
    }

    // Media: Sound
    SettingsUI_WidgetCell *sound_cell = ui.screen_settings.getCell(
                                            static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MEDIA),
                                            static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MEDIA_SOUND)
                                        );
    if (sound_cell) {
        app.getSystemContext()->getEvent().unregisterEvent(
            sound_cell->getEventObject(), onScreenSettingsCellClickEventHandler, sound_cell->getClickEventID()
        );
    } else {
        ESP_UTILS_LOGE("Get cell sound failed");
        is_success = false;
    }

    // Media: Display
    SettingsUI_WidgetCell *display_cell = ui.screen_settings.getCell(
            static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MEDIA),
            static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MEDIA_DISPLAY)
                                          );
    if (display_cell) {
        app.getSystemContext()->getEvent().unregisterEvent(
            display_cell->getEventObject(), onScreenSettingsCellClickEventHandler, display_cell->getClickEventID()
        );
    } else {
        ESP_UTILS_LOGE("Get cell display failed");
        is_success = false;
    }

    // More: Restart
    SettingsUI_WidgetCell *restart_cell = ui.screen_settings.getCell(
            static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MORE),
            static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MORE_RESTORE)
                                          );
    if (restart_cell) {
        app.getSystemContext()->getEvent().unregisterEvent(
            restart_cell->getEventObject(), onScreenSettingsCellClickEventHandler, restart_cell->getClickEventID()
        );
    } else {
        ESP_UTILS_LOGE("Get cell restart failed");
        is_success = false;
    }

    // More: Developer mode
    SettingsUI_WidgetCell *developer_mode_cell = ui.screen_settings.getCell(
                static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MORE),
                static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MORE_DEVELOPER_MODE)
            );
    if (developer_mode_cell) {
        app.getSystemContext()->getEvent().unregisterEvent(
            developer_mode_cell->getEventObject(), onScreenSettingsCellClickEventHandler, developer_mode_cell->getClickEventID()
        );
    } else {
        ESP_UTILS_LOGE("Get cell developer mode failed");
        is_success = false;
    }

    // More: About
    SettingsUI_WidgetCell *about_cell = ui.screen_settings.getCell(
                                            static_cast<int>(SettingsUI_ScreenSettingsContainerIndex::MORE),
                                            static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MORE_ABOUT)
                                        );
    if (about_cell) {
        app.getSystemContext()->getEvent().unregisterEvent(
            about_cell->getEventObject(), onScreenSettingsCellClickEventHandler, about_cell->getClickEventID()
        );
    } else {
        ESP_UTILS_LOGE("Get cell about failed");
        is_success = false;
    }

    return is_success;
}

bool SettingsManager::onScreenSettingsCellClickEventHandler(const systems::base::Event::HandlerData &data)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_NULL_RETURN(data.object, false, "Invalid object");
    ESP_UTILS_CHECK_NULL_RETURN(data.user_data, false, "Invalid user data");

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

bool SettingsManager::processRunUI_ScreenWlan()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    esp_utils::function_guard del_guard([this]() {
        ESP_UTILS_CHECK_FALSE_EXIT(processCloseUI_ScreenWlan(), "Process close UI screen WLAN failed");
    });

    // Process screen header
    ESP_UTILS_CHECK_FALSE_RETURN(
        app.getSystemContext()->getEvent().registerEvent(
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
    ESP_UTILS_CHECK_NULL_RETURN(wlan_sw, false, "Get WLAN switch failed");
    StorageNVS::Value wlan_sw_flag;
    ESP_UTILS_CHECK_FALSE_RETURN(
        StorageNVS::requestInstance().getLocalParam(Manager::SETTINGS_WLAN_SWITCH, wlan_sw_flag), false,
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

    // Process WLAN available list
    auto gesture = app.getSystem()->getManager().getGesture();
    ESP_UTILS_CHECK_NULL_RETURN(gesture, false, "Get gesture failed");
    lv_obj_add_event_cb(
        gesture->getEventObj(), onUI_ScreenWlanGestureEvent, gesture->getPressingEventCode(), this
    );
    lv_obj_add_event_cb(
        gesture->getEventObj(), onUI_ScreenWlanGestureEvent, gesture->getReleaseEventCode(), this
    );

    // Process WLAN softap
    SettingsUI_WidgetCell *softap_cell = ui.screen_wlan.getCell(
            static_cast<int>(SettingsUI_ScreenWlanContainerIndex::PROVISIONING),
            static_cast<int>(SettingsUI_ScreenWlanCellIndex::PROVISIONING_SOFTAP)
                                         );
    ESP_UTILS_CHECK_NULL_RETURN(softap_cell, false, "Get cell softap failed");
    _ui_screen_object_map[softap_cell->getEventObject()] = {
        UI_Screen::WLAN_SOFTAP, ui.screen_wlan_softap.getScreenObject()
    };
    ESP_UTILS_CHECK_FALSE_RETURN(
        ui.screen_wlan.setSoftAPVisible(wlan_sw_flag_int), false, "Set softap visible failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        app.getSystemContext()->getEvent().registerEvent(
            softap_cell->getEventObject(), onScreenSettingsCellClickEventHandler, softap_cell->getClickEventID(), this
        ), false, "Register event softap cell click failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        app.getSystemContext()->getEvent().registerEvent(
    softap_cell->getEventObject(), [](const systems::base::Event::HandlerData & data) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_CHECK_NULL_RETURN(data.object, false, "Invalid object");
        ESP_UTILS_CHECK_NULL_RETURN(data.user_data, false, "Invalid user data");

        SettingsManager *manager = static_cast<SettingsManager *>(data.user_data);
        ESP_UTILS_CHECK_NULL_RETURN(manager, false, "Manager is null");

        ESP_UTILS_CHECK_FALSE_RETURN(
            manager->processOnUI_ScreenWlanSoftAPCellClickEvent(data), false,
            "Process on UI screen WLAN softap cell click event failed"
        );

        return true;
    }, softap_cell->getClickEventID(), this
        ), false, "Register event softap cell click failed"
    );

    del_guard.release();

    return true;
}

bool SettingsManager::processCloseUI_ScreenWlan()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    bool is_success = true;

    app.getSystemContext()->getEvent().unregisterEvent(
        ui.screen_wlan.getEventObject(), onScreenNavigationClickEventHandler, ui.screen_wlan.getNavigaitionClickEventID()
    );

    // Process WLAN softap
    SettingsUI_WidgetCell *softap_cell = ui.screen_wlan.getCell(
            static_cast<int>(SettingsUI_ScreenWlanContainerIndex::PROVISIONING),
            static_cast<int>(SettingsUI_ScreenWlanCellIndex::PROVISIONING_SOFTAP)
                                         );
    if (softap_cell) {
        app.getSystemContext()->getEvent().unregisterEvent(
            softap_cell->getEventObject(), onScreenSettingsCellClickEventHandler, softap_cell->getClickEventID()
        );
    } else {
        ESP_UTILS_LOGE("Get cell softap failed");
        is_success = false;
    }

    // Avoid enter gesture event when App is closed
    auto gesture = app.getSystem()->getManager().getGesture();
    if (gesture != nullptr) {
        if (!lv_obj_remove_event_cb(gesture->getEventObj(), onUI_ScreenWlanGestureEvent)) {
            ESP_UTILS_LOGE("Remove gesture event failed");
            is_success = false;
        }
    }

    return is_success;
}

bool SettingsManager::processRunUI_ScreenWlanVerification()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    esp_utils::function_guard del_guard([this]() {
        ESP_UTILS_CHECK_FALSE_EXIT(processCloseUI_ScreenWlanVerification(), "Process close UI screen WLAN connect failed");
    });

    // Register Navigation click event
    ESP_UTILS_CHECK_FALSE_RETURN(
        app.getSystemContext()->getEvent().registerEvent(
            ui.screen_wlan_verification.getEventObject(), onScreenNavigationClickEventHandler,
            ui.screen_wlan_verification.getNavigaitionClickEventID(), this
        ), false, "Register navigation click event failed"
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

    del_guard.release();

    return true;
}

bool SettingsManager::processCloseUI_ScreenWlanVerification()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    app.getSystemContext()->getEvent().unregisterEvent(
        ui.screen_wlan_verification.getEventObject(), onScreenNavigationClickEventHandler,
        ui.screen_wlan_verification.getNavigaitionClickEventID()
    );

    return true;
}

bool SettingsManager::processOnUI_ScreenWlanVerificationKeyboardConfirmEvent(std::pair<std::string_view, std::string_view> ssid_with_pwd)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    if (_ui_current_screen != UI_Screen::WLAN_VERIFICATION) {
        ESP_UTILS_LOGD("Ignore keyboard confirm event");
        return true;
    }

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

    _wlan_connecting_info.second = pwd;
    asyncWlanConnect(0);

    return true;
}

bool SettingsManager::processRunUI_ScreenWlanSoftAP()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    esp_utils::function_guard del_guard([this]() {
        ESP_UTILS_CHECK_FALSE_EXIT(processCloseUI_ScreenWlanSoftAP(), "Process close UI screen WLAN softap failed");
    });

    // Register Navigation click event
    ESP_UTILS_CHECK_FALSE_RETURN(
        app.getSystemContext()->getEvent().registerEvent(
            ui.screen_wlan_softap.getEventObject(), onScreenNavigationClickEventHandler,
            ui.screen_wlan_softap.getNavigaitionClickEventID(), this
        ), false, "Register navigation click event failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        app.getSystemContext()->getEvent().registerEvent(
    ui.screen_wlan_softap.getEventObject(), [](const systems::base::Event::HandlerData & data) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_CHECK_NULL_RETURN(data.object, false, "Invalid object");
        ESP_UTILS_CHECK_NULL_RETURN(data.user_data, false, "Invalid user data");

        SettingsManager *manager = static_cast<SettingsManager *>(data.user_data);
        ESP_UTILS_CHECK_NULL_RETURN(manager, false, "Manager is null");

        ESP_UTILS_CHECK_FALSE_RETURN(
            manager->processOnUI_ScreenWlanSoftAPNavigationClickEvent(data), false,
            "Process on UI screen WLAN softap navigation click event failed"
        );

        return true;
    }, ui.screen_wlan_softap.getNavigaitionClickEventID(), this
        ), false, "Register event softap cell click failed"
    );

    del_guard.release();

    return true;
}

bool SettingsManager::processCloseUI_ScreenWlanSoftAP()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ApProvision::stop();

    app.getSystemContext()->getEvent().unregisterEvent(
        ui.screen_wlan_softap.getEventObject(), onScreenNavigationClickEventHandler,
        ui.screen_wlan_softap.getNavigaitionClickEventID()
    );

    return true;
}

bool SettingsManager::processOnUI_ScreenWlanSoftAPCellClickEvent(const systems::base::Event::HandlerData &data)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGI("Parameter: data(%p)", data.user_data);

    esp_utils::value_guard visible_guard(_ui_wlan_softap_visible);

    visible_guard.set(true);

    ESP_UTILS_CHECK_FALSE_RETURN(toggleWlanScanTimer(false), false, "Toggle WLAN scan timer failed");
    if (!forceWlanOperation(WlanOperation::SCAN_STOP)) {
        ESP_UTILS_LOGE("Force WLAN operation scan stop failed");
    }

    std::vector<wifi_ap_record_t> ap_records;
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > 0) {
        ap_records.resize(ap_count);
        esp_wifi_scan_get_ap_records(&ap_count, ap_records.data());
    }
    if (ap_records.empty()) {
        ESP_UTILS_LOGI("Fallback to UI available data for initial AP list");
        std::lock_guard<std::mutex> lock(_ui_wlan_available_data_mutex);
        for (const auto &item : _ui_wlan_available_data) {
            wifi_ap_record_t rec = {};
            strncpy((char *)rec.ssid, item.ssid.c_str(), sizeof(rec.ssid));
            switch (item.signal_level) {
            case SettingsUI_ScreenWlan::SignalLevel::GOOD:
                rec.rssi = -40; break;
            case SettingsUI_ScreenWlan::SignalLevel::MODERATE:
                rec.rssi = -60; break;
            case SettingsUI_ScreenWlan::SignalLevel::WEAK:
            default:
                rec.rssi = -80; break;
            }
            rec.authmode = item.is_locked ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
            ap_records.push_back(rec);
        }
    }

    ESP_UTILS_CHECK_ERROR_RETURN(ApProvision::start(
    [this](const std::string & ssid, const std::string & pwd) {
        ESP_UTILS_LOGI("Get provisioned SSID: %s, PWD: %s", ssid.empty() ? "NULL" : ssid.data(), pwd.empty() ? "NULL" : pwd.data());

        ESP_UTILS_CHECK_FALSE_EXIT(!ssid.empty(), "Invalid SSID");

        _wlan_connecting_info.first.ssid = ssid;
        _wlan_connecting_info.second = pwd;

        strncpy((char *)_wlan_config.sta.ssid, ssid.c_str(), sizeof(_wlan_config.sta.ssid) - 1);
        strncpy((char *)_wlan_config.sta.password, pwd.c_str(), sizeof(_wlan_config.sta.password) - 1);

        {
            esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
                .name  = SAVE_WLAN_CONFIG_THREAD_NAME,
                .stack_size = SAVE_WLAN_CONFIG_THREAD_STACK_SIZE,
                .stack_in_ext = SAVE_WLAN_CONFIG_THREAD_STACK_CAPS_EXT,
            });
            boost::thread([this, ssid, pwd]() {
                ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

                ESP_UTILS_CHECK_FALSE_EXIT(saveWlanConfig(ssid, pwd), "Save WLAN config failed");

                LvLockGuard gui_guard;
                systems::base::Event::HandlerData fake_data = {};
                ESP_UTILS_CHECK_FALSE_EXIT(
                    processOnUI_ScreenWlanSoftAPNavigationClickEvent(fake_data),
                    "Process on UI screen WLAN softap navigation click event failed"
                );

                ESP_UTILS_CHECK_FALSE_EXIT(processBack(), "Process back failed");
            }).detach();
        }
    },
    [this](bool running) {
        ESP_UTILS_LOGI("AP Provisioning state changed: %d", running);
    }, ap_records), false, "Start provisioning failed");

    std::string qr_string = "WIFI:T:nopass;S:" + std::string(ApProvision::get_ap_ssid()) + ";P:;;";
    auto qr_code_image = ui.screen_wlan_softap.getQRCodeImage();
    ESP_UTILS_CHECK_NULL_RETURN(qr_code_image, false, "Get QR code image failed");
    lv_qrcode_update(qr_code_image, qr_string.c_str(), qr_string.length());

    auto info_label = ui.screen_wlan_softap.getInfoLabel();
    ESP_UTILS_CHECK_NULL_RETURN(info_label, false, "Get info label failed");
    lv_label_set_text_fmt(info_label, UI_SCREEN_WLAN_SOFTAP_INFO_LABEL_TEXT(), ApProvision::get_ap_ssid());

    visible_guard.release();

    return true;
}

bool SettingsManager::processOnUI_ScreenWlanSoftAPNavigationClickEvent(const systems::base::Event::HandlerData &data)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGI("Parameter: data(%p)", data.user_data);

    _ui_wlan_softap_visible = false;

    ESP_UTILS_CHECK_ERROR_RETURN(ApProvision::stop(), false, "Stop provisioning failed");

    if (!toggleWlanScanTimer(true, true)) {
        ESP_UTILS_LOGE("Toggle WLAN scan timer failed");
    }

    return true;
}

bool SettingsManager::updateUI_ScreenWlanConnected(bool use_target, WlanGeneraState target_state)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD(
        "Parameter: use_target(%d), target_state(%s)", use_target,
        use_target ? getWlanGeneralStateStr(target_state) : getWlanGeneralStateStr(_wlan_general_state)
    );

    auto checkIsWlanGeneralState = [this, target_state, use_target](WlanGeneraState state) -> bool {
        return use_target ? ((target_state & state) == state) : (this->checkIsWlanGeneralState(state));
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
                ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

                boost::this_thread::sleep_for(boost::chrono::milliseconds(WLAN_DISCONNECT_HIDE_TIME_MS));

                if (this->checkIsWlanGeneralState(WlanGeneraState::_CONNECT)) {
                    ESP_UTILS_LOGD("WLAN is connected, skip hide");
                    return;
                }

                if (!this->checkIsWlanGeneralState(WlanGeneraState::_START)) {
                    ESP_UTILS_LOGD("WLAN is not started, skip hide");
                    return;
                }

                if (!toggleWlanScanTimer(true, true)) {
                    ESP_UTILS_LOGE("Toggle WLAN scan timer failed");
                }

                if (ui.checkInitialized()) {
                    LvLockGuard gui_guard;
                    ESP_UTILS_CHECK_FALSE_EXIT(
                        ui.screen_wlan.setConnectedVisible(false), "Set WLAN connect visible failed"
                    );
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
        ui.screen_wlan.setConnectedVisible(_is_wlan_sw_flag), false, "Set WLAN connect visible failed"
    );

    return true;
}

bool SettingsManager::updateUI_ScreenWlanAvailable(bool use_target, WlanGeneraState target_state)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGI(
        "Parameter: use_target(%d), target_state(%s)", use_target,
        use_target ? getWlanGeneralStateStr(target_state) : getWlanGeneralStateStr(_wlan_general_state)
    );

    auto checkIsWlanGeneralState = [this, target_state, use_target](WlanGeneraState state) -> bool {
        return use_target ? ((target_state & state) == state) : (this->checkIsWlanGeneralState(state));
    };

    decltype(_ui_wlan_available_data) temp_available_data;
    {
        std::lock_guard<std::mutex> lock(_ui_wlan_available_data_mutex);
        _ui_wlan_available_data.erase(std::remove_if(_ui_wlan_available_data.begin(), _ui_wlan_available_data.end(),
        [this](const SettingsUI_ScreenWlan::WlanData & data) {
            return (data.ssid == _wlan_connecting_info.first.ssid) || (data.ssid == _wlan_connected_info.first.ssid);
        }), _ui_wlan_available_data.end());
        temp_available_data = _ui_wlan_available_data;
    }
    ESP_UTILS_CHECK_FALSE_RETURN(
        ui.screen_wlan.updateAvailableData(std::move(temp_available_data), onUI_ScreenWlanAvailableCellClickEventHander, this),
        false, "Update WLAN available data failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        ui.screen_wlan.setAvailableVisible(checkIsWlanGeneralState(WlanGeneraState::_START) && _is_wlan_sw_flag), false,
        "Set WLAN available visible failed"
    );

    return true;
}

bool SettingsManager::onUI_ScreenWlanAvailableCellClickEventHander(const systems::base::Event::HandlerData &data)
{
    ESP_UTILS_LOG_TRACE_GUARD();

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
    // ESP_UTILS_LOG_TRACE_GUARD();

    // ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

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
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

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

bool SettingsManager::processOnUI_ScreenSoundVolumeSliderValueChangeEvent(lv_event_t *e)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    ESP_UTILS_CHECK_NULL_RETURN(slider, false, "Invalid slider");

    int target_value = lv_slider_get_value(slider);
    ESP_UTILS_CHECK_FALSE_RETURN(
        StorageNVS::requestInstance().setLocalParam(Manager::SETTINGS_VOLUME, target_value), false,
        "Set media sound volume failed"
    );

    return true;
}

void SettingsManager::onUI_ScreenSoundVolumeSliderValueChangeEvent(lv_event_t *e)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

    SettingsManager *manager = (SettingsManager *)lv_event_get_user_data(e);
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid app pointer");

    ESP_UTILS_CHECK_FALSE_EXIT(
        manager->processOnUI_ScreenSoundVolumeSliderValueChangeEvent(e),
        "Process on UI screen sound volume slider value change event failed"
    );
}

bool SettingsManager::processRunUI_ScreenSound()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    esp_utils::function_guard del_guard([this]() {
        ESP_UTILS_CHECK_FALSE_EXIT(processCloseUI_ScreenSound(), "Process close UI screen sound failed");
    });

    ESP_UTILS_CHECK_FALSE_RETURN(
        app.getSystemContext()->getEvent().registerEvent(
            ui.screen_sound.getEventObject(), onScreenNavigationClickEventHandler,
            ui.screen_sound.getNavigaitionClickEventID(), this
        ), false, "Register navigation click event failed"
    );

    {
        auto volume_slider = ui.screen_sound.getElementObject(
                                 static_cast<int>(SettingsUI_ScreenSoundContainerIndex::VOLUME),
                                 static_cast<int>(SettingsUI_ScreenSoundCellIndex::VOLUME_SLIDER),
                                 SettingsUI_WidgetCellElement::CENTER_SLIDER
                             );
        ESP_UTILS_CHECK_NULL_RETURN(volume_slider, false, "Get cell volume slider failed");

        StorageNVS::Value value;
        ESP_UTILS_CHECK_FALSE_RETURN(
            StorageNVS::requestInstance().getLocalParam(Manager::SETTINGS_VOLUME, value), false,
            "Get media sound volume failed"
        );
        lv_slider_set_value(volume_slider, std::get<int>(value), LV_ANIM_OFF);
        lv_obj_add_event_cb(volume_slider, onUI_ScreenSoundVolumeSliderValueChangeEvent, LV_EVENT_VALUE_CHANGED, this);
    }

    del_guard.release();

    return true;
}

bool SettingsManager::processCloseUI_ScreenSound()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    app.getSystemContext()->getEvent().unregisterEvent(
        ui.screen_sound.getEventObject(), onScreenNavigationClickEventHandler,
        ui.screen_sound.getNavigaitionClickEventID()
    );

    return true;
}

bool SettingsManager::processRunUI_ScreenDisplay()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    esp_utils::function_guard del_guard([this]() {
        ESP_UTILS_CHECK_FALSE_EXIT(processCloseUI_ScreenDisplay(), "Process close UI screen display failed");
    });

    ESP_UTILS_CHECK_FALSE_RETURN(
        app.getSystemContext()->getEvent().registerEvent(
            ui.screen_display.getEventObject(), onScreenNavigationClickEventHandler,
            ui.screen_display.getNavigaitionClickEventID(), this
        ), false, "Register navigation click event failed"
    );

    {
        auto brightness_slider = ui.screen_display.getElementObject(
                                     static_cast<int>(SettingsUI_ScreenDisplayContainerIndex::BRIGHTNESS),
                                     static_cast<int>(SettingsUI_ScreenDisplayCellIndex::BRIGHTNESS_SLIDER),
                                     SettingsUI_WidgetCellElement::CENTER_SLIDER
                                 );
        ESP_UTILS_CHECK_NULL_RETURN(brightness_slider, false, "Get cell display slider failed");

        StorageNVS::Value value;
        ESP_UTILS_CHECK_FALSE_RETURN(
            StorageNVS::requestInstance().getLocalParam(Manager::SETTINGS_BRIGHTNESS, value), false,
            "Get media display brightness failed"
        );
        lv_slider_set_value(brightness_slider, std::get<int>(value), LV_ANIM_OFF);
        lv_obj_add_event_cb(
            brightness_slider, onUI_ScreenDisplayBrightnessSliderValueChangeEvent, LV_EVENT_VALUE_CHANGED, this
        );
    }

    del_guard.release();

    return true;
}

bool SettingsManager::processCloseUI_ScreenDisplay()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    app.getSystemContext()->getEvent().unregisterEvent(
        ui.screen_display.getEventObject(), onScreenNavigationClickEventHandler,
        ui.screen_display.getNavigaitionClickEventID()
    );

    return true;
}

bool SettingsManager::processOnUI_ScreenDisplayBrightnessSliderValueChangeEvent(lv_event_t *e)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    ESP_UTILS_CHECK_NULL_RETURN(slider, false, "Invalid slider");

    int target_value = lv_slider_get_value(slider);
    ESP_UTILS_CHECK_FALSE_RETURN(
        StorageNVS::requestInstance().setLocalParam(Manager::SETTINGS_BRIGHTNESS, target_value), false,
        "Set media display brightness failed"
    );

    return true;
}

void SettingsManager::onUI_ScreenDisplayBrightnessSliderValueChangeEvent(lv_event_t *e)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_NULL_EXIT(e, "Invalid event");

    SettingsManager *manager = (SettingsManager *)lv_event_get_user_data(e);
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid app pointer");

    ESP_UTILS_CHECK_FALSE_EXIT(
        manager->processOnUI_ScreenDisplayBrightnessSliderValueChangeEvent(e),
        "Process on UI screen display_brightness slider value change event failed"
    );
}

bool SettingsManager::processRunUI_ScreenAbout()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    esp_utils::function_guard del_guard([this]() {
        ESP_UTILS_CHECK_FALSE_EXIT(processCloseUI_ScreenAbout(), "Process close UI screen about failed");
    });

    char str_buffer[128] = {0};

    ESP_UTILS_CHECK_FALSE_RETURN(
        app.getSystemContext()->getEvent().registerEvent(
            ui.screen_about.getEventObject(), onScreenNavigationClickEventHandler,
            ui.screen_about.getNavigaitionClickEventID(), this
        ), false, "Register navigation click event failed"
    );

    {
        auto cell_firmware = ui.screen_about.getCell(
                                 static_cast<int>(SettingsUI_ScreenAboutContainerIndex::SYSTEM),
                                 static_cast<int>(SettingsUI_ScreenAboutCellIndex::SYSTEM_FIRMWARE_VERSION)
                             );
        ESP_UTILS_CHECK_NULL_RETURN(cell_firmware, false, "Get cell firmware failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_firmware->updateRightMainLabel(CONFIG_APP_PROJECT_VER), false,
            "Cell firmware update failed"
        );
    }
    {
        auto cell_os_name = ui.screen_about.getCell(
                                static_cast<int>(SettingsUI_ScreenAboutContainerIndex::SYSTEM),
                                static_cast<int>(SettingsUI_ScreenAboutCellIndex::SYSTEM_OS_NAME)
                            );
        ESP_UTILS_CHECK_NULL_RETURN(cell_os_name, false, "Get cell OS name failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_os_name->updateRightMainLabel(UI_SCREEN_ABOUT_SYSTEM_OS_NAME), false, "Cell OS name update failed"
        );
    }
    {
        auto cell_os_version = ui.screen_about.getCell(
                                   static_cast<int>(SettingsUI_ScreenAboutContainerIndex::SYSTEM),
                                   static_cast<int>(SettingsUI_ScreenAboutCellIndex::SYSTEM_OS_VERSION)
                               );
        ESP_UTILS_CHECK_NULL_RETURN(cell_os_version, false, "Get cell OS version failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_os_version->updateRightMainLabel(UI_SCREEN_ABOUT_SYSTEM_OS_VERSION), false, "Cell OS version update failed"
        );
    }
    {
        auto cell_ui_name = ui.screen_about.getCell(
                                static_cast<int>(SettingsUI_ScreenAboutContainerIndex::SYSTEM),
                                static_cast<int>(SettingsUI_ScreenAboutCellIndex::SYSTEM_UI_NAME)
                            );
        ESP_UTILS_CHECK_NULL_RETURN(cell_ui_name, false, "Get cell UI name failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_ui_name->updateRightMainLabel(UI_SCREEN_ABOUT_SYSTEM_UI_NAME), false,
            "Cell UI name update failed"
        );
    }
    {
        auto cell_ui_version = ui.screen_about.getCell(
                                   static_cast<int>(SettingsUI_ScreenAboutContainerIndex::SYSTEM),
                                   static_cast<int>(SettingsUI_ScreenAboutCellIndex::SYSTEM_UI_VERSION)
                               );
        ESP_UTILS_CHECK_NULL_RETURN(cell_ui_version, false, "Get cell UI version failed");
        snprintf(
            str_buffer, sizeof(str_buffer) - 1, "%s & %s",
            UI_SCREEN_ABOUT_SYSTEM_UI_BROOKESIA_VERSION, UI_SCREEN_ABOUT_SYSTEM_UI_LVGL_VERSION
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_ui_version->updateRightMainLabel(str_buffer), false, "Cell UI version update failed"
        );
    }

    {
        auto cell_manufacturer = ui.screen_about.getCell(
                                     static_cast<int>(SettingsUI_ScreenAboutContainerIndex::DEVICE),
                                     static_cast<int>(SettingsUI_ScreenAboutCellIndex::DEVICE_MANUFACTURER)
                                 );
        ESP_UTILS_CHECK_NULL_RETURN(cell_manufacturer, false, "Get cell manufacturer failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_manufacturer->updateRightMainLabel(UI_SCREEN_ABOUT_DEVICE_MANUFACTURER), false,
            "Cell manufacturer update failed"
        );

        auto cell_board = ui.screen_about.getCell(
                              static_cast<int>(SettingsUI_ScreenAboutContainerIndex::DEVICE),
                              static_cast<int>(SettingsUI_ScreenAboutCellIndex::DEVICE_NAME)
                          );
        ESP_UTILS_CHECK_NULL_RETURN(cell_board, false, "Get cell board failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_board->updateRightMainLabel(data.about.device_board_name), false, "Cell board update failed"
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
        ESP_UTILS_CHECK_NULL_RETURN(cell_resolution, false, "Get cell resolution failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_resolution->updateRightMainLabel(str_buffer), false, "Cell resolution update failed"
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
        ESP_UTILS_CHECK_NULL_RETURN(cell_flash_size, false, "Get cell flash size failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_flash_size->updateRightMainLabel(str_buffer), false, "Cell flash size update failed"
        );
    }

    {
        auto cell_ram_size = ui.screen_about.getCell(
                                 static_cast<int>(SettingsUI_ScreenAboutContainerIndex::DEVICE),
                                 static_cast<int>(SettingsUI_ScreenAboutCellIndex::DEVICE_RAM_SIZE)
                             );
        ESP_UTILS_CHECK_NULL_RETURN(cell_ram_size, false, "Get cell RAM size failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_ram_size->updateRightMainLabel(data.about.device_ram_main), false, "Cell RAM size update failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_ram_size->updateRightMinorLabel(data.about.device_ram_minor), false, "Cell RAM size update failed"
        );
    }

    {
        auto cell_chip_name = ui.screen_about.getCell(
                                  static_cast<int>(SettingsUI_ScreenAboutContainerIndex::CHIP),
                                  static_cast<int>(SettingsUI_ScreenAboutCellIndex::CHIP_NAME)
                              );
        ESP_UTILS_CHECK_NULL_RETURN(cell_chip_name, false, "Get cell chip name failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_chip_name->updateRightMainLabel(UI_SCREEN_ABOUT_DEVICE_CHIP), false, "Cell chip name update failed"
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
        ESP_UTILS_CHECK_NULL_RETURN(cell_chip_revision, false, "Get cell chip revision failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_chip_revision->updateRightMainLabel(str_buffer), false, "Cell chip revision update failed"
        );
    }

    {
        uint8_t mac[6] = {0};
        ESP_UTILS_CHECK_FALSE_RETURN(esp_efuse_mac_get_default(mac) == ESP_OK, false, "Get MAC address failed");

        snprintf(str_buffer, sizeof(str_buffer) - 1, "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
                );
        auto cell_chip_mac = ui.screen_about.getCell(
                                 static_cast<int>(SettingsUI_ScreenAboutContainerIndex::CHIP),
                                 static_cast<int>(SettingsUI_ScreenAboutCellIndex::CHIP_MAC)
                             );
        ESP_UTILS_CHECK_NULL_RETURN(cell_chip_mac, false, "Get cell chip MAC address failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_chip_mac->updateRightMainLabel(str_buffer), false, "Cell chip MAC address update failed"
        );
    }

    {
        snprintf(str_buffer, sizeof(str_buffer) - 1, "%d CPU cores", chip_info.cores);
        auto cell_chip_features = ui.screen_about.getCell(
                                      static_cast<int>(SettingsUI_ScreenAboutContainerIndex::CHIP),
                                      static_cast<int>(SettingsUI_ScreenAboutCellIndex::CHIP_FEATURES)
                                  );
        ESP_UTILS_CHECK_NULL_RETURN(cell_chip_features, false, "Get cell chip features failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            cell_chip_features->updateRightMainLabel(str_buffer), false, "Cell chip features update failed"
        );
    }

    del_guard.release();

    return true;
}

bool SettingsManager::processCloseUI_ScreenAbout()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    app.getSystemContext()->getEvent().unregisterEvent(
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
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD(
        "Parameter: ui_screen(%d), ui_screen_object(0x%p)", static_cast<int>(ui_screen), ui_screen_object
    );
    ESP_UTILS_LOGD(
        "UI screen change(%d -> %d)", static_cast<int>(_ui_current_screen), static_cast<int>(ui_screen)
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
        auto content_object = screen->getObject(SettingsUI_ScreenBaseObject::CONTENT_OBJECT);
        if (content_object != nullptr) {
            lv_obj_scroll_to_y(content_object, 0, LV_ANIM_OFF);
        }
    }

    if (checkIsWlanGeneralState(WlanGeneraState::_START) && (last_screen != UI_Screen::WLAN_VERIFICATION) &&
            (_ui_current_screen == UI_Screen::WIRELESS_WLAN)) {
        toggleWlanScanTimer(true, true);
    }

    event_signal(EventType::EnterScreen, ui_screen);

    return true;
}

bool SettingsManager::processAppEventOperation(AppOperationData &operation_data)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    auto operation_code = operation_data.code;
    auto operation_payload = operation_data.payload;

    // Open app if not running
    auto open_app_func = [this]() -> bool {
        auto system = app.getSystem();
        ESP_UTILS_CHECK_NULL_RETURN(system, false, "Invalid system");

        if (system->getManager().getRunningAppById(app.getId()) != nullptr)
        {
            return true;
        }
        systems::base::Context::AppEventData event_data = {
            .id = app.getId(),
            .type = systems::base::Context::AppEventType::START,
            .data = nullptr
        };
        ESP_UTILS_CHECK_FALSE_RETURN(system->sendAppEvent(&event_data), false, "Send app start event failed");
        return true;
    };

    switch (operation_code) {
    case AppOperationCode::EnterScreen:
        ESP_UTILS_CHECK_FALSE_RETURN(open_app_func(), false, "Open app failed");
        // Since the manager.processRun() is called asynchronously,
        // we need to wait for the UI to be initialized before processing the enter screen event
        {
            esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
                .name = ENTER_SCREEN_THREAD_NAME,
                .stack_size = ENTER_SCREEN_THREAD_STACK_SIZE,
                .stack_in_ext = ENTER_SCREEN_THREAD_STACK_CAPS_EXT,
            });
            boost::thread([this, operation_payload]() {
                ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

                while (!_is_ui_initialized) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                auto system = app.getSystem();
                ESP_UTILS_CHECK_NULL_EXIT(system, "Invalid system");

                LvLockGuard gui_guard;
                ESP_UTILS_CHECK_FALSE_EXIT(
                    processAppEventEnterScreen(std::any_cast<AppOperationEnterScreenPayloadType>(operation_payload)),
                    "Process app event enter screen failed"
                );
            }).detach();
        }
        break;
    default:
        ESP_UTILS_LOGE("Invalid app operation code: %d", static_cast<int>(operation_code));
    }

    return true;
}

bool SettingsManager::processAppEventEnterScreen(AppOperationEnterScreenPayloadType payload)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: payload(%d)", static_cast<int>(payload));

    auto ui_screen = getUI_Screen(payload);
    ESP_UTILS_CHECK_NULL_RETURN(ui_screen, false, "Invalid UI screen");

    auto ui_screen_object = ui_screen->getScreenObject();
    ESP_UTILS_CHECK_NULL_RETURN(ui_screen_object, false, "Invalid UI screen object");

    ESP_UTILS_CHECK_FALSE_RETURN(
        processUI_ScreenChange(payload, ui_screen_object), false, "Process UI screen change failed"
    );

    return true;
}

bool SettingsManager::processStorageServiceEventSignalUpdateWlanSwitch(bool is_open)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (ui.checkInitialized()) {
        LvLockGuard gui_guard;

        // Process WLAN switch
        lv_obj_t *wlan_sw = ui.screen_wlan.getElementObject(
                                static_cast<int>(SettingsUI_ScreenWlanContainerIndex::CONTROL),
                                static_cast<int>(SettingsUI_ScreenWlanCellIndex::CONTROL_SW),
                                SettingsUI_WidgetCellElement::RIGHT_SWITCH
                            );
        ESP_UTILS_CHECK_NULL_RETURN(wlan_sw, false, "Get WLAN switch failed");
        if (is_open) {
            lv_obj_add_state(wlan_sw, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(wlan_sw, LV_STATE_CHECKED);
        }
        lv_obj_send_event(wlan_sw, LV_EVENT_VALUE_CHANGED, nullptr);
    } else {
        boost::thread([this, is_open]() {
            ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

            _is_wlan_sw_flag = is_open;
            ESP_UTILS_CHECK_FALSE_EXIT(
                forceWlanOperation(is_open ? WlanOperation::START : WlanOperation::STOP, WLAN_START_WAIT_TIMEOUT_MS),
                "Force WLAN operation start/stop failed"
            );
        }).detach();
    }

    return true;
}

bool SettingsManager::processStorageServiceEventSignalUpdateVolume(int volume)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (ui.checkInitialized()) {
        LvLockGuard gui_guard;

        auto volume_slider = ui.screen_sound.getElementObject(
                                 static_cast<int>(SettingsUI_ScreenSoundContainerIndex::VOLUME),
                                 static_cast<int>(SettingsUI_ScreenSoundCellIndex::VOLUME_SLIDER),
                                 SettingsUI_WidgetCellElement::CENTER_SLIDER
                             );
        ESP_UTILS_CHECK_NULL_RETURN(volume_slider, false, "Get cell volume slider failed");
        lv_slider_set_value(volume_slider, volume, LV_ANIM_OFF);
    }

    return true;
}

bool SettingsManager::processStorageServiceEventSignalUpdateBrightness(int brightness)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (ui.checkInitialized()) {
        LvLockGuard gui_guard;

        auto brightness_slider = ui.screen_display.getElementObject(
                                     static_cast<int>(SettingsUI_ScreenDisplayContainerIndex::BRIGHTNESS),
                                     static_cast<int>(SettingsUI_ScreenDisplayCellIndex::BRIGHTNESS_SLIDER),
                                     SettingsUI_WidgetCellElement::CENTER_SLIDER
                                 );
        ESP_UTILS_CHECK_NULL_RETURN(brightness_slider, false, "Get cell display slider failed");
        lv_slider_set_value(brightness_slider, brightness, LV_ANIM_OFF);
    }

    return true;
}

bool SettingsManager::onScreenNavigationClickEventHandler(const systems::base::Event::HandlerData &data)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_NULL_RETURN(data.user_data, false, "Invalid user data");

    SettingsManager *manager = static_cast<SettingsManager *>(data.user_data);
    ESP_UTILS_CHECK_FALSE_RETURN(manager->processBack(), false, "Process back failed");

    return true;
}

bool SettingsManager::initWlan()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

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
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

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
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    return true;
}

bool SettingsManager::toggleWlanScanTimer(bool is_start, bool is_once)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: is_start(%d), is_once(%d)", is_start, is_once);
    ESP_UTILS_CHECK_NULL_RETURN(_wlan_update_timer, false, "Invalid WLAN scan timer");

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
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_NULL_RETURN(t, false, "Invalid timer");

    if (!checkIsWlanGeneralState(WlanGeneraState::STARTED) ||
            (checkIsWlanGeneralState(WlanGeneraState::_CONNECT) && (_ui_current_screen != UI_Screen::WIRELESS_WLAN))) {
        ESP_UTILS_LOGD("Ignore scan start");
        return true;
    }

    if (!_is_wlan_retry_connecting) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            tryWlanOperation(WlanOperation::SCAN_START, 0), false, "Try WLAN operation scan start failed"
        );
    }

    if (_wlan_scan_timer_once) {
        _wlan_scan_timer_once = false;
        ESP_UTILS_CHECK_FALSE_RETURN(
            toggleWlanScanTimer(false), false, "Toggle WLAN scan timer failed"
        );
    }

    return true;
}

bool SettingsManager::triggerWlanOperation(WlanOperation operation, int timeout_ms)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: operation(%s), timeout_ms(%d)", getWlanOperationStr(operation), timeout_ms);
    ESP_UTILS_LOGD(
        "General state: %s, Scan state: %s", getWlanGeneralStateStr(_wlan_general_state),
        getWlanScanStateStr(_wlan_scan_state)
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

bool SettingsManager::asyncWlanConnect(int timeout_ms)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
        .name = WLAN_CONNECT_THREAD_NAME,
        .stack_size = WLAN_CONNECT_THREAD_STACK_SIZE,
        .stack_in_ext = WLAN_CONNECT_THREAD_STACK_CAPS_EXT,
    });
    boost::thread([this, timeout_ms]() {
        ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

        if (timeout_ms > 0) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(timeout_ms));
        }

        if (!checkIsWlanGeneralState(WlanGeneraState::STARTED)) {
            ESP_UTILS_LOGD("Ignore connect to default AP when not started");
            return;
        }

        if (ui.checkInitialized()) {
            LvLockGuard gui_guard;

            if (!checkIsWlanGeneralState(WlanGeneraState::STARTED)) {
                ESP_UTILS_LOGD("Ignore connect to default AP when not started");
                return;
            }

            ESP_UTILS_CHECK_FALSE_EXIT(
                updateUI_ScreenWlanAvailable(true, WlanGeneraState::CONNECTING),
                "Update UI screen WLAN available failed"
            );
            ESP_UTILS_CHECK_FALSE_EXIT(
                updateUI_ScreenWlanConnected(true, WlanGeneraState::CONNECTING),
                "Update UI screen WLAN connected failed"
            );
            ESP_UTILS_CHECK_FALSE_EXIT(
                ui.screen_wlan.scrollConnectedToView(), "Scroll WLAN connect to view failed"
            );
        }
        ESP_UTILS_LOGI("Connect to AP(%s)", _wlan_connecting_info.first.ssid.c_str());
        ESP_UTILS_CHECK_FALSE_EXIT(
            forceWlanOperation(WlanOperation::CONNECT, 0), "Force WLAN operation connect failed"
        );
    }).detach();

    return true;
}

bool SettingsManager::forceWlanOperation(WlanOperation operation, int timeout_ms)
{
    ESP_UTILS_LOGI("Param: operation(%s), timeout_ms(%d)", getWlanOperationStr(operation), timeout_ms);
    ESP_UTILS_LOGD(
        "General state: %s, Scan state: %s", getWlanGeneralStateStr(_wlan_general_state),
        getWlanScanStateStr(_wlan_scan_state)
    );

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
        if (checkIsWlanGeneralState(WlanGeneraState::_START) || !_is_wlan_sw_flag) {
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
        if (!checkIsWlanGeneralState(WlanGeneraState::_START) || _is_wlan_sw_flag) {
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
    ESP_UTILS_LOGD("Param: operation(%s), timeout_ms(%d)", getWlanOperationStr(operation), timeout_ms);
    ESP_UTILS_LOGD(
        "General state: %s, Scan state: %s", getWlanGeneralStateStr(_wlan_general_state),
        getWlanScanStateStr(_wlan_scan_state)
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
        if (checkIsWlanGeneralState(WlanGeneraState::_START) || !_is_wlan_sw_flag) {
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
        if (!checkIsWlanGeneralState(WlanGeneraState::_START) || _is_wlan_sw_flag) {
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
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (checkIsWlanGeneralState(WlanGeneraState::INIT)) {
        ESP_UTILS_LOGD("Ignore operation");
        return true;
    }

    esp_utils::function_guard end_guard([this]() {
        ESP_UTILS_CHECK_FALSE_EXIT(doWlanOperationDeinit(), "Do WLAN operation deinit failed");
    });

    ESP_UTILS_CHECK_ERROR_RETURN(esp_netif_init(), false, "Init netif failed");

    esp_err_t error = esp_event_loop_create_default();
    if (error == ESP_ERR_INVALID_STATE) {
        ESP_UTILS_LOGW("Default event loop already created");
    } else {
        ESP_UTILS_CHECK_ERROR_RETURN(error, false, "Create default event loop failed");
    }

    _wlan_sta_netif = esp_netif_create_default_wifi_sta();
    ESP_UTILS_CHECK_NULL_RETURN(_wlan_sta_netif, false, "Create default STA netif failed");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_UTILS_CHECK_ERROR_RETURN(esp_wifi_init(&cfg), false, "Initialize WLAN failed");

    ESP_UTILS_CHECK_ERROR_RETURN(
        esp_event_handler_instance_register(
            WIFI_EVENT, static_cast<int32_t>(ESP_EVENT_ANY_ID), onWlanEventHandler, this,
            &_wlan_event_handler_instance
        ), false, "Register WLAN event handler failed"
    );
    ESP_UTILS_CHECK_ERROR_RETURN(
        esp_event_handler_instance_register(
            IP_EVENT, static_cast<int32_t>(IP_EVENT_STA_GOT_IP), onWlanEventHandler, this,
            &_ip_event_handler_instance
        ), false, "Register IP event handler failed"
    );

    ESP_UTILS_CHECK_ERROR_RETURN(esp_wifi_set_mode(WLAN_INIT_MODE_DEFAULT), false, "Set WLAN mode failed");

    _wlan_general_state = WlanGeneraState::INIT;

    end_guard.release();

    return true;
}

bool SettingsManager::doWlanOperationDeinit()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (checkIsWlanGeneralState(WlanGeneraState::DEINIT)) {
        ESP_UTILS_LOGD("Ignore operation");
        return true;
    }

    ESP_UTILS_CHECK_ERROR_RETURN(
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, _wlan_event_handler_instance),
        false, "Unregister WLAN event handler failed"
    );

    ESP_UTILS_CHECK_ERROR_RETURN(esp_wifi_deinit(), false, "Deinitialize WLAN failed");

    if (_wlan_sta_netif != nullptr) {
        esp_netif_destroy_default_wifi(_wlan_sta_netif);
        _wlan_sta_netif = nullptr;
    }
    /* Not supported now */
    // err = esp_netif_deinit();

    _wlan_general_state = WlanGeneraState::DEINIT;

    return true;
}

bool SettingsManager::doWlanOperationStart()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (checkIsWlanGeneralState(WlanGeneraState::_START)) {
        ESP_UTILS_LOGD("Ignore operation");
        return true;
    }

    esp_utils::value_guard<WlanGeneraState> value_guard(_wlan_general_state);
    value_guard.set(WlanGeneraState::STARTING);

    ESP_UTILS_CHECK_ERROR_RETURN(esp_wifi_start(), false, "Start WLAN failed");

    value_guard.release();

    return true;
}

bool SettingsManager::doWlanOperationStop()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (checkIsWlanGeneralState(WlanGeneraState::_STOP)) {
        ESP_UTILS_LOGD("Ignore operation");
        return true;
    }

    esp_utils::value_guard<WlanGeneraState> value_guard(_wlan_general_state);
    value_guard.set(WlanGeneraState::STOPPING);

    ESP_UTILS_CHECK_ERROR_RETURN(esp_wifi_stop(), false, "Stop WLAN failed");

    value_guard.release();

    return true;
}

bool SettingsManager::doWlanOperationConnect()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (checkIsWlanGeneralState(WlanGeneraState::_CONNECT)) {
        ESP_UTILS_LOGD("Ignore operation");
        return true;
    }

    esp_utils::value_guard<WlanGeneraState> value_guard(_wlan_general_state);
    value_guard.set(WlanGeneraState::CONNECTING);

    ESP_UTILS_CHECK_FALSE_RETURN(
        !_wlan_connecting_info.first.ssid.empty(), false, "Invalid WLAN connect info ssid"
    );

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
    ESP_UTILS_CHECK_ERROR_RETURN(
        esp_wifi_set_config(WLAN_CONFIG_MODE_DEFAULT, &_wlan_config), false, "Config WLAN failed"
    );
    ESP_UTILS_CHECK_ERROR_RETURN(esp_wifi_connect(), false, "Connect WLAN failed");

    value_guard.release();

    return true;
}

bool SettingsManager::doWlanOperationDisconnect()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (checkIsWlanGeneralState(WlanGeneraState::_DISCONNECT)) {
        ESP_UTILS_LOGD("Ignore operation");
        return true;
    }

    esp_utils::value_guard<WlanGeneraState> value_guard(_wlan_general_state);
    value_guard.set(WlanGeneraState::DISCONNECTING);

    ESP_UTILS_CHECK_ERROR_RETURN(esp_wifi_disconnect(), false, "Disconnect WLAN failed");

    value_guard.release();

    return true;
}

bool SettingsManager::doWlanOperationScanStart()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (checkIsWlanScanState(WlanScanState::SCANNING)) {
        ESP_UTILS_LOGD("Ignore operation");
        return true;
    }

    esp_utils::value_guard<WlanScanState> value_guard(_wlan_scan_state);
    value_guard.set(WlanScanState::SCANNING);

    ESP_UTILS_CHECK_ERROR_RETURN(esp_wifi_scan_start(nullptr, false), false, "Start WLAN scan failed");

    value_guard.release();

    return true;
}

bool SettingsManager::doWlanOperationScanStop()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (checkIsWlanScanState(WlanScanState::SCAN_STOPPED)) {
        ESP_UTILS_LOGD("Ignore operation");
        return true;
    }

    ESP_UTILS_CHECK_ERROR_RETURN(esp_wifi_scan_stop(), false, "Stop WLAN scan failed");

    _wlan_scan_state = WlanScanState::SCAN_STOPPED;

    return true;
}

bool SettingsManager::processOnWlanOperationThread()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::unique_lock<std::mutex> start_lock(_wlan_operation_start_mutex);
    _wlan_operation_start_cv.wait(start_lock, [this] { return !_wlan_operation_queue.empty(); });
    _is_wlan_operation_stopped = false;

    WlanOperation operation = _wlan_operation_queue.front();
    _wlan_operation_queue.pop();
    ESP_UTILS_LOGI("Process on wlan operation(%s) start", getWlanOperationStr(operation));

    int timeout_ms = 0;
    std::vector<WlanGeneraState> target_general_state;
    std::vector<WlanScanState> target_scan_state;
    esp_utils::function_guard end_guard([this, operation]() {
        _is_wlan_operation_stopped = true;
        std::unique_lock<std::mutex> stop_lock(_wlan_operation_stop_mutex);
        _wlan_operation_stop_cv.notify_all();
        ESP_UTILS_LOGD("Process on WLAN operation(%s) done", getWlanOperationStr(operation));
    });

    switch (operation) {
    case WlanOperation::INIT:
        ESP_UTILS_CHECK_FALSE_RETURN(doWlanOperationInit(), false, "Do WLAN operation init failed");
        break;
    case WlanOperation::DEINIT:
        ESP_UTILS_CHECK_FALSE_RETURN(doWlanOperationDeinit(), false, "Do WLAN operation deinit failed");
        break;
    case WlanOperation::START:
        ESP_UTILS_CHECK_FALSE_RETURN(doWlanOperationStart(), false, "Do WLAN operation start failed");
        target_general_state.emplace_back(WlanGeneraState::STARTED);
        timeout_ms = WLAN_START_WAIT_TIMEOUT_MS;
        break;
    case WlanOperation::STOP:
        ESP_UTILS_CHECK_FALSE_RETURN(doWlanOperationStop(), false, "Do WLAN operation stop failed");
        target_general_state.emplace_back(WlanGeneraState::STOPPED);
        timeout_ms = WLAN_STOP_WAIT_TIMEOUT_MS;
        break;
    case WlanOperation::CONNECT:
        ESP_UTILS_CHECK_FALSE_RETURN(doWlanOperationConnect(), false, "Do WLAN operation connect failed");
        target_general_state.emplace_back(WlanGeneraState::CONNECTED);
        target_general_state.emplace_back(WlanGeneraState::DISCONNECTED);
        timeout_ms = WLAN_CONNECT_WAIT_TIMEOUT_MS;
        break;
    case WlanOperation::DISCONNECT:
        ESP_UTILS_CHECK_FALSE_RETURN(doWlanOperationDisconnect(), false, "Do WLAN operation disconnect failed");
        target_general_state.emplace_back(WlanGeneraState::DISCONNECTED);
        timeout_ms = WLAN_DISCONNECT_WAIT_TIMEOUT_MS;
        break;
    case WlanOperation::SCAN_START:
        ESP_UTILS_CHECK_FALSE_RETURN(doWlanOperationScanStart(), false, "Do WLAN operation scan start failed");
        target_scan_state.emplace_back(WlanScanState::SCAN_DONE);
        timeout_ms = WLAN_SCAN_START_WAIT_TIMEOUT_MS;
        break;
    case WlanOperation::SCAN_STOP:
        ESP_UTILS_CHECK_FALSE_RETURN(doWlanOperationScanStop(), false, "Do WLAN operation scan stop failed");
        target_scan_state.emplace_back(WlanScanState::SCAN_STOPPED);
        timeout_ms = WLAN_SCAN_STOP_WAIT_TIMEOUT_MS;
        break;
    default:
        ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid WLAN operation");
        break;
    }

    if (target_general_state.size() > 0) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            waitForWlanGeneralState(target_general_state, timeout_ms), false, "Wait for WLAN general state failed"
        );
    } else if (target_scan_state.size() > 0) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            waitForWlanScanState(target_scan_state, timeout_ms), false, "Wait for WLAN scan state failed"
        );
    }
    _wlan_prev_operation = operation;

    return true;
}

bool SettingsManager::processOnWlanUI_Thread()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::unique_lock<std::mutex> lock(_wlan_event_mutex);
    _wlan_event_cv.wait(lock, [this] {
        return _is_wlan_event_updated;
    });
    _is_wlan_event_updated = false;

    auto wlan_event = _wlan_event;
    lock.unlock();

    ESP_UTILS_LOGI("Process on wlan UI thread start (event: %s)", getWlanEventStr(wlan_event));

    bool is_wifi_event = std::holds_alternative<wifi_event_t>(wlan_event);
    auto event_id = is_wifi_event ?
                    static_cast<int>(std::get<wifi_event_t>(wlan_event)) : static_cast<int>(std::get<ip_event_t>(wlan_event));

    auto system = app.getSystem();
    auto &quick_settings = system->getDisplay().getQuickSettings();
    auto &storage_service = StorageNVS::requestInstance();
    // Use temp variable to avoid `_ui_wlan_available_data` being modified outside lvgl task
    decltype(_ui_wlan_available_data) temp_available_data;

    // Process non-UI
    if (is_wifi_event) {
        switch (event_id) {
        case WIFI_EVENT_STA_STOP: {
            std::lock_guard<std::mutex> lock(_ui_wlan_available_data_mutex);
            _ui_wlan_available_data.clear();
            break;
        }
        case WIFI_EVENT_SCAN_DONE: {
            bool psk_flag = false;
            uint16_t number = data.wlan.scan_ap_count_max;
            uint16_t ap_count = 0;
            SettingsUI_ScreenWlan::SignalLevel signal_level = SettingsUI_ScreenWlan::SignalLevel::WEAK;

            if (ApProvision::get_ap_ssid()) {
                ESP_UTILS_LOGW("AP Provisioning is running, skip update AP list to UI");
                break;
            }

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
                    if (strcmp((const char *)ap_info[i].ssid, _wlan_connecting_info.first.ssid.c_str()) == 0) {
                        ESP_UTILS_LOGD("Skip connecting AP(%s)", _wlan_connecting_info.first.ssid.c_str());
                        continue;
                    }
                } else if (checkIsWlanGeneralState(WlanGeneraState::_START)) {
                    // Only connect to default AP when WLAN is not connecting or connected
                    // Check if default AP is in scan list
                    StorageNVS::Value default_connect_ssid;
                    StorageNVS::Value default_connect_pwd;
                    ESP_UTILS_CHECK_FALSE_RETURN(
                        storage_service.getLocalParam(Manager::SETTINGS_WLAN_SSID, default_connect_ssid), false,
                        "Get default connect SSID failed"
                    );
                    ESP_UTILS_CHECK_FALSE_RETURN(
                        storage_service.getLocalParam(Manager::SETTINGS_WLAN_PASSWORD, default_connect_pwd), false,
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
                        ESP_UTILS_LOGD("Found default AP(%s), try to connect later", default_connect_ssid_str.c_str());
                        _wlan_connecting_info.first = getWlanDataFromApInfo(ap_info[i]);
                        _wlan_connecting_info.second = default_connect_pwd_str;

                        if (!ui.checkInitialized() ||
                                !ui.screen_wlan.checkConnectedVisible() ||
                                (ui.screen_wlan.getConnectedState() != SettingsUI_ScreenWlan::ConnectState::DISCONNECT)) {
                            asyncWlanConnect(WLAN_SCAN_CONNECT_AP_DELAY_MS);
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
            std::lock_guard<std::mutex> lock(_ui_wlan_available_data_mutex);
            _ui_wlan_available_data = std::move(temp_available_data);
            break;
        }
        default:
            break;
        }
    }

    LvLockGuard gui_guard;

    // Process system UI
    if (is_wifi_event) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            // Show status bar WLAN icon
            quick_settings.setWifiIconState(QuickSettings::WifiState::DISCONNECTED);
            if (!_ui_wlan_softap_visible) {
                ESP_UTILS_CHECK_FALSE_RETURN(toggleWlanScanTimer(true), false, "Toggle WLAN scan timer failed");
            }
            break;
        case WIFI_EVENT_STA_STOP:
            ESP_UTILS_CHECK_FALSE_RETURN(
                quick_settings.setWifiIconState(QuickSettings::WifiState::CLOSED), false, "Set WLAN icon state failed"
            );
            ESP_UTILS_CHECK_FALSE_RETURN(toggleWlanScanTimer(false), false, "Toggle WLAN scan timer failed");
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            ESP_UTILS_CHECK_FALSE_RETURN(
                quick_settings.setWifiIconState(QuickSettings::WifiState::DISCONNECTED), false, "Set WLAN icon state failed"
            );
            break;
        }
        default:
            break;
        }
    } else {
        switch (event_id) {
        case IP_EVENT_STA_GOT_IP: {
            ESP_UTILS_CHECK_FALSE_RETURN(
                toggleWlanScanTimer(_ui_current_screen == UI_Screen::WIRELESS_WLAN, true), false, "Toggle WLAN scan timer failed"
            );
            wifi_ap_record_t ap_info = {};
            ESP_UTILS_CHECK_FALSE_RETURN(esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK, false, "Get AP info failed");
            auto data = getWlanDataFromApInfo(ap_info);
            ESP_UTILS_LOGI("Connected to AP(%s, %d)", data.ssid.c_str(), data.signal_level);
            ESP_UTILS_CHECK_FALSE_RETURN(
                quick_settings.setWifiIconState(static_cast<QuickSettings::WifiState>(data.signal_level + 1)), false,
                "Set WLAN icon state failed"
            );
            if (!app_sntp_is_time_synced()) {
                if (!_wlan_time_sync_thread.joinable()) {
                    esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
                        .name = WLAN_TIME_SYNC_THREAD_NAME,
                        .stack_size = WLAN_TIME_SYNC_THREAD_STACK_SIZE,
                        .stack_in_ext = WLAN_TIME_SYNC_THREAD_STACK_CAPS_EXT,
                    });
                    _wlan_time_sync_thread = boost::thread([this]() {
                        ESP_UTILS_LOGD("Update time start");

                        if (!app_sntp_start()) {
                            ESP_UTILS_LOGE("Start SNTP failed, restart the device");
                            esp_restart();
                        }

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
        default:
            break;
        }
    }

    if (!ui.checkInitialized()) {
        ESP_UTILS_LOGD("Skip APP UI update when not initialized");
        return true;
    }

    // Process APP UI
    if (is_wifi_event) {
        switch (event_id) {
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
            ESP_UTILS_CHECK_FALSE_RETURN(updateUI_ScreenWlanConnected(false), false, "Update UI screen WLAN connected failed");
            break;
        }
        case WIFI_EVENT_SCAN_DONE:
            ESP_UTILS_CHECK_FALSE_RETURN(updateUI_ScreenWlanAvailable(false), false, "Update UI screen WLAN available failed");
            break;
        default:
            break;
        }
    } else {
        switch (event_id) {
        case IP_EVENT_STA_GOT_IP: {
            ESP_UTILS_CHECK_FALSE_RETURN(updateUI_ScreenWlanConnected(false), false, "Update UI screen WLAN connected failed");
            break;
        }
        default:
            break;
        }
    }

    return true;
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
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_NULL_EXIT(t, "Invalid timer");

    SettingsManager *manager = (SettingsManager *)t->user_data;
    ESP_UTILS_CHECK_FALSE_EXIT(manager->processOnWlanScanTimer(t), "Process on WLAN update timer failed");
}

void SettingsManager::onWlanOperationThread(SettingsManager *manager)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");

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
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");

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

bool SettingsManager::processOnUI_ScreenWlanControlSwitchChangeEvent(lv_event_t *e)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    ESP_UTILS_CHECK_NULL_RETURN(sw, false, "Get switch failed");

    lv_state_t state = lv_obj_get_state(sw);

    bool wlan_sw_flag = state & LV_STATE_CHECKED;
    _is_wlan_sw_flag = wlan_sw_flag;

    // Show/Hide status bar WLAN icon
    app.getSystem()->getDisplay().getQuickSettings().setWifiIconState(
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
    ESP_UTILS_CHECK_FALSE_RETURN(
        ui.screen_wlan.setSoftAPVisible(wlan_sw_flag), false, "Set softap visible failed"
    );

    ESP_UTILS_CHECK_FALSE_RETURN(
        forceWlanOperation(wlan_sw_flag ? WlanOperation::START : WlanOperation::STOP, 0), false,
        "Force WLAN operation failed"
    );

    ESP_UTILS_CHECK_FALSE_RETURN(
        StorageNVS::requestInstance().setLocalParam(Manager::SETTINGS_WLAN_SWITCH, static_cast<int>(wlan_sw_flag), this), false,
        "Set WLAN switch flag failed"
    );

    return true;
}

bool SettingsManager::processOnUI_ScreenWlanAvailableCellClickEvent(const systems::base::Event::HandlerData &data)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    SettingsUI_WidgetCell *cell = (SettingsUI_WidgetCell *)data.object;
    ESP_UTILS_CHECK_NULL_RETURN(cell, false, "Invalid cell");

    int cell_index = ui.screen_wlan.getCellContainer(
                         static_cast<int>(SettingsUI_ScreenWlanContainerIndex::AVAILABLE)
                     ) ->getCellIndex(cell);
    ESP_UTILS_CHECK_FALSE_RETURN(cell_index >= 0, false, "Get cell index failed");
    ESP_UTILS_LOGD("Cell index: %d", cell_index);

    {
        std::lock_guard<std::mutex> lock(_ui_wlan_available_data_mutex);
        _wlan_connecting_info.first = _ui_wlan_available_data[cell_index];
    }
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
        _wlan_connecting_info.second = "";

        asyncWlanConnect(0);
    }

    return true;
}

bool SettingsManager::processOnUI_ScreenWlanGestureEvent(lv_event_t *e)
{
    // ESP_UTILS_LOG_TRACE_GUARD();

    // ESP_UTILS_CHECK_NULL_RETURN(e, false, "Invalid event");

    if (_ui_current_screen != UI_Screen::WIRELESS_WLAN) {
        return true;
    }

    lv_event_code_t code = lv_event_get_code(e);
    GestureInfo *gesture_info = (GestureInfo *)lv_event_get_param(e);
    auto gesture = app.getSystem()->getManager().getGesture();

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

bool SettingsManager::processOnWlanEventHandler(WlanEvent event, void *event_data)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: event(%s), event_data(0x%p)", getWlanEventStr(event), event_data);

    bool is_wifi_event = std::holds_alternative<wifi_event_t>(event);
    auto event_id = is_wifi_event ?
                    static_cast<int>(std::get<wifi_event_t>(event)) : static_cast<int>(std::get<ip_event_t>(event));

    std::unique_lock<std::mutex> lock(_wlan_event_mutex);

    if (is_wifi_event) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            _wlan_general_state = WlanGeneraState::STARTED;
            _wlan_scan_state = WlanScanState::SCAN_STOPPED;
            break;
        case WIFI_EVENT_STA_STOP:
            _wlan_general_state = WlanGeneraState::STOPPED;
            _wlan_scan_state = WlanScanState::SCAN_STOPPED;
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
                    _wlan_connecting_info = {};
                }
            } else if (!_is_wlan_force_connecting) {
                _wlan_connecting_info = {};
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
            return true;
        }
    } else {
        switch (event_id) {
        case IP_EVENT_STA_GOT_IP: {
            _wlan_general_state = WlanGeneraState::CONNECTED;
            _wlan_connect_retry_count = 0;
            _is_wlan_retry_connecting = false;

            {
                esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
                    .name  = SAVE_WLAN_CONFIG_THREAD_NAME,
                    .stack_size = SAVE_WLAN_CONFIG_THREAD_STACK_SIZE,
                    .stack_in_ext = SAVE_WLAN_CONFIG_THREAD_STACK_CAPS_EXT,
                });
                boost::thread([this]() {
                    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

                    ESP_UTILS_CHECK_FALSE_EXIT(
                        saveWlanConfig(
                            std::string((char *)_wlan_config.sta.ssid), std::string((char *)_wlan_config.sta.password)
                        ), "Save WLAN config failed"
                    );
                }).detach();
            }
            break;
        }
        default:
            ESP_UTILS_LOGD("Ignore WLAN event(%d)", static_cast<int>(event_id));
            return true;
        }
    }

    if (is_wifi_event && (event_id == WIFI_EVENT_SCAN_DONE)) {
        ESP_UTILS_LOGI("Set WLAN scan state(%s)", getWlanScanStateStr(_wlan_scan_state));
    } else {
        ESP_UTILS_LOGI("Set WLAN general state(%s)", getWlanGeneralStateStr(_wlan_general_state));
    }

    _wlan_event = event;
    _is_wlan_event_updated = true;
    lock.unlock();
    _wlan_event_cv.notify_all();

    if (_is_wlan_retry_connecting) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            forceWlanOperation(WlanOperation::CONNECT, 0), false, "Force WLAN operation connect failed"
        );
    }

    return true;
}

bool SettingsManager::saveWlanConfig(std::string ssid, std::string pwd)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(!ssid.empty(), false, "Invalid SSID");

    auto &storage_service = StorageNVS::requestInstance();
    StorageNVS::Value last_ssid;
    StorageNVS::Value last_pwd;
    ESP_UTILS_CHECK_FALSE_RETURN(
        storage_service.getLocalParam(Manager::SETTINGS_WLAN_SSID, last_ssid), false, "Get last SSID failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        storage_service.getLocalParam(Manager::SETTINGS_WLAN_PASSWORD, last_pwd), false, "Get last PWD failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        std::holds_alternative<std::string>(last_ssid), false, "Invalid last SSID type"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        std::holds_alternative<std::string>(last_pwd), false, "Invalid last PWD type"
    );
    auto &last_ssid_str = std::get<std::string>(last_ssid);
    auto &last_pwd_str = std::get<std::string>(last_pwd);

    if ((last_ssid_str != ssid) || (last_pwd_str != pwd)) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            storage_service.setLocalParam(Manager::SETTINGS_WLAN_SSID, ssid, this), false,
            "Set last SSID failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            storage_service.setLocalParam(Manager::SETTINGS_WLAN_PASSWORD, pwd, this), false,
            "Set last PWD failed"
        );
    } else {
        ESP_UTILS_LOGD(
            "SSID and PWD are the same(%s, %s), no need to save",
            ssid.c_str(), pwd.c_str() != nullptr ? pwd.c_str() : "null"
        );
    }

    return true;
}

bool SettingsManager::waitForWlanGeneralState(const std::vector<WlanGeneraState> &state, int timeout_ms)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::string state_str = "";
    for (auto &s : state) {
        state_str += " ";
        state_str += getWlanGeneralStateStr(s);
    }

    ESP_UTILS_LOGD("Param: state(%s), timeout_ms(%d)", state_str.c_str(), timeout_ms);
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

    return status;
}

bool SettingsManager::waitForWlanScanState(const std::vector<WlanScanState> &state, int timeout_ms)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::string state_str = "";
    for (auto &s : state) {
        state_str += " ";
        state_str += getWlanScanStateStr(s);
    }

    ESP_UTILS_LOGD("Param: state(%s), timeout_ms(%d)", state_str.c_str(), timeout_ms);

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

    return true;
}

const char *SettingsManager::getWlanEventStr(WlanEvent event)
{
    bool is_wifi_event = std::holds_alternative<wifi_event_t>(event);
    auto event_id = is_wifi_event ?
                    static_cast<int>(std::get<wifi_event_t>(event)) : static_cast<int>(std::get<ip_event_t>(event));

    if (is_wifi_event) {
        auto it = _wlan_event_str.find(static_cast<wifi_event_t>(event_id));
        if (it != _wlan_event_str.end()) {
            return it->second.c_str();
        }
    } else {
        auto it = _ip_event_str.find(static_cast<ip_event_t>(event_id));
        if (it != _ip_event_str.end()) {
            return it->second.c_str();
        }
    }

    return "UNKNOWN";
}

void SettingsManager::onWlanEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_FALSE_EXIT((event_base == WIFI_EVENT) || (event_base == IP_EVENT), "Invalid event");

    SettingsManager *manager = (SettingsManager *)arg;
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");

    WlanEvent wlan_event;
    if (event_base == WIFI_EVENT) {
        wlan_event = static_cast<wifi_event_t>(event_id);
    } else if (event_base == IP_EVENT) {
        wlan_event = static_cast<ip_event_t>(event_id);
    }

    ESP_UTILS_CHECK_FALSE_EXIT(
        manager->processOnWlanEventHandler(wlan_event, event_data), "Process WLAN event failed"
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

} // namespace esp_brookesia::apps
