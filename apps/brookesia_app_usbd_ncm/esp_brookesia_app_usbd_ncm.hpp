/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_timer.h"
#include "esp_brookesia.hpp"

namespace esp_brookesia::apps {

// UI update type enumeration
enum class UiUpdateType {
    IP_ADDRESS,
    NETWORK_SPEED,
    CONNECTION_STATUS,
};

// UI data cache structure
struct UiDataCache {
    char ip_address[16] = {0};
    char upload_speed[16] = {0};    // Upload speed (uplink)
    char download_speed[16] = {0};  // Download speed (downlink)
    char connection_status[16] = {0};
    bool ip_address_updated = false;
    bool network_speed_updated = false;
    bool connection_status_updated = false;
};

// Traffic statistics structure
struct TrafficCounter {
    uint64_t bytes = 0;  // Accumulated bytes in current period
};

class UsbdNcm: public systems::speaker::App {
public:
    static UsbdNcm *requestInstance();
    ~UsbdNcm();
    bool run() override;
    bool back() override;
    bool close() override;
    bool init() override;

    // Public static methods for traffic statistics
    static void addUplinkBytes(uint32_t bytes);
    static void addDownlinkBytes(uint32_t bytes);
protected:
    UsbdNcm();
private:
    static UsbdNcm *_instance;
    static void onWiFiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    bool processWiFiEvent(esp_event_base_t event_base, int32_t event_id, void *event_data);

    // LVGL timer callback
    static void uiRefreshTimerCallback(lv_timer_t *timer);
    void startUiRefreshTimer();
    void stopUiRefreshTimer();
    void updateUiFromCache();
    void startUsbdNcm();
    void stopUsbdNcm();

    // Button event handlers
    static void onConnectButtonClick(lv_event_t *e);
    void handleConnectButtonClick();

    esp_event_handler_instance_t _wifi_event_handler_instance;
    esp_event_handler_instance_t _ip_event_handler_instance;
    bool _app_opened = false;  // Track if APP is opened
    bool _wifi_connected = false;  // Track WiFi connection status
    bool _usbd_ncm_started = false;  // Track USBD NCM status

    // UI cache and timer
    UiDataCache _ui_cache;
    lv_timer_t *_ui_refresh_timer = nullptr;
    portMUX_TYPE _ui_cache_lock = portMUX_INITIALIZER_UNLOCKED;

    // MAC address storage
    uint8_t _mac_addr[6] = {0};
    char _mac_str[18] = {0};

    // Timer for USBD NCM
    esp_timer_handle_t _usbd_ncm_timer = nullptr;
    static void usbd_ncm_timer_callback(void *arg);

    // Traffic statistics
    TrafficCounter _uplink_counter;    // Uplink counter (USB → WiFi)
    TrafficCounter _downlink_counter;  // Downlink counter (WiFi → USB)
    portMUX_TYPE _traffic_lock = portMUX_INITIALIZER_UNLOCKED;
    void addTrafficBytes(TrafficCounter &counter, uint32_t bytes);
    void calculateAndUpdateNetworkSpeed();

    // Popup dialog for close warning
    lv_obj_t *_popup_container = nullptr;
    lv_obj_t *_popup_label = nullptr;
    lv_obj_t *_popup_button = nullptr;
    void showCloseWarningPopup();
    void hideCloseWarningPopup();
    static void onPopupButtonClick(lv_event_t *e);
};

}
