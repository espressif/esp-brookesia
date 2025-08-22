/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <any>
#include <bitset>
#include <variant>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <queue>
#include <mutex>
#include <map>
#include <thread>
#include "esp_wifi.h"
#include "boost/signals2.hpp"
#include "esp_brookesia.hpp"
#include "esp_brookesia_app_settings_ui.hpp"

#ifndef WLAN_DEFAULT_SSID
#define WLAN_DEFAULT_SSID           ""
#endif
#ifndef WLAN_DEFAULT_PWD
#define WLAN_DEFAULT_PWD            ""
#endif
#define WLAN_DEFAULT_SCAN_METHOD    WIFI_FAST_SCAN
#define WLAN_DEFAULT_SORT_METHOD    WIFI_CONNECT_AP_BY_SIGNAL
#define WLAN_DEFAULT_RSSI           -127
#define WLAN_DEFAULT_AUTHMODE       WIFI_AUTH_OPEN

constexpr const char *SETTINGS_NVS_KEY_TOUCH_SENSOR_SWITCH = "touch_switch";

namespace esp_brookesia::apps {

struct SettingsManagerData {
    struct {
        int scan_ap_count_max = 15;
        int scan_interval_ms = 20000;
        const char *default_connect_ssid = WLAN_DEFAULT_SSID;
        const char *default_connect_pwd = WLAN_DEFAULT_PWD;
    } wlan;
    struct {
        const char *device_board_name = "Unknown";
        const char *device_ram_main = "Unknown";
        const char *device_ram_minor = "Unknown";
    } about;
};

class SettingsManager {
public:
    enum class UI_Screen {
        HOME,
        SETTINGS,
        MEDIA_SOUND,
        MEDIA_DISPLAY,
        WIRELESS_WLAN,
        WLAN_VERIFICATION,
        WLAN_SOFTAP,
        MORE_ABOUT,
        MAX,
    };

    enum class EventType {
        EnterDeveloperMode,
        EnterScreen,
    };
    using EventData = std::any;
    using EventDataEnterDeveloperMode = std::monostate;
    using EventDataEnterScreenIndex = UI_Screen;

    struct FirstTrue {
        using result_type = bool;
        template<typename InputIterator>
        result_type operator()(InputIterator first, InputIterator last) const
        {
            for (auto it = first; it != last; ++it) {
                if (*it) {
                    return true;
                }
            }
            return false; // All are false
        }
    };
    using EventSignal = boost::signals2::signal<bool(EventType, EventData), FirstTrue>;

    enum class AppOperationCode {
        EnterScreen,
    };
    using AppOperationEnterScreenPayloadType = UI_Screen;
    using AppOperationPayloadType = std::any;
    struct AppOperationData {
        AppOperationCode code;
        AppOperationPayloadType payload;
    };

    SettingsManager(systems::speaker::App &app_in, SettingsUI &ui_in, const SettingsManagerData &data_in);
    ~SettingsManager();

    SettingsManager(const SettingsManager &) = delete;
    SettingsManager(SettingsManager &&) = delete;
    SettingsManager &operator=(const SettingsManager &) = delete;
    SettingsManager &operator=(SettingsManager &&) = delete;

    bool processInit();
    bool processRun();
    bool processBack();
    bool processClose();

    bool checkClosed() const
    {
        return (_ui_screen_object_map.size() == 0);
    }

    systems::speaker::App &app;
    SettingsUI &ui;
    const SettingsManagerData &data;

    EventSignal event_signal;

private:
    enum WlanGeneraState {
        DEINIT          = (1UL << 0),
        INIT            = (1UL << 1),
        _START          = (INIT        | (1UL << 2)),
        STARTING        = (_START      | (1UL << 3)),
        STARTED         = (_START      | (1UL << 4)),
        _STOP           = (INIT        | (1UL << 5)),
        STOPPING        = (_STOP       | (1UL << 6)),
        STOPPED         = (_STOP       | (1UL << 7)),
        _CONNECT        = (STARTED     | (1UL << 8)),
        CONNECTING      = (_CONNECT    | (1UL << 9)),
        CONNECTED       = (_CONNECT    | (1UL << 10)),
        _DISCONNECT     = (STARTED     | (1UL << 11)),
        DISCONNECTING   = (_DISCONNECT | (1UL << 12)),
        DISCONNECTED    = (_DISCONNECT | (1UL << 13)),
    };
    enum WlanScanState {
        _SCAN_START     = (1UL << 0),
        SCANNING        = (_SCAN_START | (1UL << 1)),
        SCAN_DONE       = (_SCAN_START | (1UL << 2)),
        SCAN_STOPPED    = (1UL << 3),
    };
    enum class WlanOperation {
        NONE,
        INIT,
        DEINIT,
        START,
        STOP,
        CONNECT,
        DISCONNECT,
        SCAN_START,
        SCAN_STOP,
    };
    using WlanEvent = std::variant<wifi_event_t, ip_event_t>;

    // // Data
    // friend WlanGeneraState operator|(WlanGeneraState lhs, WlanGeneraState rhs);
    // friend WlanGeneraState operator&(WlanGeneraState lhs, WlanGeneraState rhs);
    // friend WlanGeneraState &operator|=(WlanGeneraState &lhs, WlanGeneraState rhs);
    // friend WlanGeneraState &operator&=(WlanGeneraState &lhs, WlanGeneraState rhs);

    // App Event
    bool processAppEventOperation(AppOperationData &operation_data);
    bool processAppEventEnterScreen(AppOperationEnterScreenPayloadType payload);

    // Signal
    bool processStorageServiceEventSignalUpdateWlanSwitch(bool is_open);
    bool processStorageServiceEventSignalUpdateVolume(int volume);
    bool processStorageServiceEventSignalUpdateBrightness(int brightness);

    // All Screens
    bool processUI_ScreenChange(const UI_Screen &ui_screen, lv_obj_t *ui_screen_object);
    SettingsUI_ScreenBase *getUI_Screen(const UI_Screen &ui_screen);
    std::pair<UI_Screen, lv_obj_t *> getUI_BackScreenObject(const UI_Screen &ui_screen);
    static bool onScreenNavigationClickEventHandler(const systems::base::Event::HandlerData &data);

    // Screen: Settings
    bool processRunUI_ScreenSettings();
    bool processCloseUI_ScreenSettings();
    static bool onScreenSettingsCellClickEventHandler(const systems::base::Event::HandlerData &data);

    // Screen: WLAN
    bool processRunUI_ScreenWlan();
    bool processCloseUI_ScreenWlan();
    bool processOnUI_ScreenWlanControlSwitchChangeEvent(lv_event_t *e);
    bool processOnUI_ScreenWlanAvailableCellClickEvent(const systems::base::Event::HandlerData &data);
    bool processOnUI_ScreenWlanGestureEvent(lv_event_t *e);
    bool updateUI_ScreenWlanConnected(bool use_target, WlanGeneraState target_state = WlanGeneraState::DEINIT);
    bool updateUI_ScreenWlanAvailable(bool use_target, WlanGeneraState target_state = WlanGeneraState::DEINIT);
    static void onUI_ScreenWlanControlSwitchChangeEvent(lv_event_t *e);
    static bool onUI_ScreenWlanAvailableCellClickEventHander(const systems::base::Event::HandlerData &data);
    static void onUI_ScreenWlanGestureEvent(lv_event_t *e);

    // Screen: WLAN verification
    bool processRunUI_ScreenWlanVerification();
    bool processCloseUI_ScreenWlanVerification();
    bool processOnUI_ScreenWlanVerificationKeyboardConfirmEvent(std::pair<std::string_view, std::string_view> ssid_with_pwd);

    // Screen: WLAN softap
    bool processRunUI_ScreenWlanSoftAP();
    bool processCloseUI_ScreenWlanSoftAP();
    bool processOnUI_ScreenWlanSoftAPCellClickEvent(const systems::base::Event::HandlerData &data);
    bool processOnUI_ScreenWlanSoftAPNavigationClickEvent(const systems::base::Event::HandlerData &data);

    // Screen: Sound
    bool processCloseUI_ScreenSound();
    bool processRunUI_ScreenSound();
    bool processOnUI_ScreenSoundVolumeSliderValueChangeEvent(lv_event_t *e);
    static void onUI_ScreenSoundVolumeSliderValueChangeEvent(lv_event_t *e);

    // Screen: Display
    bool processCloseUI_ScreenDisplay();
    bool processRunUI_ScreenDisplay();
    bool processOnUI_ScreenDisplayBrightnessSliderValueChangeEvent(lv_event_t *e);
    static void onUI_ScreenDisplayBrightnessSliderValueChangeEvent(lv_event_t *e);

    // Screen: About
    bool processCloseUI_ScreenAbout();
    bool processRunUI_ScreenAbout();

    // WLAN
    bool initWlan();
    bool deinitWlan();
    bool processCloseWlan();
    bool toggleWlanScanTimer(bool is_start, bool is_once = false);
    bool processOnWlanScanTimer(lv_timer_t *t);
    bool triggerWlanOperation(WlanOperation operation, int timeout_ms = 0);
    bool asyncWlanConnect(int timeout_ms = 0);
    bool forceWlanOperation(WlanOperation operation, int timeout_ms = 0);
    bool tryWlanOperation(WlanOperation operation, int timeout_ms = 0);
    bool doWlanOperationInit();
    bool doWlanOperationDeinit();
    bool doWlanOperationStart();
    bool doWlanOperationStop();
    bool doWlanOperationConnect();
    bool doWlanOperationDisconnect();
    bool doWlanOperationScanStart();
    bool doWlanOperationScanStop();
    bool waitForWlanGeneralState(const std::vector<WlanGeneraState> &state, int timeout_ms);
    bool waitForWlanScanState(const std::vector<WlanScanState> &state, int timeout_ms);
    bool processOnWlanOperationThread();
    bool processOnWlanUI_Thread();
    bool processOnWlanEventHandler(WlanEvent event, void *event_data);
    bool saveWlanConfig(std::string ssid, std::string pwd);
    bool checkIsWlanGeneralState(WlanGeneraState state)
    {
        return ((_wlan_general_state & state) == state);
    }
    bool checkIsWlanScanState(WlanScanState state)
    {
        return ((_wlan_scan_state & state) == state);
    }
    static const char *getWlanGeneralStateStr(WlanGeneraState state);
    static const char *getWlanScanStateStr(WlanScanState state);
    static const char *getWlanOperationStr(WlanOperation operation);
    static void onWlanScanTimer(lv_timer_t *t);
    static void onWlanOperationThread(SettingsManager *manager);
    static void onWlanUI_Thread(SettingsManager *manager);

    static const char *getWlanEventStr(WlanEvent event);
    static void onWlanEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    SettingsUI_ScreenWlan::WlanData getWlanDataFromApInfo(wifi_ap_record_t &ap_info);

    UI_Screen _ui_current_screen;
    std::atomic<bool> _is_ui_initialized = false;
    // All screens
    std::map<void *, std::pair<UI_Screen, lv_obj_t *>> _ui_screen_object_map;
    const std::map<UI_Screen, UI_Screen> _ui_screen_back_map;
    // Screen: WLAN
    bool _ui_wlan_available_clickable = true;
    std::mutex _ui_wlan_available_data_mutex;
    std::vector<SettingsUI_ScreenWlan::WlanData> _ui_wlan_available_data;
    // Screen: WLAN softap
    bool _ui_wlan_softap_visible = false;
    // WLAN
    int _wlan_connect_retry_count = 0;
    std::atomic<bool> _is_wlan_operation_stopped = true;
    std::atomic<bool> _is_wlan_force_connecting = false;
    std::atomic<bool> _is_wlan_retry_connecting = false;
    std::atomic<bool> _is_wlan_sw_flag = false;
    WlanGeneraState _wlan_general_state = WlanGeneraState::DEINIT;
    WlanScanState _wlan_scan_state = WlanScanState::SCAN_STOPPED;
    WlanEvent _wlan_event;
    boost::thread _wlan_operation_thread;
    boost::thread _wlan_ui_thread;
    boost::thread _wlan_time_sync_thread;
    WlanOperation _wlan_prev_operation = WlanOperation::NONE;
    std::mutex _wlan_operation_start_mutex;
    std::mutex _wlan_operation_stop_mutex;
    std::mutex _wlan_event_mutex;
    std::queue<WlanOperation> _wlan_operation_queue;
    std::condition_variable _wlan_operation_start_cv;
    std::condition_variable _wlan_operation_stop_cv;
    std::condition_variable _wlan_event_cv;
    bool _is_wlan_event_updated = false;
    bool _wlan_scan_timer_once = false;
    ESP_Brookesia_LvTimer_t _wlan_update_timer = nullptr;
    std::pair<SettingsUI_ScreenWlan::WlanData, std::string> _wlan_connecting_info = {};
    std::pair<SettingsUI_ScreenWlan::WlanData, std::string> _wlan_connected_info = {};
    static const std::unordered_map<WlanGeneraState, std::string> _wlan_general_state_str;
    static const std::unordered_map<WlanScanState, std::string> _wlan_scan_state_str;
    static const std::unordered_map<WlanOperation, std::string> _wlan_operation_str;
    // WLAN
    esp_netif_t *_wlan_sta_netif;
    wifi_config_t _wlan_config = {
        .sta = {
            .ssid = WLAN_DEFAULT_SSID,
            .password = WLAN_DEFAULT_PWD,
            .scan_method = WLAN_DEFAULT_SCAN_METHOD,
            .sort_method = WLAN_DEFAULT_SORT_METHOD,
            .threshold = {
                .rssi = WLAN_DEFAULT_RSSI,
                .authmode = WLAN_DEFAULT_AUTHMODE,
            },
        },
    };
    esp_event_handler_instance_t _wlan_event_handler_instance;
    esp_event_handler_instance_t _ip_event_handler_instance;
    static const std::unordered_map<wifi_event_t, std::string> _wlan_event_str;
    static const std::unordered_map<ip_event_t, std::string> _ip_event_str;
};

} // namespace esp_brookesia::apps
