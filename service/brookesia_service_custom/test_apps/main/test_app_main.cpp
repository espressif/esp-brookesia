/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_check.h"
#include "unity.h"
#include "unity_test_utils.h"
#include "brookesia/service_custom/service_custom.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"

#include "unity.h"
#include "boost/json.hpp"
#include "boost/thread.hpp"
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <set>
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_manager/service/local_runner.hpp"
#include "brookesia/service_custom/service_custom.hpp"
#include "common_def.hpp"

// Some resources are lazy allocated in the driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (0)

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    // Ensure service resources are reclaimed even if a test aborts on assertion.
    esp_brookesia::service::ServiceManager::get_instance().deinit();
    esp_brookesia::lib_utils::TimeProfiler::get_instance().clear();
    esp_reent_cleanup();    //clean up some of the newlib's lazy allocations
    unity_utils_evaluate_leaks_direct(TEST_MEMORY_LEAK_THRESHOLD);
}

extern "C" void app_main(void)
{
    /*
    *   ______   ________  _______   __     __  ______   ______   ________         ______   __    __   ______  ________   ______   __       __
    *  /      \ |        \|       \ |  \   |  \|      \ /      \ |        \       /      \ |  \  |  \ /      \|        \ /      \ |  \     /  \
    * |  $$$$$$\| $$$$$$$$| $$$$$$$\| $$   | $$ \$$$$$$|  $$$$$$\| $$$$$$$$      |  $$$$$$\| $$  | $$|  $$$$$$\\$$$$$$$$|  $$$$$$\| $$\   /  $$
    * | $$___\$$| $$__    | $$__| $$| $$   | $$  | $$  | $$   \$$| $$__          | $$   \$$| $$  | $$| $$___\$$  | $$   | $$  | $$| $$$\ /  $$$
    *  \$$    \ | $$  \   | $$    $$ \$$\ /  $$  | $$  | $$      | $$  \         | $$      | $$  | $$ \$$    \   | $$   | $$  | $$| $$$$\  $$$$
    *  _\$$$$$$\| $$$$$   | $$$$$$$\  \$$\  $$   | $$  | $$   __ | $$$$$         | $$   __ | $$  | $$ _\$$$$$$\  | $$   | $$  | $$| $$\$$ $$ $$
    * |  \__| $$| $$_____ | $$  | $$   \$$ $$   _| $$_ | $$__/  \| $$_____       | $$__/  \| $$__/ $$|  \__| $$  | $$   | $$__/ $$| $$ \$$$| $$
    *  \$$    $$| $$     \| $$  | $$    \$$$   |   $$ \ \$$    $$| $$     \       \$$    $$ \$$    $$ \$$    $$  | $$    \$$    $$| $$  \$ | $$
    *   \$$$$$$  \$$$$$$$$ \$$   \$$     \$     \$$$$$$  \$$$$$$  \$$$$$$$$        \$$$$$$   \$$$$$$   \$$$$$$    \$$     \$$$$$$  \$$      \$$
    */
    printf("   ______   ________  _______   __     __  ______   ______   ________         ______   __    __   ______  ________   ______   __       __ \r\n");
    printf("  /      \\ |        \\|       \\ |  \\   |  \\|      \\ /      \\ |        \\       /      \\ |  \\  |  \\ /      \\|        \\ /      \\ |  \\     /  \\ \r\n");
    printf(" |  $$$$$$\\| $$$$$$$$| $$$$$$$\\| $$   | $$ \\$$$$$$|  $$$$$$\\| $$$$$$$$      |  $$$$$$\\| $$  | $$|  $$$$$$\\$$$$$$$$|  $$$$$$\\| $$\\   /  $$ \r\n");
    printf(" | $$___\\$$| $$__    | $$__| $$| $$   | $$  | $$  | $$   \\$$| $$__          | $$   \\$$| $$  | $$| $$___\\$$  | $$   | $$  | $$| $$$\\ /  $$$ \r\n");
    printf("  \\$$    \\ | $$  \\   | $$    $$ \\$$\\ /  $$  | $$  | $$      | $$  \\         | $$      | $$  | $$ \\$$    \\   | $$   | $$  | $$| $$$$\\  $$$$ \r\n");
    printf("  _\\$$$$$$\\| $$$$$   | $$$$$$$\\  \\$$\\  $$   | $$  | $$   __ | $$$$$         | $$   __ | $$  | $$ _\\$$$$$$\\  | $$   | $$  | $$| $$\\$$ $$ $$ \r\n");
    printf(" |  \\__| $$| $$_____ | $$  | $$   \\$$ $$   _| $$_ | $$__/  \\| $$_____       | $$__/  \\| $$__/ $$|  \\__| $$  | $$   | $$__/ $$| $$ \\$$$| $$ \r\n");
    printf("  \\$$    $$| $$     \\| $$  | $$    \\$$$   |   $$ \\ \\$$    $$| $$     \\       \\$$    $$ \\$$    $$ \\$$    $$  | $$    \\$$    $$| $$  \\$ | $$ \r\n");
    printf("   \\$$$$$$  \\$$$$$$$$ \\$$   \\$$     \\$     \\$$$$$$  \\$$$$$$  \\$$$$$$$$        \\$$$$$$   \\$$$$$$   \\$$$$$$    \\$$     \\$$$$$$  \\$$      \\$$ \r\n");
    unity_run_menu();
}
