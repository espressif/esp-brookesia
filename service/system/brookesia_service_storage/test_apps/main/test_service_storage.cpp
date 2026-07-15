/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_manager/service/local_runner.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_storage.hpp"
#include "common_def.hpp"

using namespace esp_brookesia;
using storage_helper = service::helper::Storage;

static auto &service_manager = service::ServiceManager::get_instance();
static auto &time_profiler = lib_utils::TimeProfiler::get_instance();
static service::ServiceBinding storage_binding;

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
    std::vector<storage_helper::EntryInfo> entries;
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
    storage_helper::KeyValueMap pairs;
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

BROOKESIA_TEST_CASE(basic_set_and_get, "Test ServiceStorage - basic set and get", "[service][storage][basic]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_storage_basic");
    BROOKESIA_LOGI("=== Test ServiceStorage - basic set and get ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "test_basic";
    // Define test sequence: set -> get -> erase
    std::vector<service::LocalTestItem> test_items = {
        // Set key-value pairs
        service::LocalTestItem(
            "Set key-value pairs",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVSet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(storage_helper::KeyValueMap({
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
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVGet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"string_key", "int_key", "bool_key"}))
            }
        }
        ),
        // Erase key-value pairs
        service::LocalTestItem(
            "Erase key-value pairs",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVErase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"string_key", "int_key", "bool_key"}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(storage_helper::get_name()), test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(results[i], (std::string("Test failed: ") + test_items[i].name).c_str());
    }
}

#if !defined(ESP_PLATFORM)
BROOKESIA_TEST_CASE(
    pc_absolute_mount_paths,
    "Test ServiceStorage - PC absolute mount paths",
    "[service][storage][pc][mounts]"
)
{
    namespace fs = std::filesystem;

    TEST_ASSERT_TRUE_MESSAGE(fs::exists("/littlefs"), "Expected /littlefs to exist under PC launcher");
    TEST_ASSERT_TRUE_MESSAGE(fs::exists("/sdcard"), "Expected /sdcard to exist under PC launcher");

    const fs::path littlefs_file = "/littlefs/test.txt";
    const fs::path sdcard_file = "/sdcard/test.txt";
    {
        std::ofstream littlefs_stream(littlefs_file, std::ios::trunc);
        TEST_ASSERT_TRUE_MESSAGE(static_cast<bool>(littlefs_stream), "Failed to open /littlefs/test.txt for writing");
        littlefs_stream << "littlefs-smoke";
    }
    {
        std::ofstream sdcard_stream(sdcard_file, std::ios::trunc);
        TEST_ASSERT_TRUE_MESSAGE(static_cast<bool>(sdcard_stream), "Failed to open /sdcard/test.txt for writing");
        sdcard_stream << "sdcard-smoke";
    }

    std::string littlefs_value;
    std::string sdcard_value;
    {
        std::ifstream littlefs_stream(littlefs_file);
        TEST_ASSERT_TRUE_MESSAGE(static_cast<bool>(littlefs_stream), "Failed to open /littlefs/test.txt for reading");
        std::getline(littlefs_stream, littlefs_value);
    }
    {
        std::ifstream sdcard_stream(sdcard_file);
        TEST_ASSERT_TRUE_MESSAGE(static_cast<bool>(sdcard_stream), "Failed to open /sdcard/test.txt for reading");
        std::getline(sdcard_stream, sdcard_value);
    }

    TEST_ASSERT_EQUAL_STRING("littlefs-smoke", littlefs_value.c_str());
    TEST_ASSERT_EQUAL_STRING("sdcard-smoke", sdcard_value.c_str());
}
#endif

BROOKESIA_TEST_CASE(file_system_queries, "Test ServiceStorage - file system queries", "[service][storage][fs]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_storage_fs");
    BROOKESIA_LOGI("=== Test ServiceStorage - file system queries ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto fs_result = storage_helper::call_function_sync<boost::json::array>(
                         storage_helper::FunctionId::GetFileSystems
                     );
    TEST_ASSERT_TRUE_MESSAGE(fs_result.has_value(), "Failed to get storage file systems");

    std::vector<storage_helper::FileSystemInfo> infos;
    TEST_ASSERT_TRUE_MESSAGE(
        BROOKESIA_DESCRIBE_FROM_JSON(fs_result.value(), infos),
        "Failed to parse storage file system infos"
    );
    TEST_ASSERT_FALSE_MESSAGE(infos.empty(), "Expected at least one file system");
    TEST_ASSERT_FALSE_MESSAGE(infos[0].mount_point.empty(), "First file system mount point should not be empty");

    auto capacity_result = storage_helper::call_function_sync<boost::json::object>(
                               storage_helper::FunctionId::GetFileSystemCapacity,
                               infos[0].mount_point
                           );
    TEST_ASSERT_TRUE_MESSAGE(capacity_result.has_value(), "Failed to get storage file system capacity");

    storage_helper::FileSystemCapacity capacity;
    TEST_ASSERT_TRUE_MESSAGE(
        BROOKESIA_DESCRIBE_FROM_JSON(capacity_result.value(), capacity),
        "Failed to parse storage file system capacity"
    );
    TEST_ASSERT_TRUE_MESSAGE(capacity.total_bytes >= capacity.free_bytes, "Invalid file system capacity");
}

BROOKESIA_TEST_CASE(file_system_operations, "Test ServiceStorage - file system operations", "[service][storage][fs]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_storage_fs_operations");
    BROOKESIA_LOGI("=== Test ServiceStorage - file system operations ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string base_path = "/littlefs/storage_fs_api_test";
    const std::string nested_path = base_path + "/nested";
    const std::string text_path = nested_path + "/hello.txt";
    const std::string raw_path = nested_path + "/raw.bin";
    const std::string renamed_path = nested_path + "/hello_renamed.txt";
    const std::string copied_path = base_path + "_copy";

    (void)storage_helper::fs_remove(base_path);
    (void)storage_helper::fs_remove(copied_path);

    auto mkdir_result = storage_helper::fs_mkdir(nested_path);
    TEST_ASSERT_TRUE_MESSAGE(mkdir_result.has_value(), mkdir_result ? "mkdir ok" : mkdir_result.error().c_str());

    auto write_text_result = storage_helper::fs_write_text(text_path, "hello-storage");
    TEST_ASSERT_TRUE_MESSAGE(
        write_text_result.has_value(),
        write_text_result ? "write text ok" : write_text_result.error().c_str()
    );

    auto stat_result = storage_helper::fs_stat(text_path);
    TEST_ASSERT_TRUE_MESSAGE(stat_result.has_value(), stat_result ? "stat ok" : stat_result.error().c_str());
    TEST_ASSERT_TRUE_MESSAGE(stat_result->exists, "Text file should exist");
    TEST_ASSERT_TRUE_MESSAGE(stat_result->type == storage_helper::FileType::File, "Text path should be a file");
    TEST_ASSERT_EQUAL(static_cast<size_t>(13), static_cast<size_t>(stat_result->size));

    auto read_text_result = storage_helper::fs_read_text(text_path);
    TEST_ASSERT_TRUE_MESSAGE(
        read_text_result.has_value(),
        read_text_result ? "read text ok" : read_text_result.error().c_str()
    );
    TEST_ASSERT_EQUAL_STRING("hello-storage", read_text_result->c_str());

    std::vector<uint8_t> write_data(8192);
    for (size_t i = 0; i < write_data.size(); ++i) {
        write_data[i] = static_cast<uint8_t>(i & 0xff);
    }
    auto write_raw_result = storage_helper::fs_write(
                                raw_path,
                                service::RawBuffer(write_data.data(), write_data.size())
                            );
    TEST_ASSERT_TRUE_MESSAGE(
        write_raw_result.has_value(),
        write_raw_result ? "write raw ok" : write_raw_result.error().c_str()
    );

    std::vector<uint8_t> read_data(write_data.size());
    auto read_raw_result = storage_helper::fs_read(
                               raw_path,
                               service::RawBuffer(read_data.data(), read_data.size())
                           );
    TEST_ASSERT_TRUE_MESSAGE(
        read_raw_result.has_value(),
        read_raw_result ? "read raw ok" : read_raw_result.error().c_str()
    );
    TEST_ASSERT_EQUAL(write_data.size(), read_raw_result.value());
    TEST_ASSERT_TRUE_MESSAGE(read_data == write_data, "Raw file contents should roundtrip");

    auto list_result = storage_helper::fs_list(nested_path);
    TEST_ASSERT_TRUE_MESSAGE(list_result.has_value(), list_result ? "list ok" : list_result.error().c_str());
    const auto has_text_file = std::any_of(list_result->begin(), list_result->end(), [](const auto & entry) {
        return entry.name == "hello.txt";
    });
    const auto has_raw_file = std::any_of(list_result->begin(), list_result->end(), [](const auto & entry) {
        return entry.name == "raw.bin";
    });
    TEST_ASSERT_TRUE_MESSAGE(has_text_file, "List should include text file");
    TEST_ASSERT_TRUE_MESSAGE(has_raw_file, "List should include raw file");

    auto rename_result = storage_helper::fs_rename(text_path, renamed_path);
    TEST_ASSERT_TRUE_MESSAGE(rename_result.has_value(), rename_result ? "rename ok" : rename_result.error().c_str());
    auto renamed_stat = storage_helper::fs_stat(renamed_path);
    TEST_ASSERT_TRUE_MESSAGE(
        renamed_stat.has_value() && renamed_stat->exists,
        renamed_stat ? "renamed stat ok" : renamed_stat.error().c_str()
    );

    auto copy_result = storage_helper::fs_copy_tree(base_path, copied_path);
    TEST_ASSERT_TRUE_MESSAGE(copy_result.has_value(), copy_result ? "copy tree ok" : copy_result.error().c_str());
    auto copied_stat = storage_helper::fs_stat(copied_path + "/nested/raw.bin");
    TEST_ASSERT_TRUE_MESSAGE(
        copied_stat.has_value() && copied_stat->exists,
        copied_stat ? "copied raw stat ok" : copied_stat.error().c_str()
    );

    auto relative_result = storage_helper::fs_stat("relative/path");
    TEST_ASSERT_FALSE_MESSAGE(relative_result.has_value(), "Relative path should be rejected");
    auto escape_result = storage_helper::fs_stat("/littlefs/../outside");
    TEST_ASSERT_FALSE_MESSAGE(escape_result.has_value(), "Parent path escape should be rejected");

    auto remove_result = storage_helper::fs_remove(base_path);
    TEST_ASSERT_TRUE_MESSAGE(remove_result.has_value(), remove_result ? "remove ok" : remove_result.error().c_str());
    auto remove_copy_result = storage_helper::fs_remove(copied_path);
    TEST_ASSERT_TRUE_MESSAGE(
        remove_copy_result.has_value(),
        remove_copy_result ? "remove copy ok" : remove_copy_result.error().c_str()
    );
}

#if !defined(ESP_PLATFORM)
BROOKESIA_TEST_CASE(
    file_system_path_security,
    "Test ServiceStorage - PC path containment and symbolic link rejection",
    "[service][storage][pc][security]"
)
{
    namespace fs = std::filesystem;

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const fs::path base_path = "/littlefs/storage_fs_security_test";
    const fs::path source_path = base_path / "source.txt";
    const fs::path link_path = base_path / "sdcard_link";
    const fs::path escaped_path = "/sdcard/storage_fs_symlink_escape.txt";
    std::error_code error_code;
    fs::remove_all(base_path, error_code);
    error_code.clear();
    fs::remove(escaped_path, error_code);
    error_code.clear();
    fs::create_directories(base_path, error_code);
    TEST_ASSERT_FALSE_MESSAGE(static_cast<bool>(error_code), "Failed to create path-security test directory");
    {
        std::ofstream source_stream(source_path, std::ios::trunc);
        TEST_ASSERT_TRUE_MESSAGE(static_cast<bool>(source_stream), "Failed to create path-security source file");
        source_stream << "security-source";
    }
    fs::create_directory_symlink("/sdcard", link_path, error_code);
    TEST_ASSERT_FALSE_MESSAGE(static_cast<bool>(error_code), "Failed to create path-security symbolic link");

    TEST_ASSERT_FALSE_MESSAGE(
        storage_helper::fs_stat(link_path.generic_string()).has_value(),
        "Stat should reject a symbolic link path"
    );
    TEST_ASSERT_FALSE_MESSAGE(
        storage_helper::fs_read_text((link_path / "outside.txt").generic_string()).has_value(),
        "Read should reject a symbolic link component"
    );

    auto storage_service = service_manager.get_service(storage_helper::get_name().data());
    TEST_ASSERT_NOT_NULL_MESSAGE(storage_service.get(), "Storage service should be registered");
    const auto read_text_name = BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::FSReadText);
    const auto path_param_name = BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionFSPathParam::Path);
    auto batch_result = storage_service->call_functions_sync({
        {
            .name = read_text_name,
            .parameters = {{path_param_name, source_path.generic_string()}},
        },
        {
            .name = read_text_name,
            .parameters = {{path_param_name, (link_path / "outside.txt").generic_string()}},
        },
    });
    TEST_ASSERT_FALSE_MESSAGE(batch_result.success, "Batch read should reject a symbolic link component");
    TEST_ASSERT_TRUE_MESSAGE(batch_result.results.size() == 2, "Batch read should report the failed item");
    TEST_ASSERT_TRUE_MESSAGE(batch_result.results[0].success, "Batch read should accept the regular source path");
    TEST_ASSERT_FALSE_MESSAGE(batch_result.results[1].success, "Batch read should reject the symbolic link path");

    TEST_ASSERT_FALSE_MESSAGE(
        storage_helper::fs_write_text((link_path / escaped_path.filename()).generic_string(), "escaped").has_value(),
        "Write should reject a symbolic link component"
    );
    TEST_ASSERT_FALSE_MESSAGE(
        storage_helper::fs_mkdir((link_path / "new_directory").generic_string()).has_value(),
        "Mkdir should reject a symbolic link component"
    );
    TEST_ASSERT_FALSE_MESSAGE(
        storage_helper::fs_remove(link_path.generic_string()).has_value(),
        "Remove should reject a symbolic link path"
    );
    TEST_ASSERT_FALSE_MESSAGE(
        storage_helper::fs_rename(
            source_path.generic_string(), (link_path / "renamed.txt").generic_string()
        ).has_value(),
        "Rename should reject a symbolic link destination"
    );
    TEST_ASSERT_FALSE_MESSAGE(
        storage_helper::fs_copy_tree(
            source_path.generic_string(), (link_path / "copied.txt").generic_string()
        ).has_value(),
        "Copy should reject a symbolic link destination"
    );
    TEST_ASSERT_FALSE_MESSAGE(
        storage_helper::fs_copy_tree(link_path.generic_string(), (base_path / "copy").generic_string()).has_value(),
        "Copy should reject a symbolic link source"
    );
    TEST_ASSERT_FALSE_MESSAGE(
        storage_helper::fs_list(base_path.generic_string()).has_value(),
        "List should reject symbolic links found while enumerating"
    );
    TEST_ASSERT_FALSE_MESSAGE(fs::exists(escaped_path), "Rejected write escaped the mounted file system");

    TEST_ASSERT_FALSE_MESSAGE(
        storage_helper::fs_stat("/littlefs_sibling/outside.txt").has_value(),
        "A similar string prefix must not pass mount containment"
    );
    TEST_ASSERT_FALSE_MESSAGE(
        storage_helper::fs_stat("/etc/passwd").has_value(),
        "An absolute path outside mounted file systems must be rejected"
    );

    error_code.clear();
    fs::remove(link_path, error_code);
    TEST_ASSERT_FALSE_MESSAGE(static_cast<bool>(error_code), "Failed to remove path-security symbolic link");
    auto remove_result = storage_helper::fs_remove(base_path.generic_string());
    TEST_ASSERT_TRUE_MESSAGE(
        remove_result.has_value(), remove_result ? "remove ok" : remove_result.error().c_str()
    );
}
#endif

BROOKESIA_TEST_CASE(list_functionality, "Test ServiceStorage - list functionality", "[service][storage][list]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_storage_list");
    BROOKESIA_LOGI("=== Test ServiceStorage - list functionality ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "test_list";

    // Define test sequence: set -> list -> erase
    std::vector<service::LocalTestItem> test_items = {
        // Set some key-value pairs first
        service::LocalTestItem(
            "Set key-value pairs for list test",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVSet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(storage_helper::KeyValueMap({
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
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVList),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVListParam::Nspace), test_namespace}
        }
        , [](const service::FunctionValue & value)
        {
            return validate_list_result_with_keys(value, {"key1", "key2", "key3"});
        }
        ),
        // Clean up
        service::LocalTestItem(
            "Erase all entries",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVErase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(storage_helper::get_name()), test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");
}

BROOKESIA_TEST_CASE(complete_workflow, "Test ServiceStorage - complete workflow", "[service][storage][workflow]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_storage_workflow");
    BROOKESIA_LOGI("=== Test ServiceStorage - complete workflow ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "test_workflow";

    // Define comprehensive test sequence
    std::vector<service::LocalTestItem> test_items = {
        // Step 1: Set multiple key-value pairs
        service::LocalTestItem(
            "Step 1: Set multiple key-value pairs",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVSet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(storage_helper::KeyValueMap({
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
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVGet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"name", "age", "active", "score"}))
            }
        }
        ),
        // Step 3: List entries
        service::LocalTestItem(
            "Step 3: List entries",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVList),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVListParam::Nspace), test_namespace}
        }
        , [](const service::FunctionValue & value)
        {
            return validate_list_result_with_keys(value, {"name", "age", "active", "score"});
        }
        ),
        // Step 4: Update some values
        service::LocalTestItem(
            "Step 4: Update some values",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVSet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(storage_helper::KeyValueMap({
                    {"age", 31},
                    {"score", 98}
                }))
            }
        }
        ),
        // Step 5: Get updated values
        service::LocalTestItem(
            "Step 5: Get updated values",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVGet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Keys),
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
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVErase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"score"}))
            }
        }
        ),
        // Step 7: Verify erased key is gone
        service::LocalTestItem(
            "Step 7: Verify remaining keys",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVGet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Keys),
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
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVErase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(storage_helper::get_name()), test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(results[i], (std::string("Test failed: ") + test_items[i].name).c_str());
    }
}

BROOKESIA_TEST_CASE(default_namespace, "Test ServiceStorage - default namespace", "[service][storage][default]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_storage_default");
    BROOKESIA_LOGI("=== Test ServiceStorage - default namespace ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Test using default namespace (no namespace parameter)
    std::vector<service::LocalTestItem> test_items = {
        // Set using default namespace
        service::LocalTestItem(
            "Set using default namespace",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVSet),
        boost::json::object{
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(storage_helper::KeyValueMap({
                    {"default_key", std::string("default_value")}
                }))
            }
        }
        ),
        // Get using default namespace
        service::LocalTestItem(
            "Get using default namespace",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVGet),
        boost::json::object{
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Keys),
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
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVErase),
        boost::json::object{
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"default_key"}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(storage_helper::get_name()), test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");
}

BROOKESIA_TEST_CASE(
    get_all_keys_when_keys_array_is_empty,
    "Test ServiceStorage - get all keys when keys array is empty",
    "[service][storage][get_all]"
)
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_storage_get_all");
    BROOKESIA_LOGI("=== Test ServiceStorage - get all keys when keys array is empty ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "test_get_all";

    // Define test sequence: set -> get all (empty keys) -> verify
    std::vector<service::LocalTestItem> test_items = {
        // Set multiple key-value pairs
        service::LocalTestItem(
            "Set multiple key-value pairs",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVSet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(storage_helper::KeyValueMap({
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
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVGet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Keys),
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
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVGet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Nspace), test_namespace}
        }
        , [](const service::FunctionValue & value)
        {
            return validate_get_result(value, {"key1", "key2", "key3", "key4"});
        }
        ),
        // Clean up
        service::LocalTestItem(
            "Erase all entries",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVErase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(storage_helper::get_name()), test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");
}

BROOKESIA_TEST_CASE(
    handle_non_existent_keys,
    "Test ServiceStorage - handle non-existent keys",
    "[service][storage][edge_cases]"
)
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_storage_edge_cases");
    BROOKESIA_LOGI("=== Test ServiceStorage - handle non-existent keys ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "test_edge_cases";

    // Define test sequence: set -> get non-existent -> erase non-existent
    std::vector<service::LocalTestItem> test_items = {
        // Set one key-value pair
        service::LocalTestItem(
            "Set one key-value pair",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVSet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(storage_helper::KeyValueMap({
                    {"existing_key", std::string("existing_value")}
                }))
            }
        }
        ),
        // Get mix of existing and non-existent keys (should skip non-existent)
        service::LocalTestItem(
            "Get mix of existing and non-existent keys",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVGet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Keys),
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
            std::map<std::string, storage_helper::Value> pairs;
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
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVErase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({"existing_key", "non_existent_key"}))
            }
        }
        ),
        // Verify existing_key is erased
        service::LocalTestItem(
            "Verify key is erased",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVGet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVGetParam::Keys),
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
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVErase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(storage_helper::get_name()), test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");
}

BROOKESIA_TEST_CASE(
    list_result_structure,
    "Test ServiceStorage - list result structure",
    "[service][storage][list_structure]"
)
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_service_storage_list_structure");
    BROOKESIA_LOGI("=== Test ServiceStorage - list result structure ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "test_structure";

    // Define test sequence: set -> list -> verify structure
    std::vector<service::LocalTestItem> test_items = {
        // Set multiple key-value pairs with different types
        service::LocalTestItem(
            "Set multiple key-value pairs with different types",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVSet),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVSetParam::KeyValuePairs),
                BROOKESIA_DESCRIBE_TO_JSON(storage_helper::KeyValueMap({
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
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVList),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVListParam::Nspace), test_namespace}
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
            std::vector<storage_helper::EntryInfo> entries;
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
                if (entry.type != storage_helper::ValueType::String &&
                        entry.type != storage_helper::ValueType::Int &&
                        entry.type != storage_helper::ValueType::Bool) {
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
            BROOKESIA_DESCRIBE_ENUM_TO_STR(storage_helper::FunctionId::KVErase),
        boost::json::object{
            {BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Nspace), test_namespace},
            {
                BROOKESIA_DESCRIBE_TO_STR(storage_helper::FunctionKVEraseParam::Keys),
                BROOKESIA_DESCRIBE_TO_JSON(std::vector<std::string>({}))
            }
        }
        )
    };

    // Execute tests
    service::LocalTestRunner runner;
    bool all_passed = runner.run_tests(std::string(storage_helper::get_name()), test_items);

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

    if (!service_manager.start()) {
        BROOKESIA_LOGE("Failed to start service manager");
        return false;
    }
    storage_binding = service_manager.bind(storage_helper::get_name().data());
    if (!storage_binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind storage service");
        service_manager.deinit();
        return false;
    }
    return true;
}

static void shutdown()
{
    storage_binding.release();
    service_manager.deinit();
    time_profiler.report();
    time_profiler.clear();
}
