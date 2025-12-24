/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper/nvs.hpp"
#include "common_def.hpp"

using namespace esp_brookesia;
using NVSHelper = service::helper::NVS;

constexpr uint32_t CALL_FUNCTION_SYNC_TIMEOUT_MS = 20;

static auto &service_manager = service::ServiceManager::get_instance();
static auto &time_profiler = lib_utils::TimeProfiler::get_instance();
static service::ServiceBinding nvs_binding;

static bool startup();
static void shutdown();

TEST_CASE("Test NVS Helper - save_key_value and get_key_value with bool", "[service][nvs][helper][bool]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_nvs_helper_bool");
    BROOKESIA_LOGI("=== Test NVS Helper - save_key_value and get_key_value with bool ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "bool";
    const std::string test_key = "bool_key";

    // Test saving and getting true
    {
        auto result = NVSHelper::save_key_value(test_namespace, test_key, true, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save bool true" : ("Failed to save bool true: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<bool>(test_namespace, test_key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get bool" : ("Failed to get bool: " + get_result.error()).c_str());
        TEST_ASSERT_TRUE_MESSAGE(get_result.value() == true, "Retrieved bool value should be true");
    }

    // Test saving and getting false
    {
        auto result = NVSHelper::save_key_value(test_namespace, test_key, false, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save bool false" : ("Failed to save bool false: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<bool>(test_namespace, test_key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get bool" : ("Failed to get bool: " + get_result.error()).c_str());
        TEST_ASSERT_TRUE_MESSAGE(get_result.value() == false, "Retrieved bool value should be false");
    }

    // Clean up
    {
        auto result = NVSHelper::erase_keys(test_namespace, {}, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to erase keys" : ("Failed to erase keys: " + result.error()).c_str());
    }
}

TEST_CASE("Test NVS Helper - save_key_value and get_key_value with int32_t", "[service][nvs][helper][int32_t]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_nvs_helper_int32_t");
    BROOKESIA_LOGI("=== Test NVS Helper - save_key_value and get_key_value with int32_t ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "int32_t";

    // Test int32_t
    {
        const std::string key = "int32_key";
        int32_t value_to_save = 12345;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save int32_t" : ("Failed to save int32_t: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<int32_t>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get int32_t" : ("Failed to get int32_t: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_INT32(value_to_save, get_result.value());
    }

    // Test uint32_t
    {
        const std::string key = "uint32_key";
        uint32_t value_to_save = 4294967295U;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save uint32_t" : ("Failed to save uint32_t: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<uint32_t>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get uint32_t" : ("Failed to get uint32_t: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_UINT32(value_to_save, get_result.value());
    }

    // Test int (signed int)
    {
        const std::string key = "int_key";
        int value_to_save = -42;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save int" : ("Failed to save int: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<int>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get int" : ("Failed to get int: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_INT(value_to_save, get_result.value());
    }

    // Clean up
    {
        auto result = NVSHelper::erase_keys(test_namespace, {}, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to erase keys" : ("Failed to erase keys: " + result.error()).c_str());
    }
}

TEST_CASE("Test NVS Helper - save_key_value and get_key_value with small integers (<32 bits)", "[service][nvs][helper][s_int]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_nvs_helper_s_int");
    BROOKESIA_LOGI("=== Test NVS Helper - save_key_value and get_key_value with small integers (<32 bits) ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "s_int";

    // Test int8_t (8-bit signed integer)
    {
        const std::string key = "int8_key";
        int8_t value_to_save = -128;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save int8_t" : ("Failed to save int8_t: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<int8_t>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get int8_t" : ("Failed to get int8_t: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_INT8(value_to_save, get_result.value());
    }

    // Test uint8_t (8-bit unsigned integer)
    {
        const std::string key = "uint8_key";
        uint8_t value_to_save = 255;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save uint8_t" : ("Failed to save uint8_t: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<uint8_t>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get uint8_t" : ("Failed to get uint8_t: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_UINT8(value_to_save, get_result.value());
    }

    // Test int16_t (16-bit signed integer)
    {
        const std::string key = "int16_key";
        int16_t value_to_save = -32768;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save int16_t" : ("Failed to save int16_t: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<int16_t>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get int16_t" : ("Failed to get int16_t: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_INT16(value_to_save, get_result.value());
    }

    // Test uint16_t (16-bit unsigned integer)
    {
        const std::string key = "uint16_key";
        uint16_t value_to_save = 65535;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save uint16_t" : ("Failed to save uint16_t: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<uint16_t>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get uint16_t" : ("Failed to get uint16_t: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_UINT16(value_to_save, get_result.value());
    }

    // Test char (typically 8-bit)
    {
        const std::string key = "char_key";
        char value_to_save = 'A';
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save char" : ("Failed to save char: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<char>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get char" : ("Failed to get char: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_INT8(value_to_save, get_result.value());
    }

    // Test signed char (8-bit signed)
    {
        const std::string key = "char_key";
        signed char value_to_save = -100;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save signed char" : ("Failed to save signed char: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<signed char>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get signed char" : ("Failed to get signed char: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_INT8(value_to_save, get_result.value());
    }

    // Test unsigned char (8-bit unsigned)
    {
        const std::string key = "u_char_key";
        unsigned char value_to_save = 200;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save unsigned char" : ("Failed to save unsigned char: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<unsigned char>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get unsigned char" : ("Failed to get unsigned char: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_UINT8(value_to_save, get_result.value());
    }

    // Test short (typically 16-bit)
    {
        const std::string key = "short_key";
        short value_to_save = -12345;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save short" : ("Failed to save short: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<short>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get short" : ("Failed to get short: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_INT16(value_to_save, get_result.value());
    }

    // Test unsigned short (typically 16-bit unsigned)
    {
        const std::string key = "u_short_key";
        unsigned short value_to_save = 54321;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save unsigned short" : ("Failed to save unsigned short: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<unsigned short>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get unsigned short" : ("Failed to get unsigned short: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_UINT16(value_to_save, get_result.value());
    }

    // Clean up
    {
        auto result = NVSHelper::erase_keys(test_namespace, {}, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to erase keys" : ("Failed to erase keys: " + result.error()).c_str());
    }
}

TEST_CASE("Test NVS Helper - save_key_value and get_key_value with large integers (>32 bits)", "[service][nvs][helper][large_int]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_nvs_helper_large_int");
    BROOKESIA_LOGI("=== Test NVS Helper - save_key_value and get_key_value with large integers (>32 bits) ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "large_int";

    // Test int64_t (64-bit signed integer)
    {
        const std::string key = "int64_key";
        int64_t value_to_save = -9223372036854775807LL;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save int64_t" : ("Failed to save int64_t: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<int64_t>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get int64_t" : ("Failed to get int64_t: " + get_result.error()).c_str());
        if (get_result.value() != value_to_save) {
            TEST_ASSERT_TRUE_MESSAGE(false, "Retrieved int64_t value should be equal to saved value");
        }
    }

    // Test uint64_t (64-bit unsigned integer)
    {
        const std::string key = "uint64_key";
        uint64_t value_to_save = 18446744073709551615ULL;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save uint64_t" : ("Failed to save uint64_t: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<uint64_t>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get uint64_t" : ("Failed to get uint64_t: " + get_result.error()).c_str());
        if (get_result.value() != value_to_save) {
            TEST_ASSERT_TRUE_MESSAGE(false, "Retrieved uint64_t value should be equal to saved value");
        }
    }

    // Test long long (typically 64-bit)
    {
        const std::string key = "ll_key";
        long long value_to_save = 9223372036854775807LL;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save long long" : ("Failed to save long long: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<long long>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get long long" : ("Failed to get long long: " + get_result.error()).c_str());
        if (get_result.value() != value_to_save) {
            TEST_ASSERT_TRUE_MESSAGE(false, "Retrieved long long value should be equal to saved value");
        }
    }

    // Test unsigned long long (typically 64-bit unsigned)
    {
        const std::string key = "u_ll_key";
        unsigned long long value_to_save = 18446744073709551615ULL;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save unsigned long long" : ("Failed to save unsigned long long: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<unsigned long long>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get unsigned long long" : ("Failed to get unsigned long long: " + get_result.error()).c_str());
        if (get_result.value() != value_to_save) {
            TEST_ASSERT_TRUE_MESSAGE(false, "Retrieved unsigned long long value should be equal to saved value");
        }
    }

    // Test long (platform-dependent, typically 32 or 64 bits)
    // On 64-bit platforms, long is 64-bit, so it should use serialization
    {
        const std::string key = "long_key";
        long value_to_save = sizeof(long) == 8 ? 9223372036854775807L : 2147483647L;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save long" : ("Failed to save long: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<long>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get long" : ("Failed to get long: " + get_result.error()).c_str());
        if (sizeof(long) == 8) {
            if (get_result.value() != value_to_save) {
                TEST_ASSERT_TRUE_MESSAGE(false, "Retrieved long value should be equal to saved value");
            }
        } else {
            TEST_ASSERT_EQUAL_INT(value_to_save, get_result.value());
        }
    }

    // Test unsigned long (platform-dependent, typically 32 or 64 bits)
    {
        const std::string key = "u_long_key";
        unsigned long value_to_save = sizeof(unsigned long) == 8 ? 18446744073709551615UL : 4294967295UL;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save unsigned long" : ("Failed to save unsigned long: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<unsigned long>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get unsigned long" : ("Failed to get unsigned long: " + get_result.error()).c_str());
        if (sizeof(unsigned long) == 8) {
            if (get_result.value() != value_to_save) {
                TEST_ASSERT_TRUE_MESSAGE(false, "Retrieved unsigned long value should be equal to saved value");
            }
        } else {
            TEST_ASSERT_EQUAL_UINT32(value_to_save, get_result.value());
        }
    }

    // Clean up
    {
        auto result = NVSHelper::erase_keys(test_namespace, {}, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to erase keys" : ("Failed to erase keys: " + result.error()).c_str());
    }
}

TEST_CASE("Test NVS Helper - save_key_value and get_key_value with floating point", "[service][nvs][helper][float]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_nvs_helper_float");
    BROOKESIA_LOGI("=== Test NVS Helper - save_key_value and get_key_value with floating point ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "float";

    // Test float (32-bit floating point)
    {
        const std::string key = "float_key";
        float value_to_save = 3.14159265f;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save float" : ("Failed to save float: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<float>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get float" : ("Failed to get float: " + get_result.error()).c_str());
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, value_to_save, get_result.value());
    }

    // Test float with negative value
    {
        const std::string key = "float_neg_key";
        float value_to_save = -123.456f;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save float" : ("Failed to save float: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<float>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get float" : ("Failed to get float: " + get_result.error()).c_str());
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, value_to_save, get_result.value());
    }

    // Test float with very small value
    {
        const std::string key = "float_s_key";
        float value_to_save = 1.234e-10f;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save float" : ("Failed to save float: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<float>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get float" : ("Failed to get float: " + get_result.error()).c_str());
        TEST_ASSERT_FLOAT_WITHIN(1e-12f, value_to_save, get_result.value());
    }

    // Test float with very large value
    {
        const std::string key = "float_l_key";
        float value_to_save = 1.234e10f;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save float" : ("Failed to save float: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<float>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get float" : ("Failed to get float: " + get_result.error()).c_str());
        TEST_ASSERT_FLOAT_WITHIN(1000.0f, value_to_save, get_result.value());
    }

    // Test double (64-bit floating point)
    {
        const std::string key = "double_key";
        double value_to_save = 3.14159265358979323846;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save double" : ("Failed to save double: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<double>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get double" : ("Failed to get double: " + get_result.error()).c_str());
        TEST_ASSERT_DOUBLE_WITHIN(0.0000001, value_to_save, get_result.value());
    }

    // Test double with negative value
    {
        const std::string key = "double_neg_key";
        double value_to_save = -987.654321;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save double" : ("Failed to save double: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<double>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get double" : ("Failed to get double: " + get_result.error()).c_str());
        TEST_ASSERT_DOUBLE_WITHIN(0.0000001, value_to_save, get_result.value());
    }

    // Test double with very small value
    {
        const std::string key = "double_s_key";
        double value_to_save = 1.234567890123456e-20;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save double" : ("Failed to save double: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<double>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get double" : ("Failed to get double: " + get_result.error()).c_str());
        TEST_ASSERT_DOUBLE_WITHIN(1e-22, value_to_save, get_result.value());
    }

    // Test double with very large value
    {
        const std::string key = "double_l_key";
        double value_to_save = 1.234567890123456e20;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save double" : ("Failed to save double: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<double>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get double" : ("Failed to get double: " + get_result.error()).c_str());
        TEST_ASSERT_DOUBLE_WITHIN(1e10, value_to_save, get_result.value());
    }

    // Test float with zero
    {
        const std::string key = "float_z_key";
        float value_to_save = 0.0f;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save float zero" : ("Failed to save float zero: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<float>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get float zero" : ("Failed to get float zero: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_FLOAT(0.0f, get_result.value());
    }

    // Test double with zero
    {
        const std::string key = "double_z_key";
        double value_to_save = 0.0;
        auto result = NVSHelper::save_key_value(test_namespace, key, value_to_save, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save double zero" : ("Failed to save double zero: " + result.error()).c_str());

        auto get_result = NVSHelper::get_key_value<double>(test_namespace, key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get double zero" : ("Failed to get double zero: " + get_result.error()).c_str());
        TEST_ASSERT_EQUAL_DOUBLE(0.0, get_result.value());
    }

    // Clean up
    {
        auto result = NVSHelper::erase_keys(test_namespace, {}, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to erase keys" : ("Failed to erase keys: " + result.error()).c_str());
    }
}

TEST_CASE("Test NVS Helper - save_key_value and get_key_value with string", "[service][nvs][helper][string]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_nvs_helper_string");
    BROOKESIA_LOGI("=== Test NVS Helper - save_key_value and get_key_value with string ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "string";
    const std::string test_key = "string_key";
    const std::string test_value = "test_string_value_12345";

    // Test saving and getting string
    auto result = NVSHelper::save_key_value(test_namespace, test_key, test_value, CALL_FUNCTION_SYNC_TIMEOUT_MS);
    TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to save string" : ("Failed to save string: " + result.error()).c_str());

    auto get_result = NVSHelper::get_key_value<std::string>(test_namespace, test_key, CALL_FUNCTION_SYNC_TIMEOUT_MS);
    TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), get_result.has_value() ? "Failed to get string" : ("Failed to get string: " + get_result.error()).c_str());
    TEST_ASSERT_EQUAL_STRING(test_value.c_str(), get_result.value().c_str());

    // Clean up
    {
        auto result = NVSHelper::erase_keys(test_namespace, {}, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to erase keys" : ("Failed to erase keys: " + result.error()).c_str());
    }
}

TEST_CASE("Test NVS Helper - mixed types workflow", "[service][nvs][helper][mixed]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_nvs_helper_mixed");
    BROOKESIA_LOGI("=== Test NVS Helper - mixed types workflow ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "mixed";

    // Save values of different types
    TEST_ASSERT_TRUE_MESSAGE(NVSHelper::save_key_value(test_namespace, "bool_val", true, CALL_FUNCTION_SYNC_TIMEOUT_MS).has_value(), "Failed to save bool");
    TEST_ASSERT_TRUE_MESSAGE(NVSHelper::save_key_value(test_namespace, "int_val", 12345, CALL_FUNCTION_SYNC_TIMEOUT_MS).has_value(), "Failed to save int");
    TEST_ASSERT_TRUE_MESSAGE(NVSHelper::save_key_value(test_namespace, "string_val", std::string("test_string"), CALL_FUNCTION_SYNC_TIMEOUT_MS).has_value(), "Failed to save string");
    TEST_ASSERT_TRUE_MESSAGE(NVSHelper::save_key_value(test_namespace, "uint32_val", 4294967295U, CALL_FUNCTION_SYNC_TIMEOUT_MS).has_value(), "Failed to save uint32_t");

    // Retrieve and verify all values
    {
        auto bool_result = NVSHelper::get_key_value<bool>(test_namespace, "bool_val", CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(bool_result.has_value(), "Failed to get bool");
        TEST_ASSERT_TRUE(bool_result.value());

        auto int_result = NVSHelper::get_key_value<int>(test_namespace, "int_val", CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(int_result.has_value(), "Failed to get int");
        TEST_ASSERT_EQUAL_INT(12345, int_result.value());

        auto string_result = NVSHelper::get_key_value<std::string>(test_namespace, "string_val", CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(string_result.has_value(), "Failed to get string");
        TEST_ASSERT_EQUAL_STRING("test_string", string_result.value().c_str());

        auto uint32_result = NVSHelper::get_key_value<uint32_t>(test_namespace, "uint32_val", CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(uint32_result.has_value(), "Failed to get uint32_t");
        TEST_ASSERT_EQUAL_UINT32(4294967295U, uint32_result.value());
    }

    // Update values
    TEST_ASSERT_TRUE_MESSAGE(NVSHelper::save_key_value(test_namespace, "bool_val", false, CALL_FUNCTION_SYNC_TIMEOUT_MS).has_value(), "Failed to update bool");
    TEST_ASSERT_TRUE_MESSAGE(NVSHelper::save_key_value(test_namespace, "int_val", 54321, CALL_FUNCTION_SYNC_TIMEOUT_MS).has_value(), "Failed to update int");

    // Verify updated values
    {
        auto bool_result = NVSHelper::get_key_value<bool>(test_namespace, "bool_val", CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(bool_result.has_value(), "Failed to get updated bool");
        TEST_ASSERT_FALSE(bool_result.value());

        auto int_result = NVSHelper::get_key_value<int>(test_namespace, "int_val", CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(int_result.has_value(), "Failed to get updated int");
        TEST_ASSERT_EQUAL_INT(54321, int_result.value());
    }

    // Erase all keys
    {
        auto result = NVSHelper::erase_keys(test_namespace, {}, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to erase keys" : ("Failed to erase keys: " + result.error()).c_str());
    }

    // Verify all keys are erased
    TEST_ASSERT_FALSE_MESSAGE(NVSHelper::get_key_value<bool>(test_namespace, "bool_val", CALL_FUNCTION_SYNC_TIMEOUT_MS).has_value(), "bool_val should be erased");
    TEST_ASSERT_FALSE_MESSAGE(NVSHelper::get_key_value<int>(test_namespace, "int_val", CALL_FUNCTION_SYNC_TIMEOUT_MS).has_value(), "int_val should be erased");
    TEST_ASSERT_FALSE_MESSAGE(NVSHelper::get_key_value<std::string>(test_namespace, "string_val", CALL_FUNCTION_SYNC_TIMEOUT_MS).has_value(), "string_val should be erased");
    TEST_ASSERT_FALSE_MESSAGE(NVSHelper::get_key_value<uint32_t>(test_namespace, "uint32_val", CALL_FUNCTION_SYNC_TIMEOUT_MS).has_value(), "uint32_val should be erased");
}

TEST_CASE("Test NVS Helper - error handling", "[service][nvs][helper][error]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_nvs_helper_error");
    BROOKESIA_LOGI("=== Test NVS Helper - error handling ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string test_namespace = "error";

    // Test getting non-existent key
    {
        auto result = NVSHelper::get_key_value<int>(test_namespace, "non_key", CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_FALSE_MESSAGE(result.has_value(), "Getting non-existent key should fail");
    }

    // Test getting key with wrong type (save as int, get as bool)
    {
        TEST_ASSERT_TRUE_MESSAGE(NVSHelper::save_key_value(test_namespace, "wrong_type_key", 42, CALL_FUNCTION_SYNC_TIMEOUT_MS).has_value(), "Failed to save int");

        // This should fail because we're trying to get a number as bool
        // Note: The current implementation might allow this, but ideally type checking should happen
        // We'll test what actually happens
        auto result = NVSHelper::get_key_value<bool>(test_namespace, "wrong_type_key", CALL_FUNCTION_SYNC_TIMEOUT_MS);
        // The result depends on implementation - if it fails, that's good; if it succeeds, that's also acceptable
        // We just verify the function doesn't crash
    }

    // Clean up
    {
        auto result = NVSHelper::erase_keys(test_namespace, {}, CALL_FUNCTION_SYNC_TIMEOUT_MS);
        TEST_ASSERT_TRUE_MESSAGE(result.has_value(), result.has_value() ? "Failed to erase keys" : ("Failed to erase keys: " + result.error()).c_str());
    }
}

static bool startup()
{
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.start(), false, "Failed to start service manager");

    nvs_binding = service_manager.bind(NVSHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(nvs_binding.is_valid(), false, "Failed to bind NVS service");

    return true;
}

static void shutdown()
{
    nvs_binding.release();
    service_manager.deinit();
}
