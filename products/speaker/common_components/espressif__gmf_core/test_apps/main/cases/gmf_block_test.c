/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "esp_private/esp_clk.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"

#include "esp_random.h"
#include "esp_gmf_oal_mem.h"
#include "gmf_ut_common.h"
#include "esp_gmf_block.h"

static const char *TAG = "TEST_ESP_GMF_BLOCK";
static bool        is_done;
static uint8_t     block_size_type;  // 1 for random size; 2 for specific size; others is fixed size
static bool        use_random_delay;
uint8_t           *dest_buf            = NULL;
uint32_t           specific_block_size = 0;

static const char *file_name       = "/sdcard/gmf_ut_test2.mp3";
static const char *file2_name      = "/sdcard/gmf_ut_test_out.mp3";
int                file2_indx      = 0;
char               file2_path[100] = "";

static void acquire_write_task(void *param)
{
    esp_gmf_block_handle_t bk = (esp_gmf_block_handle_t)param;
    FILE *f = fopen(file_name, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Open file failed on %s, %d", file_name, __LINE__);
        is_done = true;
        vTaskDelete(NULL);
    }
    uint8_t *buf = NULL;
    uint32_t start_cnt = 0;
    uint64_t total_cnt = 0;
    uint32_t wanted_size = 2000;
    esp_gmf_data_bus_block_t blk_buf = {0};
    int result = 0;
    uint32_t file_sz = 0;
    bool run = true;

    fseek(f, 0, SEEK_END);
    file_sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    ESP_LOGI(TAG, "Going to read file, para:%p, file size:%ld,", param, file_sz);

    while (run) {
        uint32_t start_time = 0;
        do {
            start_time = (esp_random() & 0x1F);
        } while (start_time == 0);
        switch (block_size_type) {
            case 1:
                wanted_size = start_time * 70 > 4000 ? 4000 : start_time * 20;
                break;
            case 2:
                wanted_size = specific_block_size;
                break;
            default:
                break;
        }

        start_cnt = esp_clk_rtc_time();
        result = esp_gmf_block_acquire_write(bk, &blk_buf, wanted_size, portMAX_DELAY);

        total_cnt += (esp_clk_rtc_time() - start_cnt);
        if (result >= ESP_OK) {
            buf = blk_buf.buf;
        } else {
            ESP_LOGE(TAG, "Acquire write quit,ret:%d", result);
            break;
        }
        if (use_random_delay) {
            vTaskDelay(start_time * 4 / portTICK_PERIOD_MS);
        }
        int ret = fread(buf, 1, blk_buf.buf_length, f);

        file_sz += ret;
        blk_buf.valid_size = ret;
        if (ret == 0 || (blk_buf.buf_length != ret)) {
            ESP_LOGI(TAG, "File read finished, size:%ld ret:%d", file_sz, ret);
            esp_gmf_block_done_write(bk);
            run = false;
        }

        esp_gmf_block_release_write(bk, &blk_buf, 0);
        if (ret != wanted_size) {
            ESP_LOGD(TAG, "W2:%ld, ret:%d,sz:%ld,%p-%d-%d", wanted_size, ret, file_sz, blk_buf.buf, blk_buf.valid_size, blk_buf.is_last);
            blk_buf.is_last = true;
        } else {
            ESP_LOGD(TAG, "W:%ld, ret:%d, file:%ld,%p-%d-%d", wanted_size, ret, file_sz, blk_buf.buf, blk_buf.valid_size, blk_buf.is_last);
        }
    }
    ESP_LOGI(TAG, "Done to acquire write, ticks:%lld us,file_sz:%ld", total_cnt, file_sz);
    fclose(f);
    vTaskDelete(NULL);
}

static void acquire_read_task(void *param)
{
    esp_gmf_block_handle_t bk = (esp_gmf_block_handle_t)param;
    sprintf(file2_path, "%s%x", file2_name, file2_indx);
    ESP_LOGI(TAG, "Going to write file, para:%p, path:%s", param, file2_path);
    FILE *f = fopen(file2_path, "wb+");
    if (f == NULL) {
        ESP_LOGE(TAG, "Open file failed on %s, %d", file2_path, __LINE__);
        is_done = true;
        vTaskDelete(NULL);
    }
    int wanted_size = 2000;
    uint8_t *buf = NULL;
    uint32_t start_cnt = 0;
    uint64_t total_cnt = 0;
    esp_gmf_data_bus_block_t blk_buf = {0};
    uint32_t file_sz = 0;
    bool run = true;
    while (run) {
        uint32_t start_time = 0;
        do {
            start_time = (esp_random() & 0x3F);
        } while (start_time == 0);
        switch (block_size_type) {
            case 1:
                wanted_size = (start_time * 40) > 3000 ? 3000 : start_time * 20;
                break;
            case 2:
                wanted_size = specific_block_size;
                break;
            default:
                break;
        }

        start_cnt = esp_clk_rtc_time();
        int ret = esp_gmf_block_acquire_read(bk, &blk_buf, wanted_size, portMAX_DELAY);
        total_cnt += (esp_clk_rtc_time() - start_cnt);

        if (use_random_delay) {
            vTaskDelay(start_time * 1 / portTICK_PERIOD_MS);
        }

        if (ret < ESP_OK) {
            ESP_LOGE(TAG, "Acquire read quit,ret:%d", ret);
            break;
        }
        file_sz += blk_buf.valid_size;
        buf = blk_buf.buf;
        ret = fwrite(buf, 1, blk_buf.valid_size, f);

        if (blk_buf.is_last == true) {
            run = false;
        }
        // printf("%02x ", buf[0]);
        esp_gmf_block_release_read(bk, &blk_buf, 0);
        blk_buf.valid_size = 0;
        // ESP_LOGI(TAG, "R:%d, ret:%d, file:%ld, %p-%d-%d", wanted_size, ret, file_sz, blk_buf.buf, blk_buf.valid_size, blk_buf.is_last);
    }
    fclose(f);
    ESP_LOGI(TAG, "Done to acquire read, ticks:%lld us,file_sz:%ld", total_cnt, file_sz);
    is_done = true;
    vTaskDelete(NULL);
}

TEST_CASE("Read and write with RANDOM SIZE + NO DELAY on different task", "ESP_GMF_BLOCK")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    // esp_log_level_set("ESP_GMF_BLOCK", ESP_LOG_DEBUG);

    sdmmc_card_t *card = NULL;
    esp_gmf_ut_setup_sdmmc(&card);

    is_done = false;
    esp_gmf_block_handle_t bk = NULL;
    esp_gmf_block_create(2 * 1000, 4, &bk);
    TEST_ASSERT_NOT_NULL(bk);
    uint32_t start_cnt = 0;
    uint32_t total_cnt = 0;
    start_cnt = 0;
    total_cnt = 0;
    block_size_type = 1;
    use_random_delay = false;

    dest_buf = esp_gmf_oal_calloc(1, 120 * 1024);
    TEST_ASSERT_NOT_NULL(dest_buf);

    uint32_t task_prio[3][2] = {{0, 2}, {0, 2}, {0, 5}};

    uint32_t loop_times = 12;
    for (int i = 0; i < loop_times; ++i) {
        memset(dest_buf, 0, 120 * 1024);
        file2_indx = i;
        printf("\r\n\r\n ---------- RANDOM SIZE + NO DELAY, %d, priority:%ld, %ld  ---------- \r\n", i, task_prio[i % 3][0], task_prio[i % 3][1]);
        start_cnt = esp_clk_rtc_time();
        xTaskCreate(acquire_read_task, "acq_read", 4096, bk, task_prio[i % 3][0], NULL);
        xTaskCreate(acquire_write_task, "acq_write", 4096, bk, task_prio[i % 3][1], NULL);
        while (1) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            if (is_done) {
                total_cnt += (esp_clk_rtc_time() - start_cnt);
                break;
            }
        }
        ESP_LOGI(TAG, "Elapsed time: %ld us \r\n", total_cnt);
        TEST_ASSERT_EQUAL(verify_two_files(file_name, file2_path), ESP_OK);
        esp_gmf_block_reset(bk);
        is_done = false;
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    esp_gmf_oal_free(dest_buf);
    esp_gmf_block_destroy(bk);
    esp_gmf_ut_teardown_sdmmc(card);
}

TEST_CASE("Read and write with FIXED SIZE + NO DELAY on different task", "ESP_GMF_BLOCK")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    // esp_log_level_set("ESP_GMF_BLOCK", ESP_LOG_DEBUG);

    sdmmc_card_t *card = NULL;
    esp_gmf_ut_setup_sdmmc(&card);

    is_done = false;
    esp_gmf_block_handle_t bk = NULL;
    esp_gmf_block_create(2 * 1000, 4, &bk);
    TEST_ASSERT_NOT_NULL(bk);
    uint32_t start_cnt = 0;
    uint32_t total_cnt = 0;
    start_cnt = 0;
    total_cnt = 0;
    block_size_type = 0;
    use_random_delay = false;
    ESP_GMF_MEM_SHOW(TAG);
    dest_buf = esp_gmf_oal_calloc(1, 120 * 1024);
    TEST_ASSERT_NOT_NULL(dest_buf);
    uint32_t task_prio[3][2] = {{5, 2}, {2, 2}, {2, 5}};
    uint32_t loop_times = 10;
    file2_indx = 0;
    for (int i = 0; i < loop_times; ++i) {
        memset(dest_buf, 0, 120 * 1024);
        printf("\r\n\r\n ---------- FIXED SIZE + NO DELAY, %d, priority:%ld, %ld  ---------- \r\n", i, task_prio[i % 3][0], task_prio[i % 3][1]);

        start_cnt = esp_clk_rtc_time();
        xTaskCreate(acquire_read_task, "acq_read", 4096, bk, task_prio[i % 3][0], NULL);
        xTaskCreate(acquire_write_task, "acq_write", 4096, bk, task_prio[i % 3][1], NULL);
        while (1) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            if (is_done) {
                total_cnt += (esp_clk_rtc_time() - start_cnt);
                break;
            }
        }
        ESP_LOGI(TAG, "Elapsed time: %ld us \r\n", total_cnt);
        TEST_ASSERT_EQUAL(verify_two_files(file_name, file2_path), ESP_OK);
        esp_gmf_block_reset(bk);
        is_done = false;
    }
    esp_gmf_oal_free(dest_buf);
    esp_gmf_block_destroy(bk);
    esp_gmf_ut_teardown_sdmmc(card);
}

TEST_CASE("Read and write with RANDOM SIZE + RANDOM DELAY on different task", "ESP_GMF_BLOCK")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    // esp_log_level_set("ESP_GMF_BLOCK", ESP_LOG_DEBUG);

    sdmmc_card_t *card = NULL;
    esp_gmf_ut_setup_sdmmc(&card);

    is_done = false;
    esp_gmf_block_handle_t bk = NULL;
    esp_gmf_block_create(2 * 1000, 4, &bk);
    TEST_ASSERT_NOT_NULL(bk);
    uint32_t start_cnt = 0;
    uint32_t total_cnt = 0;
    start_cnt = 0;
    total_cnt = 0;
    block_size_type = 1;
    use_random_delay = true;

    dest_buf = esp_gmf_oal_calloc(1, 120 * 1024);
    TEST_ASSERT_NOT_NULL(dest_buf);

    uint32_t task_prio[3][2] = {{5, 2}, {2, 2}, {2, 5}};

    uint32_t loop_times = 10;
    file2_indx = 0;
    for (int i = 0; i < loop_times; ++i) {
        memset(dest_buf, 0, 120 * 1024);
        printf("\r\n\r\n ---------- RANDOM SIZE + RANDOM DELAY, %d, priority:%ld, %ld  ---------- \r\n", i, task_prio[i % 3][0], task_prio[i % 3][1]);
        start_cnt = esp_clk_rtc_time();
        xTaskCreate(acquire_read_task, "acq_read", 4096, bk, task_prio[i % 3][0], NULL);
        xTaskCreate(acquire_write_task, "acq_write", 4096, bk, task_prio[i % 3][1], NULL);
        while (1) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            if (is_done) {
                total_cnt += (esp_clk_rtc_time() - start_cnt);
                break;
            }
        }
        ESP_LOGI(TAG, "Elapsed time: %ld us \r\n", total_cnt);
        TEST_ASSERT_EQUAL(verify_two_files(file_name, file2_path), ESP_OK);
        esp_gmf_block_reset(bk);
        is_done = false;
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    esp_gmf_oal_free(dest_buf);
    esp_gmf_block_destroy(bk);
    esp_gmf_ut_teardown_sdmmc(card);
}

TEST_CASE("Read and write with FIXED SIZE + RANDOM DELAY", "ESP_GMF_BLOCK")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    // esp_log_level_set("ESP_GMF_BLOCK", ESP_LOG_DEBUG);

    sdmmc_card_t *card = NULL;
    esp_gmf_ut_setup_sdmmc(&card);

    is_done = false;
    esp_gmf_block_handle_t bk = NULL;
    esp_gmf_block_create(2 * 1000, 4, &bk);
    TEST_ASSERT_NOT_NULL(bk);
    uint32_t start_cnt = 0;
    uint32_t total_cnt = 0;
    start_cnt = 0;
    total_cnt = 0;
    block_size_type = 0;
    use_random_delay = true;
    ESP_GMF_MEM_SHOW(TAG);
    dest_buf = esp_gmf_oal_calloc(1, 120 * 1024);
    TEST_ASSERT_NOT_NULL(dest_buf);
    uint32_t task_prio[3][2] = {{5, 2}, {2, 2}, {2, 5}};

    uint32_t loop_times = 10;
    file2_indx = 0;
    for (int i = 0; i < loop_times; ++i) {
        memset(dest_buf, 0, 120 * 1024);
        printf("\r\n\r\n ---------- FIXED SIZE + RANDOM DELAY %d, priority:%ld, %ld  ---------- \r\n", i, task_prio[i % 3][0], task_prio[i % 3][1]);

        start_cnt = esp_clk_rtc_time();
        xTaskCreate(acquire_read_task, "acq_read", 4096, bk, task_prio[i % 3][0], NULL);
        xTaskCreate(acquire_write_task, "acq_write", 4096, bk, task_prio[i % 3][1], NULL);
        while (1) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            if (is_done) {
                total_cnt += (esp_clk_rtc_time() - start_cnt);
                break;
            }
        }
        ESP_LOGI(TAG, "Elapsed time: %ld us \r\n", total_cnt);
        TEST_ASSERT_EQUAL(verify_two_files(file_name, file2_path), ESP_OK);
        esp_gmf_block_reset(bk);
        is_done = false;
    }
    esp_gmf_oal_free(dest_buf);
    esp_gmf_block_destroy(bk);
    esp_gmf_ut_teardown_sdmmc(card);
}

TEST_CASE("Read and write with SPECIFIC SIZE + NO DELAY", "ESP_GMF_BLOCK")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    // esp_log_level_set("ESP_GMF_BLOCK", ESP_LOG_DEBUG);

    sdmmc_card_t *card = NULL;
    esp_gmf_ut_setup_sdmmc(&card);

    is_done = false;
    esp_gmf_block_handle_t bk = NULL;
    esp_gmf_block_create(2 * 1000, 4, &bk);
    TEST_ASSERT_NOT_NULL(bk);
    uint32_t start_cnt = 0;
    uint32_t total_cnt = 0;
    start_cnt = 0;
    total_cnt = 0;
    block_size_type = 2;
    use_random_delay = false;
    ESP_GMF_MEM_SHOW(TAG);
    dest_buf = esp_gmf_oal_calloc(1, 120 * 1024);
    TEST_ASSERT_NOT_NULL(dest_buf);

    uint32_t task_prio[3][2] = {{5, 2}, {2, 2}, {2, 5}};
    uint32_t size_percent[10] = {1, 2, 10, 30, 50, 60, 70, 80, 99, 100};
    uint32_t loop_times = sizeof(size_percent) / sizeof(uint32_t);
    file2_indx = 0;
    for (int i = 0; i < loop_times; ++i) {
        memset(dest_buf, 0, 120 * 1024);
        uint32_t total_size = 0;
        esp_gmf_block_get_total_size(bk, &total_size);
        specific_block_size = total_size * size_percent[i] / 100;

        printf("\r\n\r\n ---------- SPECIFIC SIZE, %d, priority:%ld, %ld, blk_sz:%ld  ---------- \r\n",
               i, task_prio[i % 3][0], task_prio[i % 3][1], specific_block_size);

        start_cnt = esp_clk_rtc_time();
        xTaskCreate(acquire_read_task, "acq_read", 4096, bk, task_prio[i % 3][0], NULL);
        xTaskCreate(acquire_write_task, "acq_write", 4096, bk, task_prio[i % 3][1], NULL);
        while (1) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            if (is_done) {
                total_cnt += (esp_clk_rtc_time() - start_cnt);
                break;
            }
        }
        ESP_LOGI(TAG, "Elapsed time: %ld us \r\n", total_cnt);
        TEST_ASSERT_EQUAL(verify_two_files(file_name, file2_path), ESP_OK);
        esp_gmf_block_reset(bk);
        is_done = false;
    }
    esp_gmf_oal_free(dest_buf);
    esp_gmf_block_destroy(bk);
    esp_gmf_ut_teardown_sdmmc(card);
}
