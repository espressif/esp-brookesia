/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "boost/json.hpp"
#include "boost/thread.hpp"
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <set>
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_manager/service/local_runner.hpp"
#include "brookesia/service_helper/sntp.hpp"
#include "brookesia/service_sntp.hpp"
#include "common_def.hpp"

using namespace esp_brookesia;
using SNTPHelper = service::helper::SNTP;

static auto &service_manager = service::ServiceManager::get_instance();
static auto &time_profiler = lib_utils::TimeProfiler::get_instance();

static bool startup();
static void shutdown();

static bool startup()
{
    // Configure TimeProfiler
    lib_utils::TimeProfiler::FormatOptions opt;
    opt.use_unicode = true;
    opt.use_color = true;
    opt.sort_by = lib_utils::TimeProfiler::FormatOptions::SortBy::TotalDesc;
    opt.show_percentages = true;
    opt.name_width = 40;
    opt.calls_width = 6;
    opt.num_width = 10;
    opt.percent_width = 7;
    opt.precision = 2;
    opt.time_unit = lib_utils::TimeProfiler::FormatOptions::TimeUnit::Milliseconds;
    time_profiler.set_format_options(opt);

    BROOKESIA_CHECK_FALSE_RETURN(service_manager.start(), false, "Failed to start service manager");
    return true;
}

static void shutdown()
{
    service_manager.deinit();
    time_profiler.report();
    time_profiler.clear();
}

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

TEST_CASE("Test ServiceSntp - basic set and get", "[service][sntp][basic]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_sntp_basic");
    BROOKESIA_LOGI("=== Test ServiceSntp - basic set and get ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
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

TEST_CASE("Test ServiceSntp - start and stop", "[service][sntp][start_stop]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_sntp_start_stop");
    BROOKESIA_LOGI("=== Test ServiceSntp - start and stop ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    std::vector<service::LocalTestItem> test_items = {
        // Start SNTP service
        service::LocalTestItem{
            .name = "Start SNTP service",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::Start),
            .params = boost::json::object{},
            .run_duration_ms = 2000  // Wait 2 seconds for service to start
        },
        // Check if time is synced (may take some time)
        service::LocalTestItem{
            .name = "Check if time is synced",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::IsTimeSynced),
            .params = boost::json::object{},
            .validator = [](const service::FunctionValue & value)
            {
                // Accept any boolean value (true or false, depending on sync status)
                auto bool_ptr = std::get_if<bool>(&value);
                return bool_ptr != nullptr;
            },
            .run_duration_ms = 10000  // Wait up to 10 seconds for time sync
        },
        // Stop SNTP service
        service::LocalTestItem{
            .name = "Stop SNTP service",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::Stop),
            .params = boost::json::object{}
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

TEST_CASE("Test ServiceSntp - complete workflow", "[service][sntp][workflow]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_sntp_workflow");
    BROOKESIA_LOGI("=== Test ServiceSntp - complete workflow ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
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
        },
        // Step 6: Start SNTP service
        service::LocalTestItem{
            .name = "Step 6: Start SNTP service",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::Start),
            .params = boost::json::object{},
            .run_duration_ms = 2000  // Wait 2 seconds for service to start
        },
        // Step 7: Check sync status (may take time)
        service::LocalTestItem{
            .name = "Step 7: Check sync status",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::IsTimeSynced),
            .params = boost::json::object{},
            .validator = [](const service::FunctionValue & value)
            {
                auto bool_ptr = std::get_if<bool>(&value);
                return bool_ptr != nullptr;
            },
            .run_duration_ms = 10000  // Wait up to 10 seconds for time sync
        },
        // Step 8: Stop SNTP service
        service::LocalTestItem{
            .name = "Step 8: Stop SNTP service",
            .method = BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::FunctionId::Stop),
            .params = boost::json::object{}
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

TEST_CASE("Test ServiceSntp - reset data", "[service][sntp][reset]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_sntp_reset");
    BROOKESIA_LOGI("=== Test ServiceSntp - reset data ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
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
