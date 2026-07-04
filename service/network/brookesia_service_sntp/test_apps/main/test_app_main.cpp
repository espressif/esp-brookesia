/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#if !defined(ESP_PLATFORM)
#define BROOKESIA_LIB_UTILS_TEST_ADAPTER_MAIN
#endif
#include "brookesia/lib_utils/test_adapter.hpp"

#if defined(ESP_PLATFORM)
#include "esp_system.h"
#include "unity_test_utils.h"

// Default: strict leak check for offline SNTP cases (set/get, start/stop without real network).
#define TEST_MEMORY_LEAK_THRESHOLD (32)

// tearDown reads memory_leak_threshold; network tests may raise it before running (see
// TEST_SNTP_NETWORK_MEMORY_LEAK_THRESHOLD in general.hpp).
int memory_leak_threshold = TEST_MEMORY_LEAK_THRESHOLD;

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    esp_reent_cleanup();
    unity_utils_evaluate_leaks_direct(memory_leak_threshold);
    memory_leak_threshold = TEST_MEMORY_LEAK_THRESHOLD;
}

extern "C" void app_main(void)
{
    unity_run_menu();
}
#else

namespace {

bool init_unit_test()
{
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    return boost::unit_test::unit_test_main(&init_unit_test, argc, argv);
}
#endif
