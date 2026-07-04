/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#define BROOKESIA_LOG_TAG "Main"
#include "brookesia/lib_utils.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_adaptor.hpp"
#include "brookesia/service_helper/network/http.hpp"
#include "brookesia/service_helper/network/wifi.hpp"
#include "brookesia/service_manager.hpp"

#ifndef EXAMPLE_WIFI_SSID
#   define EXAMPLE_WIFI_SSID ""
#endif

#ifndef EXAMPLE_WIFI_PSW
#   define EXAMPLE_WIFI_PSW ""
#endif

#ifndef EXAMPLE_HTTP_BASE_URL
#   define EXAMPLE_HTTP_BASE_URL "https://echo.apifox.com"
#endif

using namespace esp_brookesia;
using WifiHelper = service::helper::Wifi;
using HttpHelper = service::helper::Http;
using WifiGeneralEventMonitor = WifiHelper::EventMonitor<WifiHelper::EventId::GeneralEventHappened>;

namespace {

constexpr std::string_view HTTP_BASE_URL = EXAMPLE_HTTP_BASE_URL;
constexpr uint32_t WIFI_WAIT_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t HTTP_WAIT_STARTED_TIMEOUT_MS = 10 * 60 * 1000;
constexpr uint32_t HTTP_WAIT_EVENT_TIMEOUT_MS = 15000;
constexpr uint32_t HTTP_WAIT_REQUEST_TIMEOUT_MS = 30000;
constexpr uint32_t HTTP_POLL_INTERVAL_MS = 100;
constexpr uint32_t HTTP_FAILURE_BODY_LIMIT = 64;
constexpr uint32_t HTTP_FAILURE_MAX_ATTEMPTS = 3;
constexpr uint32_t HTTP_NETWORK_RETRY_ATTEMPTS = 3;
constexpr std::string_view HTTP_DOWNLOAD_FILE_PATH = "/littlefs/http_download.bin";
constexpr std::string_view HTTP_DOWNLOAD_LIMIT_FILE_PATH = "/littlefs/http_download_limit.bin";

class HttpEventCollector {
public:
    bool start()
    {
        connections_.clear();

        auto on_general_state_changed = [this](const std::string & event_name, const std::string & state) {
            BROOKESIA_LOGI("[Event: %1%] state=%2%", event_name, state);

            std::lock_guard<std::mutex> lock(mutex_);
            general_states_.push_back(state);
            cv_.notify_all();
        };
        auto on_request_started = [this](const std::string & event_name, double request_id, const std::string & state) {
            const uint64_t id = static_cast<uint64_t>(request_id);
            BROOKESIA_LOGI("[Event: %1%] request=%2%, state=%3%", event_name, id, state);

            std::lock_guard<std::mutex> lock(mutex_);
            request_states_[id] = state;
            cv_.notify_all();
        };
        auto on_request_progress = [this](
                                       const std::string & event_name, double request_id, const boost::json::object & progress_json
        ) {
            const uint64_t id = static_cast<uint64_t>(request_id);
            HttpHelper::RequestProgress progress;
            if (!BROOKESIA_DESCRIBE_FROM_JSON(progress_json, progress)) {
                BROOKESIA_LOGW("[Event: %1%] failed to parse progress for request=%2%", event_name, id);
                return;
            }

            BROOKESIA_LOGI(
                "[Event: %1%] request=%2%, downloaded=%3%, total=%4%",
                event_name, id, progress.downloaded, progress.total
            );

            std::lock_guard<std::mutex> lock(mutex_);
            progress_by_request_[id] = progress;
            cv_.notify_all();
        };
        auto on_request_completed = [this](
                                        const std::string & event_name, double request_id, const boost::json::object & response_json
        ) {
            handle_response_event(event_name, request_id, response_json, completed_responses_);
        };
        auto on_request_failed = [this](
                                     const std::string & event_name, double request_id, const boost::json::object & response_json
        ) {
            handle_response_event(event_name, request_id, response_json, failed_responses_);
        };
        auto on_request_canceled = [this](
                                       const std::string & event_name, double request_id, const boost::json::object & response_json
        ) {
            handle_response_event(event_name, request_id, response_json, canceled_responses_);
        };

        connections_.push_back(
            HttpHelper::subscribe_event(HttpHelper::EventId::GeneralStateChanged, on_general_state_changed)
        );
        connections_.push_back(HttpHelper::subscribe_event(HttpHelper::EventId::RequestStarted, on_request_started));
        connections_.push_back(HttpHelper::subscribe_event(HttpHelper::EventId::RequestProgress, on_request_progress));
        connections_.push_back(
            HttpHelper::subscribe_event(HttpHelper::EventId::RequestCompleted, on_request_completed)
        );
        connections_.push_back(HttpHelper::subscribe_event(HttpHelper::EventId::RequestFailed, on_request_failed));
        connections_.push_back(HttpHelper::subscribe_event(HttpHelper::EventId::RequestCanceled, on_request_canceled));

        for (const auto &connection : connections_) {
            BROOKESIA_CHECK_FALSE_RETURN(connection.connected(), false, "Failed to subscribe HTTP event");
        }

        return true;
    }

    bool wait_for_general_state(const std::string &state, uint32_t timeout_ms)
    {
        return wait_until([this, &state]() {
            for (const auto &received_state : general_states_) {
                if (received_state == state) {
                    return true;
                }
            }
            return false;
        }, timeout_ms);
    }

    bool wait_for_started(uint64_t request_id, uint32_t timeout_ms)
    {
        return wait_until([this, request_id]() {
            auto it = request_states_.find(request_id);
            return (it != request_states_.end()) &&
                   (it->second == BROOKESIA_DESCRIBE_TO_STR(HttpHelper::RequestState::Running));
        }, timeout_ms);
    }

    bool wait_for_progress(uint64_t request_id, uint32_t timeout_ms)
    {
        return wait_until([this, request_id]() {
            return progress_by_request_.find(request_id) != progress_by_request_.end();
        }, timeout_ms);
    }

    bool wait_for_completed(uint64_t request_id, HttpHelper::HttpResponse &response, uint32_t timeout_ms)
    {
        return wait_for_response(completed_responses_, request_id, response, timeout_ms);
    }

    bool wait_for_failed(uint64_t request_id, HttpHelper::HttpResponse &response, uint32_t timeout_ms)
    {
        return wait_for_response(failed_responses_, request_id, response, timeout_ms);
    }

    bool wait_for_canceled(uint64_t request_id, HttpHelper::HttpResponse &response, uint32_t timeout_ms)
    {
        return wait_for_response(canceled_responses_, request_id, response, timeout_ms);
    }

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
    )
    {
        std::unique_lock<std::mutex> lock(mutex_);
        const bool matched = cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]() {
            return responses.find(request_id) != responses.end();
        });
        if (!matched) {
            return false;
        }

        response = responses.at(request_id);
        return true;
    }

    void handle_response_event(
        const std::string &event_name, double request_id, const boost::json::object &response_json,
        std::map<uint64_t, HttpHelper::HttpResponse> &storage
    )
    {
        const uint64_t id = static_cast<uint64_t>(request_id);
        HttpHelper::HttpResponse response;
        if (!BROOKESIA_DESCRIBE_FROM_JSON(response_json, response)) {
            BROOKESIA_LOGW("[Event: %1%] failed to parse response for request=%2%", event_name, id);
            return;
        }

        BROOKESIA_LOGI(
            "[Event: %1%] request=%2%, status=%3%, error=%4%, body_size=%5%",
            event_name, id, response.status_code, BROOKESIA_DESCRIBE_TO_STR(response.error), response.body.size()
        );

        std::lock_guard<std::mutex> lock(mutex_);
        storage[id] = std::move(response);
        cv_.notify_all();
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::string> general_states_;
    std::map<uint64_t, std::string> request_states_;
    std::map<uint64_t, HttpHelper::RequestProgress> progress_by_request_;
    std::map<uint64_t, HttpHelper::HttpResponse> completed_responses_;
    std::map<uint64_t, HttpHelper::HttpResponse> failed_responses_;
    std::map<uint64_t, HttpHelper::HttpResponse> canceled_responses_;
    std::vector<service::EventRegistry::SignalConnection> connections_;
};

static bool demo_wifi_sta_connect(const std::string &ssid, const std::string &password);
static bool demo_http_reset_statistics();
static bool demo_http_sync_success(HttpEventCollector &collector);
static bool demo_http_sync_failure(HttpEventCollector &collector);
static bool demo_http_methods(HttpEventCollector &collector);
static bool demo_http_response_headers(HttpEventCollector &collector);
static bool demo_http_retry(HttpEventCollector &collector);
static bool demo_http_tls_modes(HttpEventCollector &collector);
static bool demo_http_download_file(HttpEventCollector &collector);
static bool demo_http_download_file_limit(HttpEventCollector &collector);
static bool demo_http_async_success(HttpEventCollector &collector);
static bool demo_http_async_cancel(HttpEventCollector &collector);
static bool demo_http_general_state_lifecycle(HttpEventCollector &collector);
static std::string make_url(std::string_view path);
static HttpHelper::HttpRequest make_default_request();
static bool call_http_request_sync(const HttpHelper::HttpRequest &request, HttpHelper::HttpResponse &response);
static bool call_http_request_async(const HttpHelper::HttpRequest &request, uint64_t &request_id);
static bool call_http_request_sync_resilient(
    const HttpHelper::HttpRequest &request, HttpHelper::HttpResponse &response,
    uint32_t max_attempts = HTTP_NETWORK_RETRY_ATTEMPTS
);
static bool call_http_request_async_resilient(
    const HttpHelper::HttpRequest &request, uint64_t &request_id,
    uint32_t max_attempts = HTTP_NETWORK_RETRY_ATTEMPTS
);
static bool wait_for_http_request_state(
    uint64_t request_id, HttpHelper::RequestState expected_state,
    HttpHelper::RequestStateInfo &state_info, uint32_t timeout_ms
);
static bool get_response_header_case_insensitive(
    const HttpHelper::HttpResponse &response, std::string_view name, std::string &value
);
static bool get_file_size(std::string_view path, size_t &size);

} // namespace

extern "C" void app_main(void)
{
    BROOKESIA_LOGI("\n\n=== Http Service Example ===\n");

    BROOKESIA_CHECK_FALSE_EXIT(!std::string_view(EXAMPLE_WIFI_SSID).empty(), "EXAMPLE_WIFI_SSID is empty");

    auto &service_manager = service::ServiceManager::get_instance();
    BROOKESIA_CHECK_FALSE_EXIT(service_manager.init(), "Failed to initialize ServiceManager");
    BROOKESIA_CHECK_FALSE_EXIT(service_manager.start(), "Failed to start ServiceManager");

    bool service_manager_stopped = false;
    lib_utils::FunctionGuard shutdown_guard([&service_manager, &service_manager_stopped]() {
        if (!service_manager_stopped) {
            BROOKESIA_LOGI("Shutting down ServiceManager...");
            service_manager.deinit();
            service_manager_stopped = true;
        }
    });

    BROOKESIA_CHECK_FALSE_EXIT(WifiHelper::is_available(), "Wifi service is not available");
    BROOKESIA_CHECK_FALSE_EXIT(HttpHelper::is_available(), "Http service is not available");

    HttpEventCollector http_event_collector;
    BROOKESIA_CHECK_FALSE_EXIT(http_event_collector.start(), "Failed to subscribe HTTP events");

    auto wifi_binding = service_manager.bind(WifiHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(wifi_binding.is_valid(), "Failed to bind Wifi service");
    BROOKESIA_LOGI("Wifi service bound successfully");

    BROOKESIA_CHECK_FALSE_EXIT(
        demo_wifi_sta_connect(EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PSW), "Failed to connect WiFi"
    );

    auto http_binding = service_manager.bind(HttpHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(http_binding.is_valid(), "Failed to bind Http service");
    BROOKESIA_LOGI("Http service bound successfully");

    BROOKESIA_CHECK_FALSE_EXIT(
        http_event_collector.wait_for_general_state(
            BROOKESIA_DESCRIBE_TO_STR(HttpHelper::GeneralState::Started), HTTP_WAIT_STARTED_TIMEOUT_MS
        ),
        "Failed to observe Http service started event"
    );

    BROOKESIA_CHECK_FALSE_EXIT(demo_http_reset_statistics(), "Failed to reset HTTP statistics before demo");
    BROOKESIA_CHECK_FALSE_EXIT(demo_http_sync_success(http_event_collector), "Failed HTTP sync success demo");
    BROOKESIA_CHECK_FALSE_EXIT(demo_http_sync_failure(http_event_collector), "Failed HTTP sync failure demo");
    BROOKESIA_CHECK_FALSE_EXIT(demo_http_methods(http_event_collector), "Failed HTTP method demos");
    BROOKESIA_CHECK_FALSE_EXIT(demo_http_response_headers(http_event_collector), "Failed HTTP response headers demo");
    BROOKESIA_CHECK_FALSE_EXIT(demo_http_retry(http_event_collector), "Failed HTTP retry demo");
    BROOKESIA_CHECK_FALSE_EXIT(demo_http_tls_modes(http_event_collector), "Failed HTTP TLS modes demo");
    BROOKESIA_CHECK_FALSE_EXIT(demo_http_download_file(http_event_collector), "Failed HTTP download file demo");
    BROOKESIA_CHECK_FALSE_EXIT(
        demo_http_download_file_limit(http_event_collector), "Failed HTTP download file limit demo"
    );
    BROOKESIA_CHECK_FALSE_EXIT(demo_http_async_success(http_event_collector), "Failed HTTP async success demo");
    BROOKESIA_CHECK_FALSE_EXIT(demo_http_async_cancel(http_event_collector), "Failed HTTP async cancel demo");
    BROOKESIA_CHECK_FALSE_EXIT(demo_http_reset_statistics(), "Failed to reset HTTP statistics after demo");
    BROOKESIA_CHECK_FALSE_EXIT(
        demo_http_general_state_lifecycle(http_event_collector), "Failed HTTP general state lifecycle demo"
    );
    service_manager_stopped = true;

    BROOKESIA_LOGI("\n\n=== Http Service Example Completed ===\n");
}

namespace {

static bool demo_wifi_sta_connect(const std::string &ssid, const std::string &password)
{
    BROOKESIA_LOGI("\n\n--- Demo: WiFi STA Connect ---\n");

    WifiGeneralEventMonitor general_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(general_event_monitor.start(), false, "Failed to start general event monitor");

    auto set_ap_result = WifiHelper::call_function_sync(WifiHelper::FunctionId::SetConnectAp, ssid, password);
    BROOKESIA_CHECK_FALSE_RETURN(set_ap_result, false, "Failed to set connect AP: %1%", set_ap_result.error());
    BROOKESIA_LOGI("Set target AP: %1%", ssid);

    auto connect_result = WifiHelper::call_function_sync(
                              WifiHelper::FunctionId::TriggerGeneralAction,
                              BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Connect)
                          );
    BROOKESIA_CHECK_FALSE_RETURN(connect_result, false, "Failed to trigger connect: %1%", connect_result.error());
    BROOKESIA_LOGI("Connection triggered, waiting for result...");

    const bool got_connected_event = general_event_monitor.wait_for(
    std::vector<service::EventItem> {
        BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Connected), false
    },
    WIFI_WAIT_CONNECT_TIMEOUT_MS
                                     );
    BROOKESIA_CHECK_FALSE_RETURN(
        got_connected_event, false, "Connection timeout after %1% seconds", WIFI_WAIT_CONNECT_TIMEOUT_MS / 1000
    );

    general_event_monitor.clear();

    // Disable Wi-Fi modem sleep for the duration of this long-running HTTPS
    // demo. With the default WIFI_PS_MIN_MODEM the modem briefly sleeps in
    // each DTIM interval; on a jittery AP this accumulates beacon misses and
    // drops handshake packets, stalling TLS for 15-25s.
    esp_err_t ps_err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (ps_err != ESP_OK) {
        BROOKESIA_LOGW("Failed to disable Wi-Fi PS: %1%", esp_err_to_name(ps_err));
    } else {
        BROOKESIA_LOGI("Wi-Fi modem sleep disabled (WIFI_PS_NONE) for HTTP demo");
    }

    BROOKESIA_LOGI("\n\n--- Demo: WiFi STA Connect Completed ---\n");

    return true;
}

static bool demo_http_reset_statistics()
{
    BROOKESIA_LOGI("\n\n--- Demo: Http ResetStatistics ---\n");

    auto reset_result = HttpHelper::call_function_sync(HttpHelper::FunctionId::ResetStatistics);
    BROOKESIA_CHECK_FALSE_RETURN(reset_result, false, "Failed to reset HTTP statistics: %1%", reset_result.error());

    BROOKESIA_LOGI("ResetStatistics finished successfully");
    BROOKESIA_LOGI("\n\n--- Demo: Http ResetStatistics Completed ---\n");
    return true;
}

static bool demo_http_sync_success(HttpEventCollector &collector)
{
    BROOKESIA_LOGI("\n\n--- Demo: Http Request (sync success) ---\n");

    HttpHelper::HttpResponse response;
    BROOKESIA_CHECK_FALSE_RETURN(
        call_http_request_sync_resilient(make_default_request(), response), false, "Failed to call HTTP Request"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        response.error == HttpHelper::ErrorCode::Ok, false,
        "Expected successful response, got %1% (%2%)",
        BROOKESIA_DESCRIBE_TO_STR(response.error), response.error_message
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        response.status_code == 200, false, "Unexpected status code: %1%", response.status_code
    );
    BROOKESIA_CHECK_FALSE_RETURN(!response.body.empty(), false, "HTTP response body is empty");
    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_started(response.request_id, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
        "Did not observe RequestStarted for request %1%", response.request_id
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_progress(response.request_id, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
        "Did not observe RequestProgress for request %1%", response.request_id
    );

    HttpHelper::HttpResponse completed_response;
    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_completed(response.request_id, completed_response, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
        "Did not observe RequestCompleted for request %1%", response.request_id
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        completed_response.error == HttpHelper::ErrorCode::Ok, false,
        "Completed event reports unexpected error: %1%",
        BROOKESIA_DESCRIBE_TO_STR(completed_response.error)
    );

    BROOKESIA_LOGI(
        "Sync request succeeded: request_id=%1%, status=%2%, body_size=%3%",
        response.request_id, response.status_code, response.body.size()
    );
    BROOKESIA_LOGI("\n\n--- Demo: Http Request (sync success) Completed ---\n");
    return true;
}

static bool demo_http_sync_failure(HttpEventCollector &collector)
{
    BROOKESIA_LOGI("\n\n--- Demo: Http Request (sync failure) ---\n");

    auto request = make_default_request();
    request.max_response_size = HTTP_FAILURE_BODY_LIMIT;

    HttpHelper::HttpResponse response;
    for (uint32_t attempt = 1; attempt <= HTTP_FAILURE_MAX_ATTEMPTS; attempt++) {
        BROOKESIA_CHECK_FALSE_RETURN(
            call_http_request_sync(request, response), false, "Failed to call HTTP Request"
        );
        if (response.error == HttpHelper::ErrorCode::BodyTooLarge) {
            break;
        }
        BROOKESIA_CHECK_FALSE_RETURN(
            response.error == HttpHelper::ErrorCode::RequestFailed, false,
            "Expected BodyTooLarge, got %1% (%2%)",
            BROOKESIA_DESCRIBE_TO_STR(response.error), response.error_message
        );
        BROOKESIA_LOGW(
            "HTTP failure-path request failed before body limit check on attempt %1%/%2%: %3%",
            attempt, HTTP_FAILURE_MAX_ATTEMPTS, response.error_message
        );
    }
    BROOKESIA_CHECK_FALSE_RETURN(
        response.error == HttpHelper::ErrorCode::BodyTooLarge, false,
        "Expected BodyTooLarge after %1% attempts, got %2% (%3%)",
        HTTP_FAILURE_MAX_ATTEMPTS, BROOKESIA_DESCRIBE_TO_STR(response.error), response.error_message
    );

    HttpHelper::HttpResponse failed_response;
    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_failed(response.request_id, failed_response, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
        "Did not observe RequestFailed for request %1%", response.request_id
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        failed_response.error == HttpHelper::ErrorCode::BodyTooLarge, false,
        "Failed event reports unexpected error: %1%",
        BROOKESIA_DESCRIBE_TO_STR(failed_response.error)
    );

    BROOKESIA_LOGI(
        "Sync failure path verified: request_id=%1%, error=%2%",
        response.request_id, BROOKESIA_DESCRIBE_TO_STR(response.error)
    );
    BROOKESIA_LOGI("\n\n--- Demo: Http Request (sync failure) Completed ---\n");
    return true;
}

static bool demo_http_async_success(HttpEventCollector &collector)
{
    BROOKESIA_LOGI("\n\n--- Demo: Http RequestAsync (success) ---\n");

    uint64_t request_id = 0;
    BROOKESIA_CHECK_FALSE_RETURN(
        call_http_request_async_resilient(make_default_request(), request_id), false,
        "Failed to call HTTP RequestAsync"
    );
    BROOKESIA_CHECK_FALSE_RETURN(request_id > 0, false, "RequestAsync returned invalid request id");

    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_started(request_id, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
        "Did not observe RequestStarted for request %1%", request_id
    );

    HttpHelper::RequestStateInfo state_info;
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_http_request_state(
            request_id, HttpHelper::RequestState::Completed, state_info, HTTP_WAIT_REQUEST_TIMEOUT_MS
        ),
        false, "Request %1% did not reach Completed state", request_id
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_info.status_code == 200, false, "Unexpected async status code: %1%", state_info.status_code
    );

    HttpHelper::HttpResponse completed_response;
    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_completed(request_id, completed_response, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
        "Did not observe RequestCompleted for request %1%", request_id
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        completed_response.error == HttpHelper::ErrorCode::Ok, false,
        "Async completed event reports unexpected error: %1%",
        BROOKESIA_DESCRIBE_TO_STR(completed_response.error)
    );
    BROOKESIA_CHECK_FALSE_RETURN(!completed_response.body.empty(), false, "Async completed response body is empty");
    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_progress(request_id, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
        "Did not observe RequestProgress for request %1%", request_id
    );

    BROOKESIA_LOGI(
        "Async request succeeded: request_id=%1%, status=%2%, body_size=%3%",
        request_id, completed_response.status_code, completed_response.body.size()
    );
    BROOKESIA_LOGI("\n\n--- Demo: Http RequestAsync (success) Completed ---\n");
    return true;
}

static bool demo_http_async_cancel(HttpEventCollector &collector)
{
    BROOKESIA_LOGI("\n\n--- Demo: Http CancelRequest ---\n");

    uint64_t request_id = 0;
    BROOKESIA_CHECK_FALSE_RETURN(
        call_http_request_async_resilient(make_default_request(), request_id), false,
        "Failed to call HTTP RequestAsync"
    );
    BROOKESIA_CHECK_FALSE_RETURN(request_id > 0, false, "RequestAsync returned invalid request id");

    auto cancel_result = HttpHelper::call_function_sync(
                             HttpHelper::FunctionId::CancelRequest, static_cast<double>(request_id)
                         );
    BROOKESIA_CHECK_FALSE_RETURN(cancel_result, false, "Failed to call CancelRequest: %1%", cancel_result.error());

    HttpHelper::RequestStateInfo state_info;
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_http_request_state(
            request_id, HttpHelper::RequestState::Canceled, state_info, HTTP_WAIT_REQUEST_TIMEOUT_MS
        ),
        false, "Request %1% did not reach Canceled state", request_id
    );

    HttpHelper::HttpResponse canceled_response;
    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_canceled(request_id, canceled_response, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
        "Did not observe RequestCanceled for request %1%", request_id
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        canceled_response.error == HttpHelper::ErrorCode::Canceled, false,
        "Canceled event reports unexpected error: %1%",
        BROOKESIA_DESCRIBE_TO_STR(canceled_response.error)
    );

    BROOKESIA_LOGI(
        "CancelRequest succeeded: request_id=%1%, error=%2%",
        request_id, BROOKESIA_DESCRIBE_TO_STR(canceled_response.error)
    );
    BROOKESIA_LOGI("\n\n--- Demo: Http CancelRequest Completed ---\n");
    return true;
}

static bool demo_http_methods(HttpEventCollector &collector)
{
    BROOKESIA_LOGI("\n\n--- Demo: Http Methods ---\n");

    struct MethodCase {
        HttpHelper::HttpMethod method;
        std::string path;
        bool expect_body;
    };
    const std::vector<MethodCase> cases = {
        {HttpHelper::HttpMethod::Post, "/post", true},
        {HttpHelper::HttpMethod::Put, "/put", true},
        {HttpHelper::HttpMethod::Patch, "/patch", true},
        {HttpHelper::HttpMethod::Delete, "/delete", true},
        {HttpHelper::HttpMethod::Head, "/get", false},
    };

    for (const auto &test_case : cases) {
        auto request = make_default_request();
        request.url = make_url(test_case.path);
        request.method = test_case.method;
        request.headers["Content-Type"] = "application/json";
        if (test_case.expect_body) {
            request.body = "{\"hello\":\"world\"}";
        }

        HttpHelper::HttpResponse response;
        BROOKESIA_CHECK_FALSE_RETURN(
            call_http_request_sync_resilient(request, response), false, "Failed to call HTTP method %1%",
            BROOKESIA_DESCRIBE_TO_STR(test_case.method)
        );
        BROOKESIA_CHECK_FALSE_RETURN(
            response.error == HttpHelper::ErrorCode::Ok, false, "HTTP method %1% failed: %2%",
            BROOKESIA_DESCRIBE_TO_STR(test_case.method), response.error_message
        );
        BROOKESIA_CHECK_FALSE_RETURN(
            response.status_code == 200, false, "Unexpected status code: %1%", response.status_code
        );
        if (test_case.expect_body) {
            BROOKESIA_CHECK_FALSE_RETURN(
                response.body.find("world") != std::string::npos, false, "HTTP method response body missing payload"
            );
        } else {
            BROOKESIA_CHECK_FALSE_RETURN(response.body.empty(), false, "HEAD response body should be empty");
        }
        HttpHelper::HttpResponse completed_response;
        BROOKESIA_CHECK_FALSE_RETURN(
            collector.wait_for_started(response.request_id, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
            "Did not observe RequestStarted for method request %1%", response.request_id
        );
        BROOKESIA_CHECK_FALSE_RETURN(
            collector.wait_for_completed(response.request_id, completed_response, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
            "Did not observe RequestCompleted for method request %1%", response.request_id
        );
    }

    BROOKESIA_LOGI("\n\n--- Demo: Http Methods Completed ---\n");
    return true;
}

static bool demo_http_response_headers(HttpEventCollector &collector)
{
    BROOKESIA_LOGI("\n\n--- Demo: Http Response Headers ---\n");

    auto request = make_default_request();
    request.url = make_url("/response-headers?X-ESP-Brookesia=present");
    HttpHelper::HttpResponse response;
    BROOKESIA_CHECK_FALSE_RETURN(
        call_http_request_sync_resilient(request, response), false, "Failed to call HTTP response headers request"
    );
    BROOKESIA_CHECK_FALSE_RETURN(response.error == HttpHelper::ErrorCode::Ok, false, "HTTP headers request failed");
    std::string header_value;
    BROOKESIA_CHECK_FALSE_RETURN(
        get_response_header_case_insensitive(response, "X-ESP-Brookesia", header_value), false,
        "Response header X-ESP-Brookesia is missing"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        header_value == "present", false, "Unexpected X-ESP-Brookesia header value"
    );

    HttpHelper::HttpResponse completed_response;
    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_completed(response.request_id, completed_response, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
        "Did not observe RequestCompleted for header request %1%", response.request_id
    );
    BROOKESIA_LOGI("\n\n--- Demo: Http Response Headers Completed ---\n");
    return true;
}

static bool demo_http_retry(HttpEventCollector &collector)
{
    BROOKESIA_LOGI("\n\n--- Demo: Http Retry ---\n");

    auto request = make_default_request();
    request.url = make_url("/status/500");
    request.retry_count = 2;
    request.timeout_ms = 8000;

    HttpHelper::HttpResponse response;
    for (uint32_t attempt = 1; attempt <= HTTP_NETWORK_RETRY_ATTEMPTS; attempt++) {
        BROOKESIA_CHECK_FALSE_RETURN(
            call_http_request_sync(request, response), false, "Failed to call HTTP retry request"
        );
        if (response.status_code == 500) {
            break;
        }
        if (response.error == HttpHelper::ErrorCode::RequestFailed) {
            BROOKESIA_LOGW(
                "Retry demo transient failure on attempt %1%/%2%: %3%",
                attempt, HTTP_NETWORK_RETRY_ATTEMPTS, response.error_message
            );
            continue;
        }
        BROOKESIA_CHECK_FALSE_RETURN(
            false, false, "Unexpected retry response: status(%1%), error(%2%)",
            response.status_code, BROOKESIA_DESCRIBE_TO_STR(response.error)
        );
    }
    BROOKESIA_CHECK_FALSE_RETURN(
        response.status_code == 500, false, "Unexpected retry status code: %1%", response.status_code
    );

    if (response.error == HttpHelper::ErrorCode::Ok) {
        HttpHelper::HttpResponse completed_response;
        BROOKESIA_CHECK_FALSE_RETURN(
            collector.wait_for_completed(response.request_id, completed_response, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
            "Did not observe RequestCompleted for retry request %1%", response.request_id
        );
    } else {
        HttpHelper::HttpResponse failed_response;
        BROOKESIA_CHECK_FALSE_RETURN(
            collector.wait_for_failed(response.request_id, failed_response, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
            "Did not observe RequestFailed for retry request %1%", response.request_id
        );
    }

    BROOKESIA_LOGI("\n\n--- Demo: Http Retry Completed ---\n");
    return true;
}

static bool demo_http_tls_modes(HttpEventCollector &collector)
{
    BROOKESIA_LOGI("\n\n--- Demo: Http TLS Modes ---\n");

    const std::vector<HttpHelper::TlsVerifyMode> modes = {
        HttpHelper::TlsVerifyMode::Default,
        HttpHelper::TlsVerifyMode::Verify,
        HttpHelper::TlsVerifyMode::Skip,
    };
    for (auto mode : modes) {
        auto request = make_default_request();
        request.tls_verify = mode;
        HttpHelper::HttpResponse response;
        BROOKESIA_CHECK_FALSE_RETURN(
            call_http_request_sync_resilient(request, response), false, "Failed TLS mode request"
        );
        BROOKESIA_CHECK_FALSE_RETURN(
            response.error == HttpHelper::ErrorCode::Ok, false, "TLS mode %1% failed: %2%",
            BROOKESIA_DESCRIBE_TO_STR(mode), response.error_message
        );
        if (mode != HttpHelper::TlsVerifyMode::Skip) {
            BROOKESIA_CHECK_FALSE_RETURN(response.status_code == 200, false, "Unexpected TLS status code");
        }
        HttpHelper::HttpResponse completed_response;
        BROOKESIA_CHECK_FALSE_RETURN(
            collector.wait_for_completed(response.request_id, completed_response, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
            "Did not observe RequestCompleted for TLS request %1%", response.request_id
        );
    }

    BROOKESIA_LOGI("\n\n--- Demo: Http TLS Modes Completed ---\n");
    return true;
}

static bool demo_http_download_file(HttpEventCollector &collector)
{
    BROOKESIA_LOGI("\n\n--- Demo: Http Download File ---\n");

    std::remove(HTTP_DOWNLOAD_FILE_PATH.data());

    auto request = make_default_request();
    request.url = make_url("/bytes/102400");
    request.download_path = HTTP_DOWNLOAD_FILE_PATH;

    HttpHelper::HttpResponse response;
    BROOKESIA_CHECK_FALSE_RETURN(
        call_http_request_sync_resilient(request, response), false, "Failed to call HTTP download request"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        response.error == HttpHelper::ErrorCode::Ok, false, "HTTP download request failed: %1%",
        response.error_message
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        response.file_path == HTTP_DOWNLOAD_FILE_PATH, false, "Unexpected download path: %1%", response.file_path
    );
    BROOKESIA_CHECK_FALSE_RETURN(response.body.empty(), false, "File download should not keep response body in memory");

    size_t file_size = 0;
    BROOKESIA_CHECK_FALSE_RETURN(
        get_file_size(HTTP_DOWNLOAD_FILE_PATH, file_size), false, "Failed to stat downloaded file"
    );
    BROOKESIA_CHECK_FALSE_RETURN(file_size > 0, false, "Downloaded file is empty");

    HttpHelper::HttpResponse completed_response;
    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_started(response.request_id, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
        "Did not observe RequestStarted for download request %1%", response.request_id
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_progress(response.request_id, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
        "Did not observe RequestProgress for download request %1%", response.request_id
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_completed(response.request_id, completed_response, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
        "Did not observe RequestCompleted for download request %1%", response.request_id
    );

    BROOKESIA_LOGI(
        "Download file succeeded: request_id=%1%, path=%2%, size=%3%",
        response.request_id, response.file_path, file_size
    );
    BROOKESIA_LOGI("\n\n--- Demo: Http Download File Completed ---\n");
    return true;
}

static bool demo_http_download_file_limit(HttpEventCollector &collector)
{
    BROOKESIA_LOGI("\n\n--- Demo: Http Download File Limit ---\n");

    std::remove(HTTP_DOWNLOAD_LIMIT_FILE_PATH.data());

    auto request = make_default_request();
    request.url = make_url("/bytes/512");
    request.download_path = HTTP_DOWNLOAD_LIMIT_FILE_PATH;
    request.max_file_size = 16;

    HttpHelper::HttpResponse response;
    BROOKESIA_CHECK_FALSE_RETURN(
        call_http_request_sync_resilient(request, response), false, "Failed to call HTTP file limit request"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        response.error == HttpHelper::ErrorCode::FileTooLarge, false,
        "Expected FileTooLarge, got %1% (%2%)",
        BROOKESIA_DESCRIBE_TO_STR(response.error), response.error_message
    );

    HttpHelper::HttpResponse failed_response;
    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_failed(response.request_id, failed_response, HTTP_WAIT_EVENT_TIMEOUT_MS), false,
        "Did not observe RequestFailed for file limit request %1%", response.request_id
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        failed_response.error == HttpHelper::ErrorCode::FileTooLarge, false,
        "Failed event reports unexpected error: %1%",
        BROOKESIA_DESCRIBE_TO_STR(failed_response.error)
    );

    std::remove(HTTP_DOWNLOAD_LIMIT_FILE_PATH.data());

    BROOKESIA_LOGI(
        "Download file limit path verified: request_id=%1%, error=%2%",
        response.request_id, BROOKESIA_DESCRIBE_TO_STR(response.error)
    );
    BROOKESIA_LOGI("\n\n--- Demo: Http Download File Limit Completed ---\n");
    return true;
}

static bool demo_http_general_state_lifecycle(HttpEventCollector &collector)
{
    BROOKESIA_LOGI("\n\n--- Demo: Http General State Lifecycle ---\n");

    // Verify that the HttpEventCollector observed the Started event published when
    // the HTTP service finished its on_start sequence. The framework's shutdown
    // path tears down the manager-wide task scheduler before per-service on_stop
    // runs, so Stopping/Stopped GeneralStateChanged events get dropped by the
    // scheduler-backed publish path; we therefore intentionally do not wait on
    // them here. Final teardown of the ServiceManager is left to app_main's
    // RAII guard (shutdown_guard) and ~ServiceBinding so the natural unbind
    // ordering can run without colliding with deinit() from inside the demo.
    BROOKESIA_CHECK_FALSE_RETURN(
        collector.wait_for_general_state(
            BROOKESIA_DESCRIBE_TO_STR(HttpHelper::GeneralState::Started), HTTP_WAIT_EVENT_TIMEOUT_MS
        ),
        false, "Failed to observe Http service started event"
    );

    BROOKESIA_LOGI("\n\n--- Demo: Http General State Lifecycle Completed ---\n");
    return true;
}

static std::string make_url(std::string_view path)
{
    std::string url(HTTP_BASE_URL);
    if (!path.empty() && path.front() != '/') {
        url += '/';
    }
    url += path;
    return url;
}

static HttpHelper::HttpRequest make_default_request()
{
    HttpHelper::HttpRequest request;
    request.url = make_url("/get");
    request.method = HttpHelper::HttpMethod::Get;
    request.headers = {
        {"Accept", "text/html"},
        {"User-Agent", "ESP-Brookesia-Http-Example"}
    };
    request.timeout_ms = 15000;
    request.tls_verify = HttpHelper::TlsVerifyMode::Verify;
    request.use_crt_bundle = true;
    request.retry_count = 1;
    request.retry_on_status_codes = {502, 503, 504};
    return request;
}

static bool call_http_request_sync(const HttpHelper::HttpRequest &request, HttpHelper::HttpResponse &response)
{
    auto result = HttpHelper::call_function_sync<boost::json::object>(
                      HttpHelper::FunctionId::Request,
                      BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                  );
    if (!result) {
        BROOKESIA_LOGW("HTTP Request helper error treated as transient: %1%", result.error());
        response = {};
        response.error = HttpHelper::ErrorCode::RequestFailed;
        response.error_message = result.error();
        return true;
    }
    BROOKESIA_CHECK_FALSE_RETURN(
        BROOKESIA_DESCRIBE_FROM_JSON(result.value(), response), false, "Failed to parse HTTP response"
    );
    return true;
}

static bool call_http_request_async(const HttpHelper::HttpRequest &request, uint64_t &request_id)
{
    auto result = HttpHelper::call_function_sync<double>(
                      HttpHelper::FunctionId::RequestAsync,
                      BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "HTTP RequestAsync failed: %1%", result.error());
    request_id = static_cast<uint64_t>(result.value());
    return true;
}

static bool call_http_request_sync_resilient(
    const HttpHelper::HttpRequest &request, HttpHelper::HttpResponse &response, uint32_t max_attempts
)
{
    for (uint32_t attempt = 1; attempt <= max_attempts; attempt++) {
        BROOKESIA_CHECK_FALSE_RETURN(call_http_request_sync(request, response), false, "Failed to call HTTP Request");
        if (response.error != HttpHelper::ErrorCode::RequestFailed) {
            return true;
        }
        BROOKESIA_LOGW(
            "Transient HTTP connection failure on attempt %1%/%2%: %3%",
            attempt, max_attempts, response.error_message
        );
    }
    return true;
}

static bool call_http_request_async_resilient(
    const HttpHelper::HttpRequest &request, uint64_t &request_id, uint32_t max_attempts
)
{
    for (uint32_t attempt = 1; attempt <= max_attempts; attempt++) {
        BROOKESIA_CHECK_FALSE_RETURN(
            call_http_request_async(request, request_id), false, "Failed to call HTTP RequestAsync"
        );
        HttpHelper::RequestStateInfo state_info;
        if (wait_for_http_request_state(
                    request_id, HttpHelper::RequestState::Running, state_info, HTTP_WAIT_EVENT_TIMEOUT_MS
                )) {
            return true;
        }
        if (wait_for_http_request_state(
                    request_id, HttpHelper::RequestState::Failed, state_info, HTTP_POLL_INTERVAL_MS
                ) && state_info.error == HttpHelper::ErrorCode::RequestFailed) {
            BROOKESIA_LOGW(
                "Transient async HTTP failure on attempt %1%/%2%: id(%3%)", attempt, max_attempts, request_id
            );
            continue;
        }
        return true;
    }
    return true;
}

static bool wait_for_http_request_state(
    uint64_t request_id, HttpHelper::RequestState expected_state,
    HttpHelper::RequestStateInfo &state_info, uint32_t timeout_ms
)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        auto result = HttpHelper::call_function_sync<boost::json::object>(
                          HttpHelper::FunctionId::GetRequestState, static_cast<double>(request_id)
                      );
        BROOKESIA_CHECK_FALSE_RETURN(result, false, "GetRequestState failed: %1%", result.error());
        BROOKESIA_CHECK_FALSE_RETURN(
            BROOKESIA_DESCRIBE_FROM_JSON(result.value(), state_info), false, "Failed to parse request state"
        );
        if (state_info.state == expected_state) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(HTTP_POLL_INTERVAL_MS));
    }

    return false;
}

static bool get_response_header_case_insensitive(
    const HttpHelper::HttpResponse &response, std::string_view name, std::string &value
)
{
    auto to_lower = [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    };

    std::string expected(name);
    std::transform(expected.begin(), expected.end(), expected.begin(), to_lower);
    for (const auto &header : response.headers) {
        std::string key(header.key().data(), header.key().size());
        std::transform(key.begin(), key.end(), key.begin(), to_lower);
        if (key != expected) {
            continue;
        }

        const auto &header_value = header.value();
        if (header_value.is_string()) {
            value = std::string(header_value.as_string().c_str());
        } else {
            value = boost::json::serialize(header_value);
        }
        return true;
    }

    return false;
}

static bool get_file_size(std::string_view path, size_t &size)
{
    auto *file = std::fopen(std::string(path).c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    lib_utils::FunctionGuard file_guard([file]() {
        std::fclose(file);
    });

    if (std::fseek(file, 0, SEEK_END) != 0) {
        return false;
    }
    const auto end_pos = std::ftell(file);
    if (end_pos < 0) {
        return false;
    }
    size = static_cast<size_t>(end_pos);
    return true;
}

} // namespace
