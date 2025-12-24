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
#include "brookesia/service_helper/nvs.hpp"
#include "brookesia/service_nvs.hpp"
#include "common_def.hpp"

using namespace esp_brookesia;
using nvs_helper = service::helper::NVS;

static auto &service_manager = service::ServiceManager::get_instance();
static auto &time_profiler = lib_utils::TimeProfiler::get_instance();

static bool startup();
static void shutdown();

// Helper function to validate list result with expected keys
static bool validate_list_result_with_keys(const service::FunctionValue &value, const std::vector<std::string> &expected_keys)
{
    auto array_ptr = std::get_if<boost::json::array>(&value);
    if (!array_ptr) {
        BROOKESIA_LOGE("validate_list_result_with_keys: value is not an array");
        return false;
    }

    // Parse JSON array to EntryInfo vector
    std::vector<nvs_helper::EntryInfo> entries;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(*array_ptr, entries)) {
        BROOKESIA_LOGE("validate_list_result_with_keys: failed to parse JSON array to EntryInfo vector");
        return false;
    }

    // Extract keys from entries
    std::set<std::string> found_keys;
    for (const auto &entry : entries) {
        found_keys.insert(entry.key);
    }

    // Check if all expected keys are present
    for (const auto &key : expected_keys) {
        if (found_keys.find(key) == found_keys.end()) {
            BROOKESIA_LOGE("validate_list_result_with_keys: expected key '%1%' not found. Found keys: %2%",
                           key, boost::json::serialize(BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>(found_keys.begin(), found_keys.end()))));
            return false;
        }
    }
    return true;
}

// Helper function to validate get result
static bool validate_get_result(const service::FunctionValue &value, const std::vector<std::string> &expected_keys)
{
    auto object_ptr = std::get_if<boost::json::object>(&value);
    if (!object_ptr) {
        BROOKESIA_LOGE("validate_get_result: value is not an object");
        return false;
    }

    // Parse JSON object to KeyValue map
    nvs_helper::KeyValueMap pairs;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(*object_ptr, pairs)) {
        BROOKESIA_LOGE("validate_get_result: failed to parse JSON object to KeyValue map");
        return false;
    }

    // Extract keys from key-value map
    std::set<std::string> found_keys;
    for (const auto &[key, value] : pairs) {
        found_keys.insert(key);
    }

    // Check if all expected keys are present
    for (const auto &key : expected_keys) {
        if (found_keys.find(key) == found_keys.end()) {
            BROOKESIA_LOGE("validate_get_result: expected key '%1%' not found. Found keys: %2%",
                           key, boost::json::serialize(BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>(found_keys.begin(), found_keys.end()))));
            return false;
        }
    }
    return true;
}

TEST_CASE("Test ServiceNvs - basic set and get", "[service][nvs][basic]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_nvs_basic");
    BROOKESIA_LOGI("=== Test ServiceNvs - basic set and get ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "test_basic";
    // Define test sequence: set -> get -> erase
    std::vector<service::LocalTestItem> test_items = {
        // Set key-value pairs
        service::LocalTestItem(
            "Set key-value pairs",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Set),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(nvs_helper::KeyValueMap({
                    {"string_key", std::string("test_value")},
                    {"int_key", 42},
                    {"bool_key", true}
                }))
            }
        }
        ),
        // Get key-value pairs
        service::LocalTestItem(
            "Get key-value pairs",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Get),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"string_key", "int_key", "bool_key"}))
            }
        }
        ),
        // Erase key-value pairs
        service::LocalTestItem(
            "Erase key-value pairs",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Erase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"string_key", "int_key", "bool_key"}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(nvs_helper::get_name()), test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(results[i], (std::string("Test failed: ") + test_items[i].name).c_str());
    }
}

TEST_CASE("Test ServiceNvs - list functionality", "[service][nvs][list]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_nvs_list");
    BROOKESIA_LOGI("=== Test ServiceNvs - list functionality ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "test_list";

    // Define test sequence: set -> list -> erase
    std::vector<service::LocalTestItem> test_items = {
        // Set some key-value pairs first
        service::LocalTestItem(
            "Set key-value pairs for list test",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Set),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(nvs_helper::KeyValueMap({
                    {"key1", std::string("value1")},
                    {"key2", 123},
                    {"key3", false}
                }))
            }
        }
        ),
        // List entries
        service::LocalTestItem(
            "List entries in namespace",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::List),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionListParam::Nspace), test_namespace}
        }
        , [](const service::FunctionValue & value)
        {
            return validate_list_result_with_keys(value, {"key1", "key2", "key3"});
        }
        ),
        // Clean up
        service::LocalTestItem(
            "Erase all entries",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Erase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(nvs_helper::get_name()), test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");
}

TEST_CASE("Test ServiceNvs - complete workflow", "[service][nvs][workflow]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_nvs_workflow");
    BROOKESIA_LOGI("=== Test ServiceNvs - complete workflow ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "test_workflow";

    // Define comprehensive test sequence
    std::vector<service::LocalTestItem> test_items = {
        // Step 1: Set multiple key-value pairs
        service::LocalTestItem(
            "Step 1: Set multiple key-value pairs",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Set),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(nvs_helper::KeyValueMap({
                    {"name", std::string("test_user")},
                    {"age", 30},
                    {"active", true},
                    {"score", 95}
                }))
            }
        }
        ),
        // Step 2: Get all keys
        service::LocalTestItem(
            "Step 2: Get all keys",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Get),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"name", "age", "active", "score"}))
            }
        }
        ),
        // Step 3: List entries
        service::LocalTestItem(
            "Step 3: List entries",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::List),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionListParam::Nspace), test_namespace}
        }
        , [](const service::FunctionValue & value)
        {
            return validate_list_result_with_keys(value, {"name", "age", "active", "score"});
        }
        ),
        // Step 4: Update some values
        service::LocalTestItem(
            "Step 4: Update some values",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Set),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(nvs_helper::KeyValueMap({
                    {"age", 31},
                    {"score", 98}
                }))
            }
        }
        ),
        // Step 5: Get updated values
        service::LocalTestItem(
            "Step 5: Get updated values",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Get),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"age", "score"}))
            }
        }
        , [](const service::FunctionValue & value)
        {
            return validate_get_result(value, {"age", "score"});
        }
        ),
        // Step 6: Erase specific keys
        service::LocalTestItem(
            "Step 6: Erase specific keys",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Erase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"score"}))
            }
        }
        ),
        // Step 7: Verify erased key is gone
        service::LocalTestItem(
            "Step 7: Verify remaining keys",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Get),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"name", "age", "active"}))
            }
        }
        , [](const service::FunctionValue & value)
        {
            return validate_get_result(value, {"name", "age", "active"});
        }
        ),
        // Step 8: Erase all remaining keys
        service::LocalTestItem(
            "Step 8: Erase all remaining keys",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Erase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(nvs_helper::get_name()), test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(results[i], (std::string("Test failed: ") + test_items[i].name).c_str());
    }
}

TEST_CASE("Test ServiceNvs - default namespace", "[service][nvs][default]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_nvs_default");
    BROOKESIA_LOGI("=== Test ServiceNvs - default namespace ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Test using default namespace (no namespace parameter)
    std::vector<service::LocalTestItem> test_items = {
        // Set using default namespace
        service::LocalTestItem(
            "Set using default namespace",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Set),
        boost::json::object{
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(nvs_helper::KeyValueMap({
                    {"default_key", std::string("default_value")}
                }))
            }
        }
        ),
        // Get using default namespace
        service::LocalTestItem(
            "Get using default namespace",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Get),
        boost::json::object{
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"default_key"}))
            }
        }
        , [](const service::FunctionValue & value)
        {
            return validate_get_result(value, {"default_key"});
        }
        ),
        // Clean up
        service::LocalTestItem(
            "Erase using default namespace",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Erase),
        boost::json::object{
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"default_key"}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(nvs_helper::get_name()), test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");
}

TEST_CASE("Test ServiceNvs - get all keys when keys array is empty", "[service][nvs][get_all]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_nvs_get_all");
    BROOKESIA_LOGI("=== Test ServiceNvs - get all keys when keys array is empty ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "test_get_all";

    // Define test sequence: set -> get all (empty keys) -> verify
    std::vector<service::LocalTestItem> test_items = {
        // Set multiple key-value pairs
        service::LocalTestItem(
            "Set multiple key-value pairs",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Set),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(nvs_helper::KeyValueMap({
                    {"key1", std::string("value1")},
                    {"key2", 42},
                    {"key3", true},
                    {"key4", std::string("test_string")}
                }))
            }
        }
        ),
        // Get all keys by providing empty keys array
        service::LocalTestItem(
            "Get all keys with empty keys array",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Get),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({}))
            }
        }
        , [](const service::FunctionValue & value)
        {
            return validate_get_result(value, {"key1", "key2", "key3", "key4"});
        }
        ),
        // Get all keys by omitting keys parameter (should use default empty array)
        service::LocalTestItem(
            "Get all keys without keys parameter",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Get),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Nspace), test_namespace}
        }
        , [](const service::FunctionValue & value)
        {
            return validate_get_result(value, {"key1", "key2", "key3", "key4"});
        }
        ),
        // Clean up
        service::LocalTestItem(
            "Erase all entries",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Erase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(nvs_helper::get_name()), test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");
}

TEST_CASE("Test ServiceNvs - handle non-existent keys", "[service][nvs][edge_cases]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_nvs_edge_cases");
    BROOKESIA_LOGI("=== Test ServiceNvs - handle non-existent keys ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "test_edge_cases";

    // Define test sequence: set -> get non-existent -> erase non-existent
    std::vector<service::LocalTestItem> test_items = {
        // Set one key-value pair
        service::LocalTestItem(
            "Set one key-value pair",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Set),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(nvs_helper::KeyValueMap({
                    {"existing_key", std::string("existing_value")}
                }))
            }
        }
        ),
        // Get mix of existing and non-existent keys (should skip non-existent)
        service::LocalTestItem(
            "Get mix of existing and non-existent keys",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Get),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"existing_key", "non_existent_key1", "non_existent_key2"}))
            }
        }
        , [](const service::FunctionValue & value)
        {
            // Should only contain existing_key, non-existent keys should be skipped
            auto object_ptr = std::get_if<boost::json::object>(&value);
            if (!object_ptr) {
                BROOKESIA_LOGE("Get mix of existing and non-existent keys: value is not an object");
                return false;
            }

            // Parse JSON object to KeyValue map
            std::map<std::string, nvs_helper::Value> pairs;
            if (!BROOKESIA_DESCRIBE_FROM_JSON(*object_ptr, pairs)) {
                BROOKESIA_LOGE("Get mix of existing and non-existent keys: failed to parse JSON object to KeyValue map");
                return false;
            }

            // Extract keys from key-value map
            std::set<std::string> found_keys;
            for (const auto &[key, value] : pairs) {
                found_keys.insert(key);
            }

            // Check existing key is present
            if (found_keys.find("existing_key") == found_keys.end()) {
                BROOKESIA_LOGE("Get mix of existing and non-existent keys: expected key 'existing_key' not found. Found keys: %1%",
                               boost::json::serialize(BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>(found_keys.begin(), found_keys.end()))));
                return false;
            }
            // Check non-existent keys are not present
            if (found_keys.find("non_existent_key1") != found_keys.end() ||
                    found_keys.find("non_existent_key2") != found_keys.end()) {
                BROOKESIA_LOGE("Get mix of existing and non-existent keys: unexpected non-existent keys found. Found keys: %1%",
                               boost::json::serialize(BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>(found_keys.begin(), found_keys.end()))));
                return false;
            }
            return true;
        }
        ),
        // Erase mix of existing and non-existent keys (should not fail)
        service::LocalTestItem(
            "Erase mix of existing and non-existent keys",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Erase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"existing_key", "non_existent_key"}))
            }
        }
        ),
        // Verify existing_key is erased
        service::LocalTestItem(
            "Verify key is erased",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Get),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionGetParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"existing_key"}))
            }
        }
        , [](const service::FunctionValue & value)
        {
            // Should return empty array (key not found)
            auto object_ptr = std::get_if<boost::json::object>(&value);
            if (!object_ptr) {
                BROOKESIA_LOGE("Verify key is erased: value is not an object");
                return false;
            }
            if (!object_ptr->empty()) {
                BROOKESIA_LOGE("Verify key is erased: expected empty object but got: %1%",
                               boost::json::serialize(*object_ptr));
                return false;
            }
            return true;
        }
        ),
        // Clean up
        service::LocalTestItem(
            "Erase all entries",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Erase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(nvs_helper::get_name()), test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");
}

TEST_CASE("Test ServiceNvs - list result structure", "[service][nvs][list_structure]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_nvs_list_structure");
    BROOKESIA_LOGI("=== Test ServiceNvs - list result structure ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "test_structure";

    // Define test sequence: set -> list -> verify structure
    std::vector<service::LocalTestItem> test_items = {
        // Set multiple key-value pairs with different types
        service::LocalTestItem(
            "Set multiple key-value pairs with different types",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Set),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(nvs_helper::KeyValueMap({
                    {"string_key", std::string("string_value")},
                    {"int_key", 123},
                    {"bool_key", true}
                }))
            }
        }
        ),
        // List entries and verify structure
        service::LocalTestItem(
            "List entries and verify structure",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::List),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionListParam::Nspace), test_namespace}
        }
        , [test_namespace](const service::FunctionValue & value)
        {
            auto array_ptr = std::get_if<boost::json::array>(&value);
            if (!array_ptr) {
                BROOKESIA_LOGE("List entries and verify structure: value is not an array");
                return false;
            }
            if (array_ptr->size() != 3) {
                BROOKESIA_LOGE("List entries and verify structure: expected 3 entries but got %1%", array_ptr->size());
                return false;
            }

            // Parse JSON object to EntryInfo map
            std::vector<nvs_helper::EntryInfo> entries;
            if (!BROOKESIA_DESCRIBE_FROM_JSON(*array_ptr, entries)) {
                BROOKESIA_LOGE("List entries and verify structure: failed to parse JSON array to EntryInfo vector");
                return false;
            }

            // Verify each entry has required fields: key, type
            for (const auto &entry : entries) {
                // Check key field (should not be empty)
                if (entry.key.empty()) {
                    BROOKESIA_LOGE("List entries and verify structure: entry[%1%] has empty key", entry.key);
                    return false;
                }

                // Check type field
                if (entry.type != nvs_helper::ValueType::String &&
                        entry.type != nvs_helper::ValueType::Int &&
                        entry.type != nvs_helper::ValueType::Bool) {
                    BROOKESIA_LOGE("List entries and verify structure: entry[%1%] has invalid type. Type: '%2%'",
                                   entry.key, BROOKESIA_DESCRIBE_TO_STR(entry.type));
                    return false;
                }
            }
            return true;
        }
        ),
        // Clean up
        service::LocalTestItem(
            "Erase all entries",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(nvs_helper::FunctionId::Erase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(nvs_helper::FunctionEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(nvs_helper::get_name()), test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");
}

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
