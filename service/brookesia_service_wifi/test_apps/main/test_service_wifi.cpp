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
#include <future>
#include <random>
#include "general.hpp"

using namespace esp_brookesia;
using WifiHelpler = service::helper::Wifi;

constexpr size_t RAPID_CONNECT_AND_DISCONNECT_COUNT = 10;
constexpr size_t RAPID_SWITCH_AP_CYCLES = 10; // Switch between SSID1 and SSID2
constexpr size_t RAPID_SCAN_CYCLES = 10;
constexpr size_t TRANSITION_CYCLES = 20;
constexpr size_t NUM_THREADS = 4;
constexpr size_t TESTS_PER_THREAD = 10; // Number of test items per thread
constexpr size_t RANDOM_OPERATIONS_COUNT = 50;

TEST_CASE("Test ServiceWifi - state transitions", "[service][wifi][state]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_state");
    BROOKESIA_LOGI("=== Test ServiceWifi - state transitions ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    auto service_name = WifiHelpler::get_name().data();
    // Test state transitions: Inited -> Started -> Connected -> Started -> Inited
    const std::vector<service::LocalTestItem> test_items = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        // Init from Deinited to Inited
        service::LocalTestItem{
            .name = "State transition: Init (Deinited -> Inited)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }},
            .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
        },
        // Start from Inited to Started
        service::LocalTestItem{
            .name = "State transition: Start (Inited -> Started)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Start)
                }},
            .run_duration_ms = TEST_WIFI_START_DURATION_MS
        },
        // Stop from Started to Inited
        service::LocalTestItem{
            .name = "State transition: Stop (Started -> Inited)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Stop)
                }}
        },
        // Deinit from Inited to Deinited
        service::LocalTestItem{
            .name = "State transition: Deinit (Inited -> Deinited)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Deinit)
                }}
        }
    };

    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(service_name, test_items);
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all state transition tests passed");

    // Wait for all events
    constexpr uint32_t timeout_ms =
        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_INITED_TIMEOUT_MS +
        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_STARTED_TIMEOUT_MS +
        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_STOPPED_TIMEOUT_MS +
        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_DEINITED_TIMEOUT_MS;
    bool actions_received = collector.wait_for_general_actions(4, timeout_ms);
    bool events_received = collector.wait_for_general_events(4, timeout_ms);
    TEST_ASSERT_TRUE_MESSAGE(actions_received, "Not all general action events received");
    TEST_ASSERT_TRUE_MESSAGE(events_received, "Not all general event events received");

    // Verify general actions triggered
    std::lock_guard<std::mutex> lock(collector.mutex);
    size_t action_index = 0;

    // Verify first action: Init (triggered during service startup)
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init).c_str(),
        collector.general_actions[action_index].action.c_str(), "First action mismatch (should be Init)"
    );
    action_index++;

    // Verify second action: Start
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Start).c_str(),
        collector.general_actions[action_index].action.c_str(), "Second action mismatch"
    );
    action_index++;

    // Verify fourth action: Stop
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Stop).c_str(),
        collector.general_actions[action_index].action.c_str(), "Fourth action mismatch"
    );
    action_index++;

    // Verify fifth action: Deinit
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Deinit).c_str(),
        collector.general_actions[action_index].action.c_str(), "Sixth action mismatch"
    );
    action_index++;

    // Verify general events happened (state changes)
    // Expected events: Inited (from Init), Started, Inited, Started, Inited
    std::vector<std::string> expected_events;
    expected_events = {
        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralEvent::Inited),
        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralEvent::Started),
        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralEvent::Stopped),
        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralEvent::Deinited)
    };

    TEST_ASSERT_EQUAL_MESSAGE(expected_events.size(), collector.general_events.size(), "Event count mismatch");
    for (size_t i = 0; i < expected_events.size() && i < collector.general_events.size(); i++) {
        TEST_ASSERT_EQUAL_STRING_MESSAGE(
            expected_events[i].c_str(),
            collector.general_events[i].event.c_str(),
            ("Event " + std::to_string(i) + " mismatch").c_str()
        );
    }

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(results[i], (std::string("Test failed: ") + test_items[i].name).c_str());
    }

    // Connections will be automatically disconnected when they go out of scope
}

TEST_CASE("Test ServiceWifi - scan functionality", "[service][wifi][scan]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_scan");
    BROOKESIA_LOGI("=== Test ServiceWifi - scan functionality ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    std::vector<service::LocalTestItem> test_items = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        // Init from Deinited to Inited
        service::LocalTestItem{
            .name = "State transition: Init (Deinited -> Inited)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }},
            .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
        },
        // Start scan with default parameters
        // Will automatically start WiFi if not started
        service::LocalTestItem{
            .name = "Start scan with default parameters",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStart),
            .run_duration_ms = TEST_WIFI_SCAN_DURATION_MS
        },
        // Stop scan
        service::LocalTestItem{
            .name = "Stop scan",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStop),
        },
        // Set scan parameters
        service::LocalTestItem{
            .name = "Set scan parameters",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetScanParams),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetScanParamsParam::Param),
                    BROOKESIA_DESCRIBE_TO_JSON(WifiHelpler::ScanParams({
                        .ap_count = 5,
                        .interval_ms = 1000,
                        .timeout_ms = 5000
                    })).as_object()
                }},
            .call_timeout_ms = TEST_WIFI_SET_SCAN_PARAMS_TIMEOUT_MS,
        },
        // Start scan with custom parameters
        service::LocalTestItem{
            .name = "Start scan with custom parameters",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStart),
            .run_duration_ms = TEST_WIFI_SCAN_DURATION_MS
        },
        // Stop scan again
        service::LocalTestItem{
            .name = "Stop scan again",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStop),
        },
        // Stop WiFi
        service::LocalTestItem{
            .name = "Stop WiFi after scan tests",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{
                {
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Stop)
                }
            }
        }
    };

    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(WifiHelpler::get_name().data(), test_items);

    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all scan tests passed");

    // Wait for scan infos updated events (scan may complete naturally or be stopped)
    boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));

    // Verify scan_ap_infos_updated events were received (at least one if scan completed)
    std::lock_guard<std::mutex> lock(collector.mutex);
    BROOKESIA_LOGI("Received %zu scan_ap_infos_updated events", collector.scan_ap_infos_updated.size());

    // Verify scan_ap_infos_updated event structure if any were received
    for (size_t i = 0; i < collector.scan_ap_infos_updated.size(); i++) {
        const auto &scan_event = collector.scan_ap_infos_updated[i];
        // Verify each AP info structure (already parsed by BROOKESIA_DESCRIBE_FROM_JSON in event handler)
        for (const auto &ap_info : scan_event.ap_infos) {
            // Verify parsed fields are valid
            TEST_ASSERT_TRUE_MESSAGE(
                !ap_info.ssid.empty(),
                "AP info ssid is empty"
            );
            // signal_level enum is already validated during parsing in event handler
        }
    }

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(results[i], (std::string("Test failed: ") + test_items[i].name).c_str());
    }

    // Connections will be automatically disconnected when they go out of scope
}

TEST_CASE("Test ServiceWifi - set connect AP", "[service][wifi][connect]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_connect");
    BROOKESIA_LOGI("=== Test ServiceWifi - set connect AP ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    static auto wifi_functions = WifiHelpler::get_function_schemas();
    std::vector<service::LocalTestItem> test_items = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        // Init from Deinited to Inited
        service::LocalTestItem{
            .name = "State transition: Init (Deinited -> Inited)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }},
            .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
        },
        // Set AP with password
        service::LocalTestItem{
            .name = "Set connect AP with password",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), "TestAP"},
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), "TestPassword123"}
            },
        },
        // Set AP without password (open network)
        service::LocalTestItem{
            .name = "Set connect AP without password",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), "OpenAP"},
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), ""}
            }
        },
        // Set AP with empty password explicitly
        service::LocalTestItem{
            .name = "Set connect AP with empty password",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), "AnotherAP"}
            }
        },
        // Set AP with long SSID
        service::LocalTestItem{
            .name = "Set connect AP with long SSID",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), "VeryLongSSIDNameThatExceedsNormalLength"},
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), "LongPassword123456789"}
            }
        }
    };

    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(WifiHelpler::get_name().data(), test_items);

    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all set connect AP tests passed");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(results[i], (std::string("Test failed: ") + test_items[i].name).c_str());
    }
}

#if defined(TEST_WIFI_SSID1) && defined(TEST_WIFI_PASSWORD1)
TEST_CASE("Test ServiceWifi - connect and manual disconnect (no auto-reconnect)", "[service][wifi][connect][scenario1]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_connect_scenario1");
    BROOKESIA_LOGI("=== Test ServiceWifi - connect and manual disconnect (no auto-reconnect) ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    // Start WiFi
    service::LocalTestRunner runner1;
    std::vector<service::LocalTestItem> test_items1 = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        // Init from Deinited to Inited
        service::LocalTestItem{
            .name = "State transition: Init (Deinited -> Inited)",
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
                }}
        },
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
        },
        service::LocalTestItem{
            .name = "Get connected APs",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
            .validator = [](const service::FunctionValue & value)
            {
                auto array_ptr = std::get_if<boost::json::array>(&value);
                BROOKESIA_CHECK_NULL_RETURN(array_ptr, false, "array_ptr is NULL");

                std::vector<WifiHelpler::ConnectApInfo> connect_ap_infos;
                auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(*array_ptr, connect_ap_infos);
                BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse connect AP infos");

                for (const auto &connect_ap_info : connect_ap_infos) {
                    if (connect_ap_info.ssid == TEST_WIFI_SSID1) {
                        return true;
                    }
                }
                return false;
            }
        }
    };

    bool all_passed1 = runner1.run_tests(WifiHelpler::get_name().data(), test_items1);
    TEST_ASSERT_TRUE_MESSAGE(all_passed1, "Failed to setup connection");

    // Wait for Connected event
    bool connected = collector.wait_for_general_events(1, TEST_WIFI_CONNECT_DURATION_MS);
    TEST_ASSERT_TRUE_MESSAGE(connected, "Failed to connect to TEST_WIFI_SSID1");

    // Verify Connected event
    {
        std::lock_guard<std::mutex> lock(collector.mutex);
        TEST_ASSERT_TRUE_MESSAGE(
            !collector.general_events.empty() &&
            collector.general_events.back().event == "Connected",
            "Connected event not received"
        );
    }

    // Manually disconnect
    collector.clear();
    service::LocalTestRunner runner2;
    std::vector<service::LocalTestItem> test_items2 = {
        service::LocalTestItem{
            .name = "Manually disconnect",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Disconnect)
                }}
        }
    };

    bool all_passed2 = runner2.run_tests(WifiHelpler::get_name().data(), test_items2);
    TEST_ASSERT_TRUE_MESSAGE(all_passed2, "Failed to disconnect");

    // Wait for Disconnected event
    bool disconnected = collector.wait_for_general_events(1, 2000);
    TEST_ASSERT_TRUE_MESSAGE(disconnected, "Failed to disconnect");

    // Verify no auto-reconnect (wait for a period and check no Connected event)
    boost::this_thread::sleep_for(boost::chrono::milliseconds(3000));
    {
        std::lock_guard<std::mutex> lock(collector.mutex);
        size_t connected_count = 0;
        for (const auto &evt : collector.general_events) {
            if (evt.event == "Connected") {
                connected_count++;
            }
        }
        TEST_ASSERT_EQUAL_MESSAGE(
            0, connected_count,
            "Auto-reconnect occurred after manual disconnect"
        );
    }

    // Connections will be automatically disconnected when they go out of scope
}

TEST_CASE("Test ServiceWifi - stop and start with auto-reconnect", "[service][wifi][connect][scenario2]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_connect_scenario2");
    BROOKESIA_LOGI("=== Test ServiceWifi - stop and start with auto-reconnect ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    // Set connect AP to TEST_WIFI_SSID1
    service::LocalTestRunner runner1;
    std::vector<service::LocalTestItem> test_items1 = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        // Init from Deinited to Inited
        service::LocalTestItem{
            .name = "State transition: Init (Deinited -> Inited)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }},
            .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
        },
        service::LocalTestItem{
            .name = "Set connect AP to TEST_WIFI_SSID1",
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

    bool all_passed1 = runner1.run_tests(WifiHelpler::get_name().data(), test_items1);
    TEST_ASSERT_TRUE_MESSAGE(all_passed1, "Failed to setup connection");

    // Wait for Connected event
    bool connected = collector.wait_for_general_events(1, TEST_WIFI_CONNECT_DURATION_MS);
    TEST_ASSERT_TRUE_MESSAGE(connected, "Failed to connect to TEST_WIFI_SSID1");

    // Stop WiFi
    collector.clear();
    service::LocalTestRunner runner2;
    std::vector<service::LocalTestItem> test_items2 = {
        service::LocalTestItem{
            .name = "Stop WiFi",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Stop)
                }}
        }
    };

    bool all_passed2 = runner2.run_tests(WifiHelpler::get_name().data(), test_items2);
    TEST_ASSERT_TRUE_MESSAGE(all_passed2, "Failed to stop WiFi");

    // Wait for Stopped event
    bool stopped = collector.wait_for_general_events(1, 2000);
    TEST_ASSERT_TRUE_MESSAGE(stopped, "Failed to stop WiFi");

    // Start WiFi again
    collector.clear();
    service::LocalTestRunner runner3;
    std::vector<service::LocalTestItem> test_items3 = {
        service::LocalTestItem{
            .name = "Start WiFi again",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Start)
                }}
        }
    };

    bool all_passed3 = runner3.run_tests(WifiHelpler::get_name().data(), test_items3);
    TEST_ASSERT_TRUE_MESSAGE(all_passed3, "Failed to start WiFi again");

    // Wait for Started event
    bool started = collector.wait_for_general_events(1, 2000);
    TEST_ASSERT_TRUE_MESSAGE(started, "Failed to start WiFi");

    // Wait for auto-reconnect (Connected event)
    bool auto_connected = collector.wait_for_general_events(2, TEST_WIFI_CONNECT_DURATION_MS);
    TEST_ASSERT_TRUE_MESSAGE(auto_connected, "Failed to auto-reconnect");

    // Verify Connected event and verify connected to TEST_WIFI_SSID1
    {
        std::lock_guard<std::mutex> lock(collector.mutex);
        bool found_connected = false;
        for (const auto &evt : collector.general_events) {
            if (evt.event == "Connected") {
                found_connected = true;
                break;
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(found_connected, "Auto-reconnect Connected event not received");

        // Verify connected AP is TEST_WIFI_SSID1
        service::LocalTestRunner runner4;
        std::vector<service::LocalTestItem> test_items4 = {
            service::LocalTestItem{
                .name = "Get connected APs",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
                .validator = [](const service::FunctionValue & value)
                {
                    auto array_ptr = std::get_if<boost::json::array>(&value);
                    BROOKESIA_CHECK_NULL_RETURN(array_ptr, false, "array_ptr is NULL");

                    std::vector<WifiHelpler::ConnectApInfo> connect_ap_infos;
                    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(*array_ptr, connect_ap_infos);
                    BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse connect AP infos");

                    for (const auto &connect_ap_info : connect_ap_infos) {
                        if (connect_ap_info.ssid == TEST_WIFI_SSID1) {
                            return true;
                        }
                    }
                    return false;
                }
            }
        };

        bool all_passed4 = runner4.run_tests(WifiHelpler::get_name().data(), test_items4);
        TEST_ASSERT_TRUE_MESSAGE(all_passed4, "Failed to verify connected AP");
    }

    // Connections will be automatically disconnected when they go out of scope
}

TEST_CASE("Test ServiceWifi - rapid connect and disconnect", "[service][wifi][connect][disconnect][rapid]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_connect_disconnect_rapid");
    BROOKESIA_LOGI("=== Test ServiceWifi - rapid connect and disconnect ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    service::LocalTestRunner runner1;
    std::vector<service::LocalTestItem> test_items1 = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        // Init from Deinited to Inited
        service::LocalTestItem{
            .name = "State transition: Init (Deinited -> Inited)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }},
            .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
        },
        service::LocalTestItem{
            .name = "Set connect AP to '" TEST_WIFI_SSID1 "'",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), TEST_WIFI_SSID1},
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), TEST_WIFI_PASSWORD1}
            }
        },
    };
    for (size_t i = 0; i < RAPID_CONNECT_AND_DISCONNECT_COUNT; i++) {
        test_items1.push_back(service::LocalTestItem{
            .name = "Trigger connect action",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
                }},
            .run_duration_ms = TEST_WIFI_CONNECT_DURATION_MS
        });
        test_items1.push_back(service::LocalTestItem{
            .name = "Trigger disconnect action",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Disconnect)
                }},
        });
    }

    bool all_passed1 = runner1.run_tests(WifiHelpler::get_name().data(), test_items1);
    TEST_ASSERT_TRUE_MESSAGE(all_passed1, "Failed to rapid connect and disconnect");

    // Calculate total test duration and wait for all events
    // Each connect/disconnect cycle: connect (TEST_WIFI_CONNECT_DURATION_MS) + disconnect (default 200ms)
    uint32_t total_test_duration = RAPID_CONNECT_AND_DISCONNECT_COUNT * (TEST_WIFI_CONNECT_DURATION_MS + 200);
    uint32_t wait_timeout = total_test_duration + 2000; // Add extra buffer time

    BROOKESIA_LOGI("Waiting for all connect/disconnect events (timeout: %u ms)", wait_timeout);

    // Wait for events to accumulate (expect at least some events from rapid operations)
    // Due to rapid operations, not all may complete, but we should get a reasonable number
    constexpr size_t MIN_EXPECTED_EVENTS = RAPID_CONNECT_AND_DISCONNECT_COUNT / 2; // At least half should produce events
    bool events_received = collector.wait_for_general_events(MIN_EXPECTED_EVENTS, wait_timeout);

    // Give additional time for any delayed events
    boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));

    // Verify event counts
    size_t connected_count = 0;
    size_t disconnected_count = 0;
    std::vector<std::string> event_sequence;

    {
        std::lock_guard<std::mutex> lock(collector.mutex);

        for (const auto &evt : collector.general_events) {
            if (evt.event == "Connected") {
                connected_count++;
                event_sequence.push_back("Connected");
            } else if (evt.event == "Disconnected") {
                disconnected_count++;
                event_sequence.push_back("Disconnected");
            }
        }

        BROOKESIA_LOGI("Event statistics: Connected=%zu, Disconnected=%zu, Total events=%zu",
                       connected_count, disconnected_count, collector.general_events.size());

        // Build event sequence string
        std::string seq_str;
        for (size_t i = 0; i < event_sequence.size(); i++) {
            if (i > 0) {
                seq_str += " -> ";
            }
            seq_str += event_sequence[i];
        }
        BROOKESIA_LOGI("Event sequence: %s", seq_str.empty() ? "(none)" : seq_str.c_str());
    }

    // Verify we received some events
    TEST_ASSERT_TRUE_MESSAGE(
        events_received || (connected_count > 0 || disconnected_count > 0),
        "No connect/disconnect events received during rapid operations"
    );

    // Verify event counts are reasonable
    // Due to rapid operations, not all operations may complete, but we should have some events
    TEST_ASSERT_TRUE_MESSAGE(
        connected_count > 0 || disconnected_count > 0,
        "No connect or disconnect events received"
    );

    // Verify event balance (connected and disconnected should be roughly balanced)
    // Allow some imbalance due to rapid operations, but difference should not be too large
    size_t event_diff = (connected_count > disconnected_count) ?
                        (connected_count - disconnected_count) :
                        (disconnected_count - connected_count);
    size_t total_events = connected_count + disconnected_count;

    if (total_events > 0) {
        // Difference should not exceed 50% of total events
        size_t max_allowed_diff = total_events / 2;
        TEST_ASSERT_TRUE_MESSAGE(
            event_diff <= max_allowed_diff,
            "Connect/disconnect events are too imbalanced"
        );
    }

    // Verify event sequence is reasonable (should start with Connected if any events)
    if (!event_sequence.empty()) {
        // First event should be Connected (if we have any events)
        // Last event could be either, but typically Disconnected after rapid operations
        BROOKESIA_LOGI("First event: %s, Last event: %s",
                       event_sequence.front().c_str(),
                       event_sequence.back().c_str());
    }

    BROOKESIA_LOGI("Rapid connect/disconnect test completed: %zu connects, %zu disconnects",
                   connected_count, disconnected_count);
}

TEST_CASE("Test ServiceWifi - connect to non-existent SSID and verify auto-reconnect", "[service][wifi][connect][scenario4]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_connect_scenario4");
    BROOKESIA_LOGI("=== Test ServiceWifi - connect to non-existent SSID and verify auto-reconnect ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    // Connect to TEST_WIFI_SSID1 first
    service::LocalTestRunner runner1;
    std::vector<service::LocalTestItem> test_items1 = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        // Init from Deinited to Inited
        service::LocalTestItem{
            .name = "State transition: Init (Deinited -> Inited)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }},
            .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
        },
        service::LocalTestItem{
            .name = "Set connect AP to TEST_WIFI_SSID1",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), TEST_WIFI_SSID1},
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), TEST_WIFI_PASSWORD1}
            }
        },
        service::LocalTestItem{
            .name = "Trigger connect action to TEST_WIFI_SSID1",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
                }},
            .run_duration_ms = TEST_WIFI_CONNECT_DURATION_MS
        }
    };

    bool all_passed1 = runner1.run_tests(WifiHelpler::get_name().data(), test_items1);
    TEST_ASSERT_TRUE_MESSAGE(all_passed1, "Failed to connect to TEST_WIFI_SSID1");

    // Wait for Connected event
    bool connected1 = collector.wait_for_general_events(1, TEST_WIFI_CONNECT_DURATION_MS);
    TEST_ASSERT_TRUE_MESSAGE(connected1, "Failed to connect to TEST_WIFI_SSID1");

    // Verify connected to TEST_WIFI_SSID1 (so it's saved as last connectable AP)
    {
        service::LocalTestRunner runner_check;
        std::vector<service::LocalTestItem> test_items_check = {
            service::LocalTestItem{
                .name = "Verify connected to TEST_WIFI_SSID1",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
                .validator = [](const service::FunctionValue & value)
                {
                    auto array_ptr = std::get_if<boost::json::array>(&value);
                    BROOKESIA_CHECK_NULL_RETURN(array_ptr, false, "array_ptr is NULL");

                    std::vector<WifiHelpler::ConnectApInfo> connect_ap_infos;
                    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(*array_ptr, connect_ap_infos);
                    BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse connect AP infos");

                    for (const auto &connect_ap_info : connect_ap_infos) {
                        if (connect_ap_info.ssid == TEST_WIFI_SSID1) {
                            return true;
                        }
                    }

                    return false;
                }
            }
        };

        bool all_passed_check = runner_check.run_tests(WifiHelpler::get_name().data(), test_items_check);
        TEST_ASSERT_TRUE_MESSAGE(all_passed_check, "Failed to verify connected to TEST_WIFI_SSID1");
    }

    // Try to connect to non-existent SSID (this will fail and trigger Disconnected event)
    collector.clear();
    const std::string non_existent_ssid = "NonExistentSSID_12345";
    service::LocalTestRunner runner2;
    std::vector<service::LocalTestItem> test_items2 = {
        service::LocalTestItem{
            .name = "Set connect AP to non-existent SSID",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), non_existent_ssid},
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), "password"}
            }
        },
        service::LocalTestItem{
            .name = "Trigger connect action to non-existent SSID",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
                }},
            .run_duration_ms = TEST_WIFI_CONNECT_DURATION_MS
        }
    };

    bool all_passed2 = runner2.run_tests(WifiHelpler::get_name().data(), test_items2);
    TEST_ASSERT_TRUE_MESSAGE(all_passed2, "Failed to attempt connection to non-existent SSID");

    // Wait for Disconnected event (connection will fail)
    // The system should automatically try to reconnect to TEST_WIFI_SSID1
    bool disconnected = collector.wait_for_general_events(1, TEST_WIFI_CONNECT_DURATION_MS);
    TEST_ASSERT_TRUE_MESSAGE(disconnected, "Disconnected event not received");

    // Wait for auto-reconnect to TEST_WIFI_SSID1 (Connected event)
    // The system should automatically reconnect to the last connectable AP (TEST_WIFI_SSID1)
    bool auto_connected = collector.wait_for_general_events(2, TEST_WIFI_CONNECT_DURATION_MS);
    TEST_ASSERT_TRUE_MESSAGE(auto_connected, "Failed to auto-reconnect to TEST_WIFI_SSID1");

    // Verify Connected event and verify connected to TEST_WIFI_SSID1
    {
        std::lock_guard<std::mutex> lock(collector.mutex);
        bool found_connected = false;
        for (const auto &evt : collector.general_events) {
            if (evt.event == "Connected") {
                found_connected = true;
                break;
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(found_connected, "Auto-reconnect Connected event not received");

        // Verify connected AP is TEST_WIFI_SSID1
        service::LocalTestRunner runner4;
        std::vector<service::LocalTestItem> test_items4 = {
            service::LocalTestItem{
                .name = "Get connected APs",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
                .validator = [](const service::FunctionValue & value)
                {
                    auto array_ptr = std::get_if<boost::json::array>(&value);
                    BROOKESIA_CHECK_NULL_RETURN(array_ptr, false, "array_ptr is NULL");

                    std::vector<WifiHelpler::ConnectApInfo> connect_ap_infos;
                    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(*array_ptr, connect_ap_infos);
                    BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse connect AP infos");

                    for (const auto &connect_ap_info : connect_ap_infos) {
                        if (connect_ap_info.ssid == TEST_WIFI_SSID1) {
                            return true;
                        }
                    }

                    return false;
                }
            }
        };

        bool all_passed4 = runner4.run_tests(WifiHelpler::get_name().data(), test_items4);
        TEST_ASSERT_TRUE_MESSAGE(all_passed4, "Failed to verify auto-reconnected to TEST_WIFI_SSID1");
    }

    // Connections will be automatically disconnected when they go out of scope
}

#if defined(TEST_WIFI_SSID2) && defined(TEST_WIFI_PASSWORD2)
TEST_CASE("Test ServiceWifi - switch connection from TEST_WIFI_SSID1 to TEST_WIFI_SSID2", "[service][wifi][connect][scenario3]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_connect_scenario3");
    BROOKESIA_LOGI("=== Test ServiceWifi - switch connection from TEST_WIFI_SSID1 to TEST_WIFI_SSID2 ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    // Connect to TEST_WIFI_SSID1 first
    service::LocalTestRunner runner1;
    std::vector<service::LocalTestItem> test_items1 = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        // Init from Deinited to Inited
        service::LocalTestItem{
            .name = "State transition: Init (Deinited -> Inited)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }},
            .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
        },
        service::LocalTestItem{
            .name = "Set connect AP to TEST_WIFI_SSID1",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), TEST_WIFI_SSID1},
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), TEST_WIFI_PASSWORD1}
            }
        },
        service::LocalTestItem{
            .name = "Trigger connect action to TEST_WIFI_SSID1",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
                }},
            .run_duration_ms = TEST_WIFI_CONNECT_DURATION_MS
        },
        service::LocalTestItem{
            .name = "Get connected APs",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
            .validator = [](const service::FunctionValue & value)
            {
                auto array_ptr = std::get_if<boost::json::array>(&value);
                BROOKESIA_CHECK_NULL_RETURN(array_ptr, false, "array_ptr is NULL");

                std::vector<WifiHelpler::ConnectApInfo> connect_ap_infos;
                auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(*array_ptr, connect_ap_infos);
                BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse connect AP infos");

                for (const auto &connect_ap_info : connect_ap_infos) {
                    BROOKESIA_LOGI("Connect AP info: %1%", BROOKESIA_DESCRIBE_TO_STR(connect_ap_info));
                    if (connect_ap_info.ssid == TEST_WIFI_SSID1) {
                        return true;
                    }
                }

                return false;
            }
        }
    };

    bool all_passed1 = runner1.run_tests(WifiHelpler::get_name().data(), test_items1);
    TEST_ASSERT_TRUE_MESSAGE(all_passed1, "Failed to connect to " TEST_WIFI_SSID1);

    // Wait for Connected event
    bool connected1 = collector.wait_for_general_events(1, TEST_WIFI_CONNECT_DURATION_MS);
    TEST_ASSERT_TRUE_MESSAGE(connected1, "Failed to connect to " TEST_WIFI_SSID1);

    // Switch to TEST_WIFI_SSID2 without disconnecting
    collector.clear();
    service::LocalTestRunner runner2;
    std::vector<service::LocalTestItem> test_items2 = {
        service::LocalTestItem{
            .name = "Set connect AP to TEST_WIFI_SSID2",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), TEST_WIFI_SSID2},
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), TEST_WIFI_PASSWORD2}
            }
        },
        service::LocalTestItem{
            .name = "Trigger connect action to TEST_WIFI_SSID2",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
                }},
            .run_duration_ms = TEST_WIFI_CONNECT_DURATION_MS
        },
        service::LocalTestItem{
            .name = "Get connected APs",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
            .validator = [](const service::FunctionValue & value)
            {
                auto array_ptr = std::get_if<boost::json::array>(&value);
                BROOKESIA_CHECK_NULL_RETURN(array_ptr, false, "array_ptr is NULL");

                std::vector<WifiHelpler::ConnectApInfo> connect_ap_infos;
                auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(*array_ptr, connect_ap_infos);
                BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse connect AP infos");

                for (const auto &connect_ap_info : connect_ap_infos) {
                    if (connect_ap_info.ssid == TEST_WIFI_SSID2) {
                        return true;
                    }
                }

                return false;
            }
        }
    };

    bool all_passed2 = runner2.run_tests(WifiHelpler::get_name().data(), test_items2);
    TEST_ASSERT_TRUE_MESSAGE(all_passed2, "Failed to switch to " TEST_WIFI_SSID2);

    // Wait for Connected event (may have Disconnected first, then Connected)
    bool connected2 = collector.wait_for_general_events(1, TEST_WIFI_CONNECT_DURATION_MS);
    TEST_ASSERT_TRUE_MESSAGE(connected2, "Failed to connect to " TEST_WIFI_SSID2);

    // Verify connected to TEST_WIFI_SSID2
    {
        std::lock_guard<std::mutex> lock(collector.mutex);
        bool found_connected = false;
        for (const auto &evt : collector.general_events) {
            if (evt.event == "Connected") {
                found_connected = true;
                break;
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(found_connected, "Connected event not received for " TEST_WIFI_SSID2);

        // Verify connected AP is TEST_WIFI_SSID2
        service::LocalTestRunner runner3;
        std::vector<service::LocalTestItem> test_items3 = {
            service::LocalTestItem{
                .name = "Get connected APs",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
                .validator = [](const service::FunctionValue & value)
                {
                    auto array_ptr = std::get_if<boost::json::array>(&value);
                    BROOKESIA_CHECK_NULL_RETURN(array_ptr, false, "array_ptr is NULL");

                    std::vector<WifiHelpler::ConnectApInfo> connect_ap_infos;
                    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(*array_ptr, connect_ap_infos);
                    BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse connect AP infos");

                    for (const auto &connect_ap_info : connect_ap_infos) {
                        if (connect_ap_info.ssid == TEST_WIFI_SSID2) {
                            return true;
                        }
                    }

                    return false;
                }
            }
        };

        bool all_passed3 = runner3.run_tests(WifiHelpler::get_name().data(), test_items3);
        TEST_ASSERT_TRUE_MESSAGE(all_passed3, "Failed to verify connected to " TEST_WIFI_SSID2);
    }

    // Connections will be automatically disconnected when they go out of scope
}

TEST_CASE("Test ServiceWifi - repeatedly switch between TEST_WIFI_SSID1 and TEST_WIFI_SSID2", "[service][wifi][connect][scenario5]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_connect_scenario5");
    BROOKESIA_LOGI("=== Test ServiceWifi - repeatedly switch between TEST_WIFI_SSID1 and TEST_WIFI_SSID2 ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    service::LocalTestRunner runner;
    std::vector<service::LocalTestItem> test_items;

    test_items.push_back(service::LocalTestItem{
        .name = "Clear storage",
        .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
        .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
    });
    test_items.push_back(service::LocalTestItem{
        .name = "State transition: Init (Deinited -> Inited)",
        .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
        .params = boost::json::object{{
                BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
            }},
        .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
    });

    // First, set and connect to TEST_WIFI_SSID1
    test_items.push_back(service::LocalTestItem{
        .name = "Set connect AP to TEST_WIFI_SSID1 (initial)",
        .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
        .params = boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), TEST_WIFI_SSID1},
            {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), TEST_WIFI_PASSWORD1}
        }
    });
    test_items.push_back(service::LocalTestItem{
        .name = "Connect to TEST_WIFI_SSID1 (initial)",
        .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
        .params = boost::json::object{{
                BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
            }},
        .run_duration_ms = TEST_WIFI_CONNECT_DURATION_MS
    });

    // Repeatedly switch between SSID1 and SSID2
    for (size_t cycle = 0; cycle < RAPID_SWITCH_AP_CYCLES; cycle++) {
        // Switch to SSID2
        test_items.push_back(service::LocalTestItem{
            .name = "Set connect AP to TEST_WIFI_SSID2 (cycle " + std::to_string(cycle) + ")",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), TEST_WIFI_SSID2},
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), TEST_WIFI_PASSWORD2}
            },
            .start_delay_ms = 200 // Wait 200ms after previous operation
        });
        test_items.push_back(service::LocalTestItem{
            .name = "Connect to TEST_WIFI_SSID2 (cycle " + std::to_string(cycle) + ")",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
                }},
            .start_delay_ms = 100,
            .run_duration_ms = TEST_WIFI_CONNECT_DURATION_MS
        });

        // Switch back to SSID1
        test_items.push_back(service::LocalTestItem{
            .name = "Set connect AP to TEST_WIFI_SSID1 (cycle " + std::to_string(cycle) + ")",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), TEST_WIFI_SSID1},
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), TEST_WIFI_PASSWORD1}
            },
            .start_delay_ms = 200
        });
        test_items.push_back(service::LocalTestItem{
            .name = "Connect to TEST_WIFI_SSID1 (cycle " + std::to_string(cycle) + ")",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
                }},
            .start_delay_ms = 100,
            .run_duration_ms = TEST_WIFI_CONNECT_DURATION_MS
        });
    }

    // Run all test items
    bool all_passed = runner.run_tests(WifiHelpler::get_name().data(), test_items);
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Failed to repeatedly switch between SSIDs");

    // Calculate total test duration and wait for all events
    uint32_t total_test_duration = RAPID_SWITCH_AP_CYCLES * 2 * (TEST_WIFI_CONNECT_DURATION_MS + 200) + TEST_WIFI_CONNECT_DURATION_MS;
    uint32_t wait_timeout = total_test_duration + 3000; // Add extra buffer time

    BROOKESIA_LOGI("Waiting for all switch events (timeout: %u ms)", wait_timeout);

    // Wait for events (expect at least some events from switching operations)
    constexpr size_t MIN_EXPECTED_EVENTS = RAPID_SWITCH_AP_CYCLES; // At least one event per cycle
    bool events_received = collector.wait_for_general_events(MIN_EXPECTED_EVENTS, wait_timeout);

    // Give additional time for any delayed events
    boost::this_thread::sleep_for(boost::chrono::milliseconds(2000));

    // Verify event counts and sequence
    size_t connected_count = 0;
    size_t disconnected_count = 0;
    std::vector<std::string> event_sequence;

    {
        std::lock_guard<std::mutex> lock(collector.mutex);

        for (const auto &evt : collector.general_events) {
            if (evt.event == "Connected") {
                connected_count++;
                event_sequence.push_back("Connected");
            } else if (evt.event == "Disconnected") {
                disconnected_count++;
                event_sequence.push_back("Disconnected");
            }
        }

        BROOKESIA_LOGI("Event statistics: Connected=%zu, Disconnected=%zu, Total events=%zu",
                       connected_count, disconnected_count, collector.general_events.size());

        // Build event sequence string
        std::string seq_str;
        for (size_t i = 0; i < event_sequence.size(); i++) {
            if (i > 0) {
                seq_str += " -> ";
            }
            seq_str += event_sequence[i];
        }
        BROOKESIA_LOGI("Event sequence: %s", seq_str.empty() ? "(none)" : seq_str.c_str());
    }

    // Verify we received some events
    TEST_ASSERT_TRUE_MESSAGE(
        events_received || (connected_count > 0 || disconnected_count > 0),
        "No connect/disconnect events received during SSID switching"
    );

    // Verify we have reasonable number of events
    // Each cycle should produce at least one Connected event (may have Disconnected events too)
    TEST_ASSERT_TRUE_MESSAGE(
        connected_count >= RAPID_SWITCH_AP_CYCLES,
        "Not enough Connected events received for the number of switch cycles"
    );

    // Verify final connection state
    service::LocalTestRunner verify_runner;
    std::vector<service::LocalTestItem> verify_items = {
        service::LocalTestItem{
            .name = "Verify final connected AP",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
            .validator = [](const service::FunctionValue & value)
            {
                auto array_ptr = std::get_if<boost::json::array>(&value);
                BROOKESIA_CHECK_NULL_RETURN(array_ptr, false, "array_ptr is NULL");

                std::vector<WifiHelpler::ConnectApInfo> connect_ap_infos;
                auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(*array_ptr, connect_ap_infos);
                BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse connect AP infos");

                for (const auto &connect_ap_info : connect_ap_infos) {
                    if (connect_ap_info.ssid == TEST_WIFI_SSID1 || connect_ap_info.ssid == TEST_WIFI_SSID2) {
                        return true;
                    }
                }
                return false;
            }
        }
    };

    bool verify_passed = verify_runner.run_tests(WifiHelpler::get_name().data(), verify_items);
    TEST_ASSERT_TRUE_MESSAGE(verify_passed, "Failed to verify final connection state");

    BROOKESIA_LOGI("Repeated SSID switching test completed: %zu cycles, %zu Connected events, %zu Disconnected events",
                   RAPID_SWITCH_AP_CYCLES, connected_count, disconnected_count);
}
#endif // defined(TEST_WIFI_SSID2) && defined(TEST_WIFI_PASSWORD2)

TEST_CASE("Test ServiceWifi - connect a invalid AP", "[service][wifi][connect][invalid_ap]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_connect_invalid_ap");
    BROOKESIA_LOGI("=== Test ServiceWifi - connect a invalid AP ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    service::LocalTestRunner runner;
    std::vector<service::LocalTestItem> test_items = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        // Directly connect to uninitialized AP (empty SSID)
        service::LocalTestItem{
            .name = "Trigger connect action to uninitialized AP",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
                }},
            .run_duration_ms = 5000 // Wait for 5 seconds
        },
        // Then set and connect to TEST_WIFI_SSID1
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
        },
        service::LocalTestItem{
            .name = "Get connected APs",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
            .validator = [](const service::FunctionValue & value)
            {
                auto array_ptr = std::get_if<boost::json::array>(&value);
                BROOKESIA_CHECK_NULL_RETURN(array_ptr, false, "array_ptr is NULL");

                std::vector<WifiHelpler::ConnectApInfo> connect_ap_infos;
                auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(*array_ptr, connect_ap_infos);
                BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse connect AP infos");

                for (const auto &connect_ap_info : connect_ap_infos) {
                    if (connect_ap_info.ssid == TEST_WIFI_SSID1) {
                        return true;
                    }
                }
                return false;
            }
        }
    };

    bool all_passed = runner.run_tests(WifiHelpler::get_name().data(), test_items);
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Failed to connect to TEST_WIFI_SSID1");

    // Wait for Connected event
    bool connected = collector.wait_for_general_events(1, TEST_WIFI_CONNECT_DURATION_MS);
    TEST_ASSERT_TRUE_MESSAGE(connected, "Failed to connect to TEST_WIFI_SSID1");

    // Verify Connected event
    {
        std::lock_guard<std::mutex> lock(collector.mutex);
        TEST_ASSERT_TRUE_MESSAGE(
            !collector.general_events.empty() &&
            collector.general_events.back().event == "Connected",
            "Connected event not received"
        );
    }
    // Connections will be automatically disconnected when they go out of scope
}

#endif // defined(TEST_WIFI_SSID1) && defined(TEST_WIFI_PASSWORD1)

// ==================== Error Handling Tests ====================

TEST_CASE("Test ServiceWifi - error handling: invalid parameters", "[service][wifi][error][invalid_params]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_error_invalid_params");
    BROOKESIA_LOGI("=== Test ServiceWifi - error handling: invalid parameters ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    static auto wifi_functions = WifiHelpler::get_function_schemas();
    std::vector<service::LocalTestItem> test_items = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        // Init from Deinited to Inited
        service::LocalTestItem{
            .name = "State transition: Init (Deinited -> Inited)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }},
            .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
        },
        // Test with empty SSID
        service::LocalTestItem{
            .name = "Set connect AP with empty SSID",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), ""},
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), "password"}
            },
            .validator = [](const service::FunctionValue & value)
            {
                // Should fail or return error
                auto obj_ptr = std::get_if<boost::json::object>(&value);
                if (obj_ptr) {
                    auto success_it = obj_ptr->find("success");
                    if (success_it != obj_ptr->end() && success_it->value().is_bool()) {
                        return !success_it->value().as_bool(); // Expect failure
                    }
                }
                return false; // Unexpected format
            }
        },
        // Test with extremely long SSID (over 32 bytes)
        service::LocalTestItem{
            .name = "Set connect AP with extremely long SSID",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID),
                    std::string(100, 'A')
                }, // 100 characters, exceeds typical SSID limit
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), "password"}
            },
            .validator = [](const service::FunctionValue & value)
            {
                // Should handle gracefully (may fail or truncate)
                return true; // Accept any result as valid handling
            }
        },
        // Test with extremely long password (over 64 bytes)
        service::LocalTestItem{
            .name = "Set connect AP with extremely long password",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), "TestSSID"},
                {
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password),
                    std::string(200, 'P')
                } // 200 characters, exceeds typical password limit
            },
            .validator = [](const service::FunctionValue & value)
            {
                // Should handle gracefully
                return true;
            }
        },
        // Test with invalid action string
        service::LocalTestItem{
            .name = "Trigger invalid general action",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action), "InvalidAction"}
            },
            .validator = [](const service::FunctionValue & value)
            {
                // Should fail with invalid action
                auto obj_ptr = std::get_if<boost::json::object>(&value);
                if (obj_ptr) {
                    auto success_it = obj_ptr->find("success");
                    if (success_it != obj_ptr->end() && success_it->value().is_bool()) {
                        return !success_it->value().as_bool();
                    }
                }
                return false;
            }
        }
    };

    service::LocalTestRunner runner;
    runner.run_tests(WifiHelpler::get_name().data(), test_items);

    // Note: Some tests may pass even if they return errors, as long as they handle errors gracefully
    BROOKESIA_LOGI("Error handling tests completed, some may intentionally fail");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
}

TEST_CASE("Test ServiceWifi - error handling: invalid state transitions", "[service][wifi][error][invalid_state]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_error_invalid_state");
    BROOKESIA_LOGI("=== Test ServiceWifi - error handling: invalid state transitions ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    // Test connecting before WiFi is started
    std::vector<service::LocalTestItem> test_items1 = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        // Init from Deinited to Inited
        service::LocalTestItem{
            .name = "State transition: Init (Deinited -> Inited)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }},
            .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
        },
        service::LocalTestItem{
            .name = "Connect before WiFi started (should fail)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
                }},
            .validator = [](const service::FunctionValue & value)
            {
                // Should fail because WiFi is not started
                auto obj_ptr = std::get_if<boost::json::object>(&value);
                if (obj_ptr) {
                    auto success_it = obj_ptr->find("success");
                    if (success_it != obj_ptr->end() && success_it->value().is_bool()) {
                        return !success_it->value().as_bool();
                    }
                }
                return false;
            }
        }
    };

    service::LocalTestRunner runner1;
    runner1.run_tests(WifiHelpler::get_name().data(), test_items1);

    BROOKESIA_LOGI("Pre-start connect test completed");

    // Test stopping before starting
    std::vector<service::LocalTestItem> test_items2 = {
        service::LocalTestItem{
            .name = "Stop before start (should handle gracefully)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Stop)
                }},
            .validator = [](const service::FunctionValue & value)
            {
                // Should handle gracefully (may succeed or fail, but not crash)
                return true;
            }
        }
    };

    service::LocalTestRunner runner2;
    runner2.run_tests(WifiHelpler::get_name().data(), test_items2);
    BROOKESIA_LOGI("Pre-start stop test completed");

    // Test deinit before init
    std::vector<service::LocalTestItem> test_items3 = {
        service::LocalTestItem{
            .name = "Deinit before init (should handle gracefully)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Deinit)
                }},
            .validator = [](const service::FunctionValue & value)
            {
                return true; // Should handle gracefully
            }
        }
    };

    service::LocalTestRunner runner3;
    runner3.run_tests(WifiHelpler::get_name().data(), test_items3);
    BROOKESIA_LOGI("Pre-init deinit test completed");
}

TEST_CASE("Test ServiceWifi - error handling: rapid state changes", "[service][wifi][error][rapid_changes]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_error_rapid_changes");
    BROOKESIA_LOGI("=== Test ServiceWifi - error handling: rapid state changes ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    // Rapidly switch between start and stop
    std::vector<service::LocalTestItem> test_items = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        // Init from Deinited to Inited
        service::LocalTestItem{
            .name = "State transition: Init (Deinited -> Inited)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }},
            .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
        },
        service::LocalTestItem{
            .name = "Rapid start",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Start)
                }},
            .start_delay_ms = 0,
            .run_duration_ms = 50
        },
        service::LocalTestItem{
            .name = "Rapid stop",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Stop)
                }},
            .start_delay_ms = 10,
            .run_duration_ms = 50
        },
        service::LocalTestItem{
            .name = "Rapid start again",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Start)
                }},
            .start_delay_ms = 20,
            .run_duration_ms = 50
        },
        service::LocalTestItem{
            .name = "Rapid stop again",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Stop)
                }},
            .start_delay_ms = 30,
            .run_duration_ms = 50
        }
    };

    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(WifiHelpler::get_name().data(), test_items);
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Rapid state change test failed");

    // Wait for system to stabilize
    boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
}

// ==================== Stress Tests ====================

TEST_CASE("Test ServiceWifi - stress test: rapid scan operations", "[service][wifi][stress][scan]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_stress_scan");
    BROOKESIA_LOGI("=== Test ServiceWifi - stress test: rapid scan operations ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    std::vector<service::LocalTestItem> test_items;

    test_items.push_back(service::LocalTestItem{
        .name = "Clear storage",
        .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
        .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
    });
    test_items.push_back(service::LocalTestItem{
        .name = "State transition: Init (Deinited -> Inited) (initial)",
        .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
        .params = boost::json::object{{
                BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
            }},
        .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
    });

    // Create rapid scan start/stop cycles
    for (size_t i = 0; i < RAPID_SCAN_CYCLES; i++) {
        test_items.push_back(service::LocalTestItem{
            .name = "Scan start " + std::to_string(i),
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStart),
        });

        test_items.push_back(service::LocalTestItem{
            .name = "Scan stop " + std::to_string(i),
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStop),
        });
    }

    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(WifiHelpler::get_name().data(), test_items);
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Rapid scan operations test failed");

    // Wait for any pending scan operations
    boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));

    BROOKESIA_LOGI("Completed %zu scan cycles", RAPID_SCAN_CYCLES);
}

TEST_CASE("Test ServiceWifi - stress test: continuous state transitions", "[service][wifi][stress][state]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_stress_state");
    BROOKESIA_LOGI("=== Test ServiceWifi - stress test: continuous state transitions ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    std::vector<service::LocalTestItem> test_items;

    test_items.push_back(service::LocalTestItem{
        .name = "Clear storage",
        .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
        .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
    });

    // Create continuous init -> start -> stop -> deinit cycles
    for (size_t i = 0; i < TRANSITION_CYCLES; i++) {
        test_items.push_back(service::LocalTestItem{
            .name = "Init cycle " + std::to_string(i),
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }},
        });

        test_items.push_back(service::LocalTestItem{
            .name = "Start cycle " + std::to_string(i),
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Start)
                }},
        });

        test_items.push_back(service::LocalTestItem{
            .name = "Stop cycle " + std::to_string(i),
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Stop)
                }},
        });

        test_items.push_back(service::LocalTestItem{
            .name = "Deinit cycle " + std::to_string(i),
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Deinit)
                }},
        });
    }

    // Setup event subscriptions
    EventCollector collector;
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(WifiHelpler::get_name().data(), test_items);
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Continuous state transitions test failed");

    // Wait for deinited event to ensure system is stabilized
    constexpr uint32_t timeout_ms = TRANSITION_CYCLES * (
                                        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_INITED_TIMEOUT_MS +
                                        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_STARTED_TIMEOUT_MS +
                                        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_STOPPED_TIMEOUT_MS +
                                        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_DEINITED_TIMEOUT_MS
                                    );
    collector.wait_for_general_events(TRANSITION_CYCLES, timeout_ms, WifiHelpler::GeneralEvent::Deinited);

    BROOKESIA_LOGI("Completed %zu transition cycles", TRANSITION_CYCLES);
}

// TODO: This test case is temporarily disabled because it often fails on ESP32-P4 CI.
// #if !defined(CONFIG_IDF_TARGET_ESP32P4)
TEST_CASE("Test ServiceWifi - stress test: multiple concurrent operations", "[service][wifi][stress][concurrent]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_stress_concurrent");
    BROOKESIA_LOGI("=== Test ServiceWifi - stress test: multiple concurrent operations ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    // Start WiFi first
    service::LocalTestRunner init_runner;
    std::vector<service::LocalTestItem> init_items = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        service::LocalTestItem{
            .name = "State transition: Init (Deinited -> Inited) (initial)",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }},
            .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
        },
        service::LocalTestItem{
            .name = "Start for concurrent test",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Start)
                }},
            .run_duration_ms = TEST_WIFI_START_DURATION_MS
        }
    };
    init_runner.run_tests(WifiHelpler::get_name().data(), init_items);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(500));

    // Now run concurrent LocalTestRunner instances in 2 threads
    std::atomic<size_t> success_count{0};
    std::atomic<size_t> failure_count{0};
    std::mutex result_mutex;
    boost::thread_group threads;

    BROOKESIA_LOGI("Starting %zu concurrent LocalTestRunner instances", NUM_THREADS);

    // Create test items for each thread (different operations)
    auto create_test_items_for_thread = [](size_t thread_id) -> std::vector<service::LocalTestItem> {
        std::vector<service::LocalTestItem> items;

        // Each thread runs different mix of operations
        for (size_t i = 0; i < TESTS_PER_THREAD; i++)
        {
            size_t op_index = (thread_id * TESTS_PER_THREAD + i) % 4;

            if (op_index == 0) {
                // Scan start operations
                items.push_back(service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Scan start " + std::to_string(i),
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStart),
                });
            } else if (op_index == 1) {
                // Get connected APs
                items.push_back(service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Get connected APs " + std::to_string(i),
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
                });
            } else if (op_index == 2) {
                // Get connect AP
                items.push_back(service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Get connect AP " + std::to_string(i),
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectAp),
                });
            } else if (op_index == 3) {
                // Scan stop operations
                items.push_back(service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Scan stop " + std::to_string(i),
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStop),
                });
            }
        }

        return items;
    };

    // Create threads, each running its own LocalTestRunner
    for (size_t thread_id = 0; thread_id < NUM_THREADS; thread_id++) {
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_size = 8 * 1024,
        });

        threads.create_thread([ &, thread_id]() {
            try {
                BROOKESIA_LOGI("Thread %zu: Starting LocalTestRunner", thread_id);

                // Create test items for this thread
                auto test_items = create_test_items_for_thread(thread_id);

                // Create and run LocalTestRunner
                service::LocalTestRunner runner;
                bool all_passed = runner.run_tests(WifiHelpler::get_name().data(), test_items);
                // Avoid compiler warning about unused variable
                (void)all_passed;

                // Get results
                const auto &results = runner.get_results();
                size_t thread_success = 0;
                size_t thread_failure = 0;

                for (bool result : results) {
                    if (result) {
                        thread_success++;
                    } else {
                        thread_failure++;
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    success_count += thread_success;
                    failure_count += thread_failure;
                    BROOKESIA_LOGI("Thread %zu: Completed - passed=%zu, failed=%zu, all_passed=%s",
                                   thread_id, thread_success, thread_failure, all_passed ? "true" : "false");
                }
            } catch (const std::exception &e) {
                std::lock_guard<std::mutex> lock(result_mutex);
                failure_count += TESTS_PER_THREAD; // Count all as failed on exception
                BROOKESIA_LOGE("Thread %zu: Exception: %s", thread_id, e.what());
            }
        });
    }

    // Wait for all threads to complete
    BROOKESIA_LOGI("Waiting for all concurrent LocalTestRunner instances to complete...");
    threads.join_all();

    // Verify results
    size_t total_ops = success_count.load() + failure_count.load();
    size_t expected_total = NUM_THREADS * TESTS_PER_THREAD;
    BROOKESIA_LOGI("Concurrent LocalTestRunner instances completed: total=%zu, success=%zu, failed=%zu",
                   total_ops, success_count.load(), failure_count.load());

    TEST_ASSERT_EQUAL_MESSAGE(expected_total, total_ops, "Not all test operations completed");

    // Allow some failures for concurrent operations (e.g., scan conflicts)
    // But most operations should succeed
    size_t min_success_rate = expected_total * 70 / 100; // At least 70% success rate
    TEST_ASSERT_TRUE_MESSAGE(success_count.load() >= min_success_rate, "Too many concurrent operations failed");

    BROOKESIA_LOGI("Completed %zu concurrent LocalTestRunner instances with %zu/%zu successful operations",
                   NUM_THREADS, success_count.load(), expected_total);
}
// #endif // CONFIG_IDF_TARGET_ESP32P4

TEST_CASE("Test ServiceWifi - stress test: random operations serial", "[service][wifi][stress][random][serial]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_stress_random_serial");
    BROOKESIA_LOGI("=== Test ServiceWifi - stress test: random operations serial ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    // Create random operations list (all operations)
    std::vector<service::LocalTestItem> random_items;
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> op_dist(0, 13); // 14 different operations

    BROOKESIA_LOGI("Generating %zu random operations", RANDOM_OPERATIONS_COUNT);

    for (size_t i = 0; i < RANDOM_OPERATIONS_COUNT; i++) {
        size_t op_type = op_dist(rng);
        service::LocalTestItem item;

        switch (op_type) {
        case 0: // TriggerScanStart
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Scan start",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStart),
            };
            break;
        case 1: // TriggerScanStop
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Scan stop",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStop),
            };
            break;
        case 2: // SetScanParams
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Set scan params",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetScanParams),
                .params = boost::json::object{{
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetScanParamsParam::Param),
                        BROOKESIA_DESCRIBE_TO_JSON(WifiHelpler::ScanParams({
                            .ap_count = 10,
                            .interval_ms = 1000,
                            .timeout_ms = 5000
                        })).as_object()
                    }},
            };
            break;
        case 3: // SetConnectAp
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Set connect AP",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
                .params = boost::json::object{
#if defined(TEST_WIFI_SSID1) && defined(TEST_WIFI_PASSWORD1)
                    {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), TEST_WIFI_SSID1},
                    {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), TEST_WIFI_PASSWORD1}
#else
                    {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), ""},
                    {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), ""}
#endif // defined(TEST_WIFI_SSID1) && defined(TEST_WIFI_PASSWORD1)
                },
            };
            break;
        case 4: // GetGeneralState
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Get general state",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetGeneralState),
            };
            break;
        case 5: // GetConnectAp
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Get connect AP",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectAp),
            };
            break;
        case 6: // GetConnectedAps
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Get connected APs",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
            };
            break;
        case 7: // ResetData
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Reset data",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
                .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
            };
            break;
        case 8: // Init
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Init",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                .params = boost::json::object{{
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                    }},
                .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
            };
            break;
        case 9: // Deinit
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Deinit",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                .params = boost::json::object{{
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Deinit)
                    }},
            };
            break;
        case 10: // Start
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Start",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                .params = boost::json::object{{
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Start)
                    }},
                .run_duration_ms = TEST_WIFI_START_DURATION_MS
            };
            break;
        case 11: // Stop
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Stop",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                .params = boost::json::object{{
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Stop)
                    }},
            };
            break;
        case 12: // Connect
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Connect",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                .params = boost::json::object{{
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
                    }},
                .run_duration_ms = TEST_WIFI_CONNECT_DURATION_MS
            };
            break;
        case 13: // Disconnect
            item = service::LocalTestItem{
                .name = "Random op " + std::to_string(i) + " - Disconnect",
                .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                .params = boost::json::object{{
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Disconnect)
                    }},
            };
            break;
        }

        random_items.push_back(std::move(item));
    }

    // Run all operations serially
    BROOKESIA_LOGI("Running %zu random operations serially", RANDOM_OPERATIONS_COUNT);
    service::LocalTestRunner runner;
    runner.run_tests(WifiHelpler::get_name().data(), random_items);

    BROOKESIA_LOGI("Serial random operations completed, waiting for system to stabilize");

    // Wait for system to stabilize after operations
    boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));

    BROOKESIA_LOGI("Completed serial random operations test (no crash)");
}

TEST_CASE("Test ServiceWifi - stress test: random operations parallel", "[service][wifi][stress][random][parallel]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_stress_random_parallel");
    BROOKESIA_LOGI("=== Test ServiceWifi - stress test: random operations parallel ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    // Now run concurrent LocalTestRunner instances with random operations
    boost::thread_group threads;

    BROOKESIA_LOGI("Starting %zu concurrent LocalTestRunner instances with random operations", NUM_THREADS);

    // Create random test items for each thread (all operations)
    auto create_random_test_items_for_thread = [](size_t thread_id, size_t seed) -> std::vector<service::LocalTestItem> {
        std::vector<service::LocalTestItem> items;
        std::mt19937 rng(seed);
        std::uniform_int_distribution<size_t> op_dist(0, 13); // 14 different operations

        for (size_t i = 0; i < TESTS_PER_THREAD; i++)
        {
            size_t op_type = op_dist(rng);
            service::LocalTestItem item;

            switch (op_type) {
            case 0: // TriggerScanStart
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Scan start",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStart),
                };
                break;
            case 1: // TriggerScanStop
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Scan stop",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStop),
                };
                break;
            case 2: // SetScanParams
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Set scan params",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetScanParams),
                    .params = boost::json::object{{
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetScanParamsParam::Param),
                            BROOKESIA_DESCRIBE_TO_JSON(WifiHelpler::ScanParams({
                                .ap_count = 10,
                                .interval_ms = 1000,
                                .timeout_ms = 5000
                            })).as_object()
                        }},
                };
                break;
            case 3: // SetConnectAp
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Set connect AP",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::SetConnectAp),
                    .params = boost::json::object{
#if defined(TEST_WIFI_SSID1) && defined(TEST_WIFI_PASSWORD1)
                        {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), TEST_WIFI_SSID1},
                        {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), TEST_WIFI_PASSWORD1}
#else
                        {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::SSID), ""},
                        {BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionSetConnectApParam::Password), ""}
#endif // defined(TEST_WIFI_SSID1) && defined(TEST_WIFI_PASSWORD1)
                    },
                };
                break;
            case 4: // GetGeneralState
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Get general state",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetGeneralState),
                };
                break;
            case 5: // GetConnectAp
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Get connect AP",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectAp),
                };
                break;
            case 6: // GetConnectedAps
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Get connected APs",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
                };
                break;
            case 7: // ResetData
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Reset data",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
                    .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
                };
                break;
            case 8: // Init
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Init",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                    .params = boost::json::object{{
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                        }},
                    .run_duration_ms = TEST_WIFI_INIT_DURATION_MS
                };
                break;
            case 9: // Deinit
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Deinit",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                    .params = boost::json::object{{
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Deinit)
                        }},
                };
                break;
            case 10: // Start
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Start",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                    .params = boost::json::object{{
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Start)
                        }},
                    .run_duration_ms = TEST_WIFI_START_DURATION_MS
                };
                break;
            case 11: // Stop
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Stop",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                    .params = boost::json::object{{
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Stop)
                        }},
                };
                break;
            case 12: // Connect
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Connect",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                    .params = boost::json::object{{
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Connect)
                        }},
                    .run_duration_ms = TEST_WIFI_CONNECT_DURATION_MS
                };
                break;
            case 13: // Disconnect
                item = service::LocalTestItem{
                    .name = "Thread " + std::to_string(thread_id) + " - Random op " + std::to_string(i) + " - Disconnect",
                    .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
                    .params = boost::json::object{{
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Disconnect)
                        }},
                };
                break;
            }

            items.push_back(std::move(item));
        }

        return items;
    };

    // Create threads, each running its own LocalTestRunner with random operations
    for (size_t thread_id = 0; thread_id < NUM_THREADS; thread_id++) {
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_size = 8 * 1024,
        });

        threads.create_thread([ &, thread_id]() {
            try {
                BROOKESIA_LOGI("Thread %zu: Starting LocalTestRunner with random operations", thread_id);

                // Create random test items for this thread (use thread_id as seed for reproducibility)
                auto test_items = create_random_test_items_for_thread(thread_id, thread_id * 1000 + 12345);

                // Create and run LocalTestRunner
                service::LocalTestRunner runner;
                runner.run_tests(WifiHelpler::get_name().data(), test_items);

                BROOKESIA_LOGI("Thread %zu: Completed", thread_id);
            } catch (const std::exception &e) {
                BROOKESIA_LOGE("Thread %zu: Exception: %s", thread_id, e.what());
            }
        });
    }

    // Wait for all threads to complete
    BROOKESIA_LOGI("Waiting for all concurrent LocalTestRunner instances to complete...");
    threads.join_all();

    BROOKESIA_LOGI("Concurrent random operations completed, waiting for system to stabilize");

    // Wait for system to stabilize after operations
    boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));

    BROOKESIA_LOGI("Completed %zu concurrent LocalTestRunner instances with random operations (no crash)",
                   NUM_THREADS);
}

TEST_CASE("Test ServiceWifi - stress test: long running operations", "[service][wifi][stress][long_running]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_wifi_stress_long_running");
    BROOKESIA_LOGI("=== Test ServiceWifi - stress test: long running operations ===");

    startup();
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Setup event subscriptions
    EventCollector collector;

    static auto wifi_functions = WifiHelpler::get_function_schemas();

    // Start WiFi and begin long scan
    std::vector<service::LocalTestItem> test_items = {
        // Clear storage
        service::LocalTestItem{
            .name = "Clear storage",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::ResetData),
            .call_timeout_ms = TEST_WIFI_RESET_DATA_TIMEOUT_MS,
        },
        service::LocalTestItem{
            .name = "Init for long running test",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Init)
                }}
        },
        service::LocalTestItem{
            .name = "Start for long running test",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerGeneralAction),
            .params = boost::json::object{{
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionTriggerGeneralActionParam::Action),
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::GeneralAction::Start)
                }}
        },
        service::LocalTestItem{
            .name = "Start long scan",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStart),
            .run_duration_ms = 10000 // Run for 10 seconds
        },
        // Periodically check status during scan
        service::LocalTestItem{
            .name = "Check connected APs during scan",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
            .run_duration_ms = 100
        },
        service::LocalTestItem{
            .name = "Check connected APs again",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::GetConnectedAps),
            .run_duration_ms = 100
        },
        service::LocalTestItem{
            .name = "Stop scan after long run",
            .method = BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::FunctionId::TriggerScanStop),
            .run_duration_ms = 500
        }
    };

    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(WifiHelpler::get_name().data(), test_items);
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Long running operations test failed");

    // Wait for final operations
    boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));

    BROOKESIA_LOGI("Long running test completed");
}
