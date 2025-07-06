/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/stat.h>
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "esp_private/esp_clk.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"

#include "esp_gmf_oal_mem.h"
#include "esp_gmf_fifo.h"
#include "gmf_ut_common.h"

static const char *TAG = "TEST_ESP_GMF_FIFO";
static bool        read_is_done;
static bool        write_is_done;
static bool        read_run;
static bool        write_run;

static const char *file_name  = "/sdcard/gmf_ut_test.mp3";
static const char *file2_name = "/sdcard/gmf_ut_test2.mp3";

static void read_task(void *param)
{
    esp_gmf_fifo_handle_t fifo = (esp_gmf_fifo_handle_t)param;
    esp_gmf_data_bus_block_t blk = {0};
    FILE *f = fopen(file_name, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Open file failed");
        TEST_ASSERT_NOT_NULL(f);
        goto read_task_err;
    }

    int len = 4096;
    struct stat sz = {0};
    stat(file_name, &sz);
    ESP_LOGI(TAG, "Going to read, %p, file size %ld", param, sz.st_size);
    read_run = true;
    uint32_t start_cnt = 0;
    uint32_t total_cnt = 0;
    uint32_t file_size = 0;
    while (read_run) {
        esp_gmf_fifo_acquire_write(fifo, &blk, len, portMAX_DELAY);
        int ret = fread(blk.buf, 1, len, f);
        blk.valid_size = ret;
        if (ret == 0) {
            blk.is_last = true;
            read_run = false;
        }
        file_size +=blk.valid_size;

        start_cnt = esp_clk_rtc_time();
        ret = esp_gmf_fifo_release_write(fifo, &blk, portMAX_DELAY);
        total_cnt += (esp_clk_rtc_time() - start_cnt);
    }
    ESP_LOGW(TAG, "Done to read, consumed time:%ld, read size:%ld", total_cnt, file_size);
read_task_err:
    if (f) {
        fclose(f);
    }
    read_is_done = true;
    vTaskDelete(NULL);
}

static void write_task(void *param)
{
    ESP_LOGI(TAG, "Going to write, %p", param);
    esp_gmf_fifo_handle_t fifo = (esp_gmf_fifo_handle_t)param;
    esp_gmf_data_bus_block_t blk = {0};
    FILE *f = fopen(file2_name, "wb+");
    if (f == NULL) {
        ESP_LOGE(TAG, "Open file failed");
        TEST_ASSERT_NOT_NULL(f);
        goto write_task_err;
    }
    int len = 4096;
    write_run = true;
    uint32_t start_cnt = 0;
    uint32_t total_cnt = 0;
    uint32_t file_size = 0;
    while (write_run) {
        start_cnt = esp_clk_rtc_time();
        int ret = esp_gmf_fifo_acquire_read(fifo, &blk, len, portMAX_DELAY);
        if (ret != ESP_GMF_ERR_OK) {
            ESP_LOGE(TAG, "Acquire read failed");
            break;
        }
        total_cnt += (esp_clk_rtc_time() - start_cnt);
        file_size += blk.valid_size;

        ret = fwrite(blk.buf, 1, blk.valid_size, f);
        fflush(f);
        esp_gmf_fifo_release_read(fifo, &blk, 0);
        if (blk.is_last) {
            break;
        }
    }
    ESP_LOGW(TAG, "Done to write, consumed time:%ld, file size:%ld", total_cnt, file_size);
write_task_err:
    if (f) {
        fclose(f);
    }
    write_is_done = true;
    vTaskDelete(NULL);
}

TEST_CASE("FIFO read and write on different task", "ESP_GMF_FIFO")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_FIFO", ESP_LOG_VERBOSE);

    sdmmc_card_t *card = NULL;
    esp_gmf_ut_setup_sdmmc(&card);

    esp_gmf_fifo_handle_t fifo = NULL;
    esp_gmf_fifo_create(10, 1, &fifo);
    ESP_LOGI(TAG, "TEST Create GMF FIFO, %p", fifo);
    TEST_ASSERT_NOT_NULL(fifo);
    uint8_t priority[][2] = { {5, 5}, {0,10,}, {10, 0}};

    for (size_t i = 0; i < sizeof(priority) / sizeof(priority[0]); i++){
        ESP_LOGW(TAG, "Test FIFO with priority %d, %d\r\n", priority[i][0], priority[i][1]);
        read_is_done = false;
        write_is_done = false;
        xTaskCreate(read_task, "read", 4096, fifo, priority[i][0], NULL);
        xTaskCreate(write_task, "write", 4096, fifo, priority[i][1], NULL);
        while (1) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            if (read_is_done && write_is_done) {
                break;
            }
        }
        int ret = verify_two_files(file_name, file2_name);
        TEST_ASSERT_EQUAL(ret, ESP_OK);
    }

    esp_gmf_fifo_destroy(fifo);

    esp_gmf_ut_teardown_sdmmc(card);
    vTaskDelay(10 / portTICK_PERIOD_MS);
}
