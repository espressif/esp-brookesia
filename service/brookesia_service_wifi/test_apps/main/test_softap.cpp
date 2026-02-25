/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "boost/json.hpp"
#include "boost/thread.hpp"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "general.hpp"

using namespace esp_brookesia;
using WifiHelpler = service::helper::Wifi;

constexpr size_t RAPID_SOFTAP_SWITCH_CYCLES = 10;
constexpr size_t RAPID_SOFTAP_PROVISION_SWITCH_CYCLES = 10;
constexpr size_t CONCURRENT_CONNECT_DISCONNECT_CYCLES = 10;
constexpr size_t CONCURRENT_SOFTAP_START_STOP_CYCLES = 10;

TEST_CASE("Test ServiceWifi - rapid softap start and stop", "[service][wifi][softap][rapid]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_softap_rapid");
    BROOKESIA_LOGI("=== Test ServiceWifi - rapid softap start and stop ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    auto service_name = WifiHelpler::get_name().data();

    // Clear storage
    service::LocalTestRunner init_runner;
    std::vector<service::LocalTestItem> init_items = {
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
    };
    bool init_passed = init_runner.run_tests(service_name, init_items);
    TEST_ASSERT_TRUE_MESSAGE(init_passed, "Failed to Clear storage");

    // Rapidly switch SoftAP on and off
    service::LocalTestRunner runner;
    std::vector<service::LocalTestItem> test_items;

    for (size_t i = 0; i < RAPID_SOFTAP_SWITCH_CYCLES; i++) {
        // Start SoftAP
        test_items.push_back(service::LocalTestItem{
            .name = "Start SoftAP cycle " + std::to_string(i + 1),
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerSoftApStart),
            .call_timeout_ms = TEST_SOFTAP_START_TIMEOUT_MS,
            .run_duration_ms = TEST_SOFTAP_START_DURATION_MS
        });

        // Stop SoftAP
        test_items.push_back(service::LocalTestItem{
            .name = "Stop SoftAP cycle " + std::to_string(i + 1),
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerSoftApStop),
            .call_timeout_ms = TEST_SOFTAP_STOP_TIMEOUT_MS,
            .run_duration_ms = TEST_SOFTAP_STOP_DURATION_MS
        });
    }

    bool all_passed = runner.run_tests(service_name, test_items);
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all rapid SoftAP switch tests passed");

    // Wait for SoftAP events
    constexpr uint32_t timeout_ms = RAPID_SOFTAP_SWITCH_CYCLES *
                                    (TEST_SOFTAP_START_DURATION_MS + TEST_SOFTAP_STOP_DURATION_MS);
    bool events_received = collector.wait_for_softap_events(RAPID_SOFTAP_SWITCH_CYCLES * 2, timeout_ms);
    TEST_ASSERT_TRUE_MESSAGE(events_received, "Not all SoftAP events received");

    // Verify SoftAP events
    std::lock_guard<std::mutex> lock(collector.mutex);
    size_t started_count = collector.get_softap_event_count(WifiHelpler::SoftApEvent::Started);
    size_t stopped_count = collector.get_softap_event_count(WifiHelpler::SoftApEvent::Stopped);
    BROOKESIA_LOGI("SoftAP Started events: %zu, Stopped events: %zu", started_count, stopped_count);
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(RAPID_SOFTAP_SWITCH_CYCLES, started_count, "Not enough SoftAP Started events");
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(RAPID_SOFTAP_SWITCH_CYCLES, stopped_count, "Not enough SoftAP Stopped events");
}

TEST_CASE("Test ServiceWifi - rapid softap provision start and stop", "[service][wifi][softap][provision][rapid]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_softap_provision_rapid");
    BROOKESIA_LOGI("=== Test ServiceWifi - rapid softap provision start and stop ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    auto service_name = WifiHelpler::get_name().data();

    // Clear storage
    service::LocalTestRunner init_runner;
    std::vector<service::LocalTestItem> init_items = {
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
    };
    bool init_passed = init_runner.run_tests(service_name, init_items);
    TEST_ASSERT_TRUE_MESSAGE(init_passed, "Failed to Clear storage");

    // Rapidly switch SoftAP Provision on and off
    service::LocalTestRunner runner;
    std::vector<service::LocalTestItem> test_items;

    for (size_t i = 0; i < RAPID_SOFTAP_PROVISION_SWITCH_CYCLES; i++) {
        // Start SoftAP Provision
        test_items.push_back(service::LocalTestItem{
            .name = "Start SoftAP Provision cycle " + std::to_string(i + 1),
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerSoftApProvisionStart),
            .call_timeout_ms = TEST_SOFTAP_PROVISION_START_TIMEOUT_MS,
            .run_duration_ms = TEST_SOFTAP_PROVISION_START_DURATION_MS
        });

        // Stop SoftAP Provision
        test_items.push_back(service::LocalTestItem{
            .name = "Stop SoftAP Provision cycle " + std::to_string(i + 1),
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerSoftApProvisionStop),
            .call_timeout_ms = TEST_SOFTAP_PROVISION_STOP_TIMEOUT_MS,
            .run_duration_ms = TEST_SOFTAP_PROVISION_STOP_DURATION_MS
        });
    }

    bool all_passed = runner.run_tests(service_name, test_items);
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all rapid SoftAP Provision switch tests passed");
}

#ifdef TEST_WIFI_SSID1
TEST_CASE("Test ServiceWifi - softap and connect coexistence", "[service][wifi][softap][connect][coexistence]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_softap_connect_coexistence");
    BROOKESIA_LOGI("=== Test ServiceWifi - softap and connect coexistence ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    auto service_name = WifiHelpler::get_name().data();

    // Clear storage
    service::LocalTestRunner init_runner;
    std::vector<service::LocalTestItem> init_items = {
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        service::LocalTestItem{
            .name = "Init WiFi",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }},
            .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
        },
        service::LocalTestItem{
            .name = "Start WiFi",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Start)
                }},
            .run_duration_ms = TEST_WIFI_START_DURATION_MS
        }
    };
    bool init_passed = init_runner.run_tests(service_name, init_items);
    TEST_ASSERT_TRUE_MESSAGE(init_passed, "Failed to initialize WiFi");

    // Set connect AP info for connect/disconnect thread
    service::LocalTestRunner setup_runner;
    std::vector<service::LocalTestItem> setup_items = {
        service::LocalTestItem{
            .name = "Set connect AP to '" TEST_WIFI_SSID1 "'",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), TEST_WIFI_SSID1},
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), TEST_WIFI_PASSWORD1}
            }
        }
    };
    bool setup_passed = setup_runner.run_tests(service_name, setup_items);
    TEST_ASSERT_TRUE_MESSAGE(setup_passed, "Failed to set connect AP");

    // Clear collector before starting concurrent operations
    collector.clear();

    BROOKESIA_THREAD_CONFIG_GUARD({
        .stack_size = 8 * 1024,
    });

    // Thread 1: Repeatedly connect/disconnect
    std::atomic<bool> connect_thread_error(false);
    boost::thread connect_thread([service_name, &connect_thread_error]() {
        BROOKESIA_LOGI("Connect/Disconnect thread started");
        for (size_t i = 0; i < CONCURRENT_CONNECT_DISCONNECT_CYCLES; i++) {
            // Connect
            service::LocalTestRunner connect_runner;
            std::vector<service::LocalTestItem> connect_items = {
                service::LocalTestItem{
                    .name = "Connect cycle " + std::to_string(i + 1),
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                    .params = boost::json::object{{
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
                        }},
                    .run_duration_ms = TEST_WIFI_CONNECT_DURATION_MS
                }
            };
            bool connect_passed = connect_runner.run_tests(service_name, connect_items);
            if (!connect_passed) {
                BROOKESIA_LOGE("Connect cycle %zu failed", i + 1);
                connect_thread_error = true;
                break;
            }

            boost::this_thread::sleep_for(boost::chrono::milliseconds(500));

            // Disconnect
            service::LocalTestRunner disconnect_runner;
            std::vector<service::LocalTestItem> disconnect_items = {
                service::LocalTestItem{
                    .name = "Disconnect cycle " + std::to_string(i + 1),
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                    .params = boost::json::object{{
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Disconnect)
                        }},
                }
            };
            bool disconnect_passed = disconnect_runner.run_tests(service_name, disconnect_items);
            if (!disconnect_passed) {
                BROOKESIA_LOGE("Disconnect cycle %zu failed", i + 1);
                connect_thread_error = true;
                break;
            }

            boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
        }
        BROOKESIA_LOGI("Connect/Disconnect thread finished");
    });

    // Thread 2: Repeatedly start/stop SoftAP
    std::atomic<bool> softap_thread_error(false);
    boost::thread softap_thread([service_name, &softap_thread_error]() {
        BROOKESIA_LOGI("SoftAP Start/Stop thread started");
        for (size_t i = 0; i < CONCURRENT_SOFTAP_START_STOP_CYCLES; i++) {
            // Start SoftAP
            service::LocalTestRunner start_runner;
            std::vector<service::LocalTestItem> start_items = {
                service::LocalTestItem{
                    .name = "Start SoftAP cycle " + std::to_string(i + 1),
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerSoftApStart),
                    .call_timeout_ms = TEST_SOFTAP_START_TIMEOUT_MS,
                    .run_duration_ms = TEST_SOFTAP_START_DURATION_MS
                }
            };
            bool start_passed = start_runner.run_tests(service_name, start_items);
            if (!start_passed) {
                BROOKESIA_LOGE("Start SoftAP cycle %zu failed", i + 1);
                softap_thread_error = true;
                break;
            }

            boost::this_thread::sleep_for(boost::chrono::milliseconds(500));

            // Stop SoftAP
            service::LocalTestRunner stop_runner;
            std::vector<service::LocalTestItem> stop_items = {
                service::LocalTestItem{
                    .name = "Stop SoftAP cycle " + std::to_string(i + 1),
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerSoftApStop),
                    .call_timeout_ms = TEST_SOFTAP_STOP_TIMEOUT_MS,
                    .run_duration_ms = TEST_SOFTAP_STOP_DURATION_MS
                }
            };
            bool stop_passed = stop_runner.run_tests(service_name, stop_items);
            if (!stop_passed) {
                BROOKESIA_LOGE("Stop SoftAP cycle %zu failed", i + 1);
                softap_thread_error = true;
                break;
            }

            boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
        }
        BROOKESIA_LOGI("SoftAP Start/Stop thread finished");
    });

    // Wait for both threads to complete
    connect_thread.join();
    softap_thread.join();

    // Verify no errors occurred
    TEST_ASSERT_FALSE_MESSAGE(connect_thread_error.load(), "Connect/Disconnect thread encountered errors");
    TEST_ASSERT_FALSE_MESSAGE(softap_thread_error.load(), "SoftAP Start/Stop thread encountered errors");

    // Verify events were received
    std::lock_guard<std::mutex> lock(collector.mutex);
    size_t connected_count = collector.get_general_event_count(WifiHelpler::GeneralEvent::Connected);
    size_t disconnected_count = collector.get_general_event_count(WifiHelpler::GeneralEvent::Disconnected);
    size_t softap_started_count = collector.get_softap_event_count(WifiHelpler::SoftApEvent::Started);
    size_t softap_stopped_count = collector.get_softap_event_count(WifiHelpler::SoftApEvent::Stopped);

    BROOKESIA_LOGI("Event statistics: Connected=%zu, Disconnected=%zu, SoftAP Started=%zu, SoftAP Stopped=%zu",
                   connected_count, disconnected_count, softap_started_count, softap_stopped_count);

    // Verify we received events from both threads
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(CONCURRENT_CONNECT_DISCONNECT_CYCLES, connected_count,
                                         "Not enough Connected events");
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(CONCURRENT_CONNECT_DISCONNECT_CYCLES, disconnected_count,
                                         "Not enough Disconnected events");
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(CONCURRENT_SOFTAP_START_STOP_CYCLES, softap_started_count,
                                         "Not enough SoftAP Started events");
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(CONCURRENT_SOFTAP_START_STOP_CYCLES, softap_stopped_count,
                                         "Not enough SoftAP Stopped events");
}

TEST_CASE("Test ServiceWifi - connect then softap provision triggers disconnect", "[service][wifi][softap][provision][connect][disconnect]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_connect_then_softap_provision_disconnect");
    BROOKESIA_LOGI("=== Test ServiceWifi - connect then softap provision triggers disconnect ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    auto service_name = WifiHelpler::get_name().data();

    // Clear storage
    service::LocalTestRunner init_runner;
    std::vector<service::LocalTestItem> init_items = {
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
    };
    bool init_passed = init_runner.run_tests(service_name, init_items);
    TEST_ASSERT_TRUE_MESSAGE(init_passed, "Failed to Clear storage");

    // Step 1: Connect to TEST_WIFI_SSID1
    service::LocalTestRunner connect_runner;
    std::vector<service::LocalTestItem> connect_items = {
        service::LocalTestItem{
            .name = "Set connect AP to '" TEST_WIFI_SSID1 "'",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), TEST_WIFI_SSID1},
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), TEST_WIFI_PASSWORD1}
            }
        },
        service::LocalTestItem{
            .name = "Trigger connect action",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
                }},
            .run_duration_ms = TEST_WIFI_CONNECT_DURATION_MS
        }
    };
    bool connect_passed = connect_runner.run_tests(service_name, connect_items);
    TEST_ASSERT_TRUE_MESSAGE(connect_passed, "Failed to connect to TEST_WIFI_SSID1");

    // Wait for Connected event
    bool connected = collector.wait_for_general_events(1, TEST_WIFI_CONNECT_DURATION_MS, WifiHelpler::GeneralEvent::Connected);
    TEST_ASSERT_TRUE_MESSAGE(connected, "Failed to receive Connected event");

    // Clear collector to start fresh counting for disconnect event
    collector.clear();

    // Step 2: Start SoftAP Provision (should trigger disconnect)
    service::LocalTestRunner provision_runner;
    std::vector<service::LocalTestItem> provision_items = {
        service::LocalTestItem{
            .name = "Start SoftAP Provision while connected",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerSoftApProvisionStart),
            .call_timeout_ms = TEST_SOFTAP_PROVISION_START_TIMEOUT_MS,
            .run_duration_ms = TEST_SOFTAP_PROVISION_START_DURATION_MS
        }
    };
    bool provision_passed = provision_runner.run_tests(service_name, provision_items);
    TEST_ASSERT_TRUE_MESSAGE(provision_passed, "Failed to start SoftAP Provision");

    // Wait for Disconnected event (should be triggered when SoftAP Provision starts)
    bool disconnected = collector.wait_for_general_events(1, TEST_SOFTAP_PROVISION_START_DURATION_MS, WifiHelpler::GeneralEvent::Disconnected);
    TEST_ASSERT_TRUE_MESSAGE(disconnected, "Failed to receive Disconnected event when SoftAP Provision started");

    // Verify Disconnected event was received
    {
        std::lock_guard<std::mutex> lock(collector.mutex);
        size_t disconnected_count = collector.get_general_event_count(WifiHelpler::GeneralEvent::Disconnected);
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(1, disconnected_count, "Should receive at least one Disconnected event");
    }

    // Step 3: Stop SoftAP Provision
    service::LocalTestRunner stop_provision_runner;
    std::vector<service::LocalTestItem> stop_provision_items = {
        service::LocalTestItem{
            .name = "Stop SoftAP Provision",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerSoftApProvisionStop),
            .call_timeout_ms = TEST_SOFTAP_PROVISION_STOP_TIMEOUT_MS,
            .run_duration_ms = TEST_SOFTAP_PROVISION_STOP_DURATION_MS
        }
    };
    bool stop_provision_passed = stop_provision_runner.run_tests(service_name, stop_provision_items);
    TEST_ASSERT_TRUE_MESSAGE(stop_provision_passed, "Failed to stop SoftAP Provision");

    // Disconnect all connections
    {
        std::lock_guard<std::mutex> lock(collector.mutex);
        collector.connections.clear();
    }
}
#endif // TEST_WIFI_SSID1
