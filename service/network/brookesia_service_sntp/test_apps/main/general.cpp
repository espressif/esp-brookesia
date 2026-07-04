/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "general.hpp"

#include <algorithm>

#if defined(ESP_PLATFORM) && TEST_SNTP_USE_WIFI
#include "esp_wifi.h"
#endif

using namespace esp_brookesia;

namespace {

static auto &service_manager = service::ServiceManager::get_instance();
static auto &time_profiler = lib_utils::TimeProfiler::get_instance();
static service::ServiceBinding sntp_binding;
#if TEST_SNTP_USE_WIFI
static service::ServiceBinding wifi_binding;
#endif

void configure_time_profiler()
{
    lib_utils::TimeProfiler::FormatOptions options;
    options.use_unicode = true;
    options.use_color = true;
    options.sort_by = lib_utils::TimeProfiler::FormatOptions::SortBy::TotalDesc;
    options.show_percentages = true;
    options.name_width = 40;
    options.calls_width = 6;
    options.num_width = 10;
    options.percent_width = 7;
    options.precision = 2;
    options.time_unit = lib_utils::TimeProfiler::FormatOptions::TimeUnit::Milliseconds;
    time_profiler.set_format_options(options);
}

#if TEST_SNTP_USE_WIFI
bool bind_wifi_service()
{
    if (wifi_binding.is_valid()) {
        return true;
    }

    wifi_binding = service_manager.bind(WifiHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(wifi_binding.is_valid(), false, "Failed to bind WiFi service");
    return true;
}
#endif

bool parse_sntp_state(const std::string &value, SNTPHelper::State &state)
{
    if (value == BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::State::Stopped)) {
        state = SNTPHelper::State::Stopped;
        return true;
    }
    if (value == BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::State::CheckingNetwork)) {
        state = SNTPHelper::State::CheckingNetwork;
        return true;
    }
    if (value == BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::State::Syncing)) {
        state = SNTPHelper::State::Syncing;
        return true;
    }
    if (value == BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::State::Synced)) {
        state = SNTPHelper::State::Synced;
        return true;
    }

    return false;
}

} // namespace

bool start_service_manager()
{
    configure_time_profiler();
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.init(), false, "Failed to initialize service manager");
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.start(), false, "Failed to start service manager");
    return true;
}

bool start_sntp_service()
{
    BROOKESIA_CHECK_FALSE_RETURN(start_service_manager(), false, "Failed to start service manager");
    sntp_binding = service_manager.bind(SNTPHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(sntp_binding.is_valid(), false, "Failed to bind SNTP service");
    return true;
}

bool start_network_services()
{
    BROOKESIA_CHECK_FALSE_RETURN(start_sntp_service(), false, "Failed to start SNTP service");
#if TEST_SNTP_USE_WIFI
    BROOKESIA_CHECK_FALSE_RETURN(bind_wifi_service(), false, "Failed to bind WiFi service");
#endif
    return true;
}

service::ServiceBinding bind_sntp_service()
{
    return service_manager.bind(SNTPHelper::get_name().data());
}

void stop_services()
{
    sntp_binding.release();
#if TEST_SNTP_USE_WIFI
    wifi_binding.release();
#endif
    service_manager.stop();
    service_manager.deinit();
    time_profiler.report();
    time_profiler.clear();
}

bool connect_wifi_station()
{
#if TEST_SNTP_USE_WIFI
    WifiGeneralEventMonitor general_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(general_event_monitor.start(), false, "Failed to start WiFi event monitor");

    auto set_ap_result = WifiHelper::call_function_sync(
                             WifiHelper::FunctionId::SetConnectAp,
                             std::string(TEST_SNTP_WIFI_SSID),
                             std::string(TEST_SNTP_WIFI_PASSWORD)
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

#if defined(ESP_PLATFORM)
    const auto ps_err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (ps_err != ESP_OK) {
        BROOKESIA_LOGW("Failed to disable Wi-Fi PS: %1%", esp_err_to_name(ps_err));
    }
#endif
#endif
    return true;
}

bool disconnect_wifi_station()
{
#if TEST_SNTP_USE_WIFI
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

bool wait_wifi_state(WifiHelper::GeneralState expected_state, uint32_t timeout_ms)
{
#if TEST_SNTP_USE_WIFI
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    const auto expected_state_str = BROOKESIA_DESCRIBE_TO_STR(expected_state);
    while (std::chrono::steady_clock::now() < deadline) {
        auto result = WifiHelper::call_function_sync<std::string>(WifiHelper::FunctionId::GetGeneralState);
        BROOKESIA_CHECK_FALSE_RETURN(result, false, "GetGeneralState failed: %1%", result.error());
        if (result.value() == expected_state_str) {
            return true;
        }
        lib_utils::test_adapter::delay_ms(SNTP_POLL_INTERVAL_MS);
    }

    return false;
#else
    (void)expected_state;
    (void)timeout_ms;
    return true;
#endif
}

bool get_sntp_state(SNTPHelper::State &state)
{
    auto result = SNTPHelper::call_function_sync<std::string>(SNTPHelper::FunctionId::GetState);
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "GetState failed: %1%", result.error());
    BROOKESIA_CHECK_FALSE_RETURN(parse_sntp_state(result.value(), state), false, "Invalid SNTP state: %1%", result.value());
    return true;
}

bool wait_sntp_state(SNTPStateEventMonitor &monitor, SNTPHelper::State expected_state, uint32_t timeout_ms)
{
    SNTPHelper::State current_state = SNTPHelper::State::Max;
    if (get_sntp_state(current_state) && (current_state == expected_state)) {
        return true;
    }

    return monitor.wait_for({BROOKESIA_DESCRIBE_TO_STR(expected_state)}, timeout_ms);
}

bool wait_time_synced(SNTPStateEventMonitor &monitor, uint32_t timeout_ms)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        SNTPHelper::State current_state = SNTPHelper::State::Max;
        if (get_sntp_state(current_state) && (current_state == SNTPHelper::State::Synced)) {
            auto result = SNTPHelper::call_function_sync<bool>(SNTPHelper::FunctionId::IsTimeSynced);
            BROOKESIA_CHECK_FALSE_RETURN(result, false, "IsTimeSynced failed: %1%", result.error());
            if (result.value()) {
                return true;
            }
        }

        const auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      deadline - std::chrono::steady_clock::now()
                                  ).count();
        if (remaining_ms <= 0) {
            break;
        }
        (void)monitor.wait_for(
        {BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::State::Synced)},
        static_cast<uint32_t>(std::min<int64_t>(remaining_ms, SNTP_POLL_INTERVAL_MS))
        );
    }

    return false;
}
