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

constexpr int SD_CARD_NOT_FOUND_RETRY_INTERVAL_MS = 1000;
constexpr int SD_CARD_NOT_FOUND_RETRY_MAX_COUNT = 10;
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

        lv_obj_t *label_title = nullptr;
        lv_obj_t *label_content = nullptr;
        {
            LvLockGuard gui_guard;

            label_title = lv_label_create(lv_screen_active());
            lv_obj_set_size(label_title, 300, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(label_title, &esp_brookesia_font_maison_neue_book_26, 0);
            lv_obj_set_style_text_color(label_title, lv_color_make(255, 0, 0), 0);
            lv_obj_set_style_text_align(label_title, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(label_title, "WARNING");
            lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 60);

            label_content = lv_label_create(lv_screen_active());
            lv_obj_set_size(label_content, LV_PCT(90), LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(label_content, &esp_brookesia_font_maison_neue_book_20, 0);
            lv_obj_set_style_text_align(label_content, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text_fmt(
                label_content,
                "SD card not detected. Please insert an SD card to continue.\nOr wait %d seconds to enter the system without an SD card (Related features will be disabled).",
                SD_CARD_NOT_FOUND_RETRY_MAX_COUNT * SD_CARD_NOT_FOUND_RETRY_INTERVAL_MS / 1000
            );
            lv_obj_align_to(label_content, label_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 40);
        }

        int retry_count = 0;
        while ((ret = bsp_sdcard_mount()) != ESP_OK) {
            ESP_UTILS_LOGE("Mount SD card failed(%s), retry...", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(SD_CARD_NOT_FOUND_RETRY_INTERVAL_MS));
            if (++retry_count >= SD_CARD_NOT_FOUND_RETRY_MAX_COUNT) {
                break;
            }
        }

        {
            LvLockGuard gui_guard;
            lv_obj_del(label_title);
            lv_obj_del(label_content);
        }

        if (retry_count < SD_CARD_NOT_FOUND_RETRY_MAX_COUNT) {
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
