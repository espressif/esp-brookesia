/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <cstddef>
#include "sdkconfig.h"
#ifdef BROOKESIA_LOG_TAG
#   undef BROOKESIA_LOG_TAG
#endif
#define BROOKESIA_LOG_TAG "TestApp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_wifi.hpp"
#include "brookesia/service_helper/wifi.hpp"

#if defined(CI_TEST_WIFI_2_4G_AP1_SSID) && defined(CI_TEST_WIFI_2_4G_AP1_PSW)
#define TEST_WIFI_SSID1      CI_TEST_WIFI_2_4G_AP1_SSID
#define TEST_WIFI_PASSWORD1  CI_TEST_WIFI_2_4G_AP1_PSW
#else
// #define TEST_WIFI_SSID1      ""
// #define TEST_WIFI_PASSWORD1  ""
#endif

#if defined(CI_TEST_WIFI_2_4G_AP2_SSID) && defined(CI_TEST_WIFI_2_4G_AP2_PSW)
#define TEST_WIFI_SSID2      CI_TEST_WIFI_2_4G_AP2_SSID
#define TEST_WIFI_PASSWORD2  CI_TEST_WIFI_2_4G_AP2_PSW
#else
// #define TEST_WIFI_SSID2      ""
// #define TEST_WIFI_PASSWORD2  ""
#endif

#if defined(CONFIG_ESP_HOSTED_ENABLED)
constexpr uint32_t TEST_WIFI_INIT_DURATION_MS = 8000;
constexpr uint32_t TEST_WIFI_START_DURATION_MS = 1000;
#else
constexpr uint32_t TEST_WIFI_INIT_DURATION_MS = 1000;
constexpr uint32_t TEST_WIFI_START_DURATION_MS = 1000;
#endif
constexpr uint32_t TEST_WIFI_CONNECT_DURATION_MS = 15000;
constexpr uint32_t TEST_WIFI_SCAN_DURATION_MS = 5000;
constexpr uint32_t TEST_WIFI_RESET_DATA_TIMEOUT_MS = 1000;
constexpr uint32_t TEST_WIFI_SET_SCAN_PARAMS_TIMEOUT_MS = 1000;

constexpr uint32_t TEST_SOFTAP_START_TIMEOUT_MS = 1000;
constexpr uint32_t TEST_SOFTAP_START_DURATION_MS = 15000;
constexpr uint32_t TEST_SOFTAP_STOP_TIMEOUT_MS = 1000;
constexpr uint32_t TEST_SOFTAP_STOP_DURATION_MS = 5000;
constexpr uint32_t TEST_SOFTAP_PROVISION_START_TIMEOUT_MS = 1000;
constexpr uint32_t TEST_SOFTAP_PROVISION_START_DURATION_MS = 15000;
constexpr uint32_t TEST_SOFTAP_PROVISION_STOP_TIMEOUT_MS = 1000;
constexpr uint32_t TEST_SOFTAP_PROVISION_STOP_DURATION_MS = 5000;

// Event data structures for verification
struct GeneralActionEvent {
    std::string action;
};

struct GeneralEventHappened {
    std::string event;
};

struct ScanApInfosUpdatedEvent {
    std::vector<esp_brookesia::service::wifi::ScanApInfo> ap_infos;
};

struct SoftApEventHappened {
    std::string event;
};

// Event collector for test verification
class EventCollector {
public:
    EventCollector();
    ~EventCollector();

    void on_general_action_triggered(const esp_brookesia::service::EventItemMap &params);
    void on_general_event_happened(const esp_brookesia::service::EventItemMap &params);
    void on_scan_ap_infos_updated(const esp_brookesia::service::EventItemMap &params);
    void on_softap_event_happened(const esp_brookesia::service::EventItemMap &params);
    size_t get_general_action_count(esp_brookesia::service::helper::Wifi::GeneralAction action);
    size_t get_general_event_count(esp_brookesia::service::helper::Wifi::GeneralEvent event);
    size_t get_softap_event_count(esp_brookesia::service::helper::Wifi::SoftApEvent event);
    void clear();
    bool wait_for_general_actions(
        size_t count, uint32_t timeout_ms = 5000,
        esp_brookesia::service::helper::Wifi::GeneralAction action =
            esp_brookesia::service::helper::Wifi::GeneralAction::Max
    );
    bool wait_for_general_events(
        size_t count, uint32_t timeout_ms = 5000,
        esp_brookesia::service::helper::Wifi::GeneralEvent event =
            esp_brookesia::service::helper::Wifi::GeneralEvent::Max
    );
    bool wait_for_scan_ap_infos_updated(size_t count, uint32_t timeout_ms = 10000);
    bool wait_for_softap_events(
        size_t count, uint32_t timeout_ms = 5000,
        esp_brookesia::service::helper::Wifi::SoftApEvent event =
            esp_brookesia::service::helper::Wifi::SoftApEvent::Max
    );

    std::vector<GeneralActionEvent> general_actions;
    std::vector<GeneralEventHappened> general_events;
    std::vector<ScanApInfosUpdatedEvent> scan_ap_infos_updated;
    std::vector<SoftApEventHappened> softap_events;
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<esp_brookesia::service::EventRegistry::SignalConnection> connections;

private:
    void setup_event_subscriptions();
};

void startup();
void shutdown();
