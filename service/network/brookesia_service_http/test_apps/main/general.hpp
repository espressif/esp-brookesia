/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include "boost/json.hpp"
#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "brookesia/hal_adaptor.hpp"
#include "brookesia/hal_interface.hpp"
#else
#include "brookesia/hal_linux.hpp"
#endif
#include "brookesia/lib_utils.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/service_helper/network/http.hpp"
#include "brookesia/service_http.hpp"
#include "brookesia/service_manager.hpp"
#if TEST_HTTP_USE_WIFI
#include "brookesia/service_helper/network/wifi.hpp"
#endif

#ifndef TEST_HTTP_ENABLE_NETWORK
#if defined(ESP_PLATFORM)
#define TEST_HTTP_ENABLE_NETWORK 0
#else
#define TEST_HTTP_ENABLE_NETWORK 1
#endif
#endif

#ifndef TEST_HTTP_USE_WIFI
#define TEST_HTTP_USE_WIFI 0
#endif

#ifndef TEST_HTTP_WIFI_SSID
#define TEST_HTTP_WIFI_SSID ""
#endif

#ifndef TEST_HTTP_WIFI_PASSWORD
#define TEST_HTTP_WIFI_PASSWORD ""
#endif

#ifndef TEST_HTTP_ECHO_URL
#define TEST_HTTP_ECHO_URL "https://echo.apifox.com"
#endif

#ifndef TEST_HTTP_DOWNLOAD_DIR
#if defined(ESP_PLATFORM)
#define TEST_HTTP_DOWNLOAD_DIR "/littlefs"
#else
#define TEST_HTTP_DOWNLOAD_DIR "/tmp"
#endif
#endif

#ifndef TEST_HTTP_STRESS_SEQUENTIAL_COUNT
#define TEST_HTTP_STRESS_SEQUENTIAL_COUNT 50
#endif

#ifndef TEST_HTTP_STRESS_CONCURRENT_ROUNDS
#define TEST_HTTP_STRESS_CONCURRENT_ROUNDS 5
#endif

#ifndef TEST_HTTP_STRESS_CONCURRENT_LIMIT
#define TEST_HTTP_STRESS_CONCURRENT_LIMIT 4
#endif

#if TEST_HTTP_USE_WIFI
#define TEST_HTTP_ASSERT_WIFI_CREDENTIALS() TEST_ASSERT_NOT_EQUAL(0, std::string_view(TEST_HTTP_WIFI_SSID).size())
#else
#define TEST_HTTP_ASSERT_WIFI_CREDENTIALS() \
    do {                                    \
    } while (0)
#endif

#define TEST_HTTP_RETURN_IF_TRANSIENT(response)                 \
    do {                                                        \
        if (is_transient_network_response(response)) {          \
            TEST_IGNORE_MESSAGE("Transient HTTP endpoint failure"); \
            return;                                             \
        }                                                       \
    } while (0)

using HttpHelper = esp_brookesia::service::helper::Http;
namespace lib_utils = esp_brookesia::lib_utils;
#if TEST_HTTP_USE_WIFI
using WifiHelper = esp_brookesia::service::helper::Wifi;
#endif

constexpr uint32_t HTTP_WAIT_EVENT_TIMEOUT_MS = 15000;
constexpr uint32_t HTTP_WAIT_REQUEST_TIMEOUT_MS = 30000;
constexpr uint32_t HTTP_POLL_INTERVAL_MS = 100;
constexpr uint32_t HTTP_FAILURE_BODY_LIMIT = 64;
constexpr uint32_t HTTP_FAILURE_MAX_ATTEMPTS = 3;
constexpr uint32_t HTTP_DOWNLOAD_FILE_LIMIT = 16;
constexpr uint32_t WIFI_WAIT_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t WIFI_WAIT_DISCONNECT_TIMEOUT_MS = 10000;
constexpr size_t HTTP_STRESS_SEQUENTIAL_COUNT = TEST_HTTP_STRESS_SEQUENTIAL_COUNT;
constexpr size_t HTTP_STRESS_CONCURRENT_ROUNDS = TEST_HTTP_STRESS_CONCURRENT_ROUNDS;
constexpr size_t HTTP_STRESS_CONCURRENT_LIMIT = TEST_HTTP_STRESS_CONCURRENT_LIMIT;
// Allowance for Wi-Fi/lwIP/TLS cleanup churn that survives netif teardown across cycles.
constexpr size_t HTTP_NETWORK_TEST_LEAK_THRESHOLD = 512;
constexpr std::string_view HTTP_DOWNLOAD_FILE_PATH = TEST_HTTP_DOWNLOAD_DIR "/http_download.bin";
constexpr std::string_view HTTP_DOWNLOAD_LIMIT_FILE_PATH = TEST_HTTP_DOWNLOAD_DIR "/http_download_limit.bin";

class HttpEventCollector {
public:
    bool start();

    bool wait_for_general_state(const std::string &state, uint32_t timeout_ms);
    bool wait_for_started(uint64_t request_id, uint32_t timeout_ms);
    bool wait_for_progress(uint64_t request_id, uint32_t timeout_ms);
    bool wait_for_completed(uint64_t request_id, HttpHelper::HttpResponse &response, uint32_t timeout_ms);
    bool wait_for_failed(uint64_t request_id, HttpHelper::HttpResponse &response, uint32_t timeout_ms);
    bool wait_for_canceled(uint64_t request_id, HttpHelper::HttpResponse &response, uint32_t timeout_ms);

private:
    template <typename Predicate>
    bool wait_until(Predicate predicate, uint32_t timeout_ms)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), predicate);
    }

    bool wait_for_response(
        const std::map<uint64_t, HttpHelper::HttpResponse> &responses, uint64_t request_id,
        HttpHelper::HttpResponse &response, uint32_t timeout_ms
    );
    void handle_response_event(
        const std::string &event_name, double request_id, const boost::json::object &response_json,
        std::map<uint64_t, HttpHelper::HttpResponse> &storage
    );

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::string> general_states_;
    std::map<uint64_t, std::string> request_states_;
    std::map<uint64_t, HttpHelper::RequestProgress> progress_by_request_;
    std::map<uint64_t, HttpHelper::HttpResponse> completed_responses_;
    std::map<uint64_t, HttpHelper::HttpResponse> failed_responses_;
    std::map<uint64_t, HttpHelper::HttpResponse> canceled_responses_;
    std::vector<esp_brookesia::service::EventRegistry::SignalConnection> connections_;
};

bool start_http_service(bool init_storage = false);
bool start_service_manager(bool init_storage = false);
esp_brookesia::service::ServiceBinding bind_http_service();
void stop_services();

#if TEST_HTTP_USE_WIFI
bool bind_wifi_service();
#endif
#if TEST_HTTP_ENABLE_NETWORK
bool start_network_services(bool init_storage = false);
bool connect_wifi_sta();
bool disconnect_wifi_sta();
#if TEST_HTTP_USE_WIFI
bool wait_wifi_state(WifiHelper::GeneralState expected_state, uint32_t timeout_ms);
#endif
#endif

HttpHelper::HttpRequest make_test_request();
std::string make_echo_url(std::string_view path);
bool call_http_request_sync(const HttpHelper::HttpRequest &request, HttpHelper::HttpResponse &response);
bool call_http_request_async(const HttpHelper::HttpRequest &request, uint64_t &request_id);
bool wait_for_http_request_state(
    uint64_t request_id, HttpHelper::RequestState expected_state,
    HttpHelper::RequestStateInfo &state_info, uint32_t timeout_ms
);
bool wait_for_http_request_terminal_state(
    uint64_t request_id, HttpHelper::RequestStateInfo &state_info, uint32_t timeout_ms
);
bool get_file_size(std::string_view path, size_t &size);
bool is_transient_network_response(const HttpHelper::HttpResponse &response);
bool get_response_header_case_insensitive(
    const HttpHelper::HttpResponse &response, std::string_view name, std::string &value
);
