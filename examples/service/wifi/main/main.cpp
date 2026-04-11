/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <future>
#include <vector>
#include <string>
#define BROOKESIA_LOG_TAG "Main"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper/wifi.hpp"

// #define EXAMPLE_WIFI_SSID ""
// #define EXAMPLE_WIFI_PSW  ""

using namespace esp_brookesia;
using WifiHelper = service::helper::Wifi;
using WifiGeneralEventMonitor = WifiHelper::EventMonitor<WifiHelper::EventId::GeneralEventHappened>;
using WifiScanStateEventMonitor = WifiHelper::EventMonitor<WifiHelper::EventId::ScanStateChanged>;
using WifiScanApEventMonitor = WifiHelper::EventMonitor<WifiHelper::EventId::ScanApInfosUpdated>;
using WifiSoftApEventMonitor = WifiHelper::EventMonitor<WifiHelper::EventId::SoftApEventHappened>;

#if CONFIG_ESP_HOSTED_ENABLED
constexpr uint32_t WIFI_WAIT_START_TIMEOUT_MS = 5000;
#else
constexpr uint32_t WIFI_WAIT_START_TIMEOUT_MS = 2000;
#endif
constexpr uint32_t WIFI_WAIT_CONNECT_TIMEOUT_MS = 10000;
constexpr uint32_t WIFI_WAIT_SCAN_START_TIMEOUT_MS = 2000;
constexpr uint32_t WIFI_WAIT_SCAN_AP_TIMEOUT_MS = 20000;
constexpr uint32_t WIFI_WAIT_DISCONNECT_TIMEOUT_MS = 2000;
constexpr uint32_t WIFI_DO_SOFTAP_PROVISION_START_TIMEOUT_MS = 1000;
constexpr uint32_t WIFI_DO_SOFTAP_PROVISION_STOP_TIMEOUT_MS = 1000;
constexpr uint32_t WIFI_WAIT_SOFTAP_PROVISION_START_TIMEOUT_MS = 10000;
constexpr uint32_t WIFI_WAIT_SOFTAP_PROVISION_STOP_TIMEOUT_MS = 5000;
constexpr uint32_t WIFI_WAIT_SOFTAP_PROVISION_CONNECT_TIMEOUT_MS = 120000;

static std::vector<service::EventRegistry::SignalConnection> global_connections;

static WifiHelper::GeneralState get_wifi_state();
static bool get_wifi_connect_ap(WifiHelper::ConnectApInfo &info);
static bool get_wifi_connected_aps(std::vector<WifiHelper::ConnectApInfo> &infos);
static bool get_wifi_softap_params(WifiHelper::SoftApParams &softap_params);
static bool demo_wifi_subscribe_events();
static bool demo_wifi_sta_scan();
static bool demo_wifi_sta_connect(const std::string &ssid, const std::string &password);
static bool demo_wifi_sta_auto_connect();
static bool demo_wifi_softap_provision();
static bool demo_wifi_reset_data();

extern "C" void app_main(void)
{
    BROOKESIA_LOGI("\n\n=== Wifi Service Example ===\n");

    // Initialize ServiceManager
    auto &service_manager = service::ServiceManager::get_instance();
    BROOKESIA_CHECK_FALSE_EXIT(service_manager.init(), "Failed to initialize ServiceManager");
    BROOKESIA_CHECK_FALSE_EXIT(service_manager.start(), "Failed to start ServiceManager");

    // Use FunctionGuard for cleanup
    lib_utils::FunctionGuard shutdown_guard([&service_manager]() {
        BROOKESIA_LOGI("Shutting down ServiceManager...");
        service_manager.stop();
    });

    // Check if the service is available
    BROOKESIA_CHECK_FALSE_EXIT(WifiHelper::is_available(), "Wifi service is not available");
    BROOKESIA_LOGI("Wifi service is available");

    // Bind the service to start it and its dependencies (RAII - service stays alive while `binding` exists)
    auto binding = service_manager.bind(WifiHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind Wifi service");
    BROOKESIA_LOGI("Wifi service bound successfully");

    // Subscribe to events
    BROOKESIA_CHECK_FALSE_EXIT(demo_wifi_subscribe_events(), "Failed to demo wifi subscribe events");

    // Create a general event monitor to wait for the specific event
    WifiGeneralEventMonitor general_event_monitor;
    BROOKESIA_CHECK_FALSE_EXIT(general_event_monitor.start(), "Failed to start general event monitor");

    /* Trigger WiFi init and start */
    /* Note: `init` is not required for most cases, and it will be automatically triggered when `start` is called */
    // auto init_result = WifiHelper::call_function_sync(
    //                        WifiHelper::FunctionId::TriggerGeneralAction,
    //                        BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Init)
    //                    );
    // BROOKESIA_CHECK_FALSE_EXIT(init_result, "Failed to init WiFi");
    auto start_result = WifiHelper::call_function_sync(
                            WifiHelper::FunctionId::TriggerGeneralAction,
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Start)
                        );
    BROOKESIA_CHECK_FALSE_EXIT(start_result, "Failed to start WiFi");

    // Wait for started event with timeout
    bool got_event = general_event_monitor.wait_for(std::vector<service::EventItem> {
        BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Started), false
    }, WIFI_WAIT_START_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_EXIT(got_event, "Failed to wait for general event");
    // Verify WiFi state is started
    BROOKESIA_CHECK_FALSE_EXIT(get_wifi_state() == WifiHelper::GeneralState::Started, "Failed to verify WiFi state");

    // Stop and clear general event monitor
    general_event_monitor.stop();
    general_event_monitor.clear();

    BROOKESIA_CHECK_FALSE_EXIT(demo_wifi_sta_scan(), "Failed to demo wifi STA scan");
#if defined(EXAMPLE_WIFI_SSID) && defined(EXAMPLE_WIFI_PSW)
    BROOKESIA_CHECK_FALSE_EXIT(
        demo_wifi_sta_connect(EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PSW), "Failed to demo wifi STA connect"
    );
    BROOKESIA_CHECK_FALSE_EXIT(demo_wifi_sta_auto_connect(), "Failed to demo wifi STA auto connect");
#endif
    BROOKESIA_CHECK_FALSE_EXIT(demo_wifi_softap_provision(), "Failed to demo wifi SoftAP provision");
    BROOKESIA_CHECK_FALSE_EXIT(demo_wifi_reset_data(), "Failed to demo wifi reset data");

    /* Trigger WiFi stop and deinit */
    /* Note: `stop` is not required for most cases, and it will be automatically triggered when `deinit` is called */
    // auto stop_result = WifiHelper::call_function_sync(
    //                        WifiHelper::FunctionId::TriggerGeneralAction,
    //                        BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Stop)
    //                    );
    // BROOKESIA_CHECK_FALSE_EXIT(stop_result, "Failed to stop WiFi");
    auto deinit_result = WifiHelper::call_function_sync(
                             WifiHelper::FunctionId::TriggerGeneralAction,
                             BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Deinit)
                         );
    BROOKESIA_CHECK_FALSE_EXIT(deinit_result, "Failed to deinit WiFi");

    BROOKESIA_LOGI("\n\n=== Wifi Service Example Completed ===\n");
}

static WifiHelper::GeneralState get_wifi_state()
{
    auto state_result = WifiHelper::call_function_sync<std::string>(WifiHelper::FunctionId::GetGeneralState);
    BROOKESIA_CHECK_FALSE_RETURN(
        state_result, WifiHelper::GeneralState::Max, "Failed to get state: %1%", state_result.error()
    );

    WifiHelper::GeneralState state = WifiHelper::GeneralState::Max;
    auto parse_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(state_result.value(), state);
    BROOKESIA_CHECK_FALSE_RETURN(
        parse_result, WifiHelper::GeneralState::Max, "Failed to parse state: %1%", state_result.value()
    );

    return state;
}

static bool get_wifi_connect_ap(WifiHelper::ConnectApInfo &info)
{
    auto get_ap_result = WifiHelper::call_function_sync<boost::json::object>(WifiHelper::FunctionId::GetConnectAp);
    BROOKESIA_CHECK_FALSE_RETURN(
        get_ap_result, false, "Failed to get connect AP: %1%", get_ap_result.error()
    );

    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(get_ap_result.value(), info);
    BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse connect AP");

    return true;
}

static bool get_wifi_connected_aps(std::vector<WifiHelper::ConnectApInfo> &infos)
{
    // Get all connected APs including the history connected APs
    auto get_connected_aps_result = WifiHelper::call_function_sync<boost::json::array>(
                                        WifiHelper::FunctionId::GetConnectedAps
                                    );
    BROOKESIA_CHECK_FALSE_RETURN(
        get_connected_aps_result, false, "Failed to get connected APs: %1%", get_connected_aps_result.error()
    );

    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(get_connected_aps_result.value(), infos);
    BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse connected APs");

    return true;
}

static bool get_wifi_softap_params(WifiHelper::SoftApParams &softap_params)
{
    auto get_softap_result = WifiHelper::call_function_sync<boost::json::object>(
                                 WifiHelper::FunctionId::GetSoftApParams
                             );
    BROOKESIA_CHECK_FALSE_RETURN(
        get_softap_result, false, "Failed to get SoftAP params: %1%", get_softap_result.error()
    );

    return BROOKESIA_DESCRIBE_FROM_JSON(get_softap_result.value(), softap_params);
}

static bool demo_wifi_subscribe_events()
{
    BROOKESIA_LOGI("\n\n--- Demo: WiFi Subscribe Events ---\n");

    // Subscribe to general action triggered event to track action
    auto on_general_action_triggered = [](const std::string & event_name, const std::string & action) {
        BROOKESIA_LOGI("[Event: %1%] %2%", event_name, action);
    };
    // RAII - unsubscribe when connection is destroyed
    auto general_action_triggered_connection = WifiHelper::subscribe_event(
                WifiHelper::EventId::GeneralActionTriggered, on_general_action_triggered
            );
    BROOKESIA_CHECK_FALSE_RETURN(
        general_action_triggered_connection.connected(), false, "Failed to subscribe general action triggered event"
    );
    // Add to global connections to keep the subscription alive
    global_connections.push_back(std::move(general_action_triggered_connection));

    // Subscribe to general event happened event to track state changes
    auto on_general_event_happened = [](const std::string & event_name, const std::string & event, bool is_unexpected) {
        BROOKESIA_LOGI("[Event: %1%] %2% (unexpected: %3%)", event_name, event, is_unexpected);
    };
    // RAII - unsubscribe when connection is destroyed
    auto general_event_happened_connection = WifiHelper::subscribe_event(
                WifiHelper::EventId::GeneralEventHappened, on_general_event_happened
            );
    BROOKESIA_CHECK_FALSE_RETURN(
        general_event_happened_connection.connected(), false, "Failed to subscribe general event happened event"
    );
    // Add to global connections to keep the subscription alive
    global_connections.push_back(std::move(general_event_happened_connection));

    // Subscribe to scan state changed event to track state changes
    auto on_scan_state_changed = [](const std::string & event_name, const bool is_running) {
        BROOKESIA_LOGI("[Event: %1%] %2%", event_name, is_running);
    };
    // RAII - unsubscribe when connection is destroyed
    auto scan_state_changed_connection = WifiHelper::subscribe_event(
            WifiHelper::EventId::ScanStateChanged, on_scan_state_changed
                                         );
    BROOKESIA_CHECK_FALSE_RETURN(
        scan_state_changed_connection.connected(), false, "Failed to subscribe scan state changed event"
    );
    // Add to global connections to keep the subscription alive
    global_connections.push_back(std::move(scan_state_changed_connection));

    // Subscribe to scan AP infos updated event
    auto on_scan_ap_infos_updated = [&](const std::string & event_name, const boost::json::array & ap_infos) {
        BROOKESIA_LOGI("[Event: %1%] Scanned %2% APs", event_name, ap_infos.size());

        std::vector<WifiHelper::ScanApInfo> scanned_aps;
        auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(ap_infos, scanned_aps);
        BROOKESIA_CHECK_FALSE_EXIT(parse_result, "Failed to parse scan AP infos");

        for (const auto &ap : scanned_aps) {
            BROOKESIA_LOGI("\t- %1%", BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(ap, BROOKESIA_DESCRIBE_FORMAT_JSON));
        }
    };
    // RAII - unsubscribe when connection is destroyed
    auto scan_ap_infos_updated_connection = WifiHelper::subscribe_event(
            WifiHelper::EventId::ScanApInfosUpdated, on_scan_ap_infos_updated
                                            );
    BROOKESIA_CHECK_FALSE_RETURN(
        scan_ap_infos_updated_connection.connected(), false, "Failed to subscribe scan AP infos updated event"
    );
    // Add to global connections to keep the subscription alive
    global_connections.push_back(std::move(scan_ap_infos_updated_connection));

    // Subscribe to softap event happened event to track state changes
    auto on_softap_event_happened = [](const std::string & event_name, const std::string & event) {
        BROOKESIA_LOGI("[Event: %1%] %2%", event_name, event);
    };
    // RAII - unsubscribe when connection is destroyed
    auto softap_event_happened_connection = WifiHelper::subscribe_event(
            WifiHelper::EventId::SoftApEventHappened, on_softap_event_happened
                                            );
    BROOKESIA_CHECK_FALSE_RETURN(
        softap_event_happened_connection.connected(), false, "Failed to subscribe softap event happened event"
    );
    // Add to global connections to keep the subscription alive
    global_connections.push_back(std::move(softap_event_happened_connection));

    BROOKESIA_LOGI("\n\n--- Demo: WiFi Subscribe Events Completed ---\n");

    return true;
}

static bool demo_wifi_sta_scan()
{
    BROOKESIA_LOGI("\n\n--- Demo: WiFi STA Scan ---\n");

    // Create the scan event monitors
    WifiScanStateEventMonitor scan_state_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(scan_state_event_monitor.start(), false, "Failed to start scan state event monitor");
    WifiScanApEventMonitor scan_ap_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(scan_ap_event_monitor.start(), false, "Failed to start scan AP event monitor");

    constexpr int MAX_SCAN_ATTEMPTS = 2;
    constexpr int SCAN_INTERVAL_MS = WIFI_WAIT_SCAN_AP_TIMEOUT_MS / MAX_SCAN_ATTEMPTS;

    // Set scan parameters
    WifiHelper::ScanParams scan_params{
        .ap_count = 10,
        .interval_ms = SCAN_INTERVAL_MS,
        // Since the scan duration may be longer than the target interval time,
        // the actual interval time may be longer than the target value
        .timeout_ms = WIFI_WAIT_SCAN_AP_TIMEOUT_MS,
        // Scan will be stopped automatically after the timeout
    };
    auto set_scan_params_result = WifiHelper::call_function_sync(
                                      WifiHelper::FunctionId::SetScanParams,
                                      BROOKESIA_DESCRIBE_TO_JSON(scan_params).as_object()
                                  );
    BROOKESIA_CHECK_FALSE_RETURN(
        set_scan_params_result, false, "Failed to set scan params: %1%", set_scan_params_result.error()
    );
    BROOKESIA_LOGI(
        "Scan params set: AP count=%1%, Interval=%2%ms, Timeout=%3%ms",
        scan_params.ap_count, scan_params.interval_ms, scan_params.timeout_ms
    );

    // Start scanning
    auto scan_start_result = WifiHelper::call_function_sync(WifiHelper::FunctionId::TriggerScanStart);
    BROOKESIA_CHECK_FALSE_RETURN(scan_start_result, false, "Failed to start scan: %1%", scan_start_result.error());
    // Wait for scan started event
    auto got_scan_started_event = scan_state_event_monitor.wait_for(std::vector<service::EventItem> {
        true
    }, WIFI_WAIT_SCAN_START_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_RETURN(got_scan_started_event, false, "Failed to wait for scan started event");
    BROOKESIA_LOGI("WiFi scan started, waiting for finished");

    // Wait for scan finished event
    auto got_scan_finished_event = scan_state_event_monitor.wait_for(std::vector<service::EventItem> {
        false
    }, WIFI_WAIT_SCAN_AP_TIMEOUT_MS + 1000);
    BROOKESIA_CHECK_FALSE_RETURN(got_scan_finished_event, false, "Failed to wait for scan finished event");
    BROOKESIA_LOGI("WiFi scan finished");

    // Check if the scan AP event is satisfied
    BROOKESIA_CHECK_OUT_RANGE_RETURN(
        scan_ap_event_monitor.get_count(), 1, MAX_SCAN_ATTEMPTS, false, "Not enough scan AP events received"
    );

    // Get last received scan AP infos
    // get_last<T>() returns std::optional<std::tuple<T>>, use std::get<0> to extract the value
    auto last_scan_event = scan_ap_event_monitor.get_last<boost::json::array>();
    BROOKESIA_CHECK_FALSE_RETURN(last_scan_event.has_value(), false, "No scan event received");
    // Extract the boost::json::array from the tuple
    const auto &last_scan_array = std::get<0>(last_scan_event.value());
    std::vector<WifiHelper::ScanApInfo> last_scan_ap_infos;
    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(last_scan_array, last_scan_ap_infos);
    BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse last scan AP infos");
    BROOKESIA_LOGI("Last received scan AP infos: %1%", last_scan_ap_infos);

    // // Stop scanning
    // auto scan_stop_result = WifiHelper::call_function_sync(WifiHelper::FunctionId::TriggerScanStop);
    // BROOKESIA_CHECK_FALSE_RETURN(scan_stop_result, false, "Failed to stop scan: %1%", scan_stop_result.error());

    BROOKESIA_LOGI("\n\n--- Demo: WiFi STA Scan Completed ---\n");

    return true;
}

static bool demo_wifi_sta_connect(const std::string &ssid, const std::string &password)
{
    BROOKESIA_LOGI("\n\n--- Demo: WiFi STA Connect ---\n");

    // Create a general event monitor to track state changes
    WifiGeneralEventMonitor general_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(general_event_monitor.start(), false, "Failed to start general event monitor");

    // Set the AP to connect to
    auto set_ap_result = WifiHelper::call_function_sync(WifiHelper::FunctionId::SetConnectAp, ssid, password);
    BROOKESIA_CHECK_FALSE_RETURN(set_ap_result, false, "Failed to set connect AP: %1%", set_ap_result.error());
    BROOKESIA_LOGI("Set target AP: %1%", ssid);
    // Verify the connect AP is the same as the set AP
    WifiHelper::ConnectApInfo connect_ap;
    BROOKESIA_CHECK_FALSE_RETURN(get_wifi_connect_ap(connect_ap), false, "Failed to get connect AP");
    BROOKESIA_CHECK_FALSE_RETURN(
        connect_ap.ssid == ssid, false, "The connect AP is not the same as the set AP: %1% != %2%", connect_ap.ssid, ssid
    );

    // Trigger connect
    auto connect_result = WifiHelper::call_function_sync(
                              WifiHelper::FunctionId::TriggerGeneralAction,
                              BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Connect)
                          );
    BROOKESIA_CHECK_FALSE_RETURN(connect_result, false, "Failed to trigger connect: %1%", connect_result.error());
    BROOKESIA_LOGI("Connection triggered, waiting for result...");

    // Wait for connection event with timeout
    auto got_connected_event = general_event_monitor.wait_for(std::vector<service::EventItem> {
        BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Connected), false
    }, WIFI_WAIT_CONNECT_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_RETURN(
        got_connected_event, false, "Connection timeout after %1% seconds", WIFI_WAIT_CONNECT_TIMEOUT_MS / 1000
    );
    // Verify WiFi state is connected
    BROOKESIA_CHECK_FALSE_RETURN(
        get_wifi_state() == WifiHelper::GeneralState::Connected, false, "Failed to verify WiFi state"
    );

    general_event_monitor.clear();

    // Verify the connected AP is the same as the set AP
    std::vector<WifiHelper::ConnectApInfo> connected_aps;
    BROOKESIA_CHECK_FALSE_RETURN(get_wifi_connected_aps(connected_aps), false, "Failed to get connected APs");
    BROOKESIA_CHECK_FALSE_RETURN(
        std::find(connected_aps.begin(), connected_aps.end(), connect_ap) != connected_aps.end(), false,
        "Not found the target AP: %1% in the connected APs", connect_ap
    );

    // Trigger disconnect
    auto disconnect_result = WifiHelper::call_function_sync(
                                 WifiHelper::FunctionId::TriggerGeneralAction,
                                 BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Disconnect)
                             );
    BROOKESIA_CHECK_FALSE_RETURN(disconnect_result, false, "Failed to disconnect: %1%", disconnect_result.error());
    BROOKESIA_LOGI("Disconnected, waiting for result...");

    // Wait for disconnected event with timeout
    auto got_disconnected_event = general_event_monitor.wait_for(std::vector<service::EventItem> {
        BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Disconnected), false
    }, WIFI_WAIT_DISCONNECT_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_RETURN(
        got_disconnected_event, false, "Disconnection timeout after %1% seconds", WIFI_WAIT_DISCONNECT_TIMEOUT_MS / 1000
    );
    // Verify WiFi state is disconnected
    BROOKESIA_CHECK_FALSE_RETURN(
        get_wifi_state() == WifiHelper::GeneralState::Started, false, "Failed to verify WiFi state"
    );

    BROOKESIA_LOGI("\n\n--- Demo: WiFi STA Connect Completed ---\n");

    return true;
}

static bool demo_wifi_sta_auto_connect()
{
    BROOKESIA_LOGI("\n\n--- Demo: WiFi STA Auto Connect ---\n");

    // Create a general event monitor to track state changes
    WifiGeneralEventMonitor general_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(general_event_monitor.start(), false, "Failed to start general event monitor");

    // Stop WiFi first
    auto stop_result = WifiHelper::call_function_sync(
                           WifiHelper::FunctionId::TriggerGeneralAction,
                           BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Stop)
                       );
    BROOKESIA_CHECK_FALSE_RETURN(stop_result, false, "Failed to stop: %1%", stop_result.error());
    // Then, restart and wait for auto connect
    auto start_result = WifiHelper::call_function_sync(
                            WifiHelper::FunctionId::TriggerGeneralAction,
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Start)
                        );
    BROOKESIA_CHECK_FALSE_RETURN(start_result, false, "Failed to start WiFi");
    BROOKESIA_LOGI("Restarted, waiting for auto connect...");

    // Wait for started event with timeout
    bool got_connected_event = general_event_monitor.wait_for(std::vector<service::EventItem> {
        BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Connected), false
    }, WIFI_WAIT_CONNECT_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_RETURN(got_connected_event, false, "Failed to wait for connected event");
    // Verify WiFi state is connected
    BROOKESIA_CHECK_FALSE_RETURN(
        get_wifi_state() == WifiHelper::GeneralState::Connected, false, "Failed to verify WiFi state"
    );

    general_event_monitor.clear();

    BROOKESIA_LOGI("\n\n--- Demo: WiFi STA Auto Connect Completed ---\n");

    return true;
}

static bool demo_wifi_softap_provision()
{
    BROOKESIA_LOGI("\n\n--- Demo: WiFi SoftAP Provision ---\n");

    // Create a softap event monitor to track state changes
    WifiSoftApEventMonitor softap_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(softap_event_monitor.start(), false, "Failed to start SoftAP event monitor");
    // Create a general event monitor to track state changes
    WifiGeneralEventMonitor general_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(general_event_monitor.start(), false, "Failed to start general event monitor");

    /* Set SoftAP parameters */
    /* Note: If not set, will use the default parameters */
    // WifiHelper::SoftApParams target_softap_params{
    //     .ssid = "Example-Ap",
    //     .channel = 1,   // If not set, will try to find the best channel from scan AP infos
    // };
    // auto set_softap_result = WifiHelper::call_function_sync(
    //                              WifiHelper::FunctionId::SetSoftApParams,
    //                              BROOKESIA_DESCRIBE_TO_JSON(target_softap_params).as_object()
    //                          );
    // BROOKESIA_CHECK_FALSE_RETURN(
    //     set_softap_result, false, "Failed to set SoftAP params: %1%", set_softap_result.error()
    // );

    // Trigger disconnect first if connected or connecting
    auto current_wifi_state = get_wifi_state();
    if ((current_wifi_state == WifiHelper::GeneralState::Connected) ||
            (current_wifi_state == WifiHelper::GeneralState::Connecting)) {
        auto disconnect_result = WifiHelper::call_function_sync(
                                     WifiHelper::FunctionId::TriggerGeneralAction,
                                     BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Disconnect)
                                 );
        BROOKESIA_CHECK_FALSE_RETURN(disconnect_result, false, "Failed to disconnect: %1%", disconnect_result.error());
        BROOKESIA_LOGI("Disconnected, waiting for result...");

        // Wait for disconnected event with timeout
        auto got_disconnected_event = general_event_monitor.wait_for(std::vector<service::EventItem> {
            BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Disconnected), false
        }, WIFI_WAIT_DISCONNECT_TIMEOUT_MS);
        BROOKESIA_CHECK_FALSE_RETURN(
            got_disconnected_event, false, "Disconnection timeout after %1% seconds", WIFI_WAIT_DISCONNECT_TIMEOUT_MS / 1000
        );

        general_event_monitor.clear();
    }

    // Start SoftAP provision
    auto provision_start_result = WifiHelper::call_function_sync(
                                      WifiHelper::FunctionId::TriggerSoftApProvisionStart,
                                      service::helper::Timeout(WIFI_DO_SOFTAP_PROVISION_START_TIMEOUT_MS)
                                  );
    BROOKESIA_CHECK_FALSE_RETURN(
        provision_start_result, false, "Failed to start provision: %1%", provision_start_result.error()
    );

    // Wait for provision started event with timeout
    auto got_provision_started_event = softap_event_monitor.wait_for(std::vector<service::EventItem> {
        BROOKESIA_DESCRIBE_TO_STR(WifiHelper::SoftApEvent::Started)
    }, WIFI_WAIT_SOFTAP_PROVISION_START_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_RETURN(
        got_provision_started_event, false, "Provision started timeout after %1% seconds",
        WIFI_WAIT_SOFTAP_PROVISION_START_TIMEOUT_MS / 1000
    );
    BROOKESIA_LOGI("SoftAP provision started");

    // Get SoftAP parameters
    WifiHelper::SoftApParams actual_softap_params;
    BROOKESIA_CHECK_FALSE_RETURN(get_wifi_softap_params(actual_softap_params), false, "Failed to get SoftAP params");
    BROOKESIA_LOGI("SoftAP params: %1%", actual_softap_params);
    BROOKESIA_LOGW(
        "\n\nPlease connect to AP '%1% (Password: %2%)' with the mobile phone in %3% seconds\n",
        actual_softap_params.ssid, actual_softap_params.password, WIFI_WAIT_SOFTAP_PROVISION_CONNECT_TIMEOUT_MS / 1000
    );

    // Wait for connected event with timeout
    bool got_connected_event = general_event_monitor.wait_for(std::vector<service::EventItem> {
        BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Connected), false
    }, WIFI_WAIT_SOFTAP_PROVISION_CONNECT_TIMEOUT_MS);
    if (got_connected_event) {
        std::vector<WifiHelper::ConnectApInfo> connected_aps;
        BROOKESIA_CHECK_FALSE_RETURN(get_wifi_connected_aps(connected_aps), false, "Failed to get connected APs");
        BROOKESIA_CHECK_FALSE_RETURN(!connected_aps.empty(), false, "No connected APs");
        BROOKESIA_LOGI("Connected to WiFi AP '%1%' successfully", connected_aps.back().ssid);
    } else {
        BROOKESIA_LOGW("Waiting for connected event timeout");
    }

    // Stop SoftAP provision
    auto provision_stop_result = WifiHelper::call_function_sync(
                                     WifiHelper::FunctionId::TriggerSoftApProvisionStop,
                                     service::helper::Timeout(WIFI_DO_SOFTAP_PROVISION_STOP_TIMEOUT_MS)
                                 );
    BROOKESIA_CHECK_FALSE_RETURN(
        provision_stop_result, false, "Failed to stop provision: %1%", provision_stop_result.error()
    );

    // Wait for provision stopped event with timeout
    auto got_provision_stopped_event = softap_event_monitor.wait_for(std::vector<service::EventItem> {
        BROOKESIA_DESCRIBE_TO_STR(WifiHelper::SoftApEvent::Stopped)
    }, WIFI_WAIT_SOFTAP_PROVISION_STOP_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_RETURN(
        got_provision_stopped_event, false, "Provision stopped timeout after %1% seconds",
        WIFI_WAIT_SOFTAP_PROVISION_STOP_TIMEOUT_MS / 1000
    );
    BROOKESIA_LOGI("SoftAP provision stopped");

    BROOKESIA_LOGI("\n\n--- Demo: WiFi SoftAP Provision Completed ---\n");

    return true;
}

static bool demo_wifi_reset_data()
{
    BROOKESIA_LOGI("\n\n--- Demo: WiFi Reset Data ---\n");

    std::vector<WifiHelper::ConnectApInfo> connected_aps;

    // Get current connected APs
    BROOKESIA_CHECK_FALSE_RETURN(get_wifi_connected_aps(connected_aps), false, "Failed to get connected APs");
    BROOKESIA_LOGI("Connected APs before reset: %1%", connected_aps);

    // Reset all WiFi data (clears NVS stored credentials)
    auto reset_result = WifiHelper::call_function_sync(WifiHelper::FunctionId::ResetData);
    BROOKESIA_CHECK_FALSE_RETURN(reset_result, false, "Failed to reset data: %1%", reset_result.error());
    BROOKESIA_LOGI("WiFi data reset successfully");

    // Verify reset - connected APs should be empty
    BROOKESIA_CHECK_FALSE_RETURN(get_wifi_connected_aps(connected_aps), false, "Failed to get connected APs");
    BROOKESIA_CHECK_FALSE_RETURN(connected_aps.empty(), false, "Connected APs are not empty after reset");

    BROOKESIA_LOGI("\n\n--- Demo: WiFi Reset Data Completed ---\n");

    return true;
}
