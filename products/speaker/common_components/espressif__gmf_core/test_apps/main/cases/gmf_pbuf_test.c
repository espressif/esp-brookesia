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
#include "gmf_ut_common.h"
#include "esp_gmf_pbuf.h"

static const char *TAG = "TEST_ESP_GMF_PBUF";
static bool        task_is_done;
static bool        read_run;
static bool        write_run;

static const char *file_name  = "/sdcard/gmf_ut_test.mp3";
static const char *file2_name = "/sdcard/gmf_ut_test_out.mp3";
extern uint32_t    mem_start;

static void read_task(void *param)
{
    ESP_LOGI(TAG, "Going to read, %p", param);
    esp_gmf_pbuf_handle_t pbuf = (esp_gmf_pbuf_handle_t)param;
    FILE *f = fopen(file_name, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Open file failed on %s", __func__);
        goto read_task_err;
    }
    int read_len = 4 * 1024;
    char *buf = esp_gmf_oal_malloc(read_len);
    ESP_GMF_MEM_CHECK(TAG, buf, goto read_task_err);
    read_run = true;
    uint32_t start_cnt = 0;
    uint32_t total_cnt = 0;
    int mem_start = 0;
    esp_gmf_data_bus_block_t blk = {0};
    while (read_run) {
        int ret = esp_gmf_pbuf_acquire_write(pbuf, &blk, read_len, portMAX_DELAY);
        if (ret == ESP_GMF_IO_FAIL) {
            // Pbuf is full, waiting the buffer to be read.
            vTaskDelay(1);
            continue;
        }
        ret = fread(blk.buf, 1, blk.buf_length, f);
        ESP_LOGI(TAG, "Reading from file, ret:%d, buf:%p, len:%d", ret, blk.buf, blk.buf_length);
        blk.valid_size = ret;
        if (ret != blk.buf_length) {
            blk.is_last = true;
            read_run = false;
        }
        start_cnt = esp_clk_rtc_time();
        esp_gmf_pbuf_release_write(pbuf, &blk, 0);
        total_cnt += (esp_clk_rtc_time() - start_cnt);
    }
    fclose(f);
    esp_gmf_oal_free(buf);
    ESP_LOGI(TAG, "Done to read, %" PRIu32 ", memory time:%d", total_cnt, mem_start);
read_task_err:
    vTaskDelete(NULL);
}

static void write_task(void *param)
{
    ESP_LOGI(TAG, "Going to write, %p", param);
    esp_gmf_pbuf_handle_t pbuf = (esp_gmf_pbuf_handle_t)param;
    FILE *f = fopen(file2_name, "wb+");
    if (f == NULL) {
        ESP_LOGE(TAG, "Open file failed");
        goto write_task_err;
    }
    write_run = true;
    uint32_t start_cnt = 0;
    uint32_t total_cnt = 0;
    esp_gmf_data_bus_block_t blk = {0};
    while (write_run) {
        start_cnt = esp_clk_rtc_time();
        int ret = esp_gmf_pbuf_acquire_read(pbuf, &blk, 0, portMAX_DELAY);
        if (ret == ESP_GMF_IO_FAIL) {
            // Pbuf is empty, waiting the buffer to be fill.
            vTaskDelay(1);
            continue;
        }
        total_cnt += (esp_clk_rtc_time() - start_cnt);
        ESP_LOGI(TAG, "Writing to file, ret:%d, buf:%p, len:%d", ret, blk.buf, blk.valid_size);
        ret = fwrite(blk.buf, 1, blk.valid_size, f);
        if (blk.is_last) {
            fclose(f);
            write_run = false;
            task_is_done = true;
        }
        esp_gmf_pbuf_release_read(pbuf, &blk, 0);
        ESP_GMF_MEM_SHOW(TAG);
    }
    ESP_LOGI(TAG, "Done to write, %" PRIu32 "", total_cnt);
write_task_err:
    task_is_done = true;
    vTaskDelete(NULL);
}

static void wr_rd_task(void *param)
{
    ESP_LOGI(TAG, "Going to write and read, %p", param);
    esp_gmf_pbuf_handle_t pbuf = (esp_gmf_pbuf_handle_t)param;
    FILE *f = fopen(file_name, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Open file failed");
        goto wr_task_err;
    }
    FILE *fw = fopen(file2_name, "wb+");
    if (fw == NULL) {
        ESP_LOGE(TAG, "Open file failed");
        goto wr_task_err;
    }
    int read_len = 4 * 1024;
    read_run = true;
    uint32_t start_cnt = 0;
    uint32_t total_cnt = 0;
    int mem_start = 0;
    esp_gmf_data_bus_block_t blk = {0};
    while (read_run) {
        esp_gmf_pbuf_acquire_write(pbuf, &blk, read_len, portMAX_DELAY);
        int ret = fread(blk.buf, 1, blk.buf_length, f);
        blk.valid_size = ret;
        if (ret != read_len) {
            blk.is_last = true;
            ESP_LOGE(TAG, "Reading, buf:%p,vld:%d, len:%d", blk.buf, blk.valid_size, blk.buf_length);

        } else {
            ESP_LOGI(TAG, "Reading, buf:%p,vld:%d, len:%d", blk.buf, blk.valid_size, blk.buf_length);
        }
        start_cnt = esp_clk_rtc_time();
        esp_gmf_pbuf_release_write(pbuf, &blk, 0);
        total_cnt += (esp_clk_rtc_time() - start_cnt);

        esp_gmf_data_bus_block_t rd_blk = {0};
        start_cnt = esp_clk_rtc_time();
        ret = esp_gmf_pbuf_acquire_read(pbuf, &rd_blk, 0, portMAX_DELAY);
        total_cnt += (esp_clk_rtc_time() - start_cnt);
        ESP_LOGI(TAG, "Witing, ret:%d, buf:%p, buf:%d, last:%d", ret, rd_blk.buf, rd_blk.valid_size, rd_blk.is_last);
        ret = fwrite(rd_blk.buf, 1, rd_blk.valid_size, fw);
        if (rd_blk.is_last == true) {
            task_is_done = true;
            read_run = 0;
        }
        esp_gmf_pbuf_release_read(pbuf, &rd_blk, 0);
    }
    fclose(f);
    fclose(fw);
    // esp_gmf_oal_free(buf);
    ESP_LOGI(TAG, "Done to read, %ld, memory time:%d", total_cnt, mem_start);
wr_task_err:
    vTaskDelete(NULL);
}

// Multiple acquire write one times
// And check the capacity
static void wr_rd_task2(void *param)
{
    ESP_LOGI(TAG, "Going to write and read, %p", param);
    esp_gmf_pbuf_handle_t pbuf = (esp_gmf_pbuf_handle_t)param;
    FILE *f = fopen(file_name, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Open file failed");
        goto wr_task_err;
    }
    FILE *fw = fopen(file2_name, "wb+");
    if (fw == NULL) {
        ESP_LOGE(TAG, "Open file failed");
        goto wr_task_err;
    }
    int read_len = 4 * 1024;
    read_run = true;
    uint32_t start_cnt = 0;
    uint32_t total_cnt = 0;
    int mem_start = 0;
    esp_gmf_data_bus_block_t blk = {0};
    while (read_run) {
        for (int i = 0; i < 3; ++i) {
            esp_gmf_pbuf_acquire_write(pbuf, &blk, read_len, portMAX_DELAY);
            int ret = fread(blk.buf, 1, blk.buf_length, f);
            blk.valid_size = ret;
            if (ret != read_len) {
                ESP_LOGW(TAG, "Write, buf:%p,vld:%d, len:%d,%d", blk.buf, blk.valid_size, blk.buf_length, blk.is_last);
                blk.is_last = true;
            } else {
                ESP_LOGI(TAG, "Write, buf:%p,vld:%d, len:%d", blk.buf, blk.valid_size, blk.buf_length);
            }
            start_cnt = esp_clk_rtc_time();
            esp_gmf_pbuf_release_write(pbuf, &blk, 0);
            total_cnt += (esp_clk_rtc_time() - start_cnt);
            if (ret != read_len) {
                break;
            }
        }

        esp_gmf_data_bus_block_t rd_blk = {0};
        start_cnt = esp_clk_rtc_time();
        while (1) {
            int ret = esp_gmf_pbuf_acquire_read(pbuf, &rd_blk, 0, portMAX_DELAY);
            if (ret == ESP_GMF_IO_FAIL) {
                break;
            }
            total_cnt += (esp_clk_rtc_time() - start_cnt);
            ret = fwrite(rd_blk.buf, 1, rd_blk.valid_size, fw);
            if (rd_blk.is_last == true) {
                task_is_done = true;
                read_run = 0;
                ESP_LOGW(TAG, "Read, %p, buf_len:%d, last:%d", rd_blk.buf, rd_blk.valid_size, rd_blk.is_last);
                break;
            } else {
                ESP_LOGI(TAG, "Read, %p, buf_len:%d, last:%d", rd_blk.buf, rd_blk.valid_size, rd_blk.is_last);
            }
            esp_gmf_pbuf_release_read(pbuf, &rd_blk, 0);
        }
    }
    fclose(f);
    fclose(fw);
    ESP_LOGI(TAG, "Done to read, %ld, memory time:%d", total_cnt, mem_start);
wr_task_err:
    vTaskDelete(NULL);
}

TEST_CASE("One task for read write test", "ESP_GMF_PBUF")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_PBUF", ESP_LOG_INFO);
    ESP_LOGI(TAG, "TEST Create GMF Pbuf");

    sdmmc_card_t *card = NULL;
    esp_gmf_ut_setup_sdmmc(&card);

    task_is_done = false;
    esp_gmf_pbuf_handle_t pbuf = NULL;
    esp_gmf_pbuf_create(10, &pbuf);
    ESP_LOGI(TAG, "TEST Create GMF, %p", pbuf);
    TEST_ASSERT_NOT_NULL(pbuf);
    uint32_t start_cnt = esp_clk_rtc_time();
    xTaskCreate(wr_rd_task, "one_rw", 4096, pbuf, 5, NULL);
    while (1) {
        vTaskDelay(2 / portTICK_PERIOD_MS);
        if (task_is_done) {
            break;
        }
    }
    ESP_LOGW(TAG, "Taken %lld ms to copy file, start to verify the files", (esp_clk_rtc_time() - start_cnt) / 1000);
    int ret = verify_two_files(file_name, file2_name);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    esp_gmf_pbuf_destroy(pbuf);
    esp_gmf_ut_teardown_sdmmc(card);
    vTaskDelay(100);
    ESP_LOGE(TAG, "%s,%d", __func__, __LINE__);
}

TEST_CASE("One task for multiple read write test", "ESP_GMF_PBUF")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_PBUF", ESP_LOG_INFO);
    ESP_LOGI(TAG, "TEST Create GMF Pbuf");

    sdmmc_card_t *card = NULL;
    esp_gmf_ut_setup_sdmmc(&card);

    task_is_done = false;
    esp_gmf_pbuf_handle_t pbuf = NULL;
    esp_gmf_pbuf_create(10, &pbuf);
    ESP_LOGI(TAG, "TEST Create GMF, %p", pbuf);
    TEST_ASSERT_NOT_NULL(pbuf);
    uint32_t start_cnt = esp_clk_rtc_time();
    xTaskCreate(wr_rd_task2, "MULT_RW", 4096, pbuf, 5, NULL);
    while (1) {
        vTaskDelay(2 / portTICK_PERIOD_MS);
        if (task_is_done) {
            break;
        }
    }
    ESP_LOGW(TAG, "Taken %lld ms to copy file.Start verify the files", (esp_clk_rtc_time() - start_cnt) / 1000);
    int ret = verify_two_files(file_name, file2_name);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    esp_gmf_pbuf_destroy(pbuf);
    esp_gmf_ut_teardown_sdmmc(card);
    vTaskDelay(100);
    ESP_LOGE(TAG, "%s,%d", __func__, __LINE__);
}

TEST_CASE("Read task and write task thread safe test", "ESP_GMF_PBUF")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_PBUF", ESP_LOG_INFO);
    ESP_LOGI(TAG, "TEST Create GMF Pbuf");

    sdmmc_card_t *card = NULL;
    esp_gmf_ut_setup_sdmmc(&card);

    task_is_done = false;
    esp_gmf_pbuf_handle_t pbuf = NULL;
    esp_gmf_pbuf_create(10, &pbuf);
    ESP_LOGI(TAG, "TEST Create GMF, %p", pbuf);
    TEST_ASSERT_NOT_NULL(pbuf);
    uint32_t start_cnt = esp_clk_rtc_time();
    xTaskCreate(read_task, "read", 4096, pbuf, 3, NULL);
    xTaskCreate(write_task, "wr_to_file", 4096, pbuf, 3, NULL);
    while (1) {
        vTaskDelay(2 / portTICK_PERIOD_MS);
        if (task_is_done) {
            break;
        }
    }
    ESP_LOGW(TAG, "Taken %lld ms to copy file.Start verify the files", (esp_clk_rtc_time() - start_cnt) / 1000);
    int ret = verify_two_files(file_name, file2_name);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    esp_gmf_pbuf_destroy(pbuf);
    esp_gmf_ut_teardown_sdmmc(card);
    ESP_LOGE(TAG, "%s,%d", __func__, __LINE__);
}
