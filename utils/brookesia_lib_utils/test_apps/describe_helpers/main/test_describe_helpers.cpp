/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
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
#include <variant>
#include <span>
#include <cmath>

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

    // Optional without value (now serialized as null)
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

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_STR - pointers", "[macro][to_str][pointers]")
{
    BROOKESIA_LOGI("=== DESCRIBE_TO_STR: Pointers ===");

    // Test int pointer
    {
        int value = 42;
        int *int_ptr = &value;
        std::string result = BROOKESIA_DESCRIBE_TO_STR(int_ptr);
        BROOKESIA_LOGI("int*: %1%", result);
        TEST_ASSERT_TRUE(result.find("@0x") == 0); // Should start with @0x
        TEST_ASSERT_TRUE(result.length() > 3); // Should have hex digits after @0x
    }

    // Test void pointer
    {
        int value = 100;
        void *void_ptr = &value;
        std::string result = BROOKESIA_DESCRIBE_TO_STR(void_ptr);
        BROOKESIA_LOGI("void*: %1%", result);
        TEST_ASSERT_TRUE(result.find("@0x") == 0);
    }

    // Test struct pointer
    {
        Point p{10, 20};
        Point *point_ptr = &p;
        std::string result = BROOKESIA_DESCRIBE_TO_STR(point_ptr);
        BROOKESIA_LOGI("Point*: %1%", result);
        TEST_ASSERT_TRUE(result.find("@0x") == 0);
    }

    // Test const pointer
    {
        int value = 200;
        const int *const_int_ptr = &value;
        std::string result = BROOKESIA_DESCRIBE_TO_STR(const_int_ptr);
        BROOKESIA_LOGI("const int*: %1%", result);
        TEST_ASSERT_TRUE(result.find("@0x") == 0);
    }

    // Test pointer to pointer
    {
        int value = 300;
        int *ptr = &value;
        int **ptr_to_ptr = &ptr;
        std::string result = BROOKESIA_DESCRIBE_TO_STR(ptr_to_ptr);
        BROOKESIA_LOGI("int**: %1%", result);
        TEST_ASSERT_TRUE(result.find("@0x") == 0);
    }

    // Test null pointer
    {
        int *null_ptr = nullptr;
        std::string result = BROOKESIA_DESCRIBE_TO_STR(null_ptr);
        BROOKESIA_LOGI("null int*: %1%", result);
        TEST_ASSERT_TRUE(result.find("@0x") == 0);
        // Null pointer should be @0x0 or @0x0000... depending on implementation
        TEST_ASSERT_TRUE(result.find("0") != std::string::npos);
    }

    // Verify char* and const char* are NOT formatted as pointers (they are strings)
    {
        const char *str_ptr = "hello";
        std::string result = BROOKESIA_DESCRIBE_TO_STR(str_ptr);
        BROOKESIA_LOGI("const char*: %1%", result);
        TEST_ASSERT_EQUAL_STRING("hello", result.c_str()); // Should be treated as string, not pointer
        TEST_ASSERT_FALSE(result.find("@0x") == 0); // Should NOT start with @0x
    }

    {
        char str[] = "world";
        char *char_ptr = str;
        std::string result = BROOKESIA_DESCRIBE_TO_STR(char_ptr);
        BROOKESIA_LOGI("char*: %1%", result);
        TEST_ASSERT_EQUAL_STRING("world", result.c_str()); // Should be treated as string, not pointer
        TEST_ASSERT_FALSE(result.find("@0x") == 0); // Should NOT start with @0x
    }

    BROOKESIA_LOGI("✓ Pointers test passed");
}

// ==================== Test BROOKESIA_DESCRIBE_TO_STR_WITH_FMT ====================

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_STR_WITH_FMT - formats", "[macro][to_str_fmt]")
{
    BROOKESIA_LOGI("=== DESCRIBE_TO_STR_WITH_FMT: Formats ===");

    Point p{100, 200};

    // DEFAULT format
    std::string str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(p, BROOKESIA_DESCRIBE_FORMAT_DEFAULT);
    BROOKESIA_LOGI("DEFAULT: %1%", str);
    TEST_ASSERT_TRUE(str.find("{ ") != std::string::npos);
    TEST_ASSERT_TRUE(str.find(": ") != std::string::npos);

    // JSON format
    str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(p, BROOKESIA_DESCRIBE_FORMAT_JSON);
    BROOKESIA_LOGI("JSON: %1%", str);
    TEST_ASSERT_TRUE(str.find("\"x\"") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("\"y\"") != std::string::npos);

    // COMPACT format
    str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(p, BROOKESIA_DESCRIBE_FORMAT_COMPACT);
    BROOKESIA_LOGI("COMPACT: %1%", str);
    TEST_ASSERT_TRUE(str.find("=") != std::string::npos);
    TEST_ASSERT_FALSE(str.find(", ") != std::string::npos); // No space after comma

    // VERBOSE format
    str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(p, BROOKESIA_DESCRIBE_FORMAT_VERBOSE);
    BROOKESIA_LOGI("VERBOSE:\n%1%", str);
    TEST_ASSERT_TRUE(str.find("\n") != std::string::npos);
    TEST_ASSERT_TRUE(str.find(" = ") != std::string::npos);

    // PYTHON format
    str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(p, BROOKESIA_DESCRIBE_FORMAT_PYTHON);
    BROOKESIA_LOGI("PYTHON: %1%", str);
    TEST_ASSERT_TRUE(str.find("{'") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("'}") != std::string::npos);

    // CPP format
    str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(p, BROOKESIA_DESCRIBE_FORMAT_CPP);
    BROOKESIA_LOGI("CPP: %1%", str);
    TEST_ASSERT_TRUE(str.find(".x = ") != std::string::npos);
    TEST_ASSERT_TRUE(str.find(".y = ") != std::string::npos);

    // Custom format
    DescribeOutputFormat custom;
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

// ==================== Test BROOKESIA_DESCRIBE_ENUM_TO_NUM ====================

TEST_CASE("Test BROOKESIA_DESCRIBE_ENUM_TO_NUM", "[macro][enum_to_number]")
{
    BROOKESIA_LOGI("=== DESCRIBE_ENUM_TO_NUMBER ===");

    TEST_ASSERT_EQUAL(0, BROOKESIA_DESCRIBE_ENUM_TO_NUM(Status::Idle));
    TEST_ASSERT_EQUAL(1, BROOKESIA_DESCRIBE_ENUM_TO_NUM(Status::Running));
    TEST_ASSERT_EQUAL(2, BROOKESIA_DESCRIBE_ENUM_TO_NUM(Status::Stopped));
    TEST_ASSERT_EQUAL(100, BROOKESIA_DESCRIBE_ENUM_TO_NUM(Status::Error));

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

// ==================== Test BROOKESIA_DESCRIBE_JSON_SERIALIZE / DESERIALIZE ====================

TEST_CASE("Test BROOKESIA_DESCRIBE_JSON_SERIALIZE - basic types", "[macro][serialize][basic]")
{
    BROOKESIA_LOGI("=== DESCRIBE_SERIALIZE: Basic Types ===");

    // Bool
    TEST_ASSERT_EQUAL_STRING("true", BROOKESIA_DESCRIBE_JSON_SERIALIZE(true).c_str());
    TEST_ASSERT_EQUAL_STRING("false", BROOKESIA_DESCRIBE_JSON_SERIALIZE(false).c_str());

    // Integer
    TEST_ASSERT_EQUAL_STRING("42", BROOKESIA_DESCRIBE_JSON_SERIALIZE(42).c_str());
    TEST_ASSERT_EQUAL_STRING("-99", BROOKESIA_DESCRIBE_JSON_SERIALIZE(-99).c_str());

    // String
    TEST_ASSERT_EQUAL_STRING("\"hello\"", BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::string("hello")).c_str());

    BROOKESIA_LOGI("✓ SERIALIZE basic types test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_JSON_SERIALIZE - struct", "[macro][serialize][struct]")
{
    BROOKESIA_LOGI("=== DESCRIBE_SERIALIZE: Struct ===");

    // Simple struct
    Point p{10, 20};
    std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(p);
    BROOKESIA_LOGI("Point serialized: %1%", json_str);
    TEST_ASSERT_TRUE(json_str.find("\"x\"") != std::string::npos);
    TEST_ASSERT_TRUE(json_str.find("10") != std::string::npos);
    TEST_ASSERT_TRUE(json_str.find("\"y\"") != std::string::npos);
    TEST_ASSERT_TRUE(json_str.find("20") != std::string::npos);

    // Nested struct
    Company company{"TechCorp", {"Beijing", 100000}};
    json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(company);
    BROOKESIA_LOGI("Company serialized: %1%", json_str);
    TEST_ASSERT_TRUE(json_str.find("TechCorp") != std::string::npos);
    TEST_ASSERT_TRUE(json_str.find("Beijing") != std::string::npos);

    // Struct with enum
    Task task{"Process", Status::Running};
    json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(task);
    BROOKESIA_LOGI("Task serialized: %1%", json_str);
    TEST_ASSERT_TRUE(json_str.find("Process") != std::string::npos);
    TEST_ASSERT_TRUE(json_str.find("Running") != std::string::npos);

    BROOKESIA_LOGI("✓ SERIALIZE struct test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_JSON_SERIALIZE - pointers", "[macro][serialize][pointers]")
{
    BROOKESIA_LOGI("=== DESCRIBE_JSON_SERIALIZE: Pointers ===");

    // Test int pointer
    {
        int value = 42;
        int *int_ptr = &value;
        std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(int_ptr);
        BROOKESIA_LOGI("int* serialized: %1%", json_str);
        TEST_ASSERT_TRUE(json_str.find("@0x") != std::string::npos);
        TEST_ASSERT_TRUE(json_str.find("\"") == 0); // Should be JSON string
    }

    // Test void pointer
    {
        int value = 100;
        void *void_ptr = &value;
        std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(void_ptr);
        BROOKESIA_LOGI("void* serialized: %1%", json_str);
        TEST_ASSERT_TRUE(json_str.find("@0x") != std::string::npos);
        TEST_ASSERT_TRUE(json_str.find("\"") == 0); // Should be JSON string
    }

    // Test struct pointer
    {
        Point p{10, 20};
        Point *point_ptr = &p;
        std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(point_ptr);
        BROOKESIA_LOGI("Point* serialized: %1%", json_str);
        TEST_ASSERT_TRUE(json_str.find("@0x") != std::string::npos);
        TEST_ASSERT_TRUE(json_str.find("\"") == 0); // Should be JSON string
    }

    // Test const pointer
    {
        int value = 200;
        const int *const_int_ptr = &value;
        std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(const_int_ptr);
        BROOKESIA_LOGI("const int* serialized: %1%", json_str);
        TEST_ASSERT_TRUE(json_str.find("@0x") != std::string::npos);
        TEST_ASSERT_TRUE(json_str.find("\"") == 0); // Should be JSON string
    }

    // Test null pointer
    {
        int *null_ptr = nullptr;
        std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(null_ptr);
        BROOKESIA_LOGI("null int* serialized: %1%", json_str);
        TEST_ASSERT_TRUE(json_str.find("@0x") != std::string::npos);
        TEST_ASSERT_TRUE(json_str.find("\"") == 0); // Should be JSON string
    }

    // Verify char* and const char* are NOT formatted as pointers (they are strings)
    {
        const char *str_ptr = "hello";
        std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(str_ptr);
        BROOKESIA_LOGI("const char* serialized: %1%", json_str);
        TEST_ASSERT_EQUAL_STRING("\"hello\"", json_str.c_str()); // Should be treated as string, not pointer
        TEST_ASSERT_FALSE(json_str.find("@0x") != std::string::npos); // Should NOT contain @0x
    }

    {
        char str[] = "world";
        char *char_ptr = str;
        std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(char_ptr);
        BROOKESIA_LOGI("char* serialized: %1%", json_str);
        TEST_ASSERT_EQUAL_STRING("\"world\"", json_str.c_str()); // Should be treated as string, not pointer
        TEST_ASSERT_FALSE(json_str.find("@0x") != std::string::npos); // Should NOT contain @0x
    }

    BROOKESIA_LOGI("✓ SERIALIZE pointers test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_JSON_DESERIALIZE - basic types", "[macro][deserialize][basic]")
{
    BROOKESIA_LOGI("=== DESCRIBE_DESERIALIZE: Basic Types ===");

    // Bool
    bool b;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_DESERIALIZE("true", b));
    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_DESERIALIZE("false", b));
    TEST_ASSERT_FALSE(b);

    // Integer
    int i;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_DESERIALIZE("42", i));
    TEST_ASSERT_EQUAL(42, i);
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_DESERIALIZE("-99", i));
    TEST_ASSERT_EQUAL(-99, i);

    // String
    std::string s;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_DESERIALIZE("\"hello\"", s));
    TEST_ASSERT_EQUAL_STRING("hello", s.c_str());

    BROOKESIA_LOGI("✓ DESERIALIZE basic types test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_JSON_DESERIALIZE - struct", "[macro][deserialize][struct]")
{
    BROOKESIA_LOGI("=== DESCRIBE_DESERIALIZE: Struct ===");

    // Simple struct
    Point p;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_DESERIALIZE("{\"x\": 30, \"y\": 40}", p));
    TEST_ASSERT_EQUAL(30, p.x);
    TEST_ASSERT_EQUAL(40, p.y);
    BROOKESIA_LOGI("Point: x=%1%, y=%2%", p.x, p.y);

    // Nested struct
    Company company;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_DESERIALIZE(
                         "{\"name\": \"TechCorp\", \"address\": {\"city\": \"Shanghai\", \"zip\": 200000}}",
                         company));
    TEST_ASSERT_EQUAL_STRING("TechCorp", company.name.c_str());
    TEST_ASSERT_EQUAL_STRING("Shanghai", company.address.city.c_str());
    TEST_ASSERT_EQUAL(200000, company.address.zip);

    // Struct with enum
    Task task;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_DESERIALIZE("{\"name\": \"Task1\", \"status\": \"Running\"}", task));
    TEST_ASSERT_EQUAL_STRING("Task1", task.name.c_str());
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Running), static_cast<int>(task.status));

    // Invalid JSON
    TEST_ASSERT_FALSE(BROOKESIA_DESCRIBE_JSON_DESERIALIZE("invalid json", p));

    BROOKESIA_LOGI("✓ DESERIALIZE struct test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_JSON_DESERIALIZE - pointers", "[macro][deserialize][pointers]")
{
    BROOKESIA_LOGI("=== DESCRIBE_JSON_DESERIALIZE: Pointers ===");

    // Test int pointer - round trip
    {
        int value = 42;
        int *original_ptr = &value;
        std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(original_ptr);
        BROOKESIA_LOGI("Serialized int*: %1%", json_str);

        int *deserialized_ptr = nullptr;
        bool result = BROOKESIA_DESCRIBE_JSON_DESERIALIZE(json_str, deserialized_ptr);
        BROOKESIA_LOGI("int* deserialized result: %1%", static_cast<int>(result));
        TEST_ASSERT_TRUE(result); // Should succeed
        TEST_ASSERT_TRUE(original_ptr == deserialized_ptr); // Addresses should match
    }

    // Test void pointer - round trip
    {
        int value = 100;
        void *original_ptr = &value;
        std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(original_ptr);
        BROOKESIA_LOGI("Serialized void*: %1%", json_str);

        void *deserialized_ptr = nullptr;
        bool result = BROOKESIA_DESCRIBE_JSON_DESERIALIZE(json_str, deserialized_ptr);
        BROOKESIA_LOGI("void* deserialized result: %1%", static_cast<int>(result));
        TEST_ASSERT_TRUE(result); // Should succeed
        TEST_ASSERT_TRUE(original_ptr == deserialized_ptr); // Addresses should match
    }

    // Test struct pointer - round trip
    {
        Point p{10, 20};
        Point *original_ptr = &p;
        std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(original_ptr);
        BROOKESIA_LOGI("Serialized Point*: %1%", json_str);

        Point *deserialized_ptr = nullptr;
        bool result = BROOKESIA_DESCRIBE_JSON_DESERIALIZE(json_str, deserialized_ptr);
        BROOKESIA_LOGI("Point* deserialized result: %1%", static_cast<int>(result));
        TEST_ASSERT_TRUE(result); // Should succeed
        TEST_ASSERT_TRUE(original_ptr == deserialized_ptr); // Addresses should match
    }

    // Test const pointer - round trip
    {
        int value = 200;
        const int *original_ptr = &value;
        std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(original_ptr);
        BROOKESIA_LOGI("Serialized const int*: %1%", json_str);

        const int *deserialized_ptr = nullptr;
        bool result = BROOKESIA_DESCRIBE_JSON_DESERIALIZE(json_str, deserialized_ptr);
        BROOKESIA_LOGI("const int* deserialized result: %1%", static_cast<int>(result));
        TEST_ASSERT_TRUE(result); // Should succeed
        TEST_ASSERT_TRUE(original_ptr == deserialized_ptr); // Addresses should match
    }

    // Test null pointer
    {
        int *null_ptr = nullptr;
        std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(null_ptr);
        BROOKESIA_LOGI("Serialized null int*: %1%", json_str);

        int *deserialized_ptr = reinterpret_cast<int *>(0x12345678); // Set to non-null first
        bool result = BROOKESIA_DESCRIBE_JSON_DESERIALIZE(json_str, deserialized_ptr);
        BROOKESIA_LOGI("null int* deserialized result: %1%", static_cast<int>(result));
        TEST_ASSERT_TRUE(result); // Should succeed
        TEST_ASSERT_TRUE(nullptr == deserialized_ptr); // Should be null
    }

    // Test with invalid JSON format (not a string) - should fail
    {
        int *int_ptr = nullptr;
        bool result = BROOKESIA_DESCRIBE_JSON_DESERIALIZE("12345", int_ptr);
        BROOKESIA_LOGI("int* from number JSON result: %1%", static_cast<int>(result));
        TEST_ASSERT_FALSE(result); // Should fail - not a valid pointer format
    }

    // Test with invalid string format (not @0x...) - should fail
    {
        int *int_ptr = nullptr;
        bool result = BROOKESIA_DESCRIBE_JSON_DESERIALIZE("\"invalid_format\"", int_ptr);
        BROOKESIA_LOGI("int* from invalid string JSON result: %1%", static_cast<int>(result));
        TEST_ASSERT_FALSE(result); // Should fail - not a valid pointer format
    }

    // Test with invalid JSON - should fail
    {
        int *int_ptr = nullptr;
        bool result = BROOKESIA_DESCRIBE_JSON_DESERIALIZE("invalid json", int_ptr);
        BROOKESIA_LOGI("int* from invalid JSON result: %1%", static_cast<int>(result));
        TEST_ASSERT_FALSE(result); // Should fail - invalid JSON
    }

    BROOKESIA_LOGI("✓ DESERIALIZE pointers test passed");
}

TEST_CASE("Test SERIALIZE/DESERIALIZE round trip", "[macro][serialize][round_trip]")
{
    BROOKESIA_LOGI("=== SERIALIZE/DESERIALIZE Round Trip ===");

    // Simple struct
    Point original1{42, 84};
    std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(original1);
    BROOKESIA_LOGI("Serialized: %1%", json_str);
    Point converted1;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_DESERIALIZE(json_str, converted1));
    TEST_ASSERT_EQUAL(original1.x, converted1.x);
    TEST_ASSERT_EQUAL(original1.y, converted1.y);

    // Nested struct
    Company original2{"GlobalCorp", {"Tokyo", 150000}};
    json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(original2);
    BROOKESIA_LOGI("Serialized: %1%", json_str);
    Company converted2;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_DESERIALIZE(json_str, converted2));
    TEST_ASSERT_EQUAL_STRING(original2.name.c_str(), converted2.name.c_str());
    TEST_ASSERT_EQUAL_STRING(original2.address.city.c_str(), converted2.address.city.c_str());
    TEST_ASSERT_EQUAL(original2.address.zip, converted2.address.zip);

    // Struct with containers
    Container original3;
    original3.numbers = {1, 2, 3};
    original3.settings["max"] = 100;
    original3.description = "test";
    json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(original3);
    BROOKESIA_LOGI("Serialized: %1%", json_str);
    Container converted3;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_DESERIALIZE(json_str, converted3));
    TEST_ASSERT_EQUAL(3, converted3.numbers.size());
    TEST_ASSERT_EQUAL(1, converted3.numbers[0]);
    TEST_ASSERT_EQUAL(100, converted3.settings["max"]);
    TEST_ASSERT_TRUE(converted3.description.has_value());
    TEST_ASSERT_EQUAL_STRING("test", converted3.description.value().c_str());

    // Pointer round trip
    {
        int value = 999;
        int *original_ptr = &value;
        std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(original_ptr);
        BROOKESIA_LOGI("Pointer serialized: %1%", json_str);
        int *converted_ptr = nullptr;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_JSON_DESERIALIZE(json_str, converted_ptr));
        TEST_ASSERT_TRUE(original_ptr == converted_ptr); // Addresses should match
    }

    BROOKESIA_LOGI("✓ SERIALIZE/DESERIALIZE round trip test passed");
}

// ==================== Test BROOKESIA_DESCRIBE_TO_JSON ====================

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_JSON", "[macro][struct_to_json]")
{
    BROOKESIA_LOGI("=== DESCRIBE_STRUCT_TO_JSON ===");

    // Simple struct
    Point p{10, 20};
    auto json = BROOKESIA_DESCRIBE_TO_JSON(p);
    BROOKESIA_LOGI("Point JSON: %1%", boost::json::serialize(json));

    TEST_ASSERT_TRUE(json.is_object());
    const auto &obj = json.as_object();
    TEST_ASSERT_TRUE(obj.contains("x"));
    TEST_ASSERT_TRUE(obj.contains("y"));
    TEST_ASSERT_EQUAL(10, obj.at("x").as_int64());
    TEST_ASSERT_EQUAL(20, obj.at("y").as_int64());

    // Nested struct
    Company company{"TechCorp", {"Beijing", 100000}};
    json = BROOKESIA_DESCRIBE_TO_JSON(company);
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
    json = BROOKESIA_DESCRIBE_TO_JSON(task);
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
    json = BROOKESIA_DESCRIBE_TO_JSON(container);
    BROOKESIA_LOGI("Container JSON: %1%", boost::json::serialize(json));

    TEST_ASSERT_TRUE(json.is_object());
    const auto &cont_obj = json.as_object();
    TEST_ASSERT_TRUE(cont_obj.at("numbers").is_array());
    TEST_ASSERT_TRUE(cont_obj.at("settings").is_object());
    TEST_ASSERT_EQUAL_STRING("test", cont_obj.at("description").as_string().c_str());

    BROOKESIA_LOGI("✓ Struct to JSON test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_JSON - pointers", "[macro][to_json][pointers]")
{
    BROOKESIA_LOGI("=== DESCRIBE_TO_JSON: Pointers ===");

    // Test int pointer
    {
        int value = 42;
        int *int_ptr = &value;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(int_ptr);
        BROOKESIA_LOGI("int* JSON: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_string());
        std::string json_str = boost::json::value_to<std::string>(json);
        TEST_ASSERT_TRUE(json_str.find("@0x") == 0); // Should start with @0x
        TEST_ASSERT_TRUE(json_str.length() > 3); // Should have hex digits after @0x
    }

    // Test void pointer
    {
        int value = 100;
        void *void_ptr = &value;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(void_ptr);
        BROOKESIA_LOGI("void* JSON: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_string());
        std::string json_str = boost::json::value_to<std::string>(json);
        TEST_ASSERT_TRUE(json_str.find("@0x") == 0);
    }

    // Test struct pointer
    {
        Point p{10, 20};
        Point *point_ptr = &p;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(point_ptr);
        BROOKESIA_LOGI("Point* JSON: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_string());
        std::string json_str = boost::json::value_to<std::string>(json);
        TEST_ASSERT_TRUE(json_str.find("@0x") == 0);
    }

    // Test const pointer
    {
        int value = 200;
        const int *const_int_ptr = &value;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(const_int_ptr);
        BROOKESIA_LOGI("const int* JSON: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_string());
        std::string json_str = boost::json::value_to<std::string>(json);
        TEST_ASSERT_TRUE(json_str.find("@0x") == 0);
    }

    // Test pointer to pointer
    {
        int value = 300;
        int *ptr = &value;
        int **ptr_to_ptr = &ptr;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(ptr_to_ptr);
        BROOKESIA_LOGI("int** JSON: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_string());
        std::string json_str = boost::json::value_to<std::string>(json);
        TEST_ASSERT_TRUE(json_str.find("@0x") == 0);
    }

    // Test null pointer
    {
        int *null_ptr = nullptr;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(null_ptr);
        BROOKESIA_LOGI("null int* JSON: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_string());
        std::string json_str = boost::json::value_to<std::string>(json);
        TEST_ASSERT_TRUE(json_str.find("@0x") == 0);
        // Null pointer should be @0x0 or @0x0000... depending on implementation
        TEST_ASSERT_TRUE(json_str.find("0") != std::string::npos);
    }

    // Verify char* and const char* are NOT formatted as pointers (they are strings)
    {
        const char *str_ptr = "hello";
        auto json = BROOKESIA_DESCRIBE_TO_JSON(str_ptr);
        BROOKESIA_LOGI("const char* JSON: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_string());
        TEST_ASSERT_EQUAL_STRING("hello", boost::json::value_to<std::string>(json).c_str()); // Should be treated as string, not pointer
        std::string json_str = boost::json::value_to<std::string>(json);
        TEST_ASSERT_FALSE(json_str.find("@0x") == 0); // Should NOT start with @0x
    }

    {
        char str[] = "world";
        char *char_ptr = str;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(char_ptr);
        BROOKESIA_LOGI("char* JSON: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_string());
        TEST_ASSERT_EQUAL_STRING("world", boost::json::value_to<std::string>(json).c_str()); // Should be treated as string, not pointer
        std::string json_str = boost::json::value_to<std::string>(json);
        TEST_ASSERT_FALSE(json_str.find("@0x") == 0); // Should NOT start with @0x
    }

    BROOKESIA_LOGI("✓ Pointers to JSON test passed");
}

// ==================== Test BROOKESIA_DESCRIBE_FROM_JSON ====================

TEST_CASE("Test BROOKESIA_DESCRIBE_FROM_JSON", "[macro][json_to_struct]")
{
    BROOKESIA_LOGI("=== DESCRIBE_JSON_TO_STRUCT ===");

    // Simple struct
    boost::json::value j = boost::json::parse("{\"x\": 30, \"y\": 40}");
    Point p;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(j, p));
    TEST_ASSERT_EQUAL(30, p.x);
    TEST_ASSERT_EQUAL(40, p.y);
    BROOKESIA_LOGI("Point: x=%1%, y=%2%", p.x, p.y);

    // Nested struct
    j = boost::json::parse(
            "{\"name\": \"TechCorp\", \"address\": {\"city\": \"Shanghai\", \"zip\": 200000}}"
        );
    Company company;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(j, company));
    TEST_ASSERT_EQUAL_STRING("TechCorp", company.name.c_str());
    TEST_ASSERT_EQUAL_STRING("Shanghai", company.address.city.c_str());
    TEST_ASSERT_EQUAL(200000, company.address.zip);
    BROOKESIA_LOGI("Company: %1%, %2%", company.name, company.address.city);

    // Struct with enum (string)
    j = boost::json::parse("{\"name\": \"Task1\", \"status\": \"Running\"}");
    Task task;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(j, task));
    TEST_ASSERT_EQUAL_STRING("Task1", task.name.c_str());
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Running), static_cast<int>(task.status));
    BROOKESIA_LOGI("Task: %1%, status=%2%", task.name, BROOKESIA_DESCRIBE_TO_STR(task.status));

    // Struct with enum (number)
    j = boost::json::parse("{\"name\": \"Task2\", \"status\": 2}");
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(j, task));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Stopped), static_cast<int>(task.status));

    // Struct with containers
    j = boost::json::parse(
            "{\"numbers\": [5, 6, 7], \"settings\": {\"max\": 99}, \"description\": \"desc\"}"
        );
    Container container;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(j, container));
    TEST_ASSERT_EQUAL(3, container.numbers.size());
    TEST_ASSERT_EQUAL(5, container.numbers[0]);
    TEST_ASSERT_EQUAL(99, container.settings["max"]);
    TEST_ASSERT_TRUE(container.description.has_value());
    TEST_ASSERT_EQUAL_STRING("desc", container.description.value().c_str());

    // Invalid JSON
    j = boost::json::parse("\"not an object\"");
    TEST_ASSERT_FALSE(BROOKESIA_DESCRIBE_FROM_JSON(j, p));

    BROOKESIA_LOGI("✓ JSON to struct test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_FROM_JSON - pointers", "[macro][from_json][pointers]")
{
    BROOKESIA_LOGI("=== DESCRIBE_FROM_JSON: Pointers ===");

    // Test int pointer - round trip
    {
        int value = 42;
        int *original_ptr = &value;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(original_ptr);
        BROOKESIA_LOGI("Serialized int*: %1%", boost::json::serialize(json));

        int *deserialized_ptr = nullptr;
        bool result = BROOKESIA_DESCRIBE_FROM_JSON(json, deserialized_ptr);
        BROOKESIA_LOGI("int* from JSON result: %1%", static_cast<int>(result));
        TEST_ASSERT_TRUE(result); // Should succeed
        TEST_ASSERT_TRUE(original_ptr == deserialized_ptr); // Addresses should match
    }

    // Test void pointer - round trip
    {
        int value = 100;
        void *original_ptr = &value;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(original_ptr);
        BROOKESIA_LOGI("Serialized void*: %1%", boost::json::serialize(json));

        void *deserialized_ptr = nullptr;
        bool result = BROOKESIA_DESCRIBE_FROM_JSON(json, deserialized_ptr);
        BROOKESIA_LOGI("void* from JSON result: %1%", static_cast<int>(result));
        TEST_ASSERT_TRUE(result); // Should succeed
        TEST_ASSERT_TRUE(original_ptr == deserialized_ptr); // Addresses should match
    }

    // Test struct pointer - round trip
    {
        Point p{10, 20};
        Point *original_ptr = &p;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(original_ptr);
        BROOKESIA_LOGI("Serialized Point*: %1%", boost::json::serialize(json));

        Point *deserialized_ptr = nullptr;
        bool result = BROOKESIA_DESCRIBE_FROM_JSON(json, deserialized_ptr);
        BROOKESIA_LOGI("Point* from JSON result: %1%", static_cast<int>(result));
        TEST_ASSERT_TRUE(result); // Should succeed
        TEST_ASSERT_TRUE(original_ptr == deserialized_ptr); // Addresses should match
    }

    // Test const pointer - round trip
    {
        int value = 200;
        const int *original_ptr = &value;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(original_ptr);
        BROOKESIA_LOGI("Serialized const int*: %1%", boost::json::serialize(json));

        const int *deserialized_ptr = nullptr;
        bool result = BROOKESIA_DESCRIBE_FROM_JSON(json, deserialized_ptr);
        BROOKESIA_LOGI("const int* from JSON result: %1%", static_cast<int>(result));
        TEST_ASSERT_TRUE(result); // Should succeed
        TEST_ASSERT_TRUE(original_ptr == deserialized_ptr); // Addresses should match
    }

    // Test pointer to pointer - round trip
    {
        int value = 300;
        int *ptr = &value;
        int **original_ptr = &ptr;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(original_ptr);
        BROOKESIA_LOGI("Serialized int**: %1%", boost::json::serialize(json));

        int **deserialized_ptr = nullptr;
        bool result = BROOKESIA_DESCRIBE_FROM_JSON(json, deserialized_ptr);
        BROOKESIA_LOGI("int** from JSON result: %1%", static_cast<int>(result));
        TEST_ASSERT_TRUE(result); // Should succeed
        TEST_ASSERT_TRUE(original_ptr == deserialized_ptr); // Addresses should match
    }

    // Test null pointer
    {
        int *null_ptr = nullptr;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(null_ptr);
        BROOKESIA_LOGI("Serialized null int*: %1%", boost::json::serialize(json));

        int *deserialized_ptr = reinterpret_cast<int *>(0x12345678); // Set to non-null first
        bool result = BROOKESIA_DESCRIBE_FROM_JSON(json, deserialized_ptr);
        BROOKESIA_LOGI("null int* from JSON result: %1%", static_cast<int>(result));
        TEST_ASSERT_TRUE(result); // Should succeed
        TEST_ASSERT_TRUE(nullptr == deserialized_ptr); // Should be null
    }

    // Test with invalid JSON format (not a string) - should fail
    {
        int *int_ptr = nullptr;
        auto json = boost::json::parse("12345");
        bool result = BROOKESIA_DESCRIBE_FROM_JSON(json, int_ptr);
        BROOKESIA_LOGI("int* from number JSON result: %1%", static_cast<int>(result));
        TEST_ASSERT_FALSE(result); // Should fail - not a valid pointer format
    }

    // Test with invalid string format (not @0x...) - should fail
    {
        int *int_ptr = nullptr;
        auto json = boost::json::parse("\"invalid_format\"");
        bool result = BROOKESIA_DESCRIBE_FROM_JSON(json, int_ptr);
        BROOKESIA_LOGI("int* from invalid string JSON result: %1%", static_cast<int>(result));
        TEST_ASSERT_FALSE(result); // Should fail - not a valid pointer format
    }

    // Note: char* and const char* have static_assert that prevents compilation,
    // so we cannot test them here (they would cause compile errors)

    BROOKESIA_LOGI("✓ Pointers from JSON test passed");
}

// ==================== Test Round Trip Conversion ====================

TEST_CASE("Test JSON round trip", "[macro][round_trip]")
{
    BROOKESIA_LOGI("=== JSON Round Trip ===");

    // Simple struct
    Point original1{42, 84};
    auto json = BROOKESIA_DESCRIBE_TO_JSON(original1);
    Point converted1;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, converted1));
    TEST_ASSERT_EQUAL(original1.x, converted1.x);
    TEST_ASSERT_EQUAL(original1.y, converted1.y);

    // Nested struct
    Company original2{"GlobalCorp", {"Tokyo", 150000}};
    json = BROOKESIA_DESCRIBE_TO_JSON(original2);
    Company converted2;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, converted2));
    TEST_ASSERT_EQUAL_STRING(original2.name.c_str(), converted2.name.c_str());
    TEST_ASSERT_EQUAL_STRING(original2.address.city.c_str(), converted2.address.city.c_str());
    TEST_ASSERT_EQUAL(original2.address.zip, converted2.address.zip);

    // Struct with enum
    Task original3{"BatchJob", Status::Error};
    json = BROOKESIA_DESCRIBE_TO_JSON(original3);
    Task converted3;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, converted3));
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
    BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(BROOKESIA_DESCRIBE_FORMAT_COMPACT);
    Point p{10, 20};
    std::string result = BROOKESIA_DESCRIBE_TO_STR(p);
    BROOKESIA_LOGI("COMPACT format: %1%", result);
    TEST_ASSERT_TRUE(result.find("=") != std::string::npos);
    TEST_ASSERT_FALSE(result.find(": ") != std::string::npos);

    // Set to JSON
    BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(BROOKESIA_DESCRIBE_FORMAT_JSON);
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
    BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(BROOKESIA_DESCRIBE_FORMAT_COMPACT);
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
    BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(BROOKESIA_DESCRIBE_FORMAT_COMPACT);
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
    BROOKESIA_LOGI("Task (JSON): %1%", BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(task, BROOKESIA_DESCRIBE_FORMAT_JSON));

    // Convert to JSON
    auto json = BROOKESIA_DESCRIBE_TO_JSON(task);
    BROOKESIA_LOGI("JSON: %1%", boost::json::serialize(json));

    // Convert back from JSON
    Task task2;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, task2));
    BROOKESIA_LOGI("Converted: %1%", BROOKESIA_DESCRIBE_TO_STR(task2));

    // Enum conversions
    auto status_num = BROOKESIA_DESCRIBE_ENUM_TO_NUM(task2.status);
    BROOKESIA_LOGI("Status number: %1%", status_num);
    TEST_ASSERT_EQUAL(1, status_num);

    Status status3;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_STR_TO_ENUM("Error", status3));
    BROOKESIA_LOGI("Status from string: %1%", BROOKESIA_DESCRIBE_TO_STR(status3));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Error), static_cast<int>(status3));

    // Format management
    auto old_fmt = BROOKESIA_DESCRIBE_GET_GLOBAL_FORMAT();
    BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(BROOKESIA_DESCRIBE_FORMAT_VERBOSE);
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
    auto json_test = BROOKESIA_DESCRIBE_TO_JSON(test_val);
    BROOKESIA_LOGI("Direct int(-1) to JSON: %1%", boost::json::serialize(json_test));
    BROOKESIA_LOGI("Type: int64=%d, uint64=%d", json_test.is_int64(), json_test.is_uint64());

    // Test with negative values
    Point p{-1, -1};

    // Test manual member access
    BROOKESIA_LOGI("Manual p.x value: %d", p.x);
    auto json_x = BROOKESIA_DESCRIBE_TO_JSON(p.x);
    BROOKESIA_LOGI("Manual p.x to JSON: %1%", boost::json::serialize(json_x));

    std::string result = BROOKESIA_DESCRIBE_TO_STR(p);
    BROOKESIA_LOGI("Point with -1: %1%", result);

    // Verify that -1 is displayed correctly (not as large unsigned number)
    TEST_ASSERT_TRUE(result.find("-1") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("18446744073709551615") == std::string::npos);

    // Test with JSON format
    auto json = BROOKESIA_DESCRIBE_TO_JSON(p);
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

// ==================== Test std::variant Support ====================

// Define test variant types
using SimpleVariant = std::variant<bool, int, std::string>;
using ComplexVariant = std::variant<int, double, std::string, std::vector<int>, std::map<std::string, int>>;

struct DataWithVariant {
    std::string name;
    SimpleVariant value;
};
BROOKESIA_DESCRIBE_STRUCT(DataWithVariant, (), (name, value))

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_JSON - variant basic types", "[macro][variant][to_json]")
{
    BROOKESIA_LOGI("=== DESCRIBE_TO_JSON: Variant Basic Types ===");

    // Test bool
    {
        SimpleVariant v = true;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(v);
        BROOKESIA_LOGI("Variant(bool): %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_bool());
        TEST_ASSERT_TRUE(json.as_bool());
    }

    // Test int
    {
        SimpleVariant v = 42;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(v);
        BROOKESIA_LOGI("Variant(int): %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_number());
        TEST_ASSERT_EQUAL(42, json.as_int64());
    }

    // Test string
    {
        SimpleVariant v = std::string("hello");
        auto json = BROOKESIA_DESCRIBE_TO_JSON(v);
        BROOKESIA_LOGI("Variant(string): %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_string());
        TEST_ASSERT_EQUAL_STRING("hello", json.as_string().c_str());
    }

    BROOKESIA_LOGI("✓ Variant basic types to JSON test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_JSON - variant complex types", "[macro][variant][to_json]")
{
    BROOKESIA_LOGI("=== DESCRIBE_TO_JSON: Variant Complex Types ===");

    // Test vector
    {
        ComplexVariant v = std::vector<int> {1, 2, 3, 4};
        auto json = BROOKESIA_DESCRIBE_TO_JSON(v);
        BROOKESIA_LOGI("Variant(vector): %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_array());
        const auto &arr = json.as_array();
        TEST_ASSERT_EQUAL(4, arr.size());
        TEST_ASSERT_EQUAL(1, arr[0].as_int64());
        TEST_ASSERT_EQUAL(4, arr[3].as_int64());
    }

    // Test map
    {
        ComplexVariant v = std::map<std::string, int> {{"timeout", 30}, {"retry", 3}};
        auto json = BROOKESIA_DESCRIBE_TO_JSON(v);
        BROOKESIA_LOGI("Variant(map): %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_object());
        const auto &obj = json.as_object();
        TEST_ASSERT_EQUAL(30, obj.at("timeout").as_int64());
        TEST_ASSERT_EQUAL(3, obj.at("retry").as_int64());
    }

    BROOKESIA_LOGI("✓ Variant complex types to JSON test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_FROM_JSON - variant basic types", "[macro][variant][from_json]")
{
    BROOKESIA_LOGI("=== DESCRIBE_FROM_JSON: Variant Basic Types ===");

    // Test bool
    {
        auto json = boost::json::parse("true");
        SimpleVariant v;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, v));
        TEST_ASSERT_TRUE((std::holds_alternative<bool>(v)));
        TEST_ASSERT_TRUE((std::get<bool>(v)));
        BROOKESIA_LOGI("Parsed bool variant: %1%", std::get<bool>(v));
    }

    // Test int
    {
        auto json = boost::json::parse("42");
        SimpleVariant v;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, v));
        // Note: int might be parsed as bool first if bool is before int in variant
        // For SimpleVariant = std::variant<bool, int, std::string>
        // The number 42 should fail bool conversion and succeed with int
        if (std::holds_alternative<bool>(v)) {
            BROOKESIA_LOGI("Parsed as bool (1 or 0): %1%", std::get<bool>(v));
            TEST_ASSERT_TRUE((std::get<bool>(v))); // 42 is truthy
        } else {
            TEST_ASSERT_TRUE((std::holds_alternative<int>(v)));
            TEST_ASSERT_EQUAL(42, std::get<int>(v));
            BROOKESIA_LOGI("Parsed int variant: %1%", std::get<int>(v));
        }
    }

    // Test string
    {
        auto json = boost::json::parse("\"hello world\"");
        SimpleVariant v;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, v));
        TEST_ASSERT_TRUE((std::holds_alternative<std::string>(v)));
        TEST_ASSERT_EQUAL_STRING("hello world", std::get<std::string>(v).c_str());
        BROOKESIA_LOGI("Parsed string variant: %1%", std::get<std::string>(v).c_str());
    }

    BROOKESIA_LOGI("✓ Variant basic types from JSON test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_FROM_JSON - variant complex types", "[macro][variant][from_json]")
{
    BROOKESIA_LOGI("=== DESCRIBE_FROM_JSON: Variant Complex Types ===");

    // Test vector
    {
        auto json = boost::json::parse("[10, 20, 30]");
        ComplexVariant v;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, v));
        TEST_ASSERT_TRUE((std::holds_alternative<std::vector<int>>(v)));
        const auto &vec = std::get<std::vector<int>>(v);
        TEST_ASSERT_EQUAL(3, vec.size());
        TEST_ASSERT_EQUAL(10, vec[0]);
        TEST_ASSERT_EQUAL(30, vec[2]);
        BROOKESIA_LOGI("Parsed vector variant: size=%1%", vec.size());
    }

    // Test map
    {
        auto json = boost::json::parse("{\"count\": 100, \"limit\": 50}");
        ComplexVariant v;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, v));
        TEST_ASSERT_TRUE((std::holds_alternative<std::map<std::string, int>>(v)));
        const auto &map = std::get<std::map<std::string, int>>(v);
        TEST_ASSERT_EQUAL(100, map.at("count"));
        TEST_ASSERT_EQUAL(50, map.at("limit"));
        BROOKESIA_LOGI("Parsed map variant: size=%1%", map.size());
    }

    BROOKESIA_LOGI("✓ Variant complex types from JSON test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_STR - variant", "[macro][variant][to_str]")
{
    BROOKESIA_LOGI("=== DESCRIBE_TO_STR: Variant ===");

    // Test bool
    {
        SimpleVariant v = false;
        std::string str = BROOKESIA_DESCRIBE_TO_STR(v);
        BROOKESIA_LOGI("Variant(bool) to string: %1%", str);
        TEST_ASSERT_TRUE(str.find("false") != std::string::npos);
    }

    // Test int
    {
        SimpleVariant v = 123;
        std::string str = BROOKESIA_DESCRIBE_TO_STR(v);
        BROOKESIA_LOGI("Variant(int) to string: %1%", str);
        TEST_ASSERT_TRUE(str.find("123") != std::string::npos);
    }

    // Test string
    {
        SimpleVariant v = std::string("test");
        std::string str = BROOKESIA_DESCRIBE_TO_STR(v);
        BROOKESIA_LOGI("Variant(string) to string: %1%", str);
        TEST_ASSERT_TRUE(str.find("test") != std::string::npos);
    }

    // Test vector
    {
        ComplexVariant v = std::vector<int> {1, 2, 3};
        std::string str = BROOKESIA_DESCRIBE_TO_STR(v);
        BROOKESIA_LOGI("Variant(vector) to string: %1%", str);
        TEST_ASSERT_TRUE(str.find("[") != std::string::npos);
        TEST_ASSERT_TRUE(str.find("1") != std::string::npos);
    }

    BROOKESIA_LOGI("✓ Variant to string test passed");
}

TEST_CASE("Test variant in struct", "[macro][variant][struct]")
{
    BROOKESIA_LOGI("=== Variant in Struct ===");

    // Test with int value
    {
        DataWithVariant data{"counter", 42};
        auto json = BROOKESIA_DESCRIBE_TO_JSON(data);
        BROOKESIA_LOGI("Struct with variant(int): %1%", boost::json::serialize(json));

        TEST_ASSERT_TRUE(json.is_object());
        const auto &obj = json.as_object();
        TEST_ASSERT_EQUAL_STRING("counter", obj.at("name").as_string().c_str());
        // Value might be bool or int depending on variant order
        TEST_ASSERT_TRUE(obj.at("value").is_number() || obj.at("value").is_bool());
    }

    // Test with string value
    {
        DataWithVariant data{"message", std::string("hello")};
        auto json = BROOKESIA_DESCRIBE_TO_JSON(data);
        BROOKESIA_LOGI("Struct with variant(string): %1%", boost::json::serialize(json));

        TEST_ASSERT_TRUE(json.is_object());
        const auto &obj = json.as_object();
        TEST_ASSERT_EQUAL_STRING("message", obj.at("name").as_string().c_str());
        TEST_ASSERT_EQUAL_STRING("hello", obj.at("value").as_string().c_str());
    }

    // Test from JSON
    {
        auto json = boost::json::parse("{\"name\": \"status\", \"value\": \"active\"}");
        DataWithVariant data;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, data));
        TEST_ASSERT_EQUAL_STRING("status", data.name.c_str());
        TEST_ASSERT_TRUE((std::holds_alternative<std::string>(data.value)));
        TEST_ASSERT_EQUAL_STRING("active", std::get<std::string>(data.value).c_str());
        BROOKESIA_LOGI("Parsed struct with variant: name=%1%, value=%2%",
                       data.name, std::get<std::string>(data.value));
    }

    BROOKESIA_LOGI("✓ Variant in struct test passed");
}

TEST_CASE("Test variant round trip", "[macro][variant][round_trip]")
{
    BROOKESIA_LOGI("=== Variant Round Trip ===");

    // Test bool round trip
    {
        SimpleVariant original = true;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(original);
        SimpleVariant converted;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, converted));
        TEST_ASSERT_TRUE((std::holds_alternative<bool>(converted)));
        bool orig_val = std::get<bool>(original);
        bool conv_val = std::get<bool>(converted);
        TEST_ASSERT_EQUAL(orig_val, conv_val);
        BROOKESIA_LOGI("Bool round trip: %1% -> %2%", orig_val, conv_val);
    }

    // Test string round trip
    {
        SimpleVariant original = std::string("round trip test");
        auto json = BROOKESIA_DESCRIBE_TO_JSON(original);
        SimpleVariant converted;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, converted));
        TEST_ASSERT_TRUE((std::holds_alternative<std::string>(converted)));
        TEST_ASSERT_EQUAL_STRING(std::get<std::string>(original).c_str(),
                                 std::get<std::string>(converted).c_str());
        BROOKESIA_LOGI("String round trip: %1% -> %2%",
                       std::get<std::string>(original), std::get<std::string>(converted));
    }

    // Test vector round trip
    {
        ComplexVariant original = std::vector<int> {5, 10, 15};
        auto json = BROOKESIA_DESCRIBE_TO_JSON(original);
        ComplexVariant converted;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, converted));
        TEST_ASSERT_TRUE((std::holds_alternative<std::vector<int>>(converted)));
        const auto &orig_vec = std::get<std::vector<int>>(original);
        const auto &conv_vec = std::get<std::vector<int>>(converted);
        TEST_ASSERT_EQUAL(orig_vec.size(), conv_vec.size());
        TEST_ASSERT_EQUAL(orig_vec[0], conv_vec[0]);
        TEST_ASSERT_EQUAL(orig_vec[2], conv_vec[2]);
        BROOKESIA_LOGI("Vector round trip: size=%1%", conv_vec.size());
    }

    // Test struct with variant round trip
    {
        DataWithVariant original{"config", std::string("enabled")};
        auto json = BROOKESIA_DESCRIBE_TO_JSON(original);
        DataWithVariant converted;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, converted));
        TEST_ASSERT_EQUAL_STRING(original.name.c_str(), converted.name.c_str());
        TEST_ASSERT_TRUE((std::holds_alternative<std::string>(converted.value)));
        TEST_ASSERT_EQUAL_STRING(std::get<std::string>(original.value).c_str(),
                                 std::get<std::string>(converted.value).c_str());
        BROOKESIA_LOGI("Struct with variant round trip: name=%1%", converted.name);
    }

    BROOKESIA_LOGI("✓ Variant round trip test passed");
}

TEST_CASE("Test variant type order priority", "[macro][variant][type_order]")
{
    BROOKESIA_LOGI("=== Variant Type Order Priority ===");

    // For variant<bool, int, string>, bool is checked first
    // JSON value `1` could be bool (true) or int (1)
    // Due to type order, it will try bool first
    using OrderedVariant = std::variant<bool, int, std::string>;

    // Test number 1 (could be bool or int)
    {
        auto json = boost::json::parse("1");
        OrderedVariant v;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, v));
        // Should be parsed as bool (first in variant) if bool conversion succeeds
        // Or as int if bool conversion fails
        BROOKESIA_LOGI("Number 1 parsed as: bool=%1%, int=%2%, string=%3%",
                       std::holds_alternative<bool>(v),
                       std::holds_alternative<int>(v),
                       std::holds_alternative<std::string>(v));
    }

    // Test string "true" (could be string or bool)
    {
        auto json = boost::json::parse("\"true\"");
        OrderedVariant v;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, v));
        // JSON string should be parsed as std::string
        TEST_ASSERT_TRUE((std::holds_alternative<std::string>(v)));
        if (std::holds_alternative<std::string>(v)) {
            TEST_ASSERT_EQUAL_STRING("true", std::get<std::string>(v).c_str());
        }
        BROOKESIA_LOGI("String \"true\" correctly parsed as string");
    }

    // Test actual JSON bool
    {
        auto json = boost::json::parse("true");
        OrderedVariant v;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, v));
        TEST_ASSERT_TRUE((std::holds_alternative<bool>(v)));
        TEST_ASSERT_TRUE((std::get<bool>(v)));
        BROOKESIA_LOGI("JSON bool `true` correctly parsed as bool");
    }

    BROOKESIA_LOGI("✓ Variant type order priority test passed");
}

TEST_CASE("Test variant with empty alternatives", "[macro][variant][edge_cases]")
{
    BROOKESIA_LOGI("=== Variant Edge Cases ===");

    // Test empty string
    {
        SimpleVariant v = std::string("");
        auto json = BROOKESIA_DESCRIBE_TO_JSON(v);
        BROOKESIA_LOGI("Empty string variant: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_string());
        TEST_ASSERT_EQUAL_STRING("", json.as_string().c_str());

        SimpleVariant converted;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, converted));
        TEST_ASSERT_TRUE((std::holds_alternative<std::string>(converted)));
        TEST_ASSERT_EQUAL_STRING("", std::get<std::string>(converted).c_str());
    }

    // Test zero value
    {
        SimpleVariant v = 0;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(v);
        BROOKESIA_LOGI("Zero int variant: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_number());
        TEST_ASSERT_EQUAL(0, json.as_int64());
    }

    // Test empty vector
    {
        ComplexVariant v = std::vector<int> {};
        auto json = BROOKESIA_DESCRIBE_TO_JSON(v);
        BROOKESIA_LOGI("Empty vector variant: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_array());
        TEST_ASSERT_EQUAL(0, json.as_array().size());

        ComplexVariant converted;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, converted));
        TEST_ASSERT_TRUE((std::holds_alternative<std::vector<int>>(converted)));
        const auto &conv_vec = std::get<std::vector<int>>(converted);
        TEST_ASSERT_EQUAL(0, conv_vec.size());
    }

    BROOKESIA_LOGI("✓ Variant edge cases test passed");
}

// ==================== std::function Tests ====================

struct CallbackHolder {
    std::function<int(int, int)> callback;
    std::string name;
};
BROOKESIA_DESCRIBE_STRUCT(CallbackHolder, (), (callback, name))

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_STR - std::function", "[macro][function][to_str]")
{
    BROOKESIA_LOGI("=== DESCRIBE_TO_STR: std::function ===");

    // Test non-empty function
    {
        std::function<int(int, int)> add = [](int a, int b) {
            return a + b;
        };
        std::string str = BROOKESIA_DESCRIBE_TO_STR(add);
        BROOKESIA_LOGI("Non-empty function: %1%", str);
        TEST_ASSERT_TRUE(str.find("<function@") != std::string::npos);
        TEST_ASSERT_TRUE(str.find(">") != std::string::npos);
    }

    // Test empty function
    {
        std::function<int(int, int)> empty_func;
        std::string str = BROOKESIA_DESCRIBE_TO_STR(empty_func);
        BROOKESIA_LOGI("Empty function: %1%", str);
        TEST_ASSERT_TRUE(str.find("<function:empty>") != std::string::npos);
    }

    // Test function with different signature
    {
        std::function<void()> void_func = []() { /* do nothing */ };
        std::string str = BROOKESIA_DESCRIBE_TO_STR(void_func);
        BROOKESIA_LOGI("Void function: %1%", str);
        TEST_ASSERT_TRUE(str.find("<function@") != std::string::npos);
    }

    BROOKESIA_LOGI("✓ std::function to_str test passed");
}

TEST_CASE("Test BROOKESIA_DESCRIBE_TO_JSON - std::function", "[macro][function][to_json]")
{
    BROOKESIA_LOGI("=== DESCRIBE_TO_JSON: std::function ===");

    // Test non-empty function
    {
        std::function<int(int, int)> add = [](int a, int b) {
            return a + b;
        };
        auto json = BROOKESIA_DESCRIBE_TO_JSON(add);
        BROOKESIA_LOGI("Non-empty function JSON: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_string());
        std::string json_str = boost::json::value_to<std::string>(json);
        TEST_ASSERT_TRUE(json_str.find("<function@") != std::string::npos);
    }

    // Test empty function
    {
        std::function<int(int, int)> empty_func;
        auto json = BROOKESIA_DESCRIBE_TO_JSON(empty_func);
        BROOKESIA_LOGI("Empty function JSON: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_string());
        TEST_ASSERT_EQUAL_STRING("<function:empty>", boost::json::value_to<std::string>(json).c_str());
    }

    BROOKESIA_LOGI("✓ std::function to_json test passed");
}

TEST_CASE("Test std::function in struct", "[macro][function][struct]")
{
    BROOKESIA_LOGI("=== std::function in Struct ===");

    // Test with non-empty function
    {
        CallbackHolder holder;
        holder.callback = [](int a, int b) {
            return a * b;
        };
        holder.name = "multiplier";

        std::string str = BROOKESIA_DESCRIBE_TO_STR(holder);
        BROOKESIA_LOGI("CallbackHolder with function: %1%", str);
        TEST_ASSERT_TRUE(str.find("callback") != std::string::npos);
        TEST_ASSERT_TRUE(str.find("<function@") != std::string::npos);
        TEST_ASSERT_TRUE(str.find("name") != std::string::npos);
        TEST_ASSERT_TRUE(str.find("multiplier") != std::string::npos);

        auto json = BROOKESIA_DESCRIBE_TO_JSON(holder);
        BROOKESIA_LOGI("CallbackHolder JSON: %1%", boost::json::serialize(json));
        TEST_ASSERT_TRUE(json.is_object());
        TEST_ASSERT_TRUE(json.as_object().contains("callback"));
        TEST_ASSERT_TRUE(json.as_object().contains("name"));
    }

    // Test with empty function
    {
        CallbackHolder holder;
        holder.name = "no-callback";

        std::string str = BROOKESIA_DESCRIBE_TO_STR(holder);
        BROOKESIA_LOGI("CallbackHolder with empty function: %1%", str);
        TEST_ASSERT_TRUE(str.find("<function:empty>") != std::string::npos);

        auto json = BROOKESIA_DESCRIBE_TO_JSON(holder);
        BROOKESIA_LOGI("CallbackHolder empty JSON: %1%", boost::json::serialize(json));
        TEST_ASSERT_EQUAL_STRING("<function:empty>",
                                 boost::json::value_to<std::string>(json.as_object().at("callback")).c_str());
    }

    BROOKESIA_LOGI("✓ std::function in struct test passed");
}

TEST_CASE("Test std::function with different formats", "[macro][function][format]")
{
    BROOKESIA_LOGI("=== std::function with Different Formats ===");

    std::function<int(int, int)> add_func = [](int a, int b) {
        return a + b;
    };

    // Test with default format
    {
        std::string str = BROOKESIA_DESCRIBE_TO_STR(add_func);
        BROOKESIA_LOGI("Default format: %1%", str);
        TEST_ASSERT_TRUE(str.find("<function@") != std::string::npos);
    }

    // Test with compact format
    {
        CallbackHolder holder;
        holder.callback = add_func;
        holder.name = "add";
        std::string str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(holder, BROOKESIA_DESCRIBE_FORMAT_COMPACT);
        BROOKESIA_LOGI("Compact format: %1%", str);
        TEST_ASSERT_TRUE(str.find("<function@") != std::string::npos);
    }

    // Test with verbose format
    {
        CallbackHolder holder;
        holder.callback = add_func;
        holder.name = "add";
        std::string str = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(holder, BROOKESIA_DESCRIBE_FORMAT_VERBOSE);
        BROOKESIA_LOGI("Verbose format: %1%", str);
        TEST_ASSERT_TRUE(str.find("<function@") != std::string::npos);
    }

    BROOKESIA_LOGI("✓ std::function format test passed");
}

// ==================== Test Complex Struct with All Supported Types ====================

// Complex struct containing all supported types
struct ComplexStruct {
    // Basic types
    bool flag;
    int number;
    float float_value;
    double double_value;
    std::string text;

    // Pointers (non-char*)
    int *int_ptr;
    void *void_ptr;
    const void *const_ptr;
    Point *point_ptr;

    // Enum
    Status status;

    // Containers
    std::vector<int> numbers;
    std::span<const int> span_values;
    std::map<std::string, int> settings;
    std::optional<std::string> description;

    // Variant
    SimpleVariant variant_value;

    // Function
    std::function<int(int, int)> callback;

    // Nested structs
    Point position;
    Address location;

    // JSON value
    boost::json::value json_data;
};
BROOKESIA_DESCRIBE_STRUCT(ComplexStruct, (),
                          (flag, number, float_value, double_value, text,
                           int_ptr, void_ptr, const_ptr, point_ptr,
                           status,
                           numbers, span_values, settings, description,
                           variant_value,
                           callback,
                           position, location,
                           json_data))

TEST_CASE("Test ComplexStruct - BROOKESIA_DESCRIBE_TO_STR", "[macro][complex_struct][to_str]")
{
    BROOKESIA_LOGI("=== ComplexStruct: DESCRIBE_TO_STR ===");

    // Create a complex struct with all supported types
    int int_value = 42;
    Point point_value{10, 20};
    static constexpr int span_data[] = {10, 20, 30};

    ComplexStruct complex;
    complex.flag = true;
    complex.number = 100;
    complex.float_value = 3.14f;
    complex.double_value = 2.71828;
    complex.text = "complex test";
    complex.int_ptr = &int_value;
    complex.void_ptr = &int_value;
    complex.const_ptr = reinterpret_cast<const void *>(&int_value);
    complex.point_ptr = &point_value;
    complex.status = Status::Running;
    complex.numbers = {1, 2, 3, 4, 5};
    complex.span_values = std::span{span_data};
    complex.settings["timeout"] = 30;
    complex.settings["retry"] = 3;
    complex.description = "test description";
    complex.variant_value = std::string("variant string");
    complex.callback = [](int a, int b) {
        return a + b;
    };
    complex.position = Point{100, 200};
    complex.location = Address{"Beijing", 100000};
    complex.json_data = boost::json::parse("{\"key\": \"value\"}");

    // Test to_str
    std::string str = BROOKESIA_DESCRIBE_TO_STR(complex);
    BROOKESIA_LOGI("ComplexStruct to_str: %1%", str);

    // Verify all fields are present
    TEST_ASSERT_TRUE(str.find("flag") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("number") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("text") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("int_ptr") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("const_ptr") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("@0x") != std::string::npos); // Pointer format
    TEST_ASSERT_TRUE(str.find("status") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("Running") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("numbers") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("span_values") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("settings") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("description") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("variant_value") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("callback") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("position") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("location") != std::string::npos);
    TEST_ASSERT_TRUE(str.find("json_data") != std::string::npos);

    BROOKESIA_LOGI("✓ ComplexStruct to_str test passed");
}

TEST_CASE("Test ComplexStruct - BROOKESIA_DESCRIBE_TO_JSON", "[macro][complex_struct][to_json]")
{
    BROOKESIA_LOGI("=== ComplexStruct: DESCRIBE_TO_JSON ===");

    // Create a complex struct with all supported types
    int int_value = 42;
    Point point_value{10, 20};
    static constexpr int span_data[] = {10, 20, 30};

    ComplexStruct complex;
    complex.flag = true;
    complex.number = 100;
    complex.float_value = 3.14f;
    complex.double_value = 2.71828;
    complex.text = "complex test";
    complex.int_ptr = &int_value;
    complex.void_ptr = &int_value;
    complex.point_ptr = &point_value;
    complex.status = Status::Running;
    complex.numbers = {1, 2, 3, 4, 5};
    complex.span_values = std::span{span_data};
    complex.settings["timeout"] = 30;
    complex.settings["retry"] = 3;
    complex.description = "test description";
    complex.variant_value = std::string("variant string");
    complex.callback = [](int a, int b) {
        return a + b;
    };
    complex.position = Point{100, 200};
    complex.location = Address{"Beijing", 100000};
    complex.json_data = boost::json::parse("{\"key\": \"value\"}");

    // Test to_json
    auto json = BROOKESIA_DESCRIBE_TO_JSON(complex);
    BROOKESIA_LOGI("ComplexStruct JSON: %1%", boost::json::serialize(json));

    TEST_ASSERT_TRUE(json.is_object());
    const auto &obj = json.as_object();

    // Verify all fields are present
    TEST_ASSERT_TRUE(obj.contains("flag"));
    TEST_ASSERT_TRUE(obj.contains("number"));
    TEST_ASSERT_TRUE(obj.contains("float_value"));
    TEST_ASSERT_TRUE(obj.contains("double_value"));
    TEST_ASSERT_TRUE(obj.contains("text"));
    TEST_ASSERT_TRUE(obj.contains("int_ptr"));
    TEST_ASSERT_TRUE(obj.contains("void_ptr"));
    TEST_ASSERT_TRUE(obj.contains("point_ptr"));
    TEST_ASSERT_TRUE(obj.contains("status"));
    TEST_ASSERT_TRUE(obj.contains("numbers"));
    TEST_ASSERT_TRUE(obj.contains("span_values"));
    TEST_ASSERT_TRUE(obj.contains("settings"));
    TEST_ASSERT_TRUE(obj.contains("description"));
    TEST_ASSERT_TRUE(obj.contains("variant_value"));
    TEST_ASSERT_TRUE(obj.contains("callback"));
    TEST_ASSERT_TRUE(obj.contains("position"));
    TEST_ASSERT_TRUE(obj.contains("location"));
    TEST_ASSERT_TRUE(obj.contains("json_data"));

    // Verify values
    TEST_ASSERT_TRUE(obj.at("flag").as_bool());
    TEST_ASSERT_EQUAL(100, obj.at("number").as_int64());
    TEST_ASSERT_EQUAL_STRING("complex test", obj.at("text").as_string().c_str());
    TEST_ASSERT_EQUAL_STRING("Running", obj.at("status").as_string().c_str());
    TEST_ASSERT_TRUE(obj.at("numbers").is_array());
    TEST_ASSERT_TRUE(obj.at("span_values").is_array());
    TEST_ASSERT_EQUAL(3, obj.at("span_values").as_array().size());
    TEST_ASSERT_EQUAL(10, obj.at("span_values").as_array()[0].as_int64());
    TEST_ASSERT_EQUAL(20, obj.at("span_values").as_array()[1].as_int64());
    TEST_ASSERT_EQUAL(30, obj.at("span_values").as_array()[2].as_int64());
    TEST_ASSERT_TRUE(obj.at("settings").is_object());
    TEST_ASSERT_EQUAL_STRING("test description", obj.at("description").as_string().c_str());
    TEST_ASSERT_TRUE(obj.at("int_ptr").is_string());
    TEST_ASSERT_TRUE(obj.at("int_ptr").as_string().find("@0x") == 0);
    TEST_ASSERT_TRUE(obj.at("position").is_object());
    TEST_ASSERT_TRUE(obj.at("location").is_object());

    BROOKESIA_LOGI("✓ ComplexStruct to_json test passed");
}

TEST_CASE("Test ComplexStruct - BROOKESIA_DESCRIBE_JSON_SERIALIZE/DESERIALIZE", "[macro][complex_struct][serialize]")
{
    BROOKESIA_LOGI("=== ComplexStruct: JSON_SERIALIZE/DESERIALIZE ===");

    // Create a complex struct with all supported types
    int int_value = 42;
    Point point_value{10, 20};
    static constexpr int span_data[] = {10, 20, 30};

    ComplexStruct original;
    original.flag = true;
    original.number = 100;
    original.float_value = 3.14f;
    original.double_value = 2.71828;
    original.text = "complex test";
    original.int_ptr = &int_value;
    original.void_ptr = &int_value;
    original.point_ptr = &point_value;
    original.status = Status::Running;
    original.numbers = {1, 2, 3, 4, 5};
    original.span_values = std::span{span_data};
    original.settings["timeout"] = 30;
    original.settings["retry"] = 3;
    original.description = "test description";
    original.variant_value = std::string("variant string");
    original.callback = [](int a, int b) {
        return a + b;
    };
    original.position = Point{100, 200};
    original.location = Address{"Beijing", 100000};
    original.json_data = boost::json::parse("{\"key\": \"value\"}");

    // Serialize
    std::string json_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(original);
    BROOKESIA_LOGI("Serialized ComplexStruct: %1%", json_str);

    // Deserialize
    ComplexStruct deserialized;
    bool result = BROOKESIA_DESCRIBE_JSON_DESERIALIZE(json_str, deserialized);
    TEST_ASSERT_TRUE(result);

    // Verify all fields match (except pointers which are runtime addresses)
    TEST_ASSERT_EQUAL(original.flag, deserialized.flag);
    TEST_ASSERT_EQUAL(original.number, deserialized.number);
    TEST_ASSERT_TRUE(std::abs(original.float_value - deserialized.float_value) < 0.001f);
    TEST_ASSERT_TRUE(std::abs(original.double_value - deserialized.double_value) < 0.000001);
    TEST_ASSERT_EQUAL_STRING(original.text.c_str(), deserialized.text.c_str());
    TEST_ASSERT_EQUAL(static_cast<int>(original.status), static_cast<int>(deserialized.status));
    TEST_ASSERT_EQUAL(original.numbers.size(), deserialized.numbers.size());
    TEST_ASSERT_EQUAL(original.numbers[0], deserialized.numbers[0]);
    TEST_ASSERT_EQUAL(original.settings["timeout"], deserialized.settings["timeout"]);
    TEST_ASSERT_TRUE(deserialized.description.has_value());
    TEST_ASSERT_EQUAL_STRING(original.description.value().c_str(), deserialized.description.value().c_str());
    TEST_ASSERT_EQUAL(original.position.x, deserialized.position.x);
    TEST_ASSERT_EQUAL(original.position.y, deserialized.position.y);
    TEST_ASSERT_EQUAL_STRING(original.location.city.c_str(), deserialized.location.city.c_str());
    TEST_ASSERT_EQUAL(original.location.zip, deserialized.location.zip);

    // Verify pointers are deserialized correctly
    TEST_ASSERT_TRUE(original.int_ptr == deserialized.int_ptr);
    TEST_ASSERT_TRUE(original.void_ptr == deserialized.void_ptr);
    TEST_ASSERT_TRUE(original.point_ptr == deserialized.point_ptr);

    // Verify variant
    TEST_ASSERT_TRUE(std::holds_alternative<std::string>(deserialized.variant_value));
    TEST_ASSERT_EQUAL_STRING(std::get<std::string>(original.variant_value).c_str(),
                             std::get<std::string>(deserialized.variant_value).c_str());

    // Verify function - deserialized function should be empty (functions cannot be restored from JSON)
    TEST_ASSERT_FALSE(deserialized.callback); // Function should be empty after deserialization

    BROOKESIA_LOGI("✓ ComplexStruct serialize/deserialize test passed");
}

TEST_CASE("Test ComplexStruct - BROOKESIA_DESCRIBE_FROM_JSON", "[macro][complex_struct][from_json]")
{
    BROOKESIA_LOGI("=== ComplexStruct: DESCRIBE_FROM_JSON ===");

    // Create JSON with all supported types
    int int_value = 42;
    Point point_value{10, 20};
    static constexpr int span_data[] = {10, 20, 30};

    ComplexStruct original;
    original.flag = true;
    original.number = 100;
    original.float_value = 3.14f;
    original.double_value = 2.71828;
    original.text = "complex test";
    original.int_ptr = &int_value;
    original.void_ptr = &int_value;
    original.point_ptr = &point_value;
    original.status = Status::Running;
    original.numbers = {1, 2, 3, 4, 5};
    original.span_values = std::span{span_data};  // Note: span cannot be deserialized
    original.settings["timeout"] = 30;
    original.settings["retry"] = 3;
    original.description = "test description";
    original.variant_value = std::string("variant string");
    original.callback = [](int a, int b) {
        return a + b;
    };
    original.position = Point{100, 200};
    original.location = Address{"Beijing", 100000};
    original.json_data = boost::json::parse("{\"key\": \"value\"}");

    auto json = BROOKESIA_DESCRIBE_TO_JSON(original);

    // Deserialize from JSON
    ComplexStruct deserialized;
    bool result = BROOKESIA_DESCRIBE_FROM_JSON(json, deserialized);
    TEST_ASSERT_TRUE(result);

    // Verify all fields match
    TEST_ASSERT_EQUAL(original.flag, deserialized.flag);
    TEST_ASSERT_EQUAL(original.number, deserialized.number);
    TEST_ASSERT_TRUE(std::abs(original.float_value - deserialized.float_value) < 0.001f);
    TEST_ASSERT_TRUE(std::abs(original.double_value - deserialized.double_value) < 0.000001);
    TEST_ASSERT_EQUAL_STRING(original.text.c_str(), deserialized.text.c_str());
    TEST_ASSERT_EQUAL(static_cast<int>(original.status), static_cast<int>(deserialized.status));
    TEST_ASSERT_EQUAL(original.numbers.size(), deserialized.numbers.size());
    TEST_ASSERT_EQUAL(original.settings["timeout"], deserialized.settings["timeout"]);
    TEST_ASSERT_TRUE(deserialized.description.has_value());
    TEST_ASSERT_EQUAL_STRING(original.description.value().c_str(), deserialized.description.value().c_str());
    TEST_ASSERT_EQUAL(original.position.x, deserialized.position.x);
    TEST_ASSERT_EQUAL(original.position.y, deserialized.position.y);
    TEST_ASSERT_EQUAL_STRING(original.location.city.c_str(), deserialized.location.city.c_str());
    TEST_ASSERT_EQUAL(original.location.zip, deserialized.location.zip);
    TEST_ASSERT_TRUE(original.int_ptr == deserialized.int_ptr);
    TEST_ASSERT_TRUE(original.void_ptr == deserialized.void_ptr);
    TEST_ASSERT_TRUE(original.point_ptr == deserialized.point_ptr);

    // Verify function - deserialized function should be empty (functions cannot be restored from JSON)
    TEST_ASSERT_FALSE(deserialized.callback); // Function should be empty after deserialization

    BROOKESIA_LOGI("✓ ComplexStruct from_json test passed");
}

TEST_CASE("Test ComplexStruct - Round Trip", "[macro][complex_struct][round_trip]")
{
    BROOKESIA_LOGI("=== ComplexStruct: Round Trip ===");

    // Create original complex struct
    int int_value = 42;
    Point point_value{10, 20};
    static constexpr int span_data[] = {100, 200, 300};

    ComplexStruct original;
    original.flag = true;
    original.number = 100;
    original.float_value = 3.14f;
    original.double_value = 2.71828;
    original.text = "round trip test";
    original.int_ptr = &int_value;
    original.void_ptr = &int_value;
    original.point_ptr = &point_value;
    original.status = Status::Error;
    original.numbers = {10, 20, 30};
    original.span_values = std::span{span_data};  // Note: span cannot be deserialized
    original.settings["max"] = 100;
    original.settings["min"] = 0;
    original.description = "round trip description";
    original.variant_value = 123; // int variant
    original.callback = [](int a, int b) {
        return a * b;
    };
    original.position = Point{50, 60};
    original.location = Address{"Shanghai", 200000};
    original.json_data = boost::json::parse("{\"round\": \"trip\"}");

    // Round trip: to_json -> from_json
    auto json = BROOKESIA_DESCRIBE_TO_JSON(original);
    ComplexStruct converted;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, converted));

    // Verify all fields match
    TEST_ASSERT_EQUAL(original.flag, converted.flag);
    TEST_ASSERT_EQUAL(original.number, converted.number);
    TEST_ASSERT_TRUE(std::abs(original.float_value - converted.float_value) < 0.001f);
    TEST_ASSERT_TRUE(std::abs(original.double_value - converted.double_value) < 0.000001);
    TEST_ASSERT_EQUAL_STRING(original.text.c_str(), converted.text.c_str());
    TEST_ASSERT_EQUAL(static_cast<int>(original.status), static_cast<int>(converted.status));
    TEST_ASSERT_EQUAL(original.numbers.size(), converted.numbers.size());
    TEST_ASSERT_EQUAL(original.numbers[0], converted.numbers[0]);
    TEST_ASSERT_EQUAL(original.settings["max"], converted.settings["max"]);
    TEST_ASSERT_EQUAL(original.settings["min"], converted.settings["min"]);
    TEST_ASSERT_TRUE(converted.description.has_value());
    TEST_ASSERT_EQUAL_STRING(original.description.value().c_str(), converted.description.value().c_str());
    TEST_ASSERT_EQUAL(original.position.x, converted.position.x);
    TEST_ASSERT_EQUAL(original.position.y, converted.position.y);
    TEST_ASSERT_EQUAL_STRING(original.location.city.c_str(), converted.location.city.c_str());
    TEST_ASSERT_EQUAL(original.location.zip, converted.location.zip);
    TEST_ASSERT_TRUE(original.int_ptr == converted.int_ptr);
    TEST_ASSERT_TRUE(original.void_ptr == converted.void_ptr);
    TEST_ASSERT_TRUE(original.point_ptr == converted.point_ptr);
    TEST_ASSERT_TRUE(std::holds_alternative<int>(converted.variant_value));
    TEST_ASSERT_EQUAL(std::get<int>(original.variant_value), std::get<int>(converted.variant_value));

    // Verify function - deserialized function should be empty (functions cannot be restored from JSON)
    TEST_ASSERT_FALSE(converted.callback); // Function should be empty after deserialization

    BROOKESIA_LOGI("✓ ComplexStruct round trip test passed");
}
