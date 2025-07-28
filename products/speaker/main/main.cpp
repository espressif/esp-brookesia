/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <cassert>
#include "lvgl.h"
#include "boost/thread.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Main"
#include "esp_lib_utils.h"
#include "modules/audio_sys.h"
#include "modules/display.hpp"
#include "modules/services.hpp"
#include "modules/audio.hpp"
#include "modules/system.hpp"
#include "modules/file_system.hpp"

constexpr bool EXAMPLE_SHOW_MEM_INFO = false;

extern "C" void app_main()
{
    printf("Project version: %s\n", CONFIG_APP_PROJECT_VER);

    assert(services_init()                      && "Initialize services failed");
    auto default_dummy_draw = !system_check_is_developer_mode();
    assert(display_init(default_dummy_draw)     && "Initialize display failed");
    assert(file_system_init()                   && "Initialize file system failed");
    assert(audio_init()                         && "Initialize audio failed");
    assert(system_init()                        && "Initialize system failed");

    if constexpr (EXAMPLE_SHOW_MEM_INFO) {
        esp_utils::thread_config_guard thread_config({
            .name = "mem_info",
            .stack_size = 4096,
        });
        boost::thread([ = ]() {
            char buffer[512];    /* Make sure buffer is enough for `sprintf` */

            while (1) {
                // heap_caps_check_integrity_all(true);

                esp_utils_mem_print_info();

                lv_mem_monitor_t mon;
                lv_mem_monitor(&mon);
                sprintf(buffer, "used: %zu (%3d %%), frag: %3d %%, biggest free: %zu, total: %zu, free: %zu",
                        mon.total_size - mon.free_size, mon.used_pct, mon.frag_pct,
                        mon.free_biggest_size, mon.total_size, mon.free_size);
                ESP_UTILS_LOGI("%s", buffer);

                audio_sys_get_real_time_stats();

                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }).detach();
    }
}
