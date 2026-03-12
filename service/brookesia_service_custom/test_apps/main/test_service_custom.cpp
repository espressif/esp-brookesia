/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "boost/json.hpp"
#include "boost/thread.hpp"
#include <functional>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <set>
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_manager/service/local_runner.hpp"
#include "brookesia/service_custom/service_custom.hpp"
#include "common_def.hpp"

using namespace esp_brookesia;
// using CustomHelper = service::helper::CustomService;

static auto &service_manager = service::ServiceManager::get_instance();
static auto &time_profiler = lib_utils::TimeProfiler::get_instance();
static auto &custom_service = esp_brookesia::service::CustomService::get_instance();
static esp_brookesia::service::ServiceBinding binding;

static bool startup();
static void shutdown();

TEST_CASE("Test CustomService - failed to register function with null handler", "[service][custom][basic]")
{
    // BROOKESIA_TIME_PROFILER_SCOPE("test_custom_service_failed_to_register_function_with_null_handler");
    BROOKESIA_LOGI("=== Test CustomService - failed to register function with null handler ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    bool success = custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "test_function_name",
        .description = "test_function_description",
        .parameters = {
            {.name = "test", .type = esp_brookesia::service::FunctionValueType::String},
        }
    }, nullptr);
    TEST_ASSERT_EQUAL(false, success);
}

TEST_CASE("Test CustomService - failed to register function with invalid name", "[service][custom][basic]")
{
    // BROOKESIA_TIME_PROFILER_SCOPE("test_custom_service_failed_to_register_function_with_invalid_name");
    BROOKESIA_LOGI("=== Test CustomService - failed to register function with invalid name ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    bool success = custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "",
        .description = "test_function_description",
        .parameters = {
            {.name = "test", .type = esp_brookesia::service::FunctionValueType::String},
        }
    }, nullptr);
    TEST_ASSERT_EQUAL(false, success);
}

TEST_CASE("Test CustomService - failed to register function before startup", "[service][custom][basic]")
{
    BROOKESIA_LOGI("=== Test CustomService - failed to register function before startup ===");

    bool success = custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_before_startup",
        .description = "should fail before startup",
        .parameters = {},
    }, [](const esp_brookesia::service::FunctionParameterMap &) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{.success = true};
    });

    TEST_ASSERT_FALSE(success);
}

TEST_CASE("Test CustomService - failed to register event before startup", "[service][custom][basic]")
{
    BROOKESIA_LOGI("=== Test CustomService - failed to register event before startup ===");

    bool success = custom_service.register_event(esp_brookesia::service::EventSchema{
        .name = "event_before_startup",
        .description = "should fail before startup",
        .items = {
            {.name = "value", .type = esp_brookesia::service::EventItemType::Number},
        },
    });

    TEST_ASSERT_FALSE(success);
}

TEST_CASE("Test CustomService - failed to register function with the same name", "[service][custom][basic]")
{
    // BROOKESIA_TIME_PROFILER_SCOPE("test_custom_service_failed_to_register_function_with_the_same_name");
    BROOKESIA_LOGI("=== Test CustomService - failed to register function with the same name ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    bool success = custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "test_function_name_same",
        .description = "test_function_description_same",
        .parameters = {
            {.name = "test", .type = esp_brookesia::service::FunctionValueType::String},
        }
    }, [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = "test_result, from function same 1",
        };
    });

    bool success2 = custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "test_function_name_same",
        .description = "test_function_description_same",
        .parameters = {
            {.name = "test", .type = esp_brookesia::service::FunctionValueType::String},
        }
    }, [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = "test_result, from function same 2",
        };
    });
    TEST_ASSERT_EQUAL(true, success);
    // for now, register_function will return true even if the function is already registered,
    // if there is the same function name, there will be a error log.
    TEST_ASSERT_EQUAL(true, success2);
}

TEST_CASE("Test CustomService - failed to call non-existing function", "[service][custom][function][reference]")
{
    // BROOKESIA_TIME_PROFILER_SCOPE("test_custom_service_failed_to_call_non_existing_function");
    BROOKESIA_LOGI("=== Test CustomService - failed to call non-existing function ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto result = custom_service.call_function_sync("no_existing_function", boost::json::object{{"test", "test_value"}}, 1000);
    TEST_ASSERT_EQUAL(false, result.success);
}

TEST_CASE("Test CustomService - standard case 1", "[service][custom][function][reference]")
{
    // BROOKESIA_TIME_PROFILER_SCOPE("test_custom_service_standard_case_1");
    BROOKESIA_LOGI("=== Test CustomService - standard case 1 ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    esp_brookesia::service::FunctionSchema schema{
        .name = "test_function_name_1",
        .description = "test_function_description_1",
        .parameters = {
            {.name = "test", .type = esp_brookesia::service::FunctionValueType::String},
        }
    };

    std::function<esp_brookesia::service::FunctionResult(const esp_brookesia::service::FunctionParameterMap &args)> handler =
    [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        BROOKESIA_LOGI("test_function_name_1: %1%", std::get<std::string>(args.at("test")));
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = "test_result, from function 1",
        };
    };

    custom_service.register_function(std::move(schema), std::move(handler));

    auto result = custom_service.call_function_sync("test_function_name_1", boost::json::object{{"test", "test_value"}}, 1000);
    TEST_ASSERT_EQUAL(true, result.success);
    std::string result_str = std::get<std::string>(result.data.value());
    TEST_ASSERT_EQUAL_STRING("test_result, from function 1", result_str.c_str());
}

TEST_CASE("Test CustomService - standard case 2", "[service][custom][function][reference]")
{
    // BROOKESIA_TIME_PROFILER_SCOPE("test_custom_service_standard_case_2");
    BROOKESIA_LOGI("=== Test CustomService - standard case 2 ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    esp_brookesia::service::FunctionSchema schema{
        .name = "test_function_name_2",
        .description = "test_function_description_2",
        .parameters = {
            {.name = "test", .type = esp_brookesia::service::FunctionValueType::String},
        }
    };

    auto handler =
    [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        BROOKESIA_LOGI("test_function_name_2: %1%", std::get<std::string>(args.at("test")));
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = "test_result, from function 2",
        };
    };

    custom_service.register_function(std::move(schema), std::move(handler));

    auto result = custom_service.call_function_sync("test_function_name_2", boost::json::object{{"test", "test_value"}}, 1000);
    TEST_ASSERT_EQUAL(true, result.success);
    std::string result_str = std::get<std::string>(result.data.value());
    TEST_ASSERT_EQUAL_STRING("test_result, from function 2", result_str.c_str());
}

TEST_CASE("Test CustomService - register function with lvalue schema and handler", "[service][custom][function][lvalue]")
{
    BROOKESIA_LOGI("=== Test CustomService - register function with lvalue schema and handler ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    esp_brookesia::service::FunctionSchema schema{
        .name = "test_function_name_lvalue",
        .description = "test function lvalue",
        .parameters = {
            {.name = "a", .type = esp_brookesia::service::FunctionValueType::Number},
        }
    };
    std::function<esp_brookesia::service::FunctionResult(const esp_brookesia::service::FunctionParameterMap &args)> handler =
    [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        double a = std::get<double>(args.at("a"));
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = esp_brookesia::service::FunctionValue(a + 1),
        };
    };

    bool success = custom_service.register_function(schema, handler);
    TEST_ASSERT_TRUE(success);

    auto result = custom_service.call_function_sync("test_function_name_lvalue", boost::json::object{{"a", 41}}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_DOUBLE(42.0, std::get<double>(result.data.value()));
}

TEST_CASE("Test CustomService - lvalue function registration copies schema and handler lifetime", "[service][custom][function][lvalue][lifecycle]")
{
    BROOKESIA_LOGI("=== Test CustomService - lvalue function registration copies schema and handler lifetime ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    esp_brookesia::service::FunctionSchema schema{
        .name = "test_function_name_lvalue_lifecycle",
        .description = "schema and handler should be copied",
        .parameters = {
            {.name = "a", .type = esp_brookesia::service::FunctionValueType::Number},
        }
    };
    std::function<esp_brookesia::service::FunctionResult(const esp_brookesia::service::FunctionParameterMap &args)> handler =
    [offset = 2.0](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        double a = std::get<double>(args.at("a"));
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = esp_brookesia::service::FunctionValue(a * offset),
        };
    };

    bool success = custom_service.register_function(schema, handler);
    TEST_ASSERT_TRUE(success);

    schema.name = "test_function_name_lvalue_lifecycle_changed";
    handler = nullptr;

    auto result = custom_service.call_function_sync(
    "test_function_name_lvalue_lifecycle", boost::json::object{{"a", 21}}, 1000
                  );
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_DOUBLE(42.0, std::get<double>(result.data.value()));

    auto missing_result = custom_service.call_function_sync(
    "test_function_name_lvalue_lifecycle_changed", boost::json::object{{"a", 21}}, 1000
                          );
    TEST_ASSERT_FALSE(missing_result.success);
}

TEST_CASE("Test CustomService - lvalue function handler remains valid after scope exit", "[service][custom][function][lvalue][lifecycle]")
{
    BROOKESIA_LOGI("=== Test CustomService - lvalue function handler remains valid after scope exit ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    {
        esp_brookesia::service::FunctionSchema schema{
            .name = "test_function_name_lvalue_scope",
            .description = "handler copied from lvalue in inner scope",
            .parameters = {
                {.name = "a", .type = esp_brookesia::service::FunctionValueType::Number},
            }
        };
        std::function<esp_brookesia::service::FunctionResult(const esp_brookesia::service::FunctionParameterMap &args)> handler =
        [factor = 6.0](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
            double a = std::get<double>(args.at("a"));
            return esp_brookesia::service::FunctionResult{
                .success = true,
                .data = esp_brookesia::service::FunctionValue(a * factor),
            };
        };

        bool success = custom_service.register_function(schema, handler);
        TEST_ASSERT_TRUE(success);
    }

    auto result = custom_service.call_function_sync("test_function_name_lvalue_scope", boost::json::object{{"a", 7}}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_DOUBLE(42.0, std::get<double>(result.data.value()));
}

TEST_CASE("Test CustomService - lambda function as function handler", "[service][custom][function][reference][lambda]")
{
    // BROOKESIA_TIME_PROFILER_SCOPE("test_custom_service_function_with_lambda_function_as_function_handler");
    BROOKESIA_LOGI("=== Test CustomService - function with lambda function as function handler ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    int test_value_to_function = 100;

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "test_function_name_lambda",
        .description = "test_function_description_lambda",
        .parameters = {
            {.name = "test", .type = esp_brookesia::service::FunctionValueType::String},
        }
    }, [test_value_to_function](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        BROOKESIA_LOGI("test_value_to_function: %1%", test_value_to_function);
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = "test_result, from function lambda",
        };
    });

    auto result = custom_service.call_function_sync("test_function_name_lambda", boost::json::object{{"test", "test_value"}}, 1000);
    TEST_ASSERT_EQUAL(true, result.success);
    std::string result_str = std::get<std::string>(result.data.value());
    TEST_ASSERT_EQUAL_STRING("test_result, from function lambda", result_str.c_str());
}


TEST_CASE("Test CustomService -  std:function as function handler", "[service][custom][function][reference][std:function]")
{
    // BROOKESIA_TIME_PROFILER_SCOPE("test_custom_service_function_with_std_function_as_function_handler");
    BROOKESIA_LOGI("=== Test CustomService - function with std:function as function handler ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    std::function<esp_brookesia::service::FunctionResult(const esp_brookesia::service::FunctionParameterMap &args)> test_function =
    [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult{
        double a = std::get<double>(args.at("a"));
        esp_brookesia::service::FunctionResult result{
            .success = true,
            .data = esp_brookesia::service::FunctionValue(a + 99),
        };
        return result;
    };

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "test_function_name_3",
        .description = "test_function_description_3",
        .parameters = {
            {.name = "a", .type = esp_brookesia::service::FunctionValueType::Number},
        }
    }, std::move(test_function));


    auto result = custom_service.call_function_sync("test_function_name_3", boost::json::object{{"a", 100}}, 1000);
    TEST_ASSERT_EQUAL(true, result.success);
    int result_int = static_cast<int>(std::get<double>(result.data.value()));
    TEST_ASSERT_EQUAL(199, result_int);
}

TEST_CASE("Test CustomService -  std:function as function handler with reference parameter", "[service][custom][function][reference][std:function][reference]")
{
    // BROOKESIA_TIME_PROFILER_SCOPE("test_custom_service_function_with_std_function_as_function_handler_with_reference_parameter");
    BROOKESIA_LOGI("=== Test CustomService - function with std:function as function handler with reference parameter ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    int test_value_to_function = 200;

    std::function<esp_brookesia::service::FunctionResult(const esp_brookesia::service::FunctionParameterMap &args)> test_function =
    [&test_value_to_function](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult{
        BROOKESIA_LOGI("test_value_to_function: %1%", test_value_to_function);
        double a = std::get<double>(args.at("a"));
        esp_brookesia::service::FunctionResult result{
            .success = true,
            .data = esp_brookesia::service::FunctionValue(a + test_value_to_function),
        };
        return result;
    };

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "test_function_name_4",
        .description = "test_function_description_4",
        .parameters = {
            {.name = "a", .type = esp_brookesia::service::FunctionValueType::Number},
        }
    }, std::move(test_function));

    test_value_to_function = 300;

    auto result = custom_service.call_function_sync("test_function_name_4", boost::json::object{{"a", 100}}, 1000);
    TEST_ASSERT_EQUAL(true, result.success);
    int result_int = static_cast<int>(std::get<double>(result.data.value()));
    TEST_ASSERT_EQUAL(400, result_int);
}

TEST_CASE("Test CustomService -  class static function as function handler", "[service][custom][function][reference][class][function]")
{
    // BROOKESIA_TIME_PROFILER_SCOPE("test_custom_service_function_with_class_static_function_as_function_handler");
    BROOKESIA_LOGI("=== Test CustomService - function with class static function as function handler ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    class TestClass {
    public:
        TestClass() = default;
        ~TestClass() = default;
        static esp_brookesia::service::FunctionResult test_function(const esp_brookesia::service::FunctionParameterMap &args)
        {
            int a = static_cast<int>(std::get<double>(args.at("a")));
            return esp_brookesia::service::FunctionResult{
                .success = true,
                .data = esp_brookesia::service::FunctionValue(a),
            };
        }
    private:
        int test_value_to_function_ = 200;
    };

    TestClass test_class;

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "test_function_name_5",
        .description = "test_function_description_5",
        .parameters = {
            {.name = "a", .type = esp_brookesia::service::FunctionValueType::Number},
        }
    }, TestClass::test_function);


    auto result = custom_service.call_function_sync("test_function_name_5", boost::json::object{{"a", 199}}, 1000);
    TEST_ASSERT_EQUAL(true, result.success);
    int result_int = static_cast<int>(std::get<double>(result.data.value()));
    TEST_ASSERT_EQUAL(199, result_int);
}

TEST_CASE("Test CustomService -  error case for lambda function with reference capture", "[service][custom][function][reference][lambda][reference]")
{
    // BROOKESIA_TIME_PROFILER_SCOPE("test_custom_service_error_case_for_lambda_function_with_reference_capture");
    BROOKESIA_LOGI("=== Test CustomService - error case for lambda function with reference capture ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    {
        int test_value_to_function = 200;
        custom_service.register_function(esp_brookesia::service::FunctionSchema{
            .name = "test_function_name_lambda_reference",
            .description = "test_function_description_lambda_reference",
            .parameters = {
                {.name = "test", .type = esp_brookesia::service::FunctionValueType::String},
            }
        }, [&test_value_to_function](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
            BROOKESIA_LOGI("test_value_to_function from reference capture:  %1%", test_value_to_function);
            return esp_brookesia::service::FunctionResult{
                .success = true,
                .data = esp_brookesia::service::FunctionValue(static_cast<double>(test_value_to_function)),
            };
        });
    }

    // this is a error case, because test_value_to_function is out of life time,
    // so do not use this reference capture in lambda function as function handler.
    auto result = custom_service.call_function_sync("test_function_name_lambda_reference", boost::json::object{{"test", "test_value"}}, 1000);
    TEST_ASSERT_EQUAL(true, result.success);
    // double result_double = std::get<double>(result.data.value());
    // test_value_to_function is out of life time, so it should be 0.0
    // TEST_ASSERT_EQUAL(0.0, result_double);
}

// ========== Different callable object types ==========

TEST_CASE("Test CustomService - free function as handler", "[service][custom][callable][free_func]")
{
    BROOKESIA_LOGI("=== Test CustomService - free function as handler ===");

    struct FreeFunc {
        static esp_brookesia::service::FunctionResult handler(
            const esp_brookesia::service::FunctionParameterMap &args)
        {
            double x = std::get<double>(args.at("x"));
            return esp_brookesia::service::FunctionResult{
                .success = true,
                .data = esp_brookesia::service::FunctionValue(x * 2),
            };
        }
    };

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_free",
        .description = "free function handler",
        .parameters = {{.name = "x", .type = esp_brookesia::service::FunctionValueType::Number}},
    }, FreeFunc::handler);

    auto result = custom_service.call_function_sync("func_free", boost::json::object{{"x", 21}}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_DOUBLE(42.0, std::get<double>(result.data.value()));
}

TEST_CASE("Test CustomService - functor as handler", "[service][custom][callable][functor]")
{
    BROOKESIA_LOGI("=== Test CustomService - functor as handler ===");

    struct AddFunctor {
        int offset;
        explicit AddFunctor(int o) : offset(o) {}
        esp_brookesia::service::FunctionResult operator()(
            const esp_brookesia::service::FunctionParameterMap &args) const
        {
            double x = std::get<double>(args.at("x"));
            return esp_brookesia::service::FunctionResult{
                .success = true,
                .data = esp_brookesia::service::FunctionValue(x + offset),
            };
        }
    };

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    AddFunctor functor(100);
    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_functor",
        .description = "functor handler",
        .parameters = {{.name = "x", .type = esp_brookesia::service::FunctionValueType::Number}},
    }, functor);

    auto result = custom_service.call_function_sync("func_functor", boost::json::object{{"x", 5}}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_DOUBLE(105.0, std::get<double>(result.data.value()));
}

TEST_CASE("Test CustomService - member function as handler", "[service][custom][callable][member_func]")
{
    BROOKESIA_LOGI("=== Test CustomService - member function as handler ===");

    struct Calculator {
        double multiplier = 3.0;
        esp_brookesia::service::FunctionResult multiply(
            const esp_brookesia::service::FunctionParameterMap &args) const
        {
            double x = std::get<double>(args.at("x"));
            return esp_brookesia::service::FunctionResult{
                .success = true,
                .data = esp_brookesia::service::FunctionValue(x * multiplier),
            };
        }
    };

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    Calculator calc;
    calc.multiplier = 7.0;
    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_member",
        .description = "member function handler",
        .parameters = {{.name = "x", .type = esp_brookesia::service::FunctionValueType::Number}},
    }, [&calc](const esp_brookesia::service::FunctionParameterMap & args) {
        return calc.multiply(args);
    });

    auto result = custom_service.call_function_sync("func_member", boost::json::object{{"x", 6}}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_DOUBLE(42.0, std::get<double>(result.data.value()));
}

TEST_CASE("Test CustomService - std::bind as handler", "[service][custom][callable][bind]")
{
    BROOKESIA_LOGI("=== Test CustomService - std::bind as handler ===");

    struct BindFunc {
        static esp_brookesia::service::FunctionResult handler(
            const esp_brookesia::service::FunctionParameterMap &args)
        {
            double x = std::get<double>(args.at("x"));
            return esp_brookesia::service::FunctionResult{
                .success = true,
                .data = esp_brookesia::service::FunctionValue(x * 2),
            };
        }
    };

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto bound_handler = std::bind(BindFunc::handler, std::placeholders::_1);
    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_bind",
        .description = "std::bind handler",
        .parameters = {{.name = "x", .type = esp_brookesia::service::FunctionValueType::Number}},
    }, bound_handler);

    auto result = custom_service.call_function_sync("func_bind", boost::json::object{{"x", 10}}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_DOUBLE(20.0, std::get<double>(result.data.value()));
}

TEST_CASE("Test CustomService - lambda with mutable capture", "[service][custom][callable][lambda_mutable]")
{
    BROOKESIA_LOGI("=== Test CustomService - lambda with mutable capture ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    int call_count = 0;
    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_mutable",
        .description = "mutable lambda",
        .parameters = {},
    }, [call_count](const esp_brookesia::service::FunctionParameterMap &) mutable
    -> esp_brookesia::service::FunctionResult {
        ++call_count;
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = esp_brookesia::service::FunctionValue(static_cast<double>(call_count)),
        };
    });

    auto r1 = custom_service.call_function_sync("func_mutable", boost::json::object{}, 1000);
    auto r2 = custom_service.call_function_sync("func_mutable", boost::json::object{}, 1000);
    TEST_ASSERT_TRUE(r1.success);
    TEST_ASSERT_TRUE(r2.success);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, std::get<double>(r1.data.value()));
    // Note : this returned value shall be 1.0
    TEST_ASSERT_EQUAL_DOUBLE(1.0, std::get<double>(r2.data.value()));
}

TEST_CASE("Test CustomService - duplicate function keeps original handler", "[service][custom][reliability]")
{
    BROOKESIA_LOGI("=== Test CustomService - duplicate function keeps original handler ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string function_name = "func_duplicate_keeps_original";
    bool first_ok = custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = function_name,
        .description = "first version",
        .parameters = {},
    }, [](const esp_brookesia::service::FunctionParameterMap &) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = std::string("first"),
        };
    });
    bool second_ok = custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = function_name,
        .description = "second version",
        .parameters = {},
    }, [](const esp_brookesia::service::FunctionParameterMap &) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = std::string("second"),
        };
    });

    TEST_ASSERT_TRUE(first_ok);
    TEST_ASSERT_TRUE(second_ok);

    auto schemas = custom_service.get_function_schemas();
    int duplicate_count = 0;
    for (const auto &schema : schemas) {
        if (schema.name == function_name) {
            duplicate_count++;
        }
    }
    TEST_ASSERT_EQUAL(1, duplicate_count);

    auto result = custom_service.call_function_sync(function_name, boost::json::object{}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_STRING("first", std::get<std::string>(result.data.value()).c_str());
}

// ========== Reliability tests ==========

TEST_CASE("Test CustomService - failed to register function with name too long", "[service][custom][reliability]")
{
    BROOKESIA_LOGI("=== Test CustomService - failed to register function with name too long ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    std::string long_name(65, 'a');
    bool success = custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = long_name,
        .description = "description",
        .parameters = {},
    }, [](const esp_brookesia::service::FunctionParameterMap &) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{.success = true};
    });
    TEST_ASSERT_EQUAL(false, success);
}

TEST_CASE("Test CustomService - handler returns failure", "[service][custom][reliability]")
{
    BROOKESIA_LOGI("=== Test CustomService - handler returns failure ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_returns_failure",
        .description = "returns success=false",
        .parameters = {},
    }, [](const esp_brookesia::service::FunctionParameterMap &) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{
            .success = false,
            .error_message = "intentional failure",
        };
    });

    auto result = custom_service.call_function_sync("func_returns_failure", boost::json::object{}, 1000);
    TEST_ASSERT_EQUAL(false, result.success);
    TEST_ASSERT_EQUAL_STRING("intentional failure", result.error_message.c_str());
}

TEST_CASE("Test CustomService - function with no parameters", "[service][custom][reliability]")
{
    BROOKESIA_LOGI("=== Test CustomService - function with no parameters ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_no_params",
        .description = "no parameters",
        .parameters = {},
    }, [](const esp_brookesia::service::FunctionParameterMap &) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = "no_params_ok",
        };
    });

    auto result = custom_service.call_function_sync("func_no_params", boost::json::object{}, 1000);
    TEST_ASSERT_EQUAL(true, result.success);
    TEST_ASSERT_EQUAL_STRING("no_params_ok", std::get<std::string>(result.data.value()).c_str());
}

TEST_CASE("Test CustomService - optional parameter uses default value", "[service][custom][reliability]")
{
    BROOKESIA_LOGI("=== Test CustomService - optional parameter uses default value ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_default_param",
        .description = "uses default value",
        .parameters = {
            {
                .name = "count",
                .type = esp_brookesia::service::FunctionValueType::Number,
                .default_value = esp_brookesia::service::FunctionValue(7.0),
            },
        },
    }, [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = esp_brookesia::service::FunctionValue(std::get<double>(args.at("count"))),
        };
    });

    auto result = custom_service.call_function_sync("func_default_param", boost::json::object{}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, std::get<double>(result.data.value()));
}

TEST_CASE("Test CustomService - missing required parameter", "[service][custom][reliability]")
{
    BROOKESIA_LOGI("=== Test CustomService - missing required parameter ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_missing_required",
        .description = "requires parameter",
        .parameters = {
            {.name = "value", .type = esp_brookesia::service::FunctionValueType::Number},
        },
    }, [](const esp_brookesia::service::FunctionParameterMap &) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{.success = true};
    });

    auto result = custom_service.call_function_sync("func_missing_required", boost::json::object{}, 1000);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.error_message.find("Missing required parameter") != std::string::npos);
}

TEST_CASE("Test CustomService - invalid parameter type", "[service][custom][reliability]")
{
    BROOKESIA_LOGI("=== Test CustomService - invalid parameter type ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_invalid_param_type",
        .description = "expects number",
        .parameters = {
            {.name = "value", .type = esp_brookesia::service::FunctionValueType::Number},
        },
    }, [](const esp_brookesia::service::FunctionParameterMap &) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{.success = true};
    });

    auto result = custom_service.call_function_sync(
    "func_invalid_param_type", boost::json::object{{"value", "not_a_number"}}, 1000
                  );
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.error_message.find("Invalid type for parameter") != std::string::npos);
}

TEST_CASE("Test CustomService - unknown parameter is rejected", "[service][custom][reliability]")
{
    BROOKESIA_LOGI("=== Test CustomService - unknown parameter is rejected ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_unknown_param",
        .description = "rejects unknown parameter",
        .parameters = {
            {.name = "value", .type = esp_brookesia::service::FunctionValueType::Number},
        },
    }, [](const esp_brookesia::service::FunctionParameterMap &) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{.success = true};
    });

    auto result = custom_service.call_function_sync(
    "func_unknown_param", boost::json::object{{"value", 1}, {"extra", 2}}, 1000
                  );
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.error_message.find("Unknown parameter") != std::string::npos);
}

TEST_CASE("Test CustomService - multiple functions registered", "[service][custom][reliability]")
{
    BROOKESIA_LOGI("=== Test CustomService - multiple functions registered ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    for (int i = 0; i < 5; i++) {
        std::string name = "multi_func_" + std::to_string(i);
        bool ok = custom_service.register_function(esp_brookesia::service::FunctionSchema{
            .name = name,
            .description = "multi " + std::to_string(i),
            .parameters = {{.name = "x", .type = esp_brookesia::service::FunctionValueType::Number}},
        }, [i](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
            double x = std::get<double>(args.at("x"));
            return esp_brookesia::service::FunctionResult{
                .success = true,
                .data = esp_brookesia::service::FunctionValue(x + i),
            };
        });
        TEST_ASSERT_TRUE_MESSAGE(ok, ("Failed to register " + name).c_str());
    }

    for (int i = 0; i < 5; i++) {
        std::string name = "multi_func_" + std::to_string(i);
        auto result = custom_service.call_function_sync(name, boost::json::object{{"x", 10}}, 1000);
        TEST_ASSERT_TRUE(result.success);
        int val = static_cast<int>(std::get<double>(result.data.value()));
        TEST_ASSERT_EQUAL(10 + i, val);
    }
}

// ========== Parameter type coverage tests ==========

TEST_CASE("Test CustomService - Boolean parameter", "[service][custom][param_types]")
{
    BROOKESIA_LOGI("=== Test CustomService - Boolean parameter ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_bool",
        .description = "boolean param",
        .parameters = {{.name = "flag", .type = esp_brookesia::service::FunctionValueType::Boolean}},
    }, [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        bool flag = std::get<bool>(args.at("flag"));
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = esp_brookesia::service::FunctionValue(!flag),
        };
    });

    auto result = custom_service.call_function_sync("func_bool", boost::json::object{{"flag", true}}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FALSE(std::get<bool>(result.data.value()));

    result = custom_service.call_function_sync("func_bool", boost::json::object{{"flag", false}}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(std::get<bool>(result.data.value()));
}

TEST_CASE("Test CustomService - multiple parameters (String, Number, Boolean)", "[service][custom][param_types]")
{
    BROOKESIA_LOGI("=== Test CustomService - multiple parameters ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_multi",
        .description = "string, number, bool",
        .parameters = {
            {.name = "s", .type = esp_brookesia::service::FunctionValueType::String},
            {.name = "n", .type = esp_brookesia::service::FunctionValueType::Number},
            {.name = "b", .type = esp_brookesia::service::FunctionValueType::Boolean},
        },
    }, [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        std::string s = std::get<std::string>(args.at("s"));
        double n = std::get<double>(args.at("n"));
        bool b = std::get<bool>(args.at("b"));
        std::string out = s + "_" + std::to_string(static_cast<int>(n)) + "_" + (b ? "T" : "F");
        return esp_brookesia::service::FunctionResult{.success = true, .data = out};
    });

    auto result = custom_service.call_function_sync("func_multi",
    boost::json::object{{"s", "hello"}, {"n", 42}, {"b", true}}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_STRING("hello_42_T", std::get<std::string>(result.data.value()).c_str());
}

TEST_CASE("Test CustomService - Object parameter", "[service][custom][param_types]")
{
    BROOKESIA_LOGI("=== Test CustomService - Object parameter ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_object",
        .description = "json object param",
        .parameters = {{.name = "obj", .type = esp_brookesia::service::FunctionValueType::Object}},
    }, [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        const auto &obj = std::get<boost::json::object>(args.at("obj"));
        std::string key = std::string(obj.at("key").as_string());
        return esp_brookesia::service::FunctionResult{.success = true, .data = key};
    });

    auto result = custom_service.call_function_sync("func_object",
    boost::json::object{{"obj", boost::json::object{{"key", "value_from_obj"}}}}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_STRING("value_from_obj", std::get<std::string>(result.data.value()).c_str());
}

TEST_CASE("Test CustomService - Array parameter", "[service][custom][param_types]")
{
    BROOKESIA_LOGI("=== Test CustomService - Array parameter ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_array",
        .description = "json array param",
        .parameters = {{.name = "arr", .type = esp_brookesia::service::FunctionValueType::Array}},
    }, [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        const auto &arr = std::get<boost::json::array>(args.at("arr"));
        double sum = 0;
        for (const auto &v : arr)
        {
            sum += v.as_double();
        }
        return esp_brookesia::service::FunctionResult{.success = true, .data = esp_brookesia::service::FunctionValue(sum)};
    });

    boost::json::array arr = {1.0, 2.0, 3.0};
    auto result = custom_service.call_function_sync("func_array", boost::json::object{{"arr", arr}}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_DOUBLE(6.0, std::get<double>(result.data.value()));
}

// ========== call_function_sync overload coverage ==========

TEST_CASE("Test CustomService - call with FunctionParameterMap", "[service][custom][call_overload]")
{
    BROOKESIA_LOGI("=== Test CustomService - call with FunctionParameterMap ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_map_call",
        .description = "called with map",
        .parameters = {{.name = "a", .type = esp_brookesia::service::FunctionValueType::Number}},
    }, [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        double a = std::get<double>(args.at("a"));
        return esp_brookesia::service::FunctionResult{.success = true, .data = esp_brookesia::service::FunctionValue(a * 2)};
    });

    esp_brookesia::service::FunctionParameterMap params;
    params["a"] = 21.0;
    auto result = custom_service.call_function_sync("func_map_call", std::move(params), 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_DOUBLE(42.0, std::get<double>(result.data.value()));
}

TEST_CASE("Test CustomService - call with vector of parameters", "[service][custom][call_overload]")
{
    BROOKESIA_LOGI("=== Test CustomService - call with vector of parameters ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_vector_call",
        .description = "called with vector",
        .parameters = {
            {.name = "a", .type = esp_brookesia::service::FunctionValueType::Number},
            {.name = "b", .type = esp_brookesia::service::FunctionValueType::Number},
        },
    }, [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        double a = std::get<double>(args.at("a"));
        double b = std::get<double>(args.at("b"));
        return esp_brookesia::service::FunctionResult{.success = true, .data = esp_brookesia::service::FunctionValue(a + b)};
    });

    std::vector<esp_brookesia::service::FunctionValue> vec = {5.0, 7.0};
    auto result = custom_service.call_function_sync("func_vector_call", std::move(vec), 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_DOUBLE(12.0, std::get<double>(result.data.value()));
}

TEST_CASE("Test CustomService - async call returns expected result", "[service][custom][call_overload][async]")
{
    BROOKESIA_LOGI("=== Test CustomService - async call returns expected result ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_async_call",
        .description = "called asynchronously",
        .parameters = {{.name = "value", .type = esp_brookesia::service::FunctionValueType::Number}},
    }, [](const esp_brookesia::service::FunctionParameterMap & args) -> esp_brookesia::service::FunctionResult {
        double value = std::get<double>(args.at("value"));
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = esp_brookesia::service::FunctionValue(value * 3),
        };
    });

    auto future = custom_service.call_function_async("func_async_call", boost::json::object{{"value", 14}});
    auto result = future.get();
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_DOUBLE(42.0, std::get<double>(result.data.value()));
}

TEST_CASE("Test CustomService - async call for missing function fails", "[service][custom][call_overload][async]")
{
    BROOKESIA_LOGI("=== Test CustomService - async call for missing function fails ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto future = custom_service.call_function_async("func_async_missing", boost::json::object{});
    auto result = future.get();
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.error_message.find("Function not found") != std::string::npos);
}

TEST_CASE("Test CustomService - handler returns no data", "[service][custom][reliability]")
{
    BROOKESIA_LOGI("=== Test CustomService - handler returns no data ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = "func_no_data",
        .description = "returns success without data",
        .parameters = {},
    }, [](const esp_brookesia::service::FunctionParameterMap &) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{.success = true};
    });

    auto result = custom_service.call_function_sync("func_no_data", boost::json::object{}, 1000);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FALSE(result.has_data());
}

// ========== Event tests ==========

TEST_CASE("Test CustomService - register event", "[service][custom][event]")
{
    BROOKESIA_LOGI("=== Test CustomService - register event ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    custom_service.register_event(esp_brookesia::service::EventSchema{
        .name = "event_test",
        .description = "test event",
        .items = {
            {.name = "value", .type = esp_brookesia::service::EventItemType::Number},
        },
    });

    auto event_received_callback = [](const std::string & event_name, const esp_brookesia::service::EventItemMap & event_items) {
        BROOKESIA_LOGI("Event received: %1%", event_name);
        TEST_ASSERT_EQUAL_STRING("event_test", event_name.c_str());
        TEST_ASSERT_EQUAL_DOUBLE(100.0, std::get<double>(event_items.at("value")));
    };
    auto connection = custom_service.subscribe_event("event_test", event_received_callback);
    TEST_ASSERT_TRUE(connection.connected());

    bool result = custom_service.publish_event("event_test", esp_brookesia::service::EventItemMap{{"value", 100}});
    TEST_ASSERT_TRUE(result);

    // wait for event to be received and received callback to be called
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
}

TEST_CASE("Test CustomService - register event with lvalue schema", "[service][custom][event][lvalue]")
{
    BROOKESIA_LOGI("=== Test CustomService - register event with lvalue schema ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    esp_brookesia::service::EventSchema event_schema{
        .name = "event_lvalue_test",
        .description = "event registered with lvalue",
        .items = {
            {.name = "value", .type = esp_brookesia::service::EventItemType::Number},
        },
    };

    bool event_registered = custom_service.register_event(event_schema);
    TEST_ASSERT_TRUE(event_registered);

    auto connection = custom_service.subscribe_event(
                          "event_lvalue_test",
    [](const std::string & event_name, const esp_brookesia::service::EventItemMap & event_items) {
        TEST_ASSERT_EQUAL_STRING("event_lvalue_test", event_name.c_str());
        TEST_ASSERT_EQUAL_DOUBLE(42.0, std::get<double>(event_items.at("value")));
    }
                      );
    TEST_ASSERT_TRUE(connection.connected());

    bool publish_result = custom_service.publish_event("event_lvalue_test", esp_brookesia::service::EventItemMap{{"value", 42}});
    TEST_ASSERT_TRUE(publish_result);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
}

TEST_CASE("Test CustomService - lvalue event schema is copied after registration", "[service][custom][event][lvalue][lifecycle]")
{
    BROOKESIA_LOGI("=== Test CustomService - lvalue event schema is copied after registration ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    esp_brookesia::service::EventSchema event_schema{
        .name = "event_lvalue_lifecycle_test",
        .description = "event schema copy test",
        .items = {
            {.name = "value", .type = esp_brookesia::service::EventItemType::Number},
        },
    };

    bool event_registered = custom_service.register_event(event_schema);
    TEST_ASSERT_TRUE(event_registered);

    event_schema.name = "event_lvalue_lifecycle_test_changed";

    auto event_schemas = custom_service.get_event_schemas();
    bool found_original = false;
    bool found_changed = false;
    for (const auto &schema : event_schemas) {
        if (schema.name == "event_lvalue_lifecycle_test") {
            found_original = true;
        }
        if (schema.name == "event_lvalue_lifecycle_test_changed") {
            found_changed = true;
        }
    }

    TEST_ASSERT_TRUE(found_original);
    TEST_ASSERT_FALSE(found_changed);
}

TEST_CASE("Test CustomService - subscribe non-existing event", "[service][custom][event]")
{
    BROOKESIA_LOGI("=== Test CustomService - subscribe non-existing event ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto connection = custom_service.subscribe_event(
                          "event_not_exists",
    [](const std::string &, const esp_brookesia::service::EventItemMap &) {}
                      );
    TEST_ASSERT_FALSE(connection.connected());
}

TEST_CASE("Test CustomService - multiple subscribers receive same event", "[service][custom][event]")
{
    BROOKESIA_LOGI("=== Test CustomService - multiple subscribers receive same event ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string event_name = "event_multi_subscribers";
    bool event_registered = custom_service.register_event(esp_brookesia::service::EventSchema{
        .name = event_name,
        .description = "multi subscriber event",
        .items = {
            {.name = "value", .type = esp_brookesia::service::EventItemType::Number},
        },
    });
    TEST_ASSERT_TRUE(event_registered);

    std::mutex mutex;
    std::condition_variable cv;
    int callback_count = 0;

    auto callback = [&](const std::string & received_event_name, const esp_brookesia::service::EventItemMap & items) {
        TEST_ASSERT_EQUAL_STRING(event_name.c_str(), received_event_name.c_str());
        TEST_ASSERT_EQUAL_DOUBLE(42.0, std::get<double>(items.at("value")));
        {
            std::lock_guard<std::mutex> lock(mutex);
            callback_count++;
        }
        cv.notify_all();
    };

    auto connection_1 = custom_service.subscribe_event(event_name, callback);
    auto connection_2 = custom_service.subscribe_event(event_name, callback);
    TEST_ASSERT_TRUE(connection_1.connected());
    TEST_ASSERT_TRUE(connection_2.connected());

    bool publish_result = custom_service.publish_event(event_name, esp_brookesia::service::EventItemMap{{"value", 42}});
    TEST_ASSERT_TRUE(publish_result);

    std::unique_lock<std::mutex> lock(mutex);
    bool received_all = cv.wait_for(lock, std::chrono::seconds(1), [&]() {
        return callback_count == 2;
    });
    TEST_ASSERT_TRUE(received_all);
}

TEST_CASE("Test CustomService - publish event with missing item fails", "[service][custom][event]")
{
    BROOKESIA_LOGI("=== Test CustomService - publish event with missing item fails ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string event_name = "event_missing_item";
    bool event_registered = custom_service.register_event(esp_brookesia::service::EventSchema{
        .name = event_name,
        .description = "event requires an item",
        .items = {
            {.name = "value", .type = esp_brookesia::service::EventItemType::Number},
        },
    });
    TEST_ASSERT_TRUE(event_registered);

    auto connection = custom_service.subscribe_event(
                          event_name,
    [](const std::string &, const esp_brookesia::service::EventItemMap &) {}
                      );
    TEST_ASSERT_TRUE(connection.connected());

    bool publish_result = custom_service.publish_event(event_name, esp_brookesia::service::EventItemMap{});
    TEST_ASSERT_FALSE(publish_result);
}

TEST_CASE("Test CustomService - publish event with invalid item type fails", "[service][custom][event]")
{
    BROOKESIA_LOGI("=== Test CustomService - publish event with invalid item type fails ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string event_name = "event_invalid_item_type";
    bool event_registered = custom_service.register_event(esp_brookesia::service::EventSchema{
        .name = event_name,
        .description = "event validates item type",
        .items = {
            {.name = "value", .type = esp_brookesia::service::EventItemType::Number},
        },
    });
    TEST_ASSERT_TRUE(event_registered);

    auto connection = custom_service.subscribe_event(
                          event_name,
    [](const std::string &, const esp_brookesia::service::EventItemMap &) {}
                      );
    TEST_ASSERT_TRUE(connection.connected());

    bool publish_result = custom_service.publish_event(
    event_name, esp_brookesia::service::EventItemMap{{"value", std::string("wrong_type")}}
                          );
    TEST_ASSERT_FALSE(publish_result);
}

TEST_CASE("Test CustomService - dynamic registration and schema query", "[service][custom][dynamic_query]")
{
    BROOKESIA_LOGI("=== Test CustomService - dynamic registration and schema query ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto function_schemas = custom_service.get_function_schemas();
    auto event_schemas = custom_service.get_event_schemas();
    TEST_ASSERT_TRUE(function_schemas.empty());
    TEST_ASSERT_TRUE(event_schemas.empty());

    constexpr int kRegisterCount = 5;
    for (int i = 0; i < kRegisterCount; ++i) {
        const std::string function_name = "dynamic_func_" + std::to_string(i);
        const std::string event_name = "dynamic_event_" + std::to_string(i);

        bool function_registered = custom_service.register_function(esp_brookesia::service::FunctionSchema{
            .name = function_name,
            .description = "dynamic function",
            .parameters = {},
        }, [i](const esp_brookesia::service::FunctionParameterMap &) -> esp_brookesia::service::FunctionResult {
            return esp_brookesia::service::FunctionResult{
                .success = true,
                .data = esp_brookesia::service::FunctionValue(static_cast<double>(i)),
            };
        });
        TEST_ASSERT_TRUE(function_registered);

        bool event_registered = custom_service.register_event(esp_brookesia::service::EventSchema{
            .name = event_name,
            .description = "dynamic event",
            .items = {
                {.name = "value", .type = esp_brookesia::service::EventItemType::Number},
            },
        });
        TEST_ASSERT_TRUE(event_registered);

        function_schemas = custom_service.get_function_schemas();
        event_schemas = custom_service.get_event_schemas();

        TEST_ASSERT_EQUAL(static_cast<int>(i + 1), static_cast<int>(function_schemas.size()));
        TEST_ASSERT_EQUAL(static_cast<int>(i + 1), static_cast<int>(event_schemas.size()));

        bool function_found = false;
        for (const auto &schema : function_schemas) {
            if (schema.name == function_name) {
                function_found = true;
                break;
            }
        }
        TEST_ASSERT_TRUE(function_found);

        bool event_found = false;
        for (const auto &schema : event_schemas) {
            if (schema.name == event_name) {
                event_found = true;
                break;
            }
        }
        TEST_ASSERT_TRUE(event_found);
    }

    function_schemas = custom_service.get_function_schemas();
    event_schemas = custom_service.get_event_schemas();
    TEST_ASSERT_EQUAL(static_cast<int>(kRegisterCount), static_cast<int>(function_schemas.size()));
    TEST_ASSERT_EQUAL(static_cast<int>(kRegisterCount), static_cast<int>(event_schemas.size()));
}

TEST_CASE("Test CustomService - unregister function", "[service][custom][unregister][function]")
{
    BROOKESIA_LOGI("=== Test CustomService - unregister function ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string function_name = "func_unregister_test";
    bool function_registered = custom_service.register_function(esp_brookesia::service::FunctionSchema{
        .name = function_name,
        .description = "function for unregister test",
        .parameters = {},
    }, [](const esp_brookesia::service::FunctionParameterMap &) -> esp_brookesia::service::FunctionResult {
        return esp_brookesia::service::FunctionResult{
            .success = true,
            .data = esp_brookesia::service::FunctionValue(std::string("ok")),
        };
    });
    TEST_ASSERT_TRUE(function_registered);

    auto function_schemas_before = custom_service.get_function_schemas();
    bool function_found_before = false;
    for (const auto &schema : function_schemas_before) {
        if (schema.name == function_name) {
            function_found_before = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(function_found_before);

    bool unregistered = custom_service.unregister_function(function_name);
    TEST_ASSERT_TRUE(unregistered);

    auto function_schemas_after = custom_service.get_function_schemas();
    bool function_found_after = false;
    for (const auto &schema : function_schemas_after) {
        if (schema.name == function_name) {
            function_found_after = true;
            break;
        }
    }
    TEST_ASSERT_FALSE(function_found_after);

    auto call_result = custom_service.call_function_sync(function_name, boost::json::object{}, 1000);
    TEST_ASSERT_FALSE(call_result.success);
}

TEST_CASE("Test CustomService - unregister event", "[service][custom][unregister][event]")
{
    BROOKESIA_LOGI("=== Test CustomService - unregister event ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const std::string event_name = "event_unregister_test";
    bool event_registered = custom_service.register_event(esp_brookesia::service::EventSchema{
        .name = event_name,
        .description = "event for unregister test",
        .items = {
            {.name = "value", .type = esp_brookesia::service::EventItemType::Number},
        },
    });
    TEST_ASSERT_TRUE(event_registered);

    auto event_schemas_before = custom_service.get_event_schemas();
    bool event_found_before = false;
    for (const auto &schema : event_schemas_before) {
        if (schema.name == event_name) {
            event_found_before = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(event_found_before);

    bool unregistered = custom_service.unregister_event(event_name);
    TEST_ASSERT_TRUE(unregistered);

    auto event_schemas_after = custom_service.get_event_schemas();
    bool event_found_after = false;
    for (const auto &schema : event_schemas_after) {
        if (schema.name == event_name) {
            event_found_after = true;
            break;
        }
    }
    TEST_ASSERT_FALSE(event_found_after);

    bool publish_result = custom_service.publish_event(event_name, esp_brookesia::service::EventItemMap{{"value", 1}});
    TEST_ASSERT_FALSE(publish_result);
}

static bool startup()
{
    // Clear stale binding first (e.g., previous test aborted before shutdown()).
    // Otherwise move-assignment to `binding` may release an old handle and unbind the newly started service.
    binding.release();

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

    binding = service_manager.bind(service::CustomServiceName);
    BROOKESIA_CHECK_FALSE_RETURN(binding.is_valid(), false, "Failed to bind service");
    return true;
}

static void shutdown()
{
    // custom_service.stop();
    binding.release();
    service_manager.deinit();
    time_profiler.report();
    time_profiler.clear();
}
