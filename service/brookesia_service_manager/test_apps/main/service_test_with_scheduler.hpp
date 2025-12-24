/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/service_manager/service/base.hpp"

namespace esp_brookesia::service {

class ServiceTestWithScheduler : public ServiceBase {
public:
    enum FunctionIndex {
        FunctionIndexAdd,
        FunctionIndexDivide,
        FunctionIndexTestAllTypes,
        FunctionIndexSuspend,
        FunctionIndexMax,
    };

    enum EventIndex {
        EventIndexValueChange,
        EventIndexMax,
    };

    static constexpr const char *SERVICE_NAME = "service_test_with_scheduler";
    inline static const FunctionSchema FUNCTION_SCHEMAS[FunctionIndexMax] = {
        [FunctionIndexAdd] = {
            "add",
            "Add two numbers together",
            {
                {"a", "First number", FunctionValueType::Number},
                {"b", "Second number", FunctionValueType::Number}
            }
        },
        [FunctionIndexDivide] = {
            "divide",
            "Divide two numbers",
            {
                {"a", "First number", FunctionValueType::Number},
                {"b", "Second number", FunctionValueType::Number}
            }
        },
        [FunctionIndexTestAllTypes] = {
            "test_all_types",
            "Test function that accepts all parameter types",
            {
                {"boolean_param", "Boolean parameter", FunctionValueType::Boolean},
                {"number_param", "Number parameter", FunctionValueType::Number},
                {"string_param", "String parameter", FunctionValueType::String, "Hello World"},
                {"object_param", "Object parameter", FunctionValueType::Object},
                {"array_param", "Array parameter", FunctionValueType::Array}
            }
        },
        [FunctionIndexSuspend] = {
            "suspend",
            "Suspend the service",
            {}
        },
    };
    inline static const EventSchema EVENT_SCHEMAS[EventIndexMax] = {
        [EventIndexValueChange] = {
            "value_change",
            "Value change event",
            {
                {"value", "", EventItemType::Number}
            }
        },
    };

    ServiceTestWithScheduler()
        : ServiceBase({
        .name = SERVICE_NAME,
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{}
    })
    {}
    ~ServiceTestWithScheduler() = default;

    std::vector<FunctionSchema> get_function_schemas() override
    {
        return std::vector<FunctionSchema>(FUNCTION_SCHEMAS, FUNCTION_SCHEMAS + FunctionIndexMax);
    }

    std::vector<EventSchema> get_event_schemas() override
    {
        return std::vector<EventSchema>(EVENT_SCHEMAS, EVENT_SCHEMAS + EventIndexMax);
    }

    bool trigger_event();
    double get_event_value() const
    {
        return event_value_;
    }

protected:
    bool on_init() override;
    void on_deinit() override;
    bool on_start() override;
    void on_stop() override;

    ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_FUNC_HANDLER_2(
                FUNCTION_SCHEMAS[FunctionIndexAdd].name,
                FUNCTION_SCHEMAS[FunctionIndexAdd].parameters[0].name,
                double,
                FUNCTION_SCHEMAS[FunctionIndexAdd].parameters[1].name,
                double,
                function_add(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_FUNC_HANDLER_2(
                FUNCTION_SCHEMAS[FunctionIndexDivide].name,
                FUNCTION_SCHEMAS[FunctionIndexDivide].parameters[0].name,
                double,
                FUNCTION_SCHEMAS[FunctionIndexDivide].parameters[1].name,
                double,
                function_divide(PARAM1, PARAM2)
            ),
            {
                FUNCTION_SCHEMAS[FunctionIndexTestAllTypes].name,
                [this](const FunctionParameterMap & args) -> FunctionResult {
                    return to_function_result(function_test_all_types(args));
                }
            },
            BROOKESIA_SERVICE_FUNC_HANDLER_0(
                FUNCTION_SCHEMAS[FunctionIndexSuspend].name,
                function_suspend()
            )
        };
    }

private:
    // Function implementations
    std::expected<double, std::string> function_add(double a, double b);
    std::expected<double, std::string> function_divide(double a, double b);
    std::expected<boost::json::object, std::string> function_test_all_types(const FunctionParameterMap &args);
    std::expected<void, std::string> function_suspend();

    double event_value_ = 0;
};

} // namespace esp_brookesia::service
