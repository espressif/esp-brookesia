/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "boost/json.hpp"
#include <string>
#include <vector>
#include <map>
#include <optional>

using namespace esp_brookesia::lib_utils;

// ==================== Test Data Structures (using BROOKESIA_DESCRIBE_STRUCT) ====================

struct Point {
    int x;
    int y;
};
BROOKESIA_DESCRIBE_STRUCT(Point, (), (x, y))

struct Person {
    std::string name;
    int age;
    bool active;
};
BROOKESIA_DESCRIBE_STRUCT(Person, (), (name, age, active))

struct Address {
    std::string city;
    int zip;
};
BROOKESIA_DESCRIBE_STRUCT(Address, (), (city, zip))

struct Company {
    std::string name;
    Address address;
};
BROOKESIA_DESCRIBE_STRUCT(Company, (), (name, address))

struct Container {
    std::vector<int> numbers;
    std::map<std::string, int> settings;
    std::optional<std::string> description;
};
BROOKESIA_DESCRIBE_STRUCT(Container, (), (numbers, settings, description))

// ==================== Test Enums (using BROOKESIA_DESCRIBE_ENUM) ====================

enum class Status {
    Idle = 0,
    Running = 1,
    Stopped = 2,
    Error = 100
};
BROOKESIA_DESCRIBE_ENUM(Status, Idle, Running, Stopped, Error)

struct Task {
    std::string name;
    Status status;
};
BROOKESIA_DESCRIBE_STRUCT(Task, (), (name, status))

// ==================== Test BROOKESIA_DESCRIBE_TO_STR ====================

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_STR - basic types", "[macro][to_str][basic]")
{
    BROOKESIA_LOGI("=== DESCRIBE_TO_STR: Basic Types ===");

    // Bool
    TEST_ASSERT_EQUAL_STRING("true", BROOKESIA_DESCRIBE_TO_STR(true).c_str());
    TEST_ASSERT_EQUAL_STRING("false", BROOKESIA_DESCRIBE_TO_STR(false).c_str());

    // Integer
    TEST_ASSERT_EQUAL_STRING("42", BROOKESIA_DESCRIBE_TO_STR(42).c_str());
    TEST_ASSERT_EQUAL_STRING("-99", BROOKESIA_DESCRIBE_TO_STR(-99).c_str());

    // Float
    std::string float_str = BROOKESIA_DESCRIBE_TO_STR(3.14f);
    TEST_ASSERT_TRUE(float_str.find("3.14") != std::string::npos);

    // String
    TEST_ASSERT_EQUAL_STRING("hello", BROOKESIA_DESCRIBE_TO_STR(std::string("hello")).c_str());
    TEST_ASSERT_EQUAL_STRING("world", BROOKESIA_DESCRIBE_TO_STR("world").c_str());

    BROOKESIA_LOGI("✓ Basic types test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_STR - enum", "[macro][to_str][enum]")
{
    BROOKESIA_LOGI("=== DESCRIBE_TO_STR: Enum ===");

    TEST_ASSERT_EQUAL_STRING("Idle", BROOKESIA_DESCRIBE_TO_STR(Status::Idle).c_str());
    TEST_ASSERT_EQUAL_STRING("Running", BROOKESIA_DESCRIBE_TO_STR(Status::Running).c_str());
    TEST_ASSERT_EQUAL_STRING("Error", BROOKESIA_DESCRIBE_TO_STR(Status::Error).c_str());

    BROOKESIA_LOGI("✓ Enum test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_STR - struct", "[macro][to_str][struct]")
{
    BROOKESIA_LOGI("=== DESCRIBE_TO_STR: Struct ===");

    // Simple struct
    Point p{10, 20};
    std::string result = BROOKESIA_DESCRIBE_TO_STR(p);
    BROOKESIA_LOGI("Point: %1%", result);
    TEST_ASSERT_TRUE(result.find("x") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("10") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("y") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("20") != std::string::npos);

    // Nested struct
    Company company{"TechCorp", {"Beijing", 100000}};
    result = BROOKESIA_DESCRIBE_TO_STR(company);
    BROOKESIA_LOGI("Company: %1%", result);
    TEST_ASSERT_TRUE(result.find("TechCorp") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("Beijing") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("100000") != std::string::npos);

    // Struct with enum
    Task task{"Process", Status::Running};
    result = BROOKESIA_DESCRIBE_TO_STR(task);
    BROOKESIA_LOGI("Task: %1%", result);
    TEST_ASSERT_TRUE(result.find("Process") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("Running") != std::string::npos);

    BROOKESIA_LOGI("✓ Struct test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_STR - containers", "[macro][to_str][containers]")
{
    BROOKESIA_LOGI("=== DESCRIBE_TO_STR: Containers ===");

    // Vector
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::string result = BROOKESIA_DESCRIBE_TO_STR(vec);
    BROOKESIA_LOGI("Vector: %1%", result);
    TEST_ASSERT_TRUE(result.find("[") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("1") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("5") != std::string::npos);

    // Map
    std::map<std::string, int> map = {{"timeout", 30}, {"retry", 3}};
    result = BROOKESIA_DESCRIBE_TO_STR(map);
    BROOKESIA_LOGI("Map: %1%", result);
    TEST_ASSERT_TRUE(result.find("timeout") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("30") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("retry") != std::string::npos);

    // Optional with value
    std::optional<int> opt1 = 42;
    result = BROOKESIA_DESCRIBE_TO_STR(opt1);
    BROOKESIA_LOGI("Optional (with value): %1%", result);
    TEST_ASSERT_TRUE(result.find("42") != std::string::npos);

    // Optional without value
    std::optional<int> opt2 = std::nullopt;
    result = BROOKESIA_DESCRIBE_TO_STR(opt2);
    BROOKESIA_LOGI("Optional (null): %1%", result);
    TEST_ASSERT_TRUE(result.find("null") != std::string::npos);

    // Struct with containers
    Container container;
    container.numbers = {10, 20, 30};
    container.settings["max"] = 100;
    container.description = "test container";
    result = BROOKESIA_DESCRIBE_TO_STR(container);
    BROOKESIA_LOGI("Container: %1%", result);
    TEST_ASSERT_TRUE(result.find("10") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("max") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("test container") != std::string::npos);

    BROOKESIA_LOGI("✓ Containers test passed");
}

// ==================== Test BROOKESIA_DESCRIBE_TO_STR_WITH_FMT ====================

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_STR_WITH_FMT - formats", "[macro][to_str_fmt]")
{
    BROOKESIA_LOGI("=== DESCRIBE_TO_STR_WITH_FMT: Formats ===");

    Point p{100, 200};

    // DEFAULT format
    std::string str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(p, detail::DESCRIBE_FORMAT_DEFAULT);
    BROOKESIA_LOGI("DEFAULT: %1%", str);
    TEST_ASSERT_TRUE(str.find("{ ") != std::string::npos);
    TEST_ASSERT_TRUE(str.find(": ") != std::string::npos);

    // JSON format
    str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(p, detail::DESCRIBE_FORMAT_JSON);
    BROOKESIA_LOGI("JSON: %1%", str);
    TEST_ASSERT_TRUE(str.find("\"x\"") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("\"y\"") != std::string::npos);

    // COMPACT format
    str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(p, detail::DESCRIBE_FORMAT_COMPACT);
    BROOKESIA_LOGI("COMPACT: %1%", str);
    TEST_ASSERT_TRUE(str.find("=") != std::string::npos);
    TEST_ASSERT_FALSE(str.find(", ") != std::string::npos); // No space after comma

    // VERBOSE format
    str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(p, detail::DESCRIBE_FORMAT_VERBOSE);
    BROOKESIA_LOGI("VERBOSE:\n%1%", str);
    TEST_ASSERT_TRUE(str.find("\n") != std::string::npos);
    TEST_ASSERT_TRUE(str.find(" = ") != std::string::npos);

    // PYTHON format
    str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(p, detail::DESCRIBE_FORMAT_PYTHON);
    BROOKESIA_LOGI("PYTHON: %1%", str);
    TEST_ASSERT_TRUE(str.find("{'") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("'}") != std::string::npos);

    // CPP format
    str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(p, detail::DESCRIBE_FORMAT_CPP);
    BROOKESIA_LOGI("CPP: %1%", str);
    TEST_ASSERT_TRUE(str.find(".x = ") != std::string::npos);
    TEST_ASSERT_TRUE(str.find(".y = ") != std::string::npos);

    // Custom format
    detail::DescribeOutputFormat custom;
    custom.struct_begin = "[";
    custom.struct_end = "]";
    custom.field_separator = " | ";
    custom.name_value_separator = " => ";
    str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(p, custom);
    BROOKESIA_LOGI("CUSTOM: %1%", str);
    TEST_ASSERT_TRUE(str.find("[") != std::string::npos);
    TEST_ASSERT_TRUE(str.find(" | ") != std::string::npos);
    TEST_ASSERT_TRUE(str.find(" => ") != std::string::npos);

    BROOKESIA_LOGI("✓ Formats test passed");
}

// ==================== Test BROOKESIA_DESCRIBE_ENUM_TO_NUMBER ====================

TEST_CASE("Test BROOKESIA_DESCRIBE_ENUM_TO_NUMBER", "[macro][enum_to_number]")
{
    BROOKESIA_LOGI("=== DESCRIBE_ENUM_TO_NUMBER ===");

    TEST_ASSERT_EQUAL(0, BROOKESIA_DESCRIBE_ENUM_TO_NUMBER(Status::Idle));
    TEST_ASSERT_EQUAL(1, BROOKESIA_DESCRIBE_ENUM_TO_NUMBER(Status::Running));
    TEST_ASSERT_EQUAL(2, BROOKESIA_DESCRIBE_ENUM_TO_NUMBER(Status::Stopped));
    TEST_ASSERT_EQUAL(100, BROOKESIA_DESCRIBE_ENUM_TO_NUMBER(Status::Error));

    BROOKESIA_LOGI("✓ Enum to number test passed");
}

// ==================== Test BROOKESIA_DESCRIBE_NUM_TO_ENUM ====================

TEST_CASE("Test BROOKESIA_DESCRIBE_NUM_TO_ENUM", "[macro][num_to_enum]")
{
    BROOKESIA_LOGI("=== DESCRIBE_NUM_TO_ENUM ===");

    Status status;

    // Valid conversions
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_NUM_TO_ENUM(0, status));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Idle), static_cast<int>(status));

    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_NUM_TO_ENUM(1, status));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Running), static_cast<int>(status));

    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_NUM_TO_ENUM(100, status));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Error), static_cast<int>(status));

    // Invalid conversion
    TEST_ASSERT_FALSE(BROOKESIA_DESCRIBE_NUM_TO_ENUM(999, status));

    BROOKESIA_LOGI("✓ Number to enum test passed");
}

// ==================== Test BROOKESIA_DESCRIBE_STR_TO_ENUM ====================

TEST_CASE("Test BROOKESIA_DESCRIBE_STR_TO_ENUM", "[macro][str_to_enum]")
{
    BROOKESIA_LOGI("=== DESCRIBE_STR_TO_ENUM ===");

    Status status;

    // Valid conversions
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_STR_TO_ENUM("Idle", status));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Idle), static_cast<int>(status));

    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_STR_TO_ENUM("Running", status));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Running), static_cast<int>(status));

    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_STR_TO_ENUM("Error", status));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Error), static_cast<int>(status));

    // Invalid conversion
    TEST_ASSERT_FALSE(BROOKESIA_DESCRIBE_STR_TO_ENUM("InvalidStatus", status));

    BROOKESIA_LOGI("✓ String to enum test passed");
}

// ==================== Test BROOKESIA_DESCRIBE_STRUCT_TO_JSON ====================

TEST_CASE("Test BROOKESIA_DESCRIBE_STRUCT_TO_JSON", "[macro][struct_to_json]")
{
    BROOKESIA_LOGI("=== DESCRIBE_STRUCT_TO_JSON ===");

    // Simple struct
    Point p{10, 20};
    auto json = BROOKESIA_DESCRIBE_STRUCT_TO_JSON(p);
    BROOKESIA_LOGI("Point JSON: %1%", boost::json::serialize(json));

    TEST_ASSERT_TRUE(json.is_object());
    const auto &obj = json.as_object();
    TEST_ASSERT_TRUE(obj.contains("x"));
    TEST_ASSERT_TRUE(obj.contains("y"));
    TEST_ASSERT_EQUAL(10, obj.at("x").as_int64());
    TEST_ASSERT_EQUAL(20, obj.at("y").as_int64());

    // Nested struct
    Company company{"TechCorp", {"Beijing", 100000}};
    json = BROOKESIA_DESCRIBE_STRUCT_TO_JSON(company);
    BROOKESIA_LOGI("Company JSON: %1%", boost::json::serialize(json));

    TEST_ASSERT_TRUE(json.is_object());
    const auto &company_obj = json.as_object();
    TEST_ASSERT_EQUAL_STRING("TechCorp", company_obj.at("name").as_string().c_str());
    TEST_ASSERT_TRUE(company_obj.at("address").is_object());
    const auto &addr_obj = company_obj.at("address").as_object();
    TEST_ASSERT_EQUAL_STRING("Beijing", addr_obj.at("city").as_string().c_str());
    TEST_ASSERT_EQUAL(100000, addr_obj.at("zip").as_int64());

    // Struct with enum
    Task task{"Process", Status::Running};
    json = BROOKESIA_DESCRIBE_STRUCT_TO_JSON(task);
    BROOKESIA_LOGI("Task JSON: %1%", boost::json::serialize(json));

    TEST_ASSERT_TRUE(json.is_object());
    const auto &task_obj = json.as_object();
    TEST_ASSERT_EQUAL_STRING("Process", task_obj.at("name").as_string().c_str());
    TEST_ASSERT_EQUAL_STRING("Running", task_obj.at("status").as_string().c_str());

    // Struct with containers
    Container container;
    container.numbers = {1, 2, 3};
    container.settings["key"] = 42;
    container.description = "test";
    json = BROOKESIA_DESCRIBE_STRUCT_TO_JSON(container);
    BROOKESIA_LOGI("Container JSON: %1%", boost::json::serialize(json));

    TEST_ASSERT_TRUE(json.is_object());
    const auto &cont_obj = json.as_object();
    TEST_ASSERT_TRUE(cont_obj.at("numbers").is_array());
    TEST_ASSERT_TRUE(cont_obj.at("settings").is_object());
    TEST_ASSERT_EQUAL_STRING("test", cont_obj.at("description").as_string().c_str());

    BROOKESIA_LOGI("✓ Struct to JSON test passed");
}

// ==================== Test BROOKESIA_DESCRIBE_JSON_TO_STRUCT ====================

TEST_CASE("Test BROOKESIA_DESCRIBE_JSON_TO_STRUCT", "[macro][json_to_struct]")
{
    BROOKESIA_LOGI("=== DESCRIBE_JSON_TO_STRUCT ===");

    // Simple struct
    boost::json::value j = boost::json::parse("{\"x\": 30, \"y\": 40}");
    Point p;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_TO_STRUCT(j, p));
    TEST_ASSERT_EQUAL(30, p.x);
    TEST_ASSERT_EQUAL(40, p.y);
    BROOKESIA_LOGI("Point: x=%1%, y=%2%", p.x, p.y);

    // Nested struct
    j = boost::json::parse(
            "{\"name\": \"TechCorp\", \"address\": {\"city\": \"Shanghai\", \"zip\": 200000}}"
        );
    Company company;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_TO_STRUCT(j, company));
    TEST_ASSERT_EQUAL_STRING("TechCorp", company.name.c_str());
    TEST_ASSERT_EQUAL_STRING("Shanghai", company.address.city.c_str());
    TEST_ASSERT_EQUAL(200000, company.address.zip);
    BROOKESIA_LOGI("Company: %1%, %2%", company.name, company.address.city);

    // Struct with enum (string)
    j = boost::json::parse("{\"name\": \"Task1\", \"status\": \"Running\"}");
    Task task;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_TO_STRUCT(j, task));
    TEST_ASSERT_EQUAL_STRING("Task1", task.name.c_str());
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Running), static_cast<int>(task.status));
    BROOKESIA_LOGI("Task: %1%, status=%2%", task.name, BROOKESIA_DESCRIBE_TO_STR(task.status));

    // Struct with enum (number)
    j = boost::json::parse("{\"name\": \"Task2\", \"status\": 2}");
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_TO_STRUCT(j, task));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Stopped), static_cast<int>(task.status));

    // Struct with containers
    j = boost::json::parse(
            "{\"numbers\": [5, 6, 7], \"settings\": {\"max\": 99}, \"description\": \"desc\"}"
        );
    Container container;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_TO_STRUCT(j, container));
    TEST_ASSERT_EQUAL(3, container.numbers.size());
    TEST_ASSERT_EQUAL(5, container.numbers[0]);
    TEST_ASSERT_EQUAL(99, container.settings["max"]);
    TEST_ASSERT_TRUE(container.description.has_value());
    TEST_ASSERT_EQUAL_STRING("desc", container.description.value().c_str());

    // Invalid JSON
    j = boost::json::parse("\"not an object\"");
    TEST_ASSERT_FALSE(BROOKESIA_DESCRIBE_JSON_TO_STRUCT(j, p));

    BROOKESIA_LOGI("✓ JSON to struct test passed");
}

// ==================== Test Round Trip Conversion ====================

TEST_CASE("Test JSON round trip", "[macro][round_trip]")
{
    BROOKESIA_LOGI("=== JSON Round Trip ===");

    // Simple struct
    Point original1{42, 84};
    auto json = BROOKESIA_DESCRIBE_STRUCT_TO_JSON(original1);
    Point converted1;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_TO_STRUCT(json, converted1));
    TEST_ASSERT_EQUAL(original1.x, converted1.x);
    TEST_ASSERT_EQUAL(original1.y, converted1.y);

    // Nested struct
    Company original2{"GlobalCorp", {"Tokyo", 150000}};
    json = BROOKESIA_DESCRIBE_STRUCT_TO_JSON(original2);
    Company converted2;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_TO_STRUCT(json, converted2));
    TEST_ASSERT_EQUAL_STRING(original2.name.c_str(), converted2.name.c_str());
    TEST_ASSERT_EQUAL_STRING(original2.address.city.c_str(), converted2.address.city.c_str());
    TEST_ASSERT_EQUAL(original2.address.zip, converted2.address.zip);

    // Struct with enum
    Task original3{"BatchJob", Status::Error};
    json = BROOKESIA_DESCRIBE_STRUCT_TO_JSON(original3);
    Task converted3;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_TO_STRUCT(json, converted3));
    TEST_ASSERT_EQUAL_STRING(original3.name.c_str(), converted3.name.c_str());
    TEST_ASSERT_EQUAL(static_cast<int>(original3.status), static_cast<int>(converted3.status));

    BROOKESIA_LOGI("✓ Round trip test passed");
}

// ==================== Test Format Management Macros ====================

TEST_CASE("Test BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT", "[macro][format_mgmt]")
{
    BROOKESIA_LOGI("=== DESCRIBE_SET_GLOBAL_FORMAT ===");

    // Get original format
    auto original = BROOKESIA_DESCRIBE_GET_GLOBAL_FORMAT();

    // Set to COMPACT
    BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(detail::DESCRIBE_FORMAT_COMPACT);
    Point p{10, 20};
    std::string result = BROOKESIA_DESCRIBE_TO_STR(p);
    BROOKESIA_LOGI("COMPACT format: %1%", result);
    TEST_ASSERT_TRUE(result.find("=") != std::string::npos);
    TEST_ASSERT_FALSE(result.find(": ") != std::string::npos);

    // Set to JSON
    BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(detail::DESCRIBE_FORMAT_JSON);
    result = BROOKESIA_DESCRIBE_TO_STR(p);
    BROOKESIA_LOGI("JSON format: %1%", result);
    TEST_ASSERT_TRUE(result.find("\"x\"") != std::string::npos);

    // Restore original
    BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(original);

    BROOKESIA_LOGI("✓ Set global format test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_GET_GLOBAL_FORMAT", "[macro][format_mgmt]")
{
    BROOKESIA_LOGI("=== DESCRIBE_GET_GLOBAL_FORMAT ===");

    auto fmt1 = BROOKESIA_DESCRIBE_GET_GLOBAL_FORMAT();
    BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(detail::DESCRIBE_FORMAT_COMPACT);
    auto fmt2 = BROOKESIA_DESCRIBE_GET_GLOBAL_FORMAT();

    // Verify format changed
    TEST_ASSERT_TRUE(fmt1.struct_begin != fmt2.struct_begin ||
                     fmt1.field_separator != fmt2.field_separator);

    // Restore
    BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(fmt1);

    BROOKESIA_LOGI("✓ Get global format test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_RESET_GLOBAL_FORMAT", "[macro][format_mgmt]")
{
    BROOKESIA_LOGI("=== DESCRIBE_RESET_GLOBAL_FORMAT ===");

    // Change format
    BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(detail::DESCRIBE_FORMAT_COMPACT);
    Point p{10, 20};
    std::string result1 = BROOKESIA_DESCRIBE_TO_STR(p);
    BROOKESIA_LOGI("Before reset: %1%", result1);
    TEST_ASSERT_TRUE(result1.find("=") != std::string::npos);

    // Reset to default
    BROOKESIA_DESCRIBE_RESET_GLOBAL_FORMAT();
    std::string result2 = BROOKESIA_DESCRIBE_TO_STR(p);
    BROOKESIA_LOGI("After reset: %1%", result2);
    TEST_ASSERT_TRUE(result2.find(": ") != std::string::npos);
    TEST_ASSERT_FALSE(result2.find("\"x\"") != std::string::npos); // Not JSON format

    BROOKESIA_LOGI("✓ Reset global format test passed");
}

// ==================== Test Combined Usage ====================

TEST_CASE("Test combined macro usage", "[macro][combined]")
{
    BROOKESIA_LOGI("=== Combined Macro Usage ===");

    // Create and display struct
    Task task{"DataProcessing", Status::Running};
    BROOKESIA_LOGI("Task (default): %1%", BROOKESIA_DESCRIBE_TO_STR(task));
    BROOKESIA_LOGI("Task (JSON): %1%", BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(task, detail::DESCRIBE_FORMAT_JSON));

    // Convert to JSON
    auto json = BROOKESIA_DESCRIBE_STRUCT_TO_JSON(task);
    BROOKESIA_LOGI("JSON: %1%", boost::json::serialize(json));

    // Convert back from JSON
    Task task2;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_TO_STRUCT(json, task2));
    BROOKESIA_LOGI("Converted: %1%", BROOKESIA_DESCRIBE_TO_STR(task2));

    // Enum conversions
    auto status_num = BROOKESIA_DESCRIBE_ENUM_TO_NUMBER(task2.status);
    BROOKESIA_LOGI("Status number: %1%", status_num);
    TEST_ASSERT_EQUAL(1, status_num);

    Status status3;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_STR_TO_ENUM("Error", status3));
    BROOKESIA_LOGI("Status from string: %1%", BROOKESIA_DESCRIBE_TO_STR(status3));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Error), static_cast<int>(status3));

    // Format management
    auto old_fmt = BROOKESIA_DESCRIBE_GET_GLOBAL_FORMAT();
    BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(detail::DESCRIBE_FORMAT_VERBOSE);
    BROOKESIA_LOGI("Verbose format:\n%1%", BROOKESIA_DESCRIBE_TO_STR(task));
    BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(old_fmt);

    BROOKESIA_LOGI("✓ Combined usage test passed");
}

// ==================== Test Negative Integer in Struct ====================

TEST_CASE("Test negative integer in struct", "[macro][negative_int]")
{
    BROOKESIA_LOGI("=== Negative Integer in Struct ===");

    // Debug: Check type properties at compile time
    BROOKESIA_LOGI("Debug: int is_signed=%d, is_integral=%d, sizeof=%zu",
                   std::is_signed_v<int>, std::is_integral_v<int>, sizeof(int));
    BROOKESIA_LOGI("Debug: decltype(Point::x) is_signed=%d",
                   std::is_signed_v<decltype(Point::x)>);

    // Test direct int value conversion
    int test_val = -1;
    auto json_test = detail::describe_member_to_json(test_val);
    BROOKESIA_LOGI("Direct int(-1) to JSON: %1%", boost::json::serialize(json_test));
    BROOKESIA_LOGI("Type: int64=%d, uint64=%d", json_test.is_int64(), json_test.is_uint64());

    // Test with negative values
    Point p{-1, -1};

    // Test manual member access
    BROOKESIA_LOGI("Manual p.x value: %d", p.x);
    auto json_x = detail::describe_member_to_json(p.x);
    BROOKESIA_LOGI("Manual p.x to JSON: %1%", boost::json::serialize(json_x));

    std::string result = BROOKESIA_DESCRIBE_TO_STR(p);
    BROOKESIA_LOGI("Point with -1: %1%", result);

    // Verify that -1 is displayed correctly (not as large unsigned number)
    TEST_ASSERT_TRUE(result.find("-1") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("18446744073709551615") == std::string::npos);

    // Test with JSON format
    auto json = BROOKESIA_DESCRIBE_STRUCT_TO_JSON(p);
    BROOKESIA_LOGI("JSON: %1%", boost::json::serialize(json));

    // Verify JSON values
    TEST_ASSERT_TRUE(json.is_object());
    const auto &obj = json.as_object();
    TEST_ASSERT_TRUE(obj.contains("x"));
    TEST_ASSERT_TRUE(obj.contains("y"));

    // Check if values are correctly serialized as signed integers
    BROOKESIA_LOGI("x type: int64=%d, uint64=%d, number=%d",
                   obj.at("x").is_int64(), obj.at("x").is_uint64(), obj.at("x").is_number());
    BROOKESIA_LOGI("x value: %lld", obj.at("x").is_int64() ? obj.at("x").as_int64() : 0);

    // The values should be -1, not large unsigned numbers
    if (obj.at("x").is_int64()) {
        TEST_ASSERT_EQUAL(-1, obj.at("x").as_int64());
        TEST_ASSERT_EQUAL(-1, obj.at("y").as_int64());
    } else {
        BROOKESIA_LOGE("ERROR: x is not stored as int64, type is uint64");
        TEST_FAIL_MESSAGE("JSON should store -1 as int64, not uint64");
    }

    BROOKESIA_LOGI("✓ Negative integer test passed");
}
