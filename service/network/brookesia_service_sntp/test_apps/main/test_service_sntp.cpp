/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <ctime>
#include <string>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/service_manager/service/local_runner.hpp"
#include "general.hpp"

using namespace esp_brookesia;

namespace {

// Helper function to validate servers result
static bool validate_servers_result(const service::FunctionValue &value, const std::vector<std::string> &expected_servers)
{
    auto array_ptr = std::get_if<boost::json::array>(&value);
    if (!array_ptr) {
        BROOKESIA_LOGE("validate_servers_result: value is not an array");
        return false;
    }

    std::vector<std::string> servers;
    for (const auto &item : *array_ptr) {
        if (!item.is_string()) {
            BROOKESIA_LOGE("validate_servers_result: array item is not a string");
            return false;
        }
        servers.push_back(std::string(item.as_string()));
    }

    if (servers.size() != expected_servers.size()) {
        BROOKESIA_LOGE("validate_servers_result: server count mismatch. Expected: %zu, Got: %zu",
                       expected_servers.size(), servers.size());
        return false;
    }

    for (size_t i = 0; i < expected_servers.size(); i++) {
        if (servers[i] != expected_servers[i]) {
            BROOKESIA_LOGE("validate_servers_result: server mismatch at index %zu. Expected: '%s', Got: '%s'",
                           i, expected_servers[i].c_str(), servers[i].c_str());
            return false;
        }
    }
    return true;
}

// Helper function to validate timezone result
static bool validate_timezone_result(const service::FunctionValue &value, const std::string &expected_timezone)
{
    auto string_ptr = std::get_if<std::string>(&value);
    if (!string_ptr) {
        BROOKESIA_LOGE("validate_timezone_result: value is not a string");
        return false;
    }

    if (*string_ptr != expected_timezone) {
        BROOKESIA_LOGE("validate_timezone_result: timezone mismatch. Expected: '%s', Got: '%s'",
                       expected_timezone.c_str(), string_ptr->c_str());
        return false;
    }
    return true;
}

static bool set_servers(const std::vector<std::string> &servers)
{
    auto result = SNTPHelper::call_function_sync<void>(
                      SNTPHelper::FunctionId::SetServers,
                      BROOKESIA_DESCRIBE_TO_JSON(servers).as_array()
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to set SNTP servers: %1%", result.error());
    return true;
}

static bool set_timezone(const std::string &timezone)
{
    auto result = SNTPHelper::call_function_sync<void>(SNTPHelper::FunctionId::SetTimezone, timezone);
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to set SNTP timezone: %1%", result.error());
    return true;
}

static bool start_sntp_sync()
{
    auto result = SNTPHelper::call_function_sync<void>(SNTPHelper::FunctionId::Start);
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to start SNTP: %1%", result.error());
    return true;
}

static bool stop_sntp_sync()
{
    auto result = SNTPHelper::call_function_sync<void>(SNTPHelper::FunctionId::Stop);
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to stop SNTP: %1%", result.error());
    return true;
}

} // namespace

BROOKESIA_TEST_CASE(test_servicesntp_basic_set_and_get, "Test ServiceSntp - basic set and get", "[service][sntp][basic]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_sntp_basic");
    BROOKESIA_LOGI("=== Test ServiceSntp - basic set and get ===");

    TEST_ASSERT_TRUE_MESSAGE(start_sntp_service(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });

    const std::vector<std::string> test_servers = {"pool.ntp.org", "time.nist.gov"};
    const std::string test_timezone = "UTC";

    std::vector<service::LocalTestItem> test_items = {
        // Set servers
        service::LocalTestItem{
            .name = "Set NTP servers",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::SetServers),
            .params = boost::json::object{
                {
                    BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionSetServersParam::Servers),
                    BROOKESIA_DESCRIBE_TO_JSON(test_servers)
                }
            }
        },
        // Get servers
        service::LocalTestItem{
            .name = "Get NTP servers",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::GetServers),
            .params = boost::json::object{},
            .validator = [test_servers](const service::FunctionValue & value)
            {
                return validate_servers_result(value, test_servers);
            }
        },
        // Set timezone
        service::LocalTestItem{
            .name = "Set timezone",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::SetTimezone),
            .params = boost::json::object{
                {
                    BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionSetTimezoneParam::Timezone),
                    test_timezone
                }
            }
        },
        // Get timezone
        service::LocalTestItem{
            .name = "Get timezone",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::GetTimezone),
            .params = boost::json::object{},
            .validator = [test_timezone](const service::FunctionValue & value)
            {
                return validate_timezone_result(value, test_timezone);
            }
        }
    };

    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(SNTPHelper::get_name()), test_items);

    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(results[i], (std::string("Test failed: ") + test_items[i].name).c_str());
    }

}

BROOKESIA_TEST_CASE(test_servicesntp_start_and_stop, "Test ServiceSntp - start and stop", "[service][sntp][start_stop]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_sntp_start_stop");
    BROOKESIA_LOGI("=== Test ServiceSntp - start and stop ===");

    TEST_ASSERT_TRUE_MESSAGE(start_sntp_service(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });

    SNTPHelper::EventMonitor<SNTPHelper::EventId::StateChanged> state_monitor;
    TEST_ASSERT_TRUE_MESSAGE(state_monitor.start(), "Failed to monitor StateChanged event");

    TEST_ASSERT_TRUE_MESSAGE(start_sntp_sync(), "Failed to start SNTP sync");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_sntp_state(state_monitor, SNTPHelper::State::CheckingNetwork, SNTP_WAIT_STATE_TIMEOUT_MS),
        "Missing CheckingNetwork state event"
    );

    TEST_ASSERT_TRUE_MESSAGE(stop_sntp_sync(), "Failed to stop SNTP sync");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_sntp_state(state_monitor, SNTPHelper::State::Stopped, SNTP_WAIT_STOP_TIMEOUT_MS),
        "Missing Stopped state event"
    );
}

BROOKESIA_TEST_CASE(test_servicesntp_complete_workflow, "Test ServiceSntp - complete workflow", "[service][sntp][workflow]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_sntp_workflow");
    BROOKESIA_LOGI("=== Test ServiceSntp - complete workflow ===");

    TEST_ASSERT_TRUE_MESSAGE(start_sntp_service(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });

    const std::vector<std::string> test_servers = {"pool.ntp.org"};
    const std::string test_timezone = "CST-8";

    std::vector<service::LocalTestItem> test_items = {
        // Step 1: Reset data
        service::LocalTestItem{
            .name = "Step 1: Reset data",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::ResetData),
            .params = boost::json::object{}
        },
        // Step 2: Set servers
        service::LocalTestItem{
            .name = "Step 2: Set NTP servers",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::SetServers),
            .params = boost::json::object{
                {
                    BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionSetServersParam::Servers),
                    BROOKESIA_DESCRIBE_TO_JSON(test_servers)
                }
            }
        },
        // Step 3: Set timezone
        service::LocalTestItem{
            .name = "Step 3: Set timezone",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::SetTimezone),
            .params = boost::json::object{
                {
                    BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionSetTimezoneParam::Timezone),
                    test_timezone
                }
            }
        },
        // Step 4: Verify servers
        service::LocalTestItem{
            .name = "Step 4: Verify servers",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::GetServers),
            .params = boost::json::object{},
            .validator = [test_servers](const service::FunctionValue & value)
            {
                return validate_servers_result(value, test_servers);
            }
        },
        // Step 5: Verify timezone
        service::LocalTestItem{
            .name = "Step 5: Verify timezone",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::GetTimezone),
            .params = boost::json::object{},
            .validator = [test_timezone](const service::FunctionValue & value)
            {
                return validate_timezone_result(value, test_timezone);
            }
        }
    };

    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(SNTPHelper::get_name()), test_items);

    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(results[i], (std::string("Test failed: ") + test_items[i].name).c_str());
    }

    SNTPStateEventMonitor state_monitor;
    TEST_ASSERT_TRUE_MESSAGE(state_monitor.start(), "Failed to monitor StateChanged event");

    TEST_ASSERT_TRUE_MESSAGE(stop_sntp_sync(), "Failed to stop SNTP sync");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_sntp_state(state_monitor, SNTPHelper::State::Stopped, SNTP_WAIT_STOP_TIMEOUT_MS),
        "Missing Stopped state event"
    );

    state_monitor.clear();
    TEST_ASSERT_TRUE_MESSAGE(start_sntp_sync(), "Failed to start SNTP sync");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_sntp_state(state_monitor, SNTPHelper::State::CheckingNetwork, SNTP_WAIT_STATE_TIMEOUT_MS),
        "Missing CheckingNetwork state event"
    );

    TEST_ASSERT_TRUE_MESSAGE(stop_sntp_sync(), "Failed to stop SNTP sync");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_sntp_state(state_monitor, SNTPHelper::State::Stopped, SNTP_WAIT_STOP_TIMEOUT_MS),
        "Missing Stopped state event"
    );
}

BROOKESIA_TEST_CASE(
    test_servicesntp_network_time_update, "Test ServiceSntp - network time update", "[service][sntp][network]"
)
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_sntp_network_time_update");
    BROOKESIA_LOGI("=== Test ServiceSntp - network time update ===");

    TEST_SNTP_ASSERT_WIFI_CREDENTIALS();

#if defined(ESP_PLATFORM)
    // Real Wi-Fi + SNTP sync can leave a small ESP-IDF heap residual; offline tests keep threshold 0.
    memory_leak_threshold = TEST_SNTP_NETWORK_MEMORY_LEAK_THRESHOLD;
#endif

    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });

    SNTPStateEventMonitor state_monitor;
    TEST_ASSERT_TRUE_MESSAGE(state_monitor.start(), "Failed to monitor StateChanged event");

    TEST_ASSERT_TRUE_MESSAGE(stop_sntp_sync(), "Failed to stop SNTP sync");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_sntp_state(state_monitor, SNTPHelper::State::Stopped, SNTP_WAIT_STOP_TIMEOUT_MS),
        "Missing Stopped state event"
    );

    const std::vector<std::string> test_servers = {TEST_SNTP_NTP_SERVER};
    TEST_ASSERT_TRUE_MESSAGE(set_servers(test_servers), "Failed to set network test NTP server");
    TEST_ASSERT_TRUE_MESSAGE(set_timezone(TEST_SNTP_TIMEZONE), "Failed to set network test timezone");
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_station(), "Failed to connect Wi-Fi station");
    lib_utils::FunctionGuard disconnect_guard([]() {
        disconnect_wifi_station();
    });

    state_monitor.clear();
    TEST_ASSERT_TRUE_MESSAGE(start_sntp_sync(), "Failed to start SNTP sync");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_sntp_state(state_monitor, SNTPHelper::State::CheckingNetwork, SNTP_WAIT_STATE_TIMEOUT_MS),
        "Missing CheckingNetwork state event"
    );
    TEST_ASSERT_TRUE_MESSAGE(
        wait_sntp_state(state_monitor, SNTPHelper::State::Syncing, SNTP_WAIT_STATE_TIMEOUT_MS),
        "Missing Syncing state event"
    );
    TEST_ASSERT_TRUE_MESSAGE(
        wait_time_synced(state_monitor, TEST_SNTP_SYNC_TIMEOUT_MS),
        "SNTP network time synchronization timeout"
    );

    const std::time_t now = std::time(nullptr);
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(
        SNTP_MIN_VALID_UNIX_TIME, now, "System time is still earlier than the minimum valid Unix time"
    );

    TEST_ASSERT_TRUE_MESSAGE(stop_sntp_sync(), "Failed to stop SNTP sync");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_sntp_state(state_monitor, SNTPHelper::State::Stopped, SNTP_WAIT_STOP_TIMEOUT_MS),
        "Missing Stopped state event"
    );
}

BROOKESIA_TEST_CASE(test_servicesntp_reset_data, "Test ServiceSntp - reset data", "[service][sntp][reset]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_sntp_reset");
    BROOKESIA_LOGI("=== Test ServiceSntp - reset data ===");

    TEST_ASSERT_TRUE_MESSAGE(start_sntp_service(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });

    const std::vector<std::string> test_servers = {"time.nist.gov", "time.google.com"};
    const std::string test_timezone = "EST-5";

    std::vector<service::LocalTestItem> test_items = {
        // Step 1: Set servers
        service::LocalTestItem{
            .name = "Step 1: Set NTP servers",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::SetServers),
            .params = boost::json::object{
                {
                    BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionSetServersParam::Servers),
                    BROOKESIA_DESCRIBE_TO_JSON(test_servers)
                }
            }
        },
        // Step 2: Set timezone
        service::LocalTestItem{
            .name = "Step 2: Set timezone",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::SetTimezone),
            .params = boost::json::object{
                {
                    BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionSetTimezoneParam::Timezone),
                    test_timezone
                }
            }
        },
        // Step 3: Verify servers are set
        service::LocalTestItem{
            .name = "Step 3: Verify servers are set",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::GetServers),
            .params = boost::json::object{},
            .validator = [test_servers](const service::FunctionValue & value)
            {
                return validate_servers_result(value, test_servers);
            }
        },
        // Step 4: Verify timezone is set
        service::LocalTestItem{
            .name = "Step 4: Verify timezone is set",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::GetTimezone),
            .params = boost::json::object{},
            .validator = [test_timezone](const service::FunctionValue & value)
            {
                return validate_timezone_result(value, test_timezone);
            }
        },
        // Step 5: Reset data
        service::LocalTestItem{
            .name = "Step 5: Reset data",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::ResetData),
            .params = boost::json::object{}
        },
        // Step 6: Verify servers are reset to default
        service::LocalTestItem{
            .name = "Step 6: Verify servers are reset",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::GetServers),
            .params = boost::json::object{},
            .validator = [](const service::FunctionValue & value)
            {
                // After reset, servers should be default or empty
                auto array_ptr = std::get_if<boost::json::array>(&value);
                return array_ptr != nullptr;
            }
        },
        // Step 7: Verify timezone is reset to default
        service::LocalTestItem{
            .name = "Step 7: Verify timezone is reset",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::GetTimezone),
            .params = boost::json::object{},
            .validator = [](const service::FunctionValue & value)
            {
                // After reset, timezone should be default or empty
                auto string_ptr = std::get_if<std::string>(&value);
                return string_ptr != nullptr;
            }
        }
    };

    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(SNTPHelper::get_name()), test_items);

    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(results[i], (std::string("Test failed: ") + test_items[i].name).c_str());
    }
}
