/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "esp_private/esp_clk.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"

#include "esp_gmf_oal_mem.h"
#include "esp_gmf_ringbuffer.h"
#include "gmf_ut_common.h"

static const char *TAG = "TEST_ESP_GMF_RINGBUF";
static bool        is_done;
static bool        read_run;
static bool        write_run;

static const char *file_name  = "/sdcard/gmf_ut_test.mp3";
static const char *file2_name = "/sdcard/gmf_ut_test_out.mp3";

static void read_task(void *param)
{
    ESP_LOGI(TAG, "Going to read, %p", param);
    esp_gmf_rb_handle_t rb = (esp_gmf_rb_handle_t)param;
    esp_gmf_data_bus_block_t blk = {0};
    FILE *f = fopen(file_name, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Open file failed");
        TEST_ASSERT_NOT_NULL(f);
        goto read_task_err;
    }
    int len = 4096;
    blk.buf = esp_gmf_oal_malloc(len);
    blk.valid_size = len;
    ESP_GMF_MEM_CHECK(TAG, blk.buf, goto read_task_err);
    // TEST_ASSERT_NOT_NULL(blk.buf);
    read_run = true;
    uint32_t start_cnt = 0;
    uint32_t total_cnt = 0;
    while (read_run) {
        esp_gmf_rb_acquire_write(rb, &blk, len, portMAX_DELAY);
        int ret = fread(blk.buf, 1, len, f);
        blk.valid_size = ret;
        if (ret == 0) {
            blk.is_last = true;
            read_run = false;
        }
        start_cnt = esp_clk_rtc_time();
        ret = esp_gmf_rb_release_write(rb, &blk, portMAX_DELAY);
        total_cnt += (esp_clk_rtc_time() - start_cnt);
    }
    ESP_LOGW(TAG, "Done to read, %ld", total_cnt);
read_task_err:
    if (f) {
        fclose(f);
    }
    if (blk.buf) {
        esp_gmf_oal_free(blk.buf);
    }
    vTaskDelete(NULL);
}

static void write_task(void *param)
{
    ESP_LOGI(TAG, "Going to write, %p", param);
    esp_gmf_rb_handle_t rb = (esp_gmf_rb_handle_t)param;
    esp_gmf_data_bus_block_t blk = {0};
    FILE *f = fopen(file2_name, "wb+");
    if (f == NULL) {
        ESP_LOGE(TAG, "Open file failed");
        TEST_ASSERT_NOT_NULL(f);
        goto write_task_err;
    }
    int len = 4096;
    blk.buf = esp_gmf_oal_malloc(len);
    blk.valid_size = len;
    ESP_GMF_MEM_CHECK(TAG, blk.buf, goto write_task_err);
    // TEST_ASSERT_NOT_NULL(blk.buf);
    write_run = true;
    uint32_t start_cnt = 0;
    uint32_t total_cnt = 0;
    while (write_run) {
        start_cnt = esp_clk_rtc_time();
        int ret = esp_gmf_rb_acquire_read(rb, &blk, len, portMAX_DELAY);
        if (ret != ESP_GMF_ERR_OK) {
            ESP_LOGE(TAG, "Acquire read failed");
            break;
        }
        total_cnt += (esp_clk_rtc_time() - start_cnt);

        ret = fwrite(blk.buf, 1, blk.valid_size, f);
        esp_gmf_rb_release_read(rb, &blk, 0);
        if (blk.is_last) {
            break;
        }
    }
    ESP_LOGW(TAG, "Done to write, %ld", total_cnt);
write_task_err:
    if (f) {
        fclose(f);
    }
    if (blk.buf) {
        esp_gmf_oal_free(blk.buf);
    }
    is_done = true;
    vTaskDelete(NULL);
}

TEST_CASE("Ringbuffer read and write on different task", "ESP_GMF_RINGBUF")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_RINGBUF", ESP_LOG_VERBOSE);

    sdmmc_card_t *card = NULL;
    esp_gmf_ut_setup_sdmmc(&card);

    is_done = false;
    esp_gmf_rb_handle_t rb = NULL;
    esp_gmf_rb_create(2, 8 * 1024, &rb);
    ESP_LOGI(TAG, "TEST Create GMF ringbuffer, %p", rb);
    TEST_ASSERT_NOT_NULL(rb);
    xTaskCreate(read_task, "read", 4096, rb, 5, NULL);
    xTaskCreate(write_task, "write", 4096, rb, 5, NULL);
    while (1) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        if (is_done) {
            break;
        }
    }
    int ret = verify_two_files(file_name, file2_name);
    TEST_ASSERT_EQUAL(ret, ESP_OK);

    esp_gmf_rb_destroy(rb);

    esp_gmf_ut_teardown_sdmmc(card);
    vTaskDelay(10 / portTICK_PERIOD_MS);
}
