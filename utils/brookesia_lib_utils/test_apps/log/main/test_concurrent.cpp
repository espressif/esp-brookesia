/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <atomic>
#include <string>
#include <vector>
#include "boost/chrono.hpp"
#include "boost/thread/thread.hpp"
#include "unity.h"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/thread_config.hpp"

using namespace esp_brookesia::lib_utils;

TEST_CASE("Repro concurrent formatted BROOKESIA_LOGI crash in worker threads",
          "[utils][log][concurrent][manual][repro][crash]")
{
    BROOKESIA_LOGI("=== Repro concurrent formatted BROOKESIA_LOGI crash in worker threads ===");

    constexpr int worker_count = 4;
    std::atomic<int> ready_count{0};
    std::atomic<bool> start_logging{false};
    std::atomic<int> finished_count{0};

    std::vector<boost::thread> workers;
    workers.reserve(worker_count);

    for (int i = 0; i < worker_count; ++i) {
        ThreadConfig worker_config = {
            .name = std::string("LOG_Worker") + std::to_string(i + 1),
            .core_id = i % 2,
            .stack_size = 20 * 1024,
        };

        BROOKESIA_THREAD_CONFIG_GUARD(worker_config);
        workers.emplace_back([&ready_count, &start_logging, &finished_count]() {
            int value = ready_count.fetch_add(1) + 1;
            while (!start_logging.load()) {
                boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
            }

            // Minimal repro: concurrent formatted logging from worker threads.
            BROOKESIA_LOGI("%1%", value);

            finished_count.fetch_add(1);
        });
    }

    while (ready_count.load() < worker_count) {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
    }

    start_logging.store(true);

    for (auto &worker : workers) {
        worker.join();
    }

    TEST_ASSERT_EQUAL(worker_count, finished_count.load());
}
