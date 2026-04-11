/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#define BROOKESIA_LOG_TAG "Main"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper/nvs.hpp"

/**
 * @brief NVS service usage demos: basic operations vs type-safe operations.
 *
 * - demo_basic_operations(): uses the generic RPC-style API (call_function_sync with
 *   FunctionId::Set/Get/List/Erase). You pass/parse JSON and handle KeyValueMap; suitable
 *   when you need fine-grained control or integration with other RPC callers.
 *
 * - demo_type_safe_operations(): uses the type-safe helpers save_key_value() and
 *   get_key_value(). Types are serialized/deserialized automatically; supports bool,
 *   integers, strings, floats, vectors, and described structs.
 *
 * Prefer the APIs demonstrated in demo_type_safe_operations() for normal application
 * code: they are easier to use and less error-prone than manual JSON handling.
 */

using namespace esp_brookesia;
using NVS_Helper = service::helper::NVS;

constexpr uint32_t DEMO_TIMEOUT_MS = 100;
constexpr const char *DEMO_NAMESPACE_STORAGE = "storage";
constexpr const char *DEMO_NAMESPACE_CONFIG = "config";
constexpr const char *DEMO_NAMESPACE_STATS = "stats";

static bool demo_basic_operations();
static bool demo_type_safe_operations();
static bool demo_namespace_management();
static bool demo_error_handling();

extern "C" void app_main(void)
{
    BROOKESIA_LOGI("\n\n=== NVS Service Example ===\n");

    // Initialize ServiceManager
    auto &service_manager = service::ServiceManager::get_instance();
    BROOKESIA_CHECK_FALSE_EXIT(service_manager.init(), "Failed to initialize ServiceManager");
    BROOKESIA_CHECK_FALSE_EXIT(service_manager.start(), "Failed to start ServiceManager");

    // Use FunctionGuard for cleanup
    lib_utils::FunctionGuard shutdown_guard([&service_manager]() {
        BROOKESIA_LOGI("Shutting down ServiceManager...");
        service_manager.stop();
    });

    // Check if NVS service is available
    BROOKESIA_CHECK_FALSE_EXIT(NVS_Helper::is_available(), "NVS service is not available");
    BROOKESIA_LOGI("NVS service is available");

    // Bind NVS service to start it and its dependencies (RAII - service stays alive while binding exists)
    auto binding = service_manager.bind(NVS_Helper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind NVS service");
    BROOKESIA_LOGI("NVS service bound successfully");

    // Erase all namespaces to avoid last test result affecting the current test
    auto nspaces = {DEMO_NAMESPACE_STORAGE, DEMO_NAMESPACE_CONFIG, DEMO_NAMESPACE_STATS};
    for (const auto &nspace : nspaces) {
        auto erase_result = NVS_Helper::erase_keys(nspace);
        BROOKESIA_CHECK_FALSE_EXIT(erase_result, "Failed to erase namespace: %1%", erase_result.error());
    }

    BROOKESIA_CHECK_FALSE_EXIT(demo_basic_operations(), "Failed to demo basic operations");
    BROOKESIA_CHECK_FALSE_EXIT(demo_type_safe_operations(), "Failed to demo type safe operations");
    BROOKESIA_CHECK_FALSE_EXIT(demo_namespace_management(), "Failed to demo namespace management");
    BROOKESIA_CHECK_FALSE_EXIT(demo_error_handling(), "Failed to demo error handling");

    BROOKESIA_LOGI("\n\n=== NVS Service Example Completed ===\n");
}

/**
 * @brief Demonstrate basic NVS operations: List, Set, Get, Erase
 */
static bool demo_basic_operations()
{
    BROOKESIA_LOGI("\n\n=== Demo: Basic NVS Operations ===\n");

    const std::string nspace = DEMO_NAMESPACE_STORAGE;

    // 1. Set multiple key-value pairs
    BROOKESIA_LOGI("\n\n--- Step 1: Set key-value pairs ---\n");
    NVS_Helper::KeyValueMap kv_pairs = {
        {"key_str", std::string("Espressif")},
        {"key_int", int32_t(100)},
        {"key_bool", bool(true)},
    };
    auto set_result = NVS_Helper::call_function_sync(
                          NVS_Helper::FunctionId::Set,
                          nspace,
                          BROOKESIA_DESCRIBE_TO_JSON(kv_pairs).as_object()
                      );
    BROOKESIA_CHECK_FALSE_RETURN(set_result, false, "Failed to set key-value pairs: %1%", set_result.error());
    BROOKESIA_LOGI("Successfully set %1% key-value pairs", kv_pairs.size());

    // 2. List all entries in namespace
    BROOKESIA_LOGI("\n\n--- Step 2: List entries ---\n");
    auto list_result = NVS_Helper::call_function_sync<boost::json::array>(
                           NVS_Helper::FunctionId::List, nspace
                       );
    BROOKESIA_CHECK_FALSE_RETURN(list_result, false, "Failed to list entries: %1%", list_result.error());
    BROOKESIA_LOGI("Found %1% entries in namespace '%2%'", list_result.value().size(), nspace);
    std::vector<NVS_Helper::EntryInfo> entries;
    BROOKESIA_CHECK_FALSE_RETURN(
        BROOKESIA_DESCRIBE_FROM_JSON(list_result.value(), entries), false, "Failed to parse entries: %1%",
        list_result.error()
    );
    for (const auto &entry : entries) {
        BROOKESIA_LOGI("  - Key: %1%, Type: %2%", entry.key, BROOKESIA_DESCRIBE_ENUM_TO_STR(entry.type));
    }

    // 3. Get specific keys and verify values
    BROOKESIA_LOGI("\n\n--- Step 3: Get specific keys and verify values ---\n");
    std::vector<std::string> keys_to_get = {"key_str", "key_int", "key_bool"};
    auto get_result = NVS_Helper::call_function_sync<boost::json::object>(
                          NVS_Helper::FunctionId::Get,
                          nspace,
                          BROOKESIA_DESCRIBE_TO_JSON(keys_to_get).as_array()
                      );
    BROOKESIA_CHECK_FALSE_RETURN(get_result, false, "Failed to get keys: %1%", get_result.error());
    BROOKESIA_LOGI("Retrieved %1% keys:", get_result.value().size());

    // Verify "key_str" value
    BROOKESIA_CHECK_FALSE_RETURN(
        get_result.value().contains("key_str"), false, "Key 'key_str' not found"
    );
    std::string retrieved_str = std::string(get_result.value().at("key_str").as_string());
    BROOKESIA_LOGI("  - key_str: %1%", retrieved_str);
    BROOKESIA_CHECK_FALSE_RETURN(
        retrieved_str == std::get<std::string>(kv_pairs.at("key_str")), false,
        "Value mismatch for 'key_str': expected '%1%', got '%2%'", std::get<std::string>(kv_pairs.at("key_str")),
        retrieved_str
    );
    BROOKESIA_LOGI("    ✓ Verified: key_str value matches");

    // Verify "key_int" value
    BROOKESIA_CHECK_FALSE_RETURN(
        get_result.value().contains("key_int"), false, "Key 'key_int' not found"
    );
    int32_t retrieved_int = static_cast<int32_t>(get_result.value().at("key_int").as_int64());
    BROOKESIA_LOGI("  - key_int: %1%", retrieved_int);
    BROOKESIA_CHECK_FALSE_RETURN(
        retrieved_int == std::get<int32_t>(kv_pairs.at("key_int")), false,
        "Value mismatch for 'key_int': expected %1%, got %2%", std::get<int32_t>(kv_pairs.at("key_int")), retrieved_int
    );
    BROOKESIA_LOGI("    ✓ Verified: key_int value matches");

    // Verify "key_bool" value
    BROOKESIA_CHECK_FALSE_RETURN(
        get_result.value().contains("key_bool"), false, "Key 'key_bool' not found"
    );
    bool retrieved_bool = get_result.value().at("key_bool").as_bool();
    BROOKESIA_LOGI("  - key_bool: %1%", retrieved_bool);
    BROOKESIA_CHECK_FALSE_RETURN(
        retrieved_bool == std::get<bool>(kv_pairs.at("key_bool")), false,
        "Value mismatch for 'key_bool': expected %1%, got %2%", std::get<bool>(kv_pairs.at("key_bool")), retrieved_bool
    );
    BROOKESIA_LOGI("    ✓ Verified: key_bool value matches");

    BROOKESIA_LOGI("✓ All values verified successfully");

    // 4. Erase specific keys
    BROOKESIA_LOGI("\n\n--- Step 4: Erase specific keys ---\n");
    std::vector<std::string> keys_to_erase = {"key_bool"};
    auto erase_result = NVS_Helper::call_function_sync(
                            NVS_Helper::FunctionId::Erase,
                            nspace,
                            BROOKESIA_DESCRIBE_TO_JSON(keys_to_erase).as_array()
                        );
    BROOKESIA_CHECK_FALSE_RETURN(erase_result, false, "Failed to erase keys: %1%", erase_result.error());
    BROOKESIA_LOGI("Successfully erased %1% keys", keys_to_erase.size());

    // 5. Verify after erase
    BROOKESIA_LOGI("\n\n--- Step 5: Verify after erase ---\n");
    auto verify_result = NVS_Helper::call_function_sync<boost::json::array>(
                             NVS_Helper::FunctionId::List, nspace
                         );
    BROOKESIA_CHECK_FALSE_RETURN(verify_result, false, "Failed to verify after erase: %1%", verify_result.error());
    std::vector<NVS_Helper::EntryInfo> verify_entries;
    BROOKESIA_CHECK_FALSE_RETURN(
        BROOKESIA_DESCRIBE_FROM_JSON(verify_result.value(), verify_entries), false, "Failed to parse entries: %1%",
        verify_result.error()
    );
    BROOKESIA_LOGI("Remaining entries: %1%", verify_entries.size());
    for (const auto &entry : verify_entries) {
        BROOKESIA_LOGI("  - Key: %1%", entry.key);
    }
    BROOKESIA_CHECK_FALSE_RETURN(
        verify_entries.size() == (keys_to_get.size() - keys_to_erase.size()), false, "Remaining entries should be %1%",
        keys_to_get.size() - keys_to_erase.size()
    );

    return true;
}

// Complex struct for NVS operations
enum class Status {
    Idle = 0,
    Running = 1,
    Stopped = 2,
    Error = 100
};
BROOKESIA_DESCRIBE_ENUM(Status, Idle, Running, Stopped, Error)

struct Point {
    int x;
    int y;
};
BROOKESIA_DESCRIBE_STRUCT(Point, (), (x, y))

struct Address {
    std::string city;
    int zip;
};
BROOKESIA_DESCRIBE_STRUCT(Address, (), (city, zip))

struct ComplexStruct {
    // Basic types
    bool flag;
    int number;
    float float_value;
    double double_value;
    std::string text;

    // Enum
    Status status;

    // Containers
    std::vector<int> numbers;
    std::map<std::string, int> settings;
    std::optional<std::string> description;

    // Variant
    std::variant<bool, int, std::string> variant_value;

    // Nested structs
    Point position;
    Address location;

    // JSON value
    boost::json::value json_data;

    bool operator==(const ComplexStruct &other) const
    {
        // Basic types
        if (flag != other.flag) {
            return false;
        }
        if (number != other.number) {
            return false;
        }
        constexpr float float_epsilon = 1e-6f;
        if (std::abs(float_value - other.float_value) >= float_epsilon) {
            return false;
        }
        constexpr double double_epsilon = 1e-9;
        if (std::abs(double_value - other.double_value) >= double_epsilon) {
            return false;
        }
        if (text != other.text) {
            return false;
        }

        // Enum
        if (status != other.status) {
            return false;
        }

        // Containers
        if (numbers != other.numbers) {
            return false;
        }
        if (settings != other.settings) {
            return false;
        }
        if (description != other.description) {
            return false;
        }

        // Variant: compare index and value
        if (variant_value.index() != other.variant_value.index()) {
            return false;
        }
        if (variant_value.index() == 0) {
            if (std::get<bool>(variant_value) != std::get<bool>(other.variant_value)) {
                return false;
            }
        } else if (variant_value.index() == 1) {
            if (std::get<int>(variant_value) != std::get<int>(other.variant_value)) {
                return false;
            }
        } else if (variant_value.index() == 2) {
            if (std::get<std::string>(variant_value) != std::get<std::string>(other.variant_value)) {
                return false;
            }
        }

        // Nested structs
        if (position.x != other.position.x || position.y != other.position.y) {
            return false;
        }
        if (location.city != other.location.city || location.zip != other.location.zip) {
            return false;
        }

        // JSON value
        if (json_data != other.json_data) {
            return false;
        }

        return true;
    }

    bool operator!=(const ComplexStruct &other) const
    {
        return !(*this == other);
    }
};
BROOKESIA_DESCRIBE_STRUCT(ComplexStruct, (),
                          (flag, number, float_value, double_value, text,
                           status,
                           numbers, settings, description,
                           variant_value,
                           position, location,
                           json_data))

/**
 * @brief Demonstrate save_key_value and get_key_value with different types
 */
static bool demo_type_safe_operations()
{
    BROOKESIA_LOGI("\n\n=== Demo: Type-Safe Operations ===\n");

    const std::string nspace = DEMO_NAMESPACE_CONFIG;

    // Direct storage types (bool, int32_t, small integers)
    BROOKESIA_LOGI("\n\n--- Step 1: Direct storage types ---\n");
    {
        // bool
        bool bool_val = true;
        auto save_result = NVS_Helper::save_key_value(nspace, "enable_feature", bool_val);
        BROOKESIA_CHECK_FALSE_RETURN(save_result, false, "Failed to save bool: %1%", save_result.error());
        auto get_bool_result = NVS_Helper::get_key_value<bool>(nspace, "enable_feature");
        BROOKESIA_CHECK_FALSE_RETURN(get_bool_result, false, "Failed to get bool: %1%", get_bool_result.error());
        BROOKESIA_LOGI("Retrieved bool: %1%", get_bool_result.value());
        BROOKESIA_CHECK_FALSE_RETURN(
            get_bool_result.value() == bool_val, false, "Value mismatch for bool: expected %1%, got %2%",
            bool_val, get_bool_result.value()
        );
        BROOKESIA_LOGI("  ✓ Verified: bool value matches");

        // int32_t
        int32_t int32_val = -12345;
        save_result = NVS_Helper::save_key_value(nspace, "int", int32_val);
        BROOKESIA_CHECK_FALSE_RETURN(save_result, false, "Failed to save int32_t: %1%", save_result.error());
        auto get_int32_result = NVS_Helper::get_key_value<int32_t>(nspace, "int");
        BROOKESIA_CHECK_FALSE_RETURN(get_int32_result, false, "Failed to get int32_t: %1%", get_int32_result.error());
        BROOKESIA_LOGI("Retrieved int32_t: %1%", get_int32_result.value());
        BROOKESIA_CHECK_FALSE_RETURN(
            get_int32_result.value() == int32_val, false,
            "Value mismatch for int32_t: expected %1%, got %2%", int32_val, get_int32_result.value()
        );
        BROOKESIA_LOGI("  ✓ Verified: int32_t value matches");

        // uint8_t (small integer, <= 32 bits)
        uint8_t uint8_val = 255;
        save_result = NVS_Helper::save_key_value(nspace, "brightness", uint8_val);
        BROOKESIA_CHECK_FALSE_RETURN(save_result, false, "Failed to save uint8_t: %1%", save_result.error());
        auto get_uint8_result = NVS_Helper::get_key_value<uint8_t>(nspace, "brightness");
        BROOKESIA_CHECK_FALSE_RETURN(get_uint8_result, false, "Failed to get uint8_t: %1%", get_uint8_result.error());
        BROOKESIA_LOGI("Retrieved uint8_t: %1%", get_uint8_result.value());
        BROOKESIA_CHECK_FALSE_RETURN(
            get_uint8_result.value() == uint8_val, false,
            "Value mismatch for uint8_t: expected %1%, got %2%", uint8_val, get_uint8_result.value()
        );
        BROOKESIA_LOGI("  ✓ Verified: uint8_t value matches");

        // int16_t
        int16_t int16_val = -32768;
        save_result = NVS_Helper::save_key_value(nspace, "volume", int16_val);
        BROOKESIA_CHECK_FALSE_RETURN(save_result, false, "Failed to save int16_t: %1%", save_result.error());
        auto get_int16_result = NVS_Helper::get_key_value<int16_t>(nspace, "volume");
        BROOKESIA_CHECK_FALSE_RETURN(get_int16_result, false, "Failed to get int16_t: %1%", get_int16_result.error());
        BROOKESIA_LOGI("Retrieved int16_t: %1%", get_int16_result.value());
        BROOKESIA_CHECK_FALSE_RETURN(
            get_int16_result.value() == int16_val, false,
            "Value mismatch for int16_t: expected %1%, got %2%", int16_val, get_int16_result.value()
        );
        BROOKESIA_LOGI("  ✓ Verified: int16_t value matches");
    }

    // Serialized storage types (string, large integers, floating point, complex types)
    BROOKESIA_LOGI("\n\n--- Step 2: Serialized storage types ---\n");
    {
        // std::string
        std::string str_val = "Hello, NVS!";
        auto save_result = NVS_Helper::save_key_value(nspace, "greeting", str_val);
        BROOKESIA_CHECK_FALSE_RETURN(save_result, false, "Failed to save string: %1%", save_result.error());
        auto get_str_result = NVS_Helper::get_key_value<std::string>(nspace, "greeting");
        BROOKESIA_CHECK_FALSE_RETURN(get_str_result, false, "Failed to get string: %1%", get_str_result.error());
        BROOKESIA_LOGI("Retrieved string: %1%", get_str_result.value());
        BROOKESIA_CHECK_FALSE_RETURN(
            get_str_result.value() == str_val, false,
            "Value mismatch for string: expected '%1%', got '%2%'", str_val, get_str_result.value()
        );
        BROOKESIA_LOGI("  ✓ Verified: string value matches");

        // int64_t (large integer, > 32 bits)
        int64_t int64_val = -9223372036854775807LL;
        save_result = NVS_Helper::save_key_value(nspace, "large_number", int64_val);
        BROOKESIA_CHECK_FALSE_RETURN(save_result, false, "Failed to save int64_t: %1%", save_result.error());
        auto get_int64_result = NVS_Helper::get_key_value<int64_t>(nspace, "large_number");
        BROOKESIA_CHECK_FALSE_RETURN(get_int64_result, false, "Failed to get int64_t: %1%", get_int64_result.error());
        BROOKESIA_LOGI("Retrieved int64_t: %1%", get_int64_result.value());
        BROOKESIA_CHECK_FALSE_RETURN(
            get_int64_result.value() == int64_val, false,
            "Value mismatch for int64_t: expected %1%, got %2%", int64_val, get_int64_result.value()
        );
        BROOKESIA_LOGI("  ✓ Verified: int64_t value matches");

        // uint64_t
        uint64_t uint64_val = 18446744073709551615ULL;
        save_result = NVS_Helper::save_key_value(nspace, "large_unsigned", uint64_val);
        BROOKESIA_CHECK_FALSE_RETURN(save_result, false, "Failed to save uint64_t: %1%", save_result.error());
        auto get_uint64_result = NVS_Helper::get_key_value<uint64_t>(nspace, "large_unsigned");
        BROOKESIA_CHECK_FALSE_RETURN(get_uint64_result, false, "Failed to get uint64_t: %1%", get_uint64_result.error());
        BROOKESIA_LOGI("Retrieved uint64_t: %1%", get_uint64_result.value());
        BROOKESIA_CHECK_FALSE_RETURN(
            get_uint64_result.value() == uint64_val, false,
            "Value mismatch for uint64_t: expected %1%, got %2%", uint64_val, get_uint64_result.value()
        );
        BROOKESIA_LOGI("  ✓ Verified: uint64_t value matches");

        // double (floating point)
        double double_val = 3.14159265358979323846;
        save_result = NVS_Helper::save_key_value(nspace, "pi_value", double_val);
        BROOKESIA_CHECK_FALSE_RETURN(save_result, false, "Failed to save double: %1%", save_result.error());
        auto get_double_result = NVS_Helper::get_key_value<double>(nspace, "pi_value");
        BROOKESIA_CHECK_FALSE_RETURN(get_double_result, false, "Failed to get double: %1%", get_double_result.error());
        BROOKESIA_LOGI("Retrieved double: %1%", get_double_result.value());
        // Use epsilon comparison for floating point
        constexpr double double_epsilon = 1e-9;
        BROOKESIA_CHECK_FALSE_RETURN(
            std::abs(get_double_result.value() - double_val) < double_epsilon, false,
            "Value mismatch for double: expected %1%, got %2%", double_val, get_double_result.value()
        );
        BROOKESIA_LOGI("  ✓ Verified: double value matches");

        // float
        float float_val = 2.71828f;
        save_result = NVS_Helper::save_key_value(nspace, "e_value", float_val);
        BROOKESIA_CHECK_FALSE_RETURN(save_result, false, "Failed to save float: %1%", save_result.error());
        auto get_float_result = NVS_Helper::get_key_value<float>(nspace, "e_value");
        BROOKESIA_CHECK_FALSE_RETURN(get_float_result, false, "Failed to get float: %1%", get_float_result.error());
        BROOKESIA_LOGI("Retrieved float: %1%", get_float_result.value());
        // Use epsilon comparison for floating point
        constexpr float float_epsilon = 1e-6f;
        BROOKESIA_CHECK_FALSE_RETURN(
            std::abs(get_float_result.value() - float_val) < float_epsilon, false,
            "Value mismatch for float: expected %1%, got %2%", float_val, get_float_result.value()
        );
        BROOKESIA_LOGI("  ✓ Verified: float value matches");

        // std::vector<int>
        std::vector<int> vec_val = {1, 2, 3, 4, 5};
        save_result = NVS_Helper::save_key_value(nspace, "number_list", vec_val);
        BROOKESIA_CHECK_FALSE_RETURN(save_result, false, "Failed to save vector: %1%", save_result.error());
        auto get_vec_result = NVS_Helper::get_key_value<std::vector<int>>(nspace, "number_list");
        BROOKESIA_CHECK_FALSE_RETURN(get_vec_result, false, "Failed to get vector: %1%", get_vec_result.error());
        BROOKESIA_LOGI("Retrieved vector: %1%", BROOKESIA_DESCRIBE_TO_STR(get_vec_result.value()));
        BROOKESIA_CHECK_FALSE_RETURN(
            get_vec_result.value() == vec_val, false,
            "Value mismatch for vector: size or content differs"
        );
        BROOKESIA_LOGI("  ✓ Verified: vector value matches");

        // Struct
        ComplexStruct complex_struct = {
            .flag = true,
            .number = 100,
            .float_value = 25.0f,
            .double_value = 25.0,
            .text = "Espressif",
            .status = Status::Running,
            .numbers = {1, 2, 3, 4, 5},
            .settings = {{"timeout", 30}, {"retry", 3}},
            .description = "test description",
            .variant_value = std::string("variant string"),
            .position = Point{100, 200},
            .location = Address{"Beijing", 100000},
            .json_data = boost::json::parse("{\"key\": \"value\"}"),
        };
        save_result = NVS_Helper::save_key_value(nspace, "complex_struct", complex_struct);
        BROOKESIA_CHECK_FALSE_RETURN(save_result, false, "Failed to save complex struct: %1%", save_result.error());
        auto get_struct_result = NVS_Helper::get_key_value<ComplexStruct>(nspace, "complex_struct");
        BROOKESIA_CHECK_FALSE_RETURN(get_struct_result, false, "Failed to get complex struct: %1%", get_struct_result.error());
        const auto &retrieved_struct = get_struct_result.value();
        BROOKESIA_LOGI(
            "Retrieved complex struct: \n%1%",
            BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(retrieved_struct, BROOKESIA_DESCRIBE_FORMAT_VERBOSE)
        );
        BROOKESIA_CHECK_FALSE_RETURN(retrieved_struct == complex_struct, false, "Value mismatch for struct: fields differ");
        BROOKESIA_LOGI("  ✓ Verified: complex struct value matches");
    }

    // Erase all keys in namespace
    auto erase_result = NVS_Helper::erase_keys(nspace, {});
    BROOKESIA_CHECK_FALSE_RETURN(erase_result, false, "Failed to erase keys: %1%", erase_result.error());
    BROOKESIA_LOGI("Successfully erased all keys in namespace '%1%'", nspace);

    return true;
}

/**
 * @brief Demonstrate namespace management
 */
static bool demo_namespace_management()
{
    BROOKESIA_LOGI("\n\n=== Demo: Namespace Management ===\n");

    // Use different namespaces for different purposes
    const std::string storage_nspace = DEMO_NAMESPACE_STORAGE;
    const std::string config_nspace = DEMO_NAMESPACE_CONFIG;
    const std::string stats_nspace = DEMO_NAMESPACE_STATS;

    // Store data in different namespaces
    BROOKESIA_LOGI("\n\n--- Step 1: Store data in different namespaces ---\n");
    {
        auto result1 = NVS_Helper::save_key_value(storage_nspace, "data_key", std::string("storage_data"));
        BROOKESIA_CHECK_FALSE_RETURN(result1, false, "Failed to save to storage namespace: %1%", result1.error());

        auto result2 = NVS_Helper::save_key_value(config_nspace, "data_key", std::string("config_data"));
        BROOKESIA_CHECK_FALSE_RETURN(result2, false, "Failed to save to config namespace: %1%", result2.error());

        auto result3 = NVS_Helper::save_key_value(stats_nspace, "data_key", std::string("stats_data"));
        BROOKESIA_CHECK_FALSE_RETURN(result3, false, "Failed to save to stats namespace: %1%", result3.error());
    }

    // Retrieve from different namespaces
    BROOKESIA_LOGI("\n\n--- Step 2: Retrieve from different namespaces ---\n");
    {
        auto result1 = NVS_Helper::get_key_value<std::string>(storage_nspace, "data_key");
        BROOKESIA_CHECK_FALSE_RETURN(result1, false, "Failed to get from storage namespace: %1%", result1.error());
        BROOKESIA_LOGI("Storage namespace value: %1%", result1.value());

        auto result2 = NVS_Helper::get_key_value<std::string>(config_nspace, "data_key");
        BROOKESIA_CHECK_FALSE_RETURN(result2, false, "Failed to get from config namespace: %1%", result2.error());
        BROOKESIA_LOGI("Config namespace value: %1%", result2.value());

        auto result3 = NVS_Helper::get_key_value<std::string>(stats_nspace, "data_key");
        BROOKESIA_CHECK_FALSE_RETURN(result3, false, "Failed to get from stats namespace: %1%", result3.error());
        BROOKESIA_LOGI("Stats namespace value: %1%", result3.value());
    }

    // List all namespaces (using default namespace listing)
    BROOKESIA_LOGI("\n\n--- Step 3: List entries in each namespace ---\n");
    for (const auto &nspace : {
                storage_nspace, config_nspace, stats_nspace
            }) {
        auto list_result = NVS_Helper::call_function_sync<boost::json::array>(
                               NVS_Helper::FunctionId::List, nspace
                           );
        BROOKESIA_CHECK_FALSE_RETURN(list_result, false, "Failed to list entries in namespace: %1%", list_result.error());

        std::vector<NVS_Helper::EntryInfo> entries;
        BROOKESIA_CHECK_FALSE_RETURN(
            BROOKESIA_DESCRIBE_FROM_JSON(list_result.value(), entries), false, "Failed to parse entries: %1%",
            list_result.error()
        );

        BROOKESIA_LOGI("Namespace '%1%' has %2% entries", nspace, entries.size());
        for (const auto &entry : entries) {
            BROOKESIA_LOGI("  - Key: %1%, Type: %2%", entry.key, BROOKESIA_DESCRIBE_ENUM_TO_STR(entry.type));
        }
    }

    return true;
}

/**
 * @brief Demonstrate error handling
 */
static bool demo_error_handling()
{
    BROOKESIA_LOGI("\n\n=== Demo: Error Handling ===\n");

    const std::string nspace = DEMO_NAMESPACE_STORAGE;

    // Try to get a non-existent key
    BROOKESIA_LOGI("\n\n--- Step 1: Get non-existent key ---\n");
    {
        auto result = NVS_Helper::get_key_value<std::string>(nspace, "non_existent_key", DEMO_TIMEOUT_MS);
        BROOKESIA_CHECK_FALSE_RETURN(!result, false, "Expected error not triggered");
        BROOKESIA_LOGI("Expected error (key not found): %1%", result.error());
    }

    // Try to get with wrong type
    BROOKESIA_LOGI("\n\n--- Step 2: Type mismatch ---\n");
    {
        // First save as string
        auto save_result = NVS_Helper::save_key_value(nspace, "type_test", std::string("test_string"), DEMO_TIMEOUT_MS);
        BROOKESIA_CHECK_FALSE_RETURN(save_result, false, "Failed to save string: %1%", save_result.error());
        // Try to get as int (should fail or return error)
        auto get_result = NVS_Helper::get_key_value<int32_t>(nspace, "type_test", DEMO_TIMEOUT_MS);
        BROOKESIA_CHECK_FALSE_RETURN(!get_result, false, "Expected error (type mismatch): %1%", get_result.error());
        BROOKESIA_LOGI("Expected error (type mismatch): %1%", get_result.error());
    }

    return true;
}
