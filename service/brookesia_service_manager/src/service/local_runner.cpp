/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager/service/manager.hpp"
#include "brookesia/service_manager/service/local_runner.hpp"

using namespace esp_brookesia::lib_utils;
using namespace esp_brookesia::service;

bool LocalTestRunner::run_tests(const RunTestsConfig &config, const std::vector<LocalTestItem> &test_items)
{
    BROOKESIA_LOGI(
        "Starting test sequence with config: %1%",
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(config, DESCRIBE_FORMAT_VERBOSE)
    );

    auto &service_manager = ServiceManager::get_instance();

    BROOKESIA_CHECK_FALSE_RETURN(service_manager.is_initialized(), false, "Service manager not initialized");

    // Bind the service
    auto service_binding = service_manager.bind(config.service_name);
    BROOKESIA_CHECK_FALSE_RETURN(service_binding.is_valid(), false, "Failed to bind service");

    // Get the service instance
    auto service = service_binding.get_service();
    BROOKESIA_CHECK_NULL_RETURN(service, false, "Failed to get service");

    // Start the scheduler
    std::shared_ptr<TaskScheduler> scheduler;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        scheduler = std::make_shared<TaskScheduler>(), false, "Failed to create scheduler"
    );
    BROOKESIA_CHECK_FALSE_RETURN(scheduler->start(config.scheduler_config), false, "Failed to start scheduler");

    // Reset the state
    test_results_.clear();
    test_results_.resize(test_items.size(), false);
    completed_count_ = 0;
    failed_count_ = 0;

    // Submit the test tasks in order
    for (size_t i = 0; i < test_items.size(); i++) {
        const auto &item = test_items[i];

        // Calculate the accumulated delay
        uint32_t accumulated_delay = 0;
        for (size_t j = 0; j < i; j++) {
            accumulated_delay += test_items[j].start_delay_ms;
            accumulated_delay += test_items[j].run_duration_ms;
        }
        accumulated_delay += item.start_delay_ms;

        BROOKESIA_LOGI("Scheduling test[%1%] at %2%ms", i, accumulated_delay);

        // Submit the test task
        scheduler->post_delayed([this, i, service, &item]() {
            execute_test(i, service, item);
        }, accumulated_delay);
    }

    // Wait for all tests to complete
    uint32_t total_timeout = 0;
    for (const auto &item : test_items) {
        total_timeout += item.start_delay_ms + item.run_duration_ms;
    }
    total_timeout += config.extra_timeout_ms;

    BROOKESIA_LOGI("Waiting for all tests to complete, timeout: %1% ms", total_timeout);

    uint32_t elapsed = 0;
    const uint32_t poll_interval = 10;
    while (completed_count_ < test_items.size() || elapsed < total_timeout) {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(poll_interval));
        elapsed += poll_interval;
    }

    BROOKESIA_CHECK_FALSE_RETURN(scheduler->wait_all(2000), false, "Wait for all tests to complete timeout");

    scheduler->stop();

    // Count the results
    bool all_passed = (failed_count_ == 0) && (completed_count_ == test_items.size());

    BROOKESIA_LOGI("Test sequence completed: total=%1%, completed=%2%, passed=%3%, failed=%4%",
                   test_items.size(), completed_count_.load(),
                   completed_count_.load() - failed_count_.load(), failed_count_.load());

    return all_passed;
}

const std::vector<bool> &LocalTestRunner::get_results() const
{
    return test_results_;
}

void LocalTestRunner::execute_test(
    size_t index, std::shared_ptr<ServiceBase> service, const LocalTestItem &item
)
{
    BROOKESIA_LOGI(
        "Executing test[%1%]: %2%", index,
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(item, DESCRIBE_FORMAT_VERBOSE)
    );

    bool test_passed = false;
    std::string error_msg;

    // Convert the parameters format
    FunctionParameterMap parameters;
    for (const auto &[key, value] : item.params) {
        if (value.is_bool()) {
            parameters[key] = FunctionValue(value.as_bool());
        } else if (value.is_double()) {
            parameters[key] = FunctionValue(value.as_double());
        } else if (value.is_string()) {
            parameters[key] = FunctionValue(std::string(value.as_string()));
        } else if (value.is_object()) {
            parameters[key] = FunctionValue(value.as_object());
        } else if (value.is_array()) {
            parameters[key] = FunctionValue(value.as_array());
        }
    }

    // Execute the service call
    FunctionResult result;
    {
        BROOKESIA_TIME_PROFILER_SCOPE(item.method);
        result = service->call_function_sync(item.method, std::move(parameters), item.call_timeout_ms);
    }

    // Validate the result
    if (result.success) {
        if (result.has_data() && item.validator) {
            test_passed = item.validator(result.data.value());
            if (!test_passed) {
                error_msg = "Validation failed";
            }
        } else {
            test_passed = true;
        }
    } else {
        error_msg = result.error_message;
    }

    // Record the result
    test_results_[index] = test_passed;
    completed_count_++;

    if (!test_passed) {
        failed_count_++;
        BROOKESIA_LOGE("Test[%1%] FAILED: %2% - %3%", index, item.name, error_msg);
    } else {
        BROOKESIA_LOGI("Test[%1%] PASSED: %2%", index, item.name);
    }
}
