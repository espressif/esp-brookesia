/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "bsp/esp-bsp.h"
#include "esp_brookesia.hpp"
#include "boost/thread.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Main"
#include "esp_lib_utils.h"

using namespace esp_brookesia::gui;

#define LVGL_PORT_INIT_CONFIG() \
    {                               \
        .task_priority = 4,       \
        .task_stack = 10 * 1024,       \
        .task_affinity = -1,      \
        .task_max_sleep_ms = 500, \
        .timer_period_ms = 5,     \
    }

constexpr bool EXAMPLE_SHOW_MEM_INFO = false;

extern "C" void app_main(void)
{
    ESP_UTILS_LOGI("Display ESP-Brookesia phone demo");

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_DRAW_BUF_HEIGHT,
#if CONFIG_BSP_LCD_DRAW_BUF_DOUBLE
        .double_buffer = 1,
#else
        .double_buffer = 0,
#endif
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
        }
    };
    ESP_UTILS_CHECK_NULL_EXIT(bsp_display_start_with_config(&cfg), "Start display failed");
    ESP_UTILS_CHECK_ERROR_EXIT(bsp_display_backlight_on(), "Turn on display backlight failed");

    /* Configure GUI lock */
    LvLock::registerCallbacks([](int timeout_ms) {
        if (timeout_ms < 0) {
            timeout_ms = 0;
        } else if (timeout_ms == 0) {
            timeout_ms = 1;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(bsp_display_lock(timeout_ms), false, "Lock failed");

        return true;
    }, []() {
        bsp_display_unlock();

        return true;
    });

    /* Create a phone object */
    ESP_Brookesia_Phone *phone = new (std::nothrow) ESP_Brookesia_Phone();
    ESP_UTILS_CHECK_NULL_EXIT(phone, "Create phone failed");

    /* Try using a stylesheet that corresponds to the resolution */
    if ((BSP_LCD_H_RES == 320) && (BSP_LCD_V_RES == 240)) {
        ESP_Brookesia_PhoneStylesheet_t *stylesheet = new (std::nothrow) ESP_Brookesia_PhoneStylesheet_t(ESP_BROOKESIA_PHONE_320_240_DARK_STYLESHEET());
        ESP_UTILS_CHECK_NULL_EXIT(stylesheet, "Create stylesheet failed");

        ESP_UTILS_LOGI("Using stylesheet (%s)", stylesheet->core.name);
        ESP_UTILS_CHECK_FALSE_EXIT(phone->addStylesheet(stylesheet), "Add stylesheet failed");
        ESP_UTILS_CHECK_FALSE_EXIT(phone->activateStylesheet(stylesheet), "Activate stylesheet failed");
        delete stylesheet;
    }

    {
        // When operating on non-GUI tasks, should acquire a lock before operating on LVGL
        LvLockGuard gui_guard;

        /* Begin the phone */
        ESP_UTILS_CHECK_FALSE_EXIT(phone->begin(), "Begin failed");
        // assert(phone->getCoreHome().showContainerBorder() && "Show container border failed");

        /* Init and install apps from registry */
        std::vector<ESP_Brookesia_CoreManager::RegistryAppInfo> inited_apps;
        ESP_UTILS_CHECK_FALSE_EXIT(phone->initAppFromRegistry(inited_apps), "Init app registry failed");
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installAppFromRegistry(inited_apps), "Install app registry failed");

        /* Create a timer to update the clock */
        lv_timer_create([](lv_timer_t *t) {
            time_t now;
            struct tm timeinfo;
            ESP_Brookesia_Phone *phone = (ESP_Brookesia_Phone *)t->user_data;

            ESP_UTILS_CHECK_NULL_EXIT(phone, "Invalid phone");

            time(&now);
            localtime_r(&now, &timeinfo);

            ESP_UTILS_CHECK_FALSE_EXIT(
                phone->getHome().getStatusBar()->setClock(timeinfo.tm_hour, timeinfo.tm_min),
                "Refresh status bar failed"
            );
        }, 1000, phone);
    }

    if constexpr (EXAMPLE_SHOW_MEM_INFO) {
        esp_utils::thread_config_guard thread_config({
            .name = "mem_info",
            .stack_size = 4096,
        });
        boost::thread([ = ]() {
            while (1) {
                esp_utils_mem_print_info();

                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }).detach();
    }
}
