/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "general.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#if TEST_HTTP_USE_WIFI
#include "esp_wifi.h"
#endif

using namespace esp_brookesia;

#if TEST_HTTP_USE_WIFI
using WifiGeneralEventMonitor = WifiHelper::EventMonitor<WifiHelper::EventId::GeneralEventHappened>;
#endif

namespace {

static auto &service_manager = service::ServiceManager::get_instance();
static service::ServiceBinding http_binding;
#if TEST_HTTP_USE_WIFI
static service::ServiceBinding wifi_binding;
#endif
#if defined(ESP_PLATFORM)
static hal::InterfaceHandle<hal::storage::FileSystemIface> storage_fs_handle;
#endif

#if defined(ESP_PLATFORM) && TEST_HTTP_ENABLE_NETWORK
void apply_network_test_leak_threshold()
{
    unity_utils_set_leak_level(HTTP_NETWORK_TEST_LEAK_THRESHOLD);
}
#endif

} // namespace

bool HttpEventCollector::start()
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
                                   const std::string & event_name, double request_id,
                                   const boost::json::object & progress_json
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
                                    const std::string & event_name, double request_id,
                                    const boost::json::object & response_json
    ) {
        handle_response_event(event_name, request_id, response_json, completed_responses_);
    };
    auto on_request_failed = [this](
                                 const std::string & event_name, double request_id,
                                 const boost::json::object & response_json
    ) {
        handle_response_event(event_name, request_id, response_json, failed_responses_);
    };
    auto on_request_canceled = [this](
                                   const std::string & event_name, double request_id,
                                   const boost::json::object & response_json
    ) {
        handle_response_event(event_name, request_id, response_json, canceled_responses_);
    };

    connections_.push_back(
        HttpHelper::subscribe_event(HttpHelper::EventId::GeneralStateChanged, on_general_state_changed)
    );
    connections_.push_back(HttpHelper::subscribe_event(HttpHelper::EventId::RequestStarted, on_request_started));
    connections_.push_back(HttpHelper::subscribe_event(HttpHelper::EventId::RequestProgress, on_request_progress));
    connections_.push_back(HttpHelper::subscribe_event(HttpHelper::EventId::RequestCompleted, on_request_completed));
    connections_.push_back(HttpHelper::subscribe_event(HttpHelper::EventId::RequestFailed, on_request_failed));
    connections_.push_back(HttpHelper::subscribe_event(HttpHelper::EventId::RequestCanceled, on_request_canceled));

    for (const auto &connection : connections_) {
        BROOKESIA_CHECK_FALSE_RETURN(connection.connected(), false, "Failed to subscribe HTTP event");
    }

    return true;
}

bool HttpEventCollector::wait_for_general_state(const std::string &state, uint32_t timeout_ms)
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

bool HttpEventCollector::wait_for_started(uint64_t request_id, uint32_t timeout_ms)
{
    return wait_until([this, request_id]() {
        auto it = request_states_.find(request_id);
        return (it != request_states_.end()) &&
               (it->second == BROOKESIA_DESCRIBE_TO_STR(HttpHelper::RequestState::Running));
    }, timeout_ms);
}

bool HttpEventCollector::wait_for_progress(uint64_t request_id, uint32_t timeout_ms)
{
    return wait_until([this, request_id]() {
        return progress_by_request_.find(request_id) != progress_by_request_.end();
    }, timeout_ms);
}

bool HttpEventCollector::wait_for_completed(
    uint64_t request_id, HttpHelper::HttpResponse &response, uint32_t timeout_ms
)
{
    return wait_for_response(completed_responses_, request_id, response, timeout_ms);
}

bool HttpEventCollector::wait_for_failed(
    uint64_t request_id, HttpHelper::HttpResponse &response, uint32_t timeout_ms
)
{
    return wait_for_response(failed_responses_, request_id, response, timeout_ms);
}

bool HttpEventCollector::wait_for_canceled(
    uint64_t request_id, HttpHelper::HttpResponse &response, uint32_t timeout_ms
)
{
    return wait_for_response(canceled_responses_, request_id, response, timeout_ms);
}

bool HttpEventCollector::wait_for_response(
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

void HttpEventCollector::handle_response_event(
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

bool start_http_service(bool init_storage)
{
    BROOKESIA_CHECK_FALSE_RETURN(start_service_manager(init_storage), false, "Failed to start service manager");
    http_binding = service_manager.bind(HttpHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(http_binding.is_valid(), false, "Failed to bind HTTP service");
    return true;
}

bool start_service_manager(bool init_storage)
{
#if defined(ESP_PLATFORM)
    if (init_storage && !storage_fs_handle) {
        storage_fs_handle = hal::acquire_first_interface<hal::storage::FileSystemIface>();
        BROOKESIA_CHECK_FALSE_RETURN(storage_fs_handle, false, "Failed to acquire StorageFs HAL interface");
    }
#else
    (void)init_storage;
#endif
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.init(), false, "Failed to initialize service manager");
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.start(), false, "Failed to start service manager");
#if !defined(ESP_PLATFORM)
    auto http_service = std::shared_ptr<service::ServiceBase>(
                            &service::http::Http::get_instance(),
    [](service::ServiceBase *) {}
                        );
    if (service_manager.get_service(HttpHelper::get_name().data()) == nullptr) {
        BROOKESIA_CHECK_FALSE_RETURN(service_manager.add_service(http_service), false, "Failed to add HTTP service");
    }
#endif
    return true;
}

service::ServiceBinding bind_http_service()
{
    return service_manager.bind(HttpHelper::get_name().data());
}

void stop_services()
{
    http_binding.release();
#if TEST_HTTP_USE_WIFI
    wifi_binding.release();
#endif
    service_manager.stop();
    service_manager.deinit();
#if defined(ESP_PLATFORM)
    storage_fs_handle.reset();
#endif
}

#if TEST_HTTP_USE_WIFI
bool bind_wifi_service()
{
#if defined(ESP_PLATFORM) && TEST_HTTP_ENABLE_NETWORK
    apply_network_test_leak_threshold();
#endif
    if (wifi_binding.is_valid()) {
        return true;
    }

    wifi_binding = service_manager.bind(WifiHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(wifi_binding.is_valid(), false, "Failed to bind WiFi service");
    return true;
}
#endif

#if TEST_HTTP_ENABLE_NETWORK
bool start_network_services(bool init_storage)
{
#if defined(ESP_PLATFORM)
    apply_network_test_leak_threshold();
#else
    (void)init_storage;
#endif

    BROOKESIA_CHECK_FALSE_RETURN(start_http_service(init_storage), false, "Failed to start HTTP service");
#if TEST_HTTP_USE_WIFI
    BROOKESIA_CHECK_FALSE_RETURN(bind_wifi_service(), false, "Failed to bind WiFi service");
#endif
    return true;
}

bool connect_wifi_sta()
{
#if TEST_HTTP_USE_WIFI
    WifiGeneralEventMonitor general_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(general_event_monitor.start(), false, "Failed to start WiFi event monitor");

    auto set_ap_result = WifiHelper::call_function_sync(
                             WifiHelper::FunctionId::SetConnectAp,
                             std::string(TEST_HTTP_WIFI_SSID),
                             std::string(TEST_HTTP_WIFI_PASSWORD)
                         );
    BROOKESIA_CHECK_FALSE_RETURN(set_ap_result, false, "Failed to set connect AP: %1%", set_ap_result.error());

    auto connect_result = WifiHelper::call_function_sync(
                              WifiHelper::FunctionId::TriggerGeneralAction,
                              BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Connect)
                          );
    BROOKESIA_CHECK_FALSE_RETURN(connect_result, false, "Failed to trigger connect: %1%", connect_result.error());

    const bool got_connected_event = general_event_monitor.wait_for(
    std::vector<service::EventItem> {
        BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Connected), false
    },
    WIFI_WAIT_CONNECT_TIMEOUT_MS
                                     );
    BROOKESIA_CHECK_FALSE_RETURN(got_connected_event, false, "WiFi connection timeout");

    const auto ps_err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (ps_err != ESP_OK) {
        BROOKESIA_LOGW("Failed to disable Wi-Fi PS: %1%", esp_err_to_name(ps_err));
    }

#endif
    return true;
}

bool disconnect_wifi_sta()
{
#if TEST_HTTP_USE_WIFI
    WifiGeneralEventMonitor general_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(general_event_monitor.start(), false, "Failed to start WiFi event monitor");

    auto disconnect_result = WifiHelper::call_function_sync(
                                 WifiHelper::FunctionId::TriggerGeneralAction,
                                 BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Disconnect)
                             );
    BROOKESIA_CHECK_FALSE_RETURN(
        disconnect_result, false, "Failed to trigger disconnect: %1%", disconnect_result.error()
    );

    const bool got_disconnected_event = general_event_monitor.wait_for(
    std::vector<service::EventItem> {
        BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Disconnected), false
    },
    WIFI_WAIT_DISCONNECT_TIMEOUT_MS
                                        );
    const bool got_started_state = wait_wifi_state(
                                       WifiHelper::GeneralState::Started, WIFI_WAIT_DISCONNECT_TIMEOUT_MS
                                   );
    BROOKESIA_CHECK_FALSE_RETURN(
        got_disconnected_event || got_started_state, false, "WiFi disconnection timeout"
    );

#endif
    return true;
}

#if TEST_HTTP_USE_WIFI
bool wait_wifi_state(WifiHelper::GeneralState expected_state, uint32_t timeout_ms)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    const auto expected_state_str = BROOKESIA_DESCRIBE_TO_STR(expected_state);
    while (std::chrono::steady_clock::now() < deadline) {
        auto result = WifiHelper::call_function_sync<std::string>(WifiHelper::FunctionId::GetGeneralState);
        BROOKESIA_CHECK_FALSE_RETURN(result, false, "GetGeneralState failed: %1%", result.error());
        if (result.value() == expected_state_str) {
            return true;
        }
        lib_utils::test_adapter::delay_ms(HTTP_POLL_INTERVAL_MS);
    }

    return false;
}
#endif
#endif

HttpHelper::HttpRequest make_test_request()
{
    HttpHelper::HttpRequest request;
    request.url = make_echo_url("/get");
    request.method = HttpHelper::HttpMethod::Get;
    request.headers = {
        {"Accept", "text/html"},
        {"User-Agent", "ESP-Brookesia-Http-Test"}
    };
    request.timeout_ms = 15000;
    request.tls_verify = HttpHelper::TlsVerifyMode::Verify;
    request.use_crt_bundle = true;
    request.retry_count = 1;
    request.retry_on_status_codes = {502, 503, 504};
    return request;
}

std::string make_echo_url(std::string_view path)
{
    std::string url(TEST_HTTP_ECHO_URL);
    if (!path.empty() && path.front() != '/') {
        url += '/';
    }
    url += path;
    return url;
}

bool call_http_request_sync(const HttpHelper::HttpRequest &request, HttpHelper::HttpResponse &response)
{
    auto result = HttpHelper::call_function_sync<boost::json::object>(
                      HttpHelper::FunctionId::Request,
                      BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "HTTP Request failed: %1%", result.error());
    BROOKESIA_CHECK_FALSE_RETURN(
        BROOKESIA_DESCRIBE_FROM_JSON(result.value(), response), false, "Failed to parse HTTP response"
    );
    return true;
}

bool call_http_request_async(const HttpHelper::HttpRequest &request, uint64_t &request_id)
{
    auto result = HttpHelper::call_function_sync<double>(
                      HttpHelper::FunctionId::RequestAsync,
                      BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "HTTP RequestAsync failed: %1%", result.error());
    request_id = static_cast<uint64_t>(result.value());
    return true;
}

bool wait_for_http_request_state(
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
        lib_utils::test_adapter::delay_ms(HTTP_POLL_INTERVAL_MS);
    }

    return false;
}

bool wait_for_http_request_terminal_state(
    uint64_t request_id, HttpHelper::RequestStateInfo &state_info, uint32_t timeout_ms
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
        if ((state_info.state == HttpHelper::RequestState::Completed) ||
                (state_info.state == HttpHelper::RequestState::Failed) ||
                (state_info.state == HttpHelper::RequestState::Canceled)) {
            return true;
        }
        lib_utils::test_adapter::delay_ms(HTTP_POLL_INTERVAL_MS);
    }

    return false;
}

bool get_file_size(std::string_view path, size_t &size)
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

bool is_transient_network_response(const HttpHelper::HttpResponse &response)
{
    switch (response.status_code) {
    case 502:
    case 503:
    case 504:
        return true;
    default:
        break;
    }

    return (response.status_code == 0) && (response.error == HttpHelper::ErrorCode::RequestFailed);
}

bool get_response_header_case_insensitive(
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
