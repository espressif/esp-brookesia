/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <chrono>
#include <ctime>
#include <cstdint>
#include <string>
#include <string_view>

#include "boost/json.hpp"
#if defined(ESP_PLATFORM)
#include "brookesia/hal_adaptor.hpp"
#include "sdkconfig.h"

// Per-test leak threshold override (see test_app_main.cpp tearDown). Used by the network
// time-update case, which drives real Wi-Fi connect/disconnect and SNTP sync; ESP-IDF may
// retain a small amount of heap across that cycle (same rationale as brookesia_service_wifi).
#define TEST_SNTP_NETWORK_MEMORY_LEAK_THRESHOLD (32)

extern int memory_leak_threshold;
#else
#include "brookesia/hal_linux.hpp"
#endif
#include "brookesia/lib_utils.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/service_helper/network/sntp.hpp"
#include "brookesia/service_helper/network/wifi.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_sntp.hpp"

#ifndef TEST_SNTP_ENABLE_NETWORK
#define TEST_SNTP_ENABLE_NETWORK 1
#endif

#ifndef TEST_SNTP_USE_WIFI
#define TEST_SNTP_USE_WIFI 1
#endif

#ifndef TEST_SNTP_WIFI_SSID
#define TEST_SNTP_WIFI_SSID ""
#endif

#ifndef TEST_SNTP_WIFI_PASSWORD
#define TEST_SNTP_WIFI_PASSWORD ""
#endif

#ifndef TEST_SNTP_NTP_SERVER
#define TEST_SNTP_NTP_SERVER "pool.ntp.org"
#endif

#ifndef TEST_SNTP_TIMEZONE
#define TEST_SNTP_TIMEZONE "UTC"
#endif

#ifndef TEST_SNTP_SYNC_TIMEOUT_MS
#define TEST_SNTP_SYNC_TIMEOUT_MS 300000
#endif

#ifndef TEST_SNTP_MIN_VALID_UNIX_TIME
#define TEST_SNTP_MIN_VALID_UNIX_TIME 1704067200
#endif

#if TEST_SNTP_USE_WIFI
#define TEST_SNTP_ASSERT_WIFI_CREDENTIALS()                                                          \
    do {                                                                                             \
        TEST_ASSERT_TRUE_MESSAGE(                                                                    \
            !std::string_view(TEST_SNTP_WIFI_SSID).empty(), "TEST_SNTP_WIFI_SSID must be configured" \
        );                                                                                           \
        TEST_ASSERT_TRUE_MESSAGE(                                                                    \
            !std::string_view(TEST_SNTP_WIFI_PASSWORD).empty(),                                      \
            "TEST_SNTP_WIFI_PASSWORD must be configured"                                             \
        );                                                                                           \
    } while (0)
#else
#define TEST_SNTP_ASSERT_WIFI_CREDENTIALS() \
    do {                                    \
    } while (0)
#endif

using SNTPHelper = esp_brookesia::service::helper::SNTP;
using WifiHelper = esp_brookesia::service::helper::Wifi;
using SNTPStateEventMonitor = SNTPHelper::EventMonitor<SNTPHelper::EventId::StateChanged>;
using WifiGeneralEventMonitor = WifiHelper::EventMonitor<WifiHelper::EventId::GeneralEventHappened>;

constexpr uint32_t SNTP_WAIT_STATE_TIMEOUT_MS = TEST_SNTP_SYNC_TIMEOUT_MS;
constexpr uint32_t SNTP_WAIT_STOP_TIMEOUT_MS = 5000;
constexpr uint32_t SNTP_POLL_INTERVAL_MS = 100;
constexpr uint32_t WIFI_WAIT_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t WIFI_WAIT_DISCONNECT_TIMEOUT_MS = 10000;
constexpr std::time_t SNTP_MIN_VALID_UNIX_TIME = TEST_SNTP_MIN_VALID_UNIX_TIME;

bool start_service_manager();
bool start_sntp_service();
bool start_network_services();
esp_brookesia::service::ServiceBinding bind_sntp_service();
void stop_services();

bool connect_wifi_station();
bool disconnect_wifi_station();
bool wait_wifi_state(WifiHelper::GeneralState expected_state, uint32_t timeout_ms);
bool wait_sntp_state(SNTPStateEventMonitor &monitor, SNTPHelper::State expected_state, uint32_t timeout_ms);
bool wait_time_synced(SNTPStateEventMonitor &monitor, uint32_t timeout_ms);
bool get_sntp_state(SNTPHelper::State &state);
