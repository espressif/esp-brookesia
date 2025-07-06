/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_gmf_cache.h"

#include "freertos/FreeRTOS.h"
#include "esp_private/esp_clk.h"
#include "driver/sdmmc_host.h"

#include "esp_gmf_oal_mem.h"
#include "gmf_ut_common.h"

#define CACHE_SIZE 1024
#define TEST_DATA_SIZE 256
static const char *TAG = "TEST_ESP_GMF_CACHE";

static const char *file_name  = "/sdcard/gmf_ut_test.mp3";

TEST_CASE("Test esp_gmf_cache creation and deletion", "[esp_gmf_cache]")
{
    esp_gmf_cache_t *cache = NULL;

    // Test NULL input
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_INVALID_ARG, esp_gmf_cache_new(CACHE_SIZE, NULL));

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cache_new(CACHE_SIZE, &cache));
    TEST_ASSERT_NOT_NULL(cache);
    TEST_ASSERT_NOT_NULL(cache->buf);
    TEST_ASSERT_EQUAL_UINT32(CACHE_SIZE, cache->buf_len);
    TEST_ASSERT_EQUAL_UINT32(0, cache->buf_filled);

    // Delete cache
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cache_delete(cache));

    // Test NULL deletion
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_INVALID_ARG, esp_gmf_cache_delete(NULL));
}

TEST_CASE("Test esp_gmf_cache acquire and release", "[esp_gmf_cache]")
{
    esp_gmf_cache_t *cache = NULL;
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cache_new(CACHE_SIZE, &cache));

    esp_gmf_payload_t load_in, *load_out = NULL;

    // Test NULL input
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_INVALID_ARG, esp_gmf_cache_acquire(cache, TEST_DATA_SIZE, NULL));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_INVALID_ARG, esp_gmf_cache_load(cache, NULL));

    // Prepare load_in
    load_in.buf = (uint8_t *)malloc(TEST_DATA_SIZE);
    load_in.buf_length = TEST_DATA_SIZE;
    load_in.valid_size = TEST_DATA_SIZE;
    memset(load_in.buf, 0xAB, TEST_DATA_SIZE);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cache_load(cache, &load_in));

    // Acquire data
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cache_acquire(cache, TEST_DATA_SIZE, &load_out));
    TEST_ASSERT_EQUAL_UINT32(TEST_DATA_SIZE, load_out->valid_size);

    // Release data
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cache_release(cache, load_out));

    // Free resources
    free(load_in.buf);
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cache_delete(cache));
}

TEST_CASE("Test esp_gmf_cache get cached size", "[esp_gmf_cache]")
{
    esp_gmf_cache_t *cache = NULL;
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cache_new(CACHE_SIZE, &cache));

    int filled = 0;

    // Test NULL input
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_INVALID_ARG, esp_gmf_cache_get_cached_size(NULL, &filled));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_INVALID_ARG, esp_gmf_cache_get_cached_size(cache, NULL));

    // Initially empty cache
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cache_get_cached_size(cache, &filled));
    TEST_ASSERT_EQUAL_UINT32(0, filled);

    // Fill cache with test data
    memset(cache->buf, 0x55, TEST_DATA_SIZE);
    cache->buf_filled = TEST_DATA_SIZE;

    // Check cached size
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cache_get_cached_size(cache, &filled));
    TEST_ASSERT_EQUAL_UINT32(TEST_DATA_SIZE, filled);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cache_delete(cache));
}

int random_fluctuate_int(int value)
{
    int range = value / 2;  // 计算浮动范围（±50%）
    int random_offset = rand() % (2 * range + 1) - range;  // 生成 [-range, range] 的随机偏移
    return value + random_offset;
}

static void read_write_test1(const char *wr_name, int payload_size, int cache_size)
{
    esp_gmf_payload_t *read_payload = NULL;
    esp_gmf_cache_t *cache = NULL;
    esp_gmf_cache_new(cache_size, &cache);
    TEST_ASSERT_NOT_NULL(cache);
    esp_gmf_payload_new_with_len(payload_size*2, &read_payload);
    TEST_ASSERT_NOT_NULL(read_payload);

    FILE *f_rd = fopen(file_name, "rb");
    if (f_rd == NULL) {
        ESP_LOGE(TAG, "Open file failed");
        TEST_ASSERT_NOT_NULL(f_rd);
        goto read_task_err;
    }
    FILE *f_wr = fopen(wr_name, "wb+");
    if (f_wr == NULL) {
        ESP_LOGE(TAG, "Open file failed");
        TEST_ASSERT_NOT_NULL(f_wr);
        goto read_task_err;
    }
    struct stat sz = {0};
    stat(file_name, &sz);
    ESP_LOGI(TAG, "Read file size: %ld, IN Payload:%d, Cache size:%d", sz.st_size, payload_size, cache_size);
    bool read_run = true;
    uint32_t read_file_size = 0;
    uint32_t file_size = 0;
    esp_gmf_payload_t *out_payload = NULL;
    int expect_sz = cache_size + 512;
    while (read_run) {
        int read_len = random_fluctuate_int(payload_size);
        // int read_len = (payload_size);

        int ret = fread(read_payload->buf, 1, read_len, f_rd);
        read_payload->valid_size = ret;
        if (ret != read_len) {
            read_payload->is_done = true;
            read_run = false;
        }
        read_file_size += read_payload->valid_size;
        ESP_LOGI(TAG, "Payload, buf:%p, vld:%d, len:%d, done:%d", read_payload->buf,read_payload->valid_size,
            read_payload->buf_length, read_payload->is_done);
        esp_gmf_cache_load(cache, read_payload);
        while (1) {
            esp_gmf_cache_acquire(cache, expect_sz, &out_payload);
            ESP_LOGW(TAG, "Cache out, expect:%d, buf:%p, vld:%d, len:%d, done:%d, file w:%ld(r:%ld)", expect_sz, out_payload->buf,out_payload->valid_size,
                out_payload->buf_length, out_payload->is_done, file_size, read_file_size);
            if (out_payload->valid_size == expect_sz || out_payload->is_done) {
                ret = fwrite(out_payload->buf, 1, out_payload->valid_size, f_wr);
                file_size += out_payload->valid_size;
            }
            if (out_payload->is_done || out_payload->valid_size != expect_sz) {
                esp_gmf_cache_release(cache, out_payload);
                break;
            }
            esp_gmf_cache_release(cache, out_payload);
        }
        printf("\r\n");
    }

    ESP_LOGW(TAG, "Done to read, read size:%ld", file_size);
read_task_err:
    if (cache) {
        esp_gmf_cache_delete(cache);
    }
    if (read_payload) {
        esp_gmf_payload_delete(read_payload);
    }
    if (f_rd) {
        fclose(f_rd);
    }
    if (f_wr) {
        fclose(f_wr);
    }
}

static void read_write_test2(const char *wr_name, int payload_size, int cache_size)
{
    esp_gmf_payload_t *read_payload = NULL;
    esp_gmf_cache_t *cache = NULL;
    esp_gmf_cache_new(cache_size, &cache);
    TEST_ASSERT_NOT_NULL(cache);
    esp_gmf_payload_new_with_len(payload_size*2, &read_payload);
    TEST_ASSERT_NOT_NULL(read_payload);

    FILE *f_rd = fopen(file_name, "rb");
    if (f_rd == NULL) {
        ESP_LOGE(TAG, "Open file failed");
        TEST_ASSERT_NOT_NULL(f_rd);
        goto read_task_err;
    }
    FILE *f_wr = fopen(wr_name, "wb+");
    if (f_wr == NULL) {
        ESP_LOGE(TAG, "Open file failed");
        TEST_ASSERT_NOT_NULL(f_wr);
        goto read_task_err;
    }
    struct stat sz = {0};
    stat(file_name, &sz);
    ESP_LOGI(TAG, "Read file size: %ld, IN Payload:%d, Cache size:%d", sz.st_size, payload_size, cache_size);
    bool read_run = true;
    uint32_t read_file_size = 0;
    uint32_t file_size = 0;
    esp_gmf_payload_t *out_payload = NULL;
    int ret = 0;
    int expect_sz = cache_size + 1024;
    while (read_run) {
        bool needed_load = false;
        esp_gmf_cache_ready_for_load(cache, &needed_load);
        if (needed_load) {
            int read_len = random_fluctuate_int(payload_size);
            // int read_len = (payload_size);
            ret = fread(read_payload->buf, 1, read_len, f_rd);
            read_payload->valid_size = ret;
            if (ret != read_len) {
                read_payload->is_done = true;
            }
            read_file_size += read_payload->valid_size;
            printf("\r\n");
            ESP_LOGI(TAG, "Loading, buf:%p, vld:%d, len:%d, done:%d", read_payload->buf,read_payload->valid_size,
                read_payload->buf_length, read_payload->is_done);
            esp_gmf_cache_load(cache, read_payload);
        }

        esp_gmf_cache_acquire(cache, expect_sz, &out_payload);

        ESP_LOGW(TAG, "Cache out, expect:%d, buf:%p, vld:%d, len:%d, done:%d, file:%ld(%ld)", expect_sz, out_payload->buf,out_payload->valid_size,
            out_payload->buf_length, out_payload->is_done, file_size, read_file_size);
        if ((!out_payload->is_done) && (out_payload->valid_size != expect_sz)) {
            esp_gmf_cache_release(cache, out_payload);
            continue;
        }
        if (out_payload->valid_size == expect_sz || out_payload->is_done) {
            ret = fwrite(out_payload->buf, 1, out_payload->valid_size, f_wr);
            file_size += out_payload->valid_size;
        }
        if (out_payload->is_done || out_payload->valid_size != expect_sz) {
            read_run = false;
        }
        esp_gmf_cache_release(cache, out_payload);
    }

    ESP_LOGW(TAG, "Done to read, read size:%ld", file_size);
read_task_err:
    if (cache) {
        esp_gmf_cache_delete(cache);
    }
    if (read_payload) {
        esp_gmf_payload_delete(read_payload);
    }
    if (f_rd) {
        fclose(f_rd);
    }
    if (f_wr) {
        fclose(f_wr);
    }
}

TEST_CASE("Test Cache with a file - case 1", "[esp_gmf_cache]")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("GMF_CACHE", ESP_LOG_VERBOSE);

    sdmmc_card_t *card = NULL;
    esp_gmf_ut_setup_sdmmc(&card);

    int payload_size[][2] = { {1536, 2048}, {2000, 1500},{1111, 1300}, {2000, 500}, {1000, 3000}};
    char wr_name[128] = {0};
    for (size_t i = 0; i < sizeof(payload_size) / sizeof(payload_size[0]); i++){
        ESP_LOGW(TAG, "Test Cache with payload_size %d, %d\r\n\r\n", payload_size[i][0], payload_size[i][1]);
        snprintf(wr_name, 127, "/sdcard/esp_gmf_test_cache_%02d.txt", i);
        read_write_test1(wr_name, payload_size[i][0], payload_size[i][1]);
        int ret = verify_two_files(file_name, wr_name);
        TEST_ASSERT_EQUAL(ret, ESP_OK);
    }

    esp_gmf_ut_teardown_sdmmc(card);
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

TEST_CASE("Test cache with file - case 2", "[esp_gmf_cache]")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("GMF_CACHE", ESP_LOG_VERBOSE);

    sdmmc_card_t *card = NULL;
    esp_gmf_ut_setup_sdmmc(&card);

    int payload_size[][2] = { {1536, 2048},{2000, 1500}, {1111, 1300}, {2000, 500}, {1000, 3000}};
    char wr_name[128] = {0};
    for (size_t i = 0; i < sizeof(payload_size) / sizeof(payload_size[0]); i++){
        ESP_LOGW(TAG, "Test Cache with payload_size %d, %d\r\n\r\n", payload_size[i][0], payload_size[i][1]);
        snprintf(wr_name, 127, "/sdcard/esp_gmf_test_cache_%02d.txt", i);
        read_write_test2(wr_name, payload_size[i][0], payload_size[i][1]);
        int ret = verify_two_files(file_name, wr_name);
        TEST_ASSERT_EQUAL(ret, ESP_OK);
    }

    esp_gmf_ut_teardown_sdmmc(card);
    vTaskDelay(10 / portTICK_PERIOD_MS);
}
