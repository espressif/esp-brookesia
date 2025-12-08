/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "boost/json.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/service/base.hpp"

namespace esp_brookesia::service {

/**
 * @brief Configuration for a single test item
 */
struct LocalTestItem {
    std::string name;                                           // Test name
    std::string method;                                         // Service method name
    boost::json::object params = boost::json::object{};         // Method parameters
    std::function<bool(const FunctionValue &)> validator = nullptr; // Result validator
    uint32_t start_delay_ms = 100;                              // Start delay in milliseconds
    uint32_t call_timeout_ms = 100;                             // Call timeout in milliseconds
    uint32_t run_duration_ms = 200;                             // Run duration in milliseconds, should be greater than call timeout
};

/**
 * @brief Local service test runner
 *
 * A test framework based on TaskScheduler that supports executing test sequences in order,
 * where each test item can specify a start delay and run duration.
 */
class LocalTestRunner {
public:
    struct RunTestsConfig {
        std::string service_name;
        lib_utils::TaskScheduler::StartConfig scheduler_config;
        uint32_t extra_timeout_ms;

        RunTestsConfig(std::string service_name)
            : service_name(service_name)
            , scheduler_config{
            .worker_configs = {
                {
                    .stack_size = 10 * 1024,
                }
            }}
        , extra_timeout_ms(1000)
        {}
    };

    LocalTestRunner() = default;
    ~LocalTestRunner() = default;

    /**
     * @brief Run test sequence
     *
     * @param[in] config Run tests configuration
     * @param[in] test_items List of test items
     * @return true if all tests passed, false otherwise
     */
    bool run_tests(const RunTestsConfig &config, const std::vector<LocalTestItem> &test_items);

    /**
     * @brief Run test sequence with default configuration
     *
     * @param[in] service_name Service name to test
     * @param[in] test_items List of test items
     * @return true if all tests passed, false otherwise
     */
    bool run_tests(const std::string &service_name, const std::vector<LocalTestItem> &test_items)
    {
        return run_tests(RunTestsConfig(service_name), test_items);
    }

    /**
     * @brief Get test results
     *
     * @return const std::vector<bool>& Result of each test item
     */
    const std::vector<bool> &get_results() const;

private:
    /**
     * @brief Execute a single test
     *
     * @param[in] index Test item index
     * @param[in] service Service instance
     * @param[in] item Test item configuration
     */
    void execute_test(size_t index, std::shared_ptr<ServiceBase> service, const LocalTestItem &item);

    std::vector<bool> test_results_;
    std::atomic<size_t> completed_count_{0};
    std::atomic<size_t> failed_count_{0};
};

BROOKESIA_DESCRIBE_STRUCT(LocalTestItem, (), (name, method, params, validator, start_delay_ms, call_timeout_ms, run_duration_ms))
BROOKESIA_DESCRIBE_STRUCT(LocalTestRunner::RunTestsConfig, (), (service_name, scheduler_config, extra_timeout_ms))

} // namespace esp_brookesia::service
