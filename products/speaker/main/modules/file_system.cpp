/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_spiffs.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "File System"
#include "esp_lib_utils.h"
#include "file_system.hpp"
#include "display.hpp"

constexpr const char *MUSIC_PARTITION_LABEL = "spiffs_data";

using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems::speaker;

bool file_system_init()
{
    ESP_UTILS_LOG_TRACE_GUARD();

    auto ret = bsp_sdcard_mount();
    if (ret == ESP_OK) {
        ESP_UTILS_LOGI("Mount SD card successfully");
    } else {
        ESP_UTILS_LOGE("Mount SD card failed(%s)", esp_err_to_name(ret));

        Display::on_dummy_draw_signal(false);

        bsp_display_lock(0);

        auto label = lv_label_create(lv_screen_active());
        lv_obj_set_size(label, 300, LV_SIZE_CONTENT);
        lv_obj_set_style_text_font(label, &esp_brookesia_font_maison_neue_book_26, 0);
        lv_label_set_text(label, "SD card not found, please insert a SD card!");
        lv_obj_center(label);

        bsp_display_unlock();

        while ((ret = bsp_sdcard_mount()) != ESP_OK) {
            ESP_UTILS_LOGE("Mount SD card failed(%s), retry...", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        bsp_display_lock(0);
        lv_obj_del(label);
        bsp_display_unlock();
        {
            uint64_t total = 0;
            uint64_t free = 0;
            ESP_UTILS_CHECK_ERROR_RETURN(
                esp_vfs_fat_info(BSP_SD_MOUNT_POINT, &total, &free), false, "Failed to get FAT partition information"
            );
            ESP_UTILS_LOGI("SD card size: total: %d, free: %d", static_cast<int>(total), static_cast<int>(free));
        }

        Display::on_dummy_draw_signal(true);
    }

    /* Initialize SPIFFS */
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = MUSIC_PARTITION_LABEL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    ESP_UTILS_CHECK_ERROR_RETURN(esp_vfs_spiffs_register(&conf), false, "Failed to initialize SPIFFS");
    {
        size_t total = 0;
        size_t used = 0;
        ESP_UTILS_CHECK_ERROR_RETURN(
            esp_spiffs_info(MUSIC_PARTITION_LABEL, &total, &used), false, "Failed to get SPIFFS partition information"
        );
        ESP_UTILS_LOGI("SPIFFS size: total: %d, free: %d", static_cast<int>(total), static_cast<int>(total - used));
    }

    return true;
}
