/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_log.h"

#include "esp_gmf_element.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_data_bus.h"

#include "esp_gmf_oal_mem.h"
#include "esp_gmf_new_databus.h"
#include "gmf_fake_io.h"
#include "gmf_fake_dec.h"
#include "gmf_ut_common.h"

static const char *TAG           = "TEST_ESP_GMF_POOL";
static const char *test_file_uri = "/sdcard/gmf_ut_test1.mp3";

#define PIPELINE_BLOCK_BIT BIT(0)

esp_err_t _pipeline_event(esp_gmf_event_pkt_t *event, void *ctx)
{
    ESP_LOGE(TAG, "CB: RECV Pipeline EVT: el:%s-%p, type:%d, sub:%s, payload:%p, size:%d,%p",
             "OBJ_GET_TAG(event->from)", event->from, event->type, esp_gmf_event_get_state_str(event->sub),
             event->payload, event->payload_size, ctx);
    if ((event->sub == ESP_GMF_EVENT_STATE_STOPPED)
        || (event->sub == ESP_GMF_EVENT_STATE_FINISHED)
        || (event->sub == ESP_GMF_EVENT_STATE_ERROR)) {
        if (ctx) {
            xEventGroupSetBits((EventGroupHandle_t)ctx, PIPELINE_BLOCK_BIT);
        }
    }
    return 0;
}

static inline void pool_register_io_func(esp_gmf_pool_handle_t pool)
{
    fake_io_cfg_t io_cfg = FAKE_IO_CFG_DEFAULT();
    io_cfg.dir = ESP_GMF_IO_DIR_READER;
    esp_gmf_io_handle_t fs = NULL;
    fake_io_init(&io_cfg, &fs);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_io(pool, fs, NULL));

    io_cfg.dir = ESP_GMF_IO_DIR_WRITER;
    fake_io_init(&io_cfg, &fs);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_io(pool, fs, NULL));
}

static inline void pool_register_dec_func(esp_gmf_pool_handle_t pool)
{
    fake_dec_cfg_t fake_dec_cfg = DEFAULT_FAKE_DEC_CONFIG();
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.name = "dec1";
    esp_gmf_element_handle_t fake_dec = NULL;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.name = "dec2";
    fake_dec_cfg.in_buf_size = 5 * 1024;
    fake_dec_cfg.out_buf_size = 8 * 1024;
    fake_dec_cfg.cb = NULL;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.name = "dec3";
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.in_buf_size = 8 * 1024;
    fake_dec_cfg.out_buf_size = 10 * 1024;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.name = "dec4";
    fake_dec_cfg.in_buf_size = 12 * 1024;
    fake_dec_cfg.out_buf_size = 12 * 1024;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));
}

static inline void pool_register_dec_func2(esp_gmf_pool_handle_t pool)
{
    // All element use same payload for IN and OUT
    fake_dec_cfg_t fake_dec_cfg = DEFAULT_FAKE_DEC_CONFIG();
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.is_pass = true;
    fake_dec_cfg.in_buf_size = 10 * 1024;
    fake_dec_cfg.out_buf_size = 10 * 1024;
    fake_dec_cfg.name = "dec1";
    esp_gmf_element_handle_t fake_dec = NULL;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.name = "dec2";
    fake_dec_cfg.in_buf_size = 10 * 1024;
    fake_dec_cfg.out_buf_size = 10 * 1024;
    fake_dec_cfg.cb = NULL;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.name = "dec3";
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.in_buf_size = 10 * 1024;
    fake_dec_cfg.out_buf_size = 10 * 1024;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.name = "dec4";
    fake_dec_cfg.in_buf_size = 10 * 1024;
    fake_dec_cfg.out_buf_size = 10 * 1024;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));
}

static inline void pool_register_dec_func3(esp_gmf_pool_handle_t pool)
{
    // The middle element use same payload for IN and OUT
    fake_dec_cfg_t fake_dec_cfg = DEFAULT_FAKE_DEC_CONFIG();
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.name = "dec1";
    esp_gmf_element_handle_t fake_dec = NULL;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.name = "dec2";
    fake_dec_cfg.is_pass = true;  // The element IN and OUT use same payload
    fake_dec_cfg.in_buf_size = 5 * 1024;
    fake_dec_cfg.out_buf_size = 5 * 1024;
    fake_dec_cfg.cb = NULL;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.name = "dec3";
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.is_pass = false;
    fake_dec_cfg.in_buf_size = 8 * 1024;
    fake_dec_cfg.out_buf_size = 10 * 1024;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.name = "dec4";
    fake_dec_cfg.in_buf_size = 10 * 1024;
    fake_dec_cfg.out_buf_size = 12 * 1024;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));
}

static inline void pool_register_dec_func4(esp_gmf_pool_handle_t pool)
{
    // The middle element use same payload for IN and OUT
    fake_dec_cfg_t fake_dec_cfg = DEFAULT_FAKE_DEC_CONFIG();
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.name = "dec1";
    esp_gmf_element_handle_t fake_dec = NULL;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.name = "dec2";
    fake_dec_cfg.in_buf_size = 6 * 1024;
    fake_dec_cfg.out_buf_size = 7 * 1024;
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.is_pass = false;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.name = "dec3";
    fake_dec_cfg.is_pass = true;
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.in_buf_size = 7 * 1024;
    fake_dec_cfg.out_buf_size = 7 * 1024;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.name = "dec4";
    fake_dec_cfg.in_buf_size = 7 * 1024;
    fake_dec_cfg.out_buf_size = 7 * 1024;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));
}

static inline void pool_register_dec_func5(esp_gmf_pool_handle_t pool)
{
    // The middle element use same payload for IN and OUT
    fake_dec_cfg_t fake_dec_cfg = DEFAULT_FAKE_DEC_CONFIG();
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.name = "dec1";
    fake_dec_cfg.is_shared = false;
    esp_gmf_element_handle_t fake_dec = NULL;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.name = "dec2";
    fake_dec_cfg.in_buf_size = 6 * 1024;
    fake_dec_cfg.out_buf_size = 7 * 1024;
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.is_pass = false;
    fake_dec_cfg.is_shared = true;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.name = "dec3";
    fake_dec_cfg.is_pass = true;
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.in_buf_size = 7 * 1024;
    fake_dec_cfg.out_buf_size = 7 * 1024;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));

    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.name = "dec4";
    fake_dec_cfg.in_buf_size = 7 * 1024;
    fake_dec_cfg.out_buf_size = 7 * 1024;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmf_pool_register_element(pool, fake_dec, NULL));
}

TEST_CASE("Create and destroy pipeline", "ELEMENT_POOL")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_PIPELINE", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_ELEMENT", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_POOL", ESP_LOG_DEBUG);

    esp_gmf_pool_handle_t pool = NULL;
    esp_gmf_pool_init(&pool);
    TEST_ASSERT_NOT_NULL(pool);
    pool_register_io_func(pool);
    pool_register_dec_func(pool);
    ESP_GMF_POOL_SHOW_ITEMS(pool);

    // Create the new pipeline
    esp_gmf_pipeline_handle_t pipe = NULL;
    const char *name[] = {"dec1", "dec1", "dec1"};
    esp_gmf_pool_new_pipeline(pool, "file", name, sizeof(name) / sizeof(char *), "file", &pipe);
    TEST_ASSERT_NOT_NULL(pipe);

    esp_gmf_pipeline_destroy(pipe);
    esp_gmf_pool_deinit(pool);
}

TEST_CASE("One Pipe, [FILE->dec->dec->dec->FILE]", "ELEMENT_POOL")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_PIPELINE", ESP_LOG_DEBUG);
    esp_log_level_set("FAKE_DEC", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_POOL", ESP_LOG_DEBUG);

    esp_gmf_pool_handle_t pool = NULL;
    esp_gmf_pool_init(&pool);
    TEST_ASSERT_NOT_NULL(pool);
    pool_register_io_func(pool);
    pool_register_dec_func(pool);
    ESP_GMF_POOL_SHOW_ITEMS(pool);

    // Create the new elements
    esp_gmf_pipeline_handle_t pipe = NULL;
    const char *name[] = {"dec1", "dec1", "dec1"};
    esp_gmf_pool_new_pipeline(pool, "file", name, sizeof(name) / sizeof(char *), "file", &pipe);
    TEST_ASSERT_NOT_NULL(pipe);

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t work_task = NULL;
    esp_gmf_task_init(&cfg, &work_task);
    TEST_ASSERT_NOT_NULL(work_task);

    esp_gmf_pipeline_bind_task(pipe, work_task);
    esp_gmf_pipeline_loading_jobs(pipe);
    esp_gmf_pipeline_set_event(pipe, _pipeline_event, NULL);
    esp_gmf_pipeline_set_in_uri(pipe, test_file_uri);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_run(pipe));
    vTaskDelay(300 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_pause(pipe));
    vTaskDelay(800 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_resume(pipe));
    vTaskDelay(300 / portTICK_PERIOD_MS);

    ESP_LOGE(TAG, "%s-%d", __func__, __LINE__);
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_stop(pipe));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(work_task));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_destroy(pipe));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pool_deinit(pool));
}

TEST_CASE("One Pipe, [FILE->dec->FILE]", "ELEMENT_POOL")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_PIPELINE", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_ELEMENT", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_POOL", ESP_LOG_DEBUG);

    esp_gmf_pool_handle_t pool = NULL;
    esp_gmf_pool_init(&pool);
    TEST_ASSERT_NOT_NULL(pool);
    pool_register_io_func(pool);
    pool_register_dec_func(pool);
    ESP_GMF_POOL_SHOW_ITEMS(pool);

    // Create the new elements
    esp_gmf_pipeline_handle_t pipe = NULL;
    const char *name[] = {"dec1"};
    esp_gmf_pool_new_pipeline(pool, "file", name, sizeof(name) / sizeof(char *), "file", &pipe);
    TEST_ASSERT_NOT_NULL(pipe);

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t work_task = NULL;
    esp_gmf_task_init(&cfg, &work_task);
    TEST_ASSERT_NOT_NULL(work_task);

    esp_gmf_pipeline_bind_task(pipe, work_task);
    esp_gmf_pipeline_loading_jobs(pipe);
    esp_gmf_pipeline_set_event(pipe, _pipeline_event, NULL);
    esp_gmf_pipeline_set_in_uri(pipe, test_file_uri);
    esp_gmf_pipeline_set_out_uri(pipe, "/sdcard/esp_gmf_ut_test_out.mp3");

    TEST_ASSERT_EQUAL(esp_gmf_pipeline_run(pipe), ESP_GMF_ERR_OK);
    // Wait to finished or got error
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_stop(pipe));

    ESP_LOGE(TAG, "%s-%d", __func__, __LINE__);
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(work_task));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_destroy(pipe));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pool_deinit(pool));
}

TEST_CASE("IN-OUT Different payload, [FILE->dec->FILE]", "ELEMENT_PORT")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_PIPELINE", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_ELEMENT", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_PORT", ESP_LOG_DEBUG);
    ESP_GMF_MEM_SHOW(TAG);

    esp_gmf_pool_handle_t pool = NULL;
    esp_gmf_pool_init(&pool);
    TEST_ASSERT_NOT_NULL(pool);
    pool_register_io_func(pool);
    pool_register_dec_func(pool);
    ESP_GMF_POOL_SHOW_ITEMS(pool);

    // Create the new elements
    esp_gmf_pipeline_handle_t pipe = NULL;
    const char *name[] = {"dec1", "dec2", "dec3", "dec4"};
    esp_gmf_pool_new_pipeline(pool, "file", name, sizeof(name) / sizeof(char *), "file", &pipe);
    TEST_ASSERT_NOT_NULL(pipe);

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t work_task = NULL;
    esp_gmf_task_init(&cfg, &work_task);
    TEST_ASSERT_NOT_NULL(work_task);

    esp_gmf_pipeline_bind_task(pipe, work_task);
    esp_gmf_pipeline_loading_jobs(pipe);
    esp_gmf_pipeline_set_event(pipe, _pipeline_event, NULL);
    esp_gmf_pipeline_set_in_uri(pipe, test_file_uri);
    esp_gmf_pipeline_set_out_uri(pipe, "/sdcard/esp_gmf_ut_test_out.mp3");

    ESP_GMF_MEM_SHOW(TAG);
    TEST_ASSERT_EQUAL(esp_gmf_pipeline_run(pipe), ESP_GMF_ERR_OK);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    ESP_GMF_MEM_SHOW(TAG);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    ESP_GMF_MEM_SHOW(TAG);
    // Wait to finished or got error
    vTaskDelay(300 / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_stop(pipe));

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(work_task));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_destroy(pipe));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pool_deinit(pool));
}

TEST_CASE("ALL element IN-OUT SAME, [FILE->dec->FILE]", "ELEMENT_PORT")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_PIPELINE", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_ELEMENT", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_PORT", ESP_LOG_DEBUG);
    ESP_GMF_MEM_SHOW(TAG);

    esp_gmf_pool_handle_t pool = NULL;
    esp_gmf_pool_init(&pool);
    TEST_ASSERT_NOT_NULL(pool);
    pool_register_io_func(pool);
    pool_register_dec_func2(pool);
    ESP_GMF_POOL_SHOW_ITEMS(pool);

    // Create the new elements
    esp_gmf_pipeline_handle_t pipe = NULL;
    const char *name[] = {"dec1", "dec2", "dec3", "dec4"};
    esp_gmf_pool_new_pipeline(pool, "file", name, sizeof(name) / sizeof(char *), "file", &pipe);
    TEST_ASSERT_NOT_NULL(pipe);

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t work_task = NULL;
    esp_gmf_task_init(&cfg, &work_task);
    TEST_ASSERT_NOT_NULL(work_task);

    esp_gmf_pipeline_bind_task(pipe, work_task);
    esp_gmf_pipeline_loading_jobs(pipe);
    esp_gmf_pipeline_set_event(pipe, _pipeline_event, NULL);
    esp_gmf_pipeline_set_in_uri(pipe, test_file_uri);
    esp_gmf_pipeline_set_out_uri(pipe, "/sdcard/esp_gmf_ut_test_out.mp3");
    ESP_GMF_MEM_SHOW(TAG);

    TEST_ASSERT_EQUAL(esp_gmf_pipeline_run(pipe), ESP_GMF_ERR_OK);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    ESP_GMF_MEM_SHOW(TAG);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    ESP_GMF_MEM_SHOW(TAG);

    // Wait to finished or got error
    vTaskDelay(300 / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_stop(pipe));

    ESP_LOGE(TAG, "%s-%d", __func__, __LINE__);
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(work_task));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_destroy(pipe));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pool_deinit(pool));
}

TEST_CASE("The middle element IN-OUT SAME, [FILE->dec->FILE]", "ELEMENT_PORT")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_PIPELINE", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_ELEMENT", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_PORT", ESP_LOG_DEBUG);
    ESP_GMF_MEM_SHOW(TAG);

    esp_gmf_pool_handle_t pool = NULL;
    esp_gmf_pool_init(&pool);
    TEST_ASSERT_NOT_NULL(pool);
    pool_register_io_func(pool);
    pool_register_dec_func3(pool);
    ESP_GMF_POOL_SHOW_ITEMS(pool);

    // Create the new elements
    esp_gmf_pipeline_handle_t pipe = NULL;
    const char *name[] = {"dec1", "dec2", "dec3", "dec4"};
    esp_gmf_pool_new_pipeline(pool, "file", name, sizeof(name) / sizeof(char *), "file", &pipe);
    TEST_ASSERT_NOT_NULL(pipe);

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t work_task = NULL;
    esp_gmf_task_init(&cfg, &work_task);
    TEST_ASSERT_NOT_NULL(work_task);

    esp_gmf_pipeline_bind_task(pipe, work_task);
    esp_gmf_pipeline_loading_jobs(pipe);
    esp_gmf_pipeline_set_event(pipe, _pipeline_event, NULL);
    esp_gmf_pipeline_set_in_uri(pipe, test_file_uri);
    esp_gmf_pipeline_set_out_uri(pipe, "/sdcard/esp_gmf_ut_test_out.mp3");
    ESP_GMF_MEM_SHOW(TAG);

    TEST_ASSERT_EQUAL(esp_gmf_pipeline_run(pipe), ESP_GMF_ERR_OK);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    ESP_GMF_MEM_SHOW(TAG);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    ESP_GMF_MEM_SHOW(TAG);

    // Wait to finished or got error
    vTaskDelay(300 / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_stop(pipe));

    ESP_LOGE(TAG, "%s-%d", __func__, __LINE__);
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(work_task));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_destroy(pipe));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pool_deinit(pool));
}

TEST_CASE("The element IN-OUT SAME, [FILE->dec->FILE]", "ELEMENT_PORT")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_PIPELINE", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_ELEMENT", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_PORT", ESP_LOG_DEBUG);
    ESP_GMF_MEM_SHOW(TAG);

    esp_gmf_pool_handle_t pool = NULL;
    esp_gmf_pool_init(&pool);
    TEST_ASSERT_NOT_NULL(pool);
    pool_register_io_func(pool);
    pool_register_dec_func4(pool);
    ESP_GMF_POOL_SHOW_ITEMS(pool);

    // Create the new elements
    esp_gmf_pipeline_handle_t pipe = NULL;
    const char *name[] = {"dec1", "dec2", "dec3", "dec4"};
    esp_gmf_pool_new_pipeline(pool, "file", name, sizeof(name) / sizeof(char *), "file", &pipe);
    TEST_ASSERT_NOT_NULL(pipe);

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t work_task = NULL;
    esp_gmf_task_init(&cfg, &work_task);
    TEST_ASSERT_NOT_NULL(work_task);

    esp_gmf_pipeline_bind_task(pipe, work_task);
    esp_gmf_pipeline_loading_jobs(pipe);
    esp_gmf_pipeline_set_event(pipe, _pipeline_event, NULL);
    esp_gmf_pipeline_set_in_uri(pipe, test_file_uri);
    esp_gmf_pipeline_set_out_uri(pipe, "/sdcard/esp_gmf_ut_test_out.mp3");
    ESP_GMF_MEM_SHOW(TAG);

    TEST_ASSERT_EQUAL(esp_gmf_pipeline_run(pipe), ESP_GMF_ERR_OK);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    ESP_GMF_MEM_SHOW(TAG);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    ESP_GMF_MEM_SHOW(TAG);

    // Wait to finished or got error
    vTaskDelay(300 / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_stop(pipe));

    ESP_LOGE(TAG, "%s-%d", __func__, __LINE__);
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(work_task));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_destroy(pipe));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pool_deinit(pool));
}

TEST_CASE("Un-Shared port, Same payload, [FILE->dec->FILE]", "ELEMENT_PORT")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_PIPELINE", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_ELEMENT", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_PORT", ESP_LOG_DEBUG);
    ESP_GMF_MEM_SHOW(TAG);

    esp_gmf_pool_handle_t pool = NULL;
    esp_gmf_pool_init(&pool);
    TEST_ASSERT_NOT_NULL(pool);
    pool_register_io_func(pool);
    pool_register_dec_func5(pool);
    ESP_GMF_POOL_SHOW_ITEMS(pool);

    // Create the new elements
    esp_gmf_pipeline_handle_t pipe = NULL;
    const char *name[] = {"dec1", "dec2", "dec3", "dec4"};
    esp_gmf_pool_new_pipeline(pool, "file", name, sizeof(name) / sizeof(char *), "file", &pipe);
    TEST_ASSERT_NOT_NULL(pipe);

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t work_task = NULL;
    esp_gmf_task_init(&cfg, &work_task);
    TEST_ASSERT_NOT_NULL(work_task);

    esp_gmf_pipeline_bind_task(pipe, work_task);
    esp_gmf_pipeline_loading_jobs(pipe);
    esp_gmf_pipeline_set_event(pipe, _pipeline_event, NULL);
    esp_gmf_pipeline_set_in_uri(pipe, test_file_uri);
    esp_gmf_pipeline_set_out_uri(pipe, "/sdcard/esp_gmf_ut_test_out.mp3");
    ESP_GMF_MEM_SHOW(TAG);

    TEST_ASSERT_EQUAL(esp_gmf_pipeline_run(pipe), ESP_GMF_ERR_OK);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    ESP_GMF_MEM_SHOW(TAG);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    ESP_GMF_MEM_SHOW(TAG);

    // Wait to finished or got error
    vTaskDelay(300 / portTICK_PERIOD_MS);
    ESP_GMF_MEM_SHOW(TAG);
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_stop(pipe));

    ESP_LOGE(TAG, "%s-%d", __func__, __LINE__);
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(work_task));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pipeline_destroy(pipe));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_pool_deinit(pool));
}

#define TEST_LENGTH (3 * 1024)
static uint8_t *test_buffer = NULL;

static esp_gmf_err_io_t _acquire_read(esp_gmf_io_handle_t handle, void *payload, uint32_t wanted_size, int block_ticks)
{
    esp_gmf_payload_t *pload = (esp_gmf_payload_t *)payload;
    pload->buf = test_buffer;
    pload->buf_length = TEST_LENGTH;
    pload->valid_size = wanted_size > TEST_LENGTH ? TEST_LENGTH : wanted_size;
    printf("acquire_read %d\n", pload->valid_size);
    // memset(pload->buf, 1, pload->valid_size);
    return ESP_GMF_IO_OK;
}

static esp_gmf_err_io_t _release_read(esp_gmf_io_handle_t handle, void *payload, int block_ticks)
{
    return ESP_GMF_IO_OK;
}

static esp_gmf_err_io_t _acquire_write(esp_gmf_io_handle_t handle, void *payload, uint32_t wanted_size, int block_ticks)
{
    esp_gmf_payload_t *pload = (esp_gmf_payload_t *)payload;
    pload->valid_size = wanted_size;
    return ESP_GMF_IO_OK;
}
static esp_gmf_err_io_t _acquire_write_fail(esp_gmf_io_handle_t handle, void *payload, uint32_t wanted_size, int block_ticks)
{
    esp_gmf_payload_t *pload = (esp_gmf_payload_t *)payload;
    pload->valid_size = wanted_size > TEST_LENGTH ? TEST_LENGTH : wanted_size;
    if(wanted_size > TEST_LENGTH) {
        pload->valid_size = TEST_LENGTH;
        return ESP_GMF_IO_FAIL;
    }
    pload->valid_size = wanted_size;
    return ESP_GMF_IO_OK;
}

static esp_gmf_err_io_t _release_write(esp_gmf_io_handle_t handle, void *payload, int block_ticks)
{
    return ESP_GMF_IO_OK;
}

TEST_CASE("Un-shared port, [port callback -> dec -> port callback]", "[ELEMENT_POOL]")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_PIPELINE", ESP_LOG_DEBUG);
    esp_log_level_set("FAKE_DEC", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_POOL", ESP_LOG_DEBUG);

    ESP_GMF_MEM_SHOW(TAG);
    esp_gmf_port_handle_t in_port = NULL;
    esp_gmf_port_handle_t out_port = NULL;
    esp_gmf_obj_handle_t obj_hd = {NULL};

    test_buffer = (uint8_t *)malloc(TEST_LENGTH);
    TEST_ASSERT_NOT_EQUAL(NULL, test_buffer);
    // New obj
    fake_dec_cfg_t fake_dec_cfg = DEFAULT_FAKE_DEC_CONFIG();
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.name = "dec1";
    esp_gmf_element_handle_t fake_dec = NULL;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    obj_hd = fake_dec;
    TEST_ASSERT_NOT_NULL(fake_dec);
    // Print obj tag
    ESP_LOGE(TAG, "%s-%d,obj_hd:%p", (OBJ_GET_TAG(obj_hd)), __LINE__, obj_hd);
    // Create in/out port
    in_port = NEW_ESP_GMF_PORT_IN_BLOCK(_acquire_read, _release_read, NULL, NULL, TEST_LENGTH, 100);
    out_port = NEW_ESP_GMF_PORT_OUT_BLOCK(_acquire_write, _release_write, NULL, NULL, TEST_LENGTH, 100);
    // Register in/out port
    esp_gmf_element_register_in_port(obj_hd, in_port);
    esp_gmf_element_register_out_port(obj_hd, out_port);
    // Open obj
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_element_process_open(obj_hd, NULL));
    // Run obj
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_element_process_running(obj_hd, NULL));
    // Close obj
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_element_process_close(obj_hd, NULL));
    // Delete obj: port will be deleted when obj handle pool is destroyed
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_obj_delete(obj_hd));
    free(test_buffer);
}

TEST_CASE("Shared port, [port callback -> dec -> port callback]", "[ELEMENT_POOL]")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_PIPELINE", ESP_LOG_DEBUG);
    esp_log_level_set("FAKE_DEC", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_POOL", ESP_LOG_DEBUG);

    ESP_GMF_MEM_SHOW(TAG);
    esp_gmf_port_handle_t in_port = NULL;
    esp_gmf_port_handle_t out_port = NULL;
    esp_gmf_obj_handle_t obj_hd = {NULL};

    test_buffer = (uint8_t *)malloc(TEST_LENGTH);
    TEST_ASSERT_NOT_EQUAL(NULL, test_buffer);
    // New obj
    fake_dec_cfg_t fake_dec_cfg = DEFAULT_FAKE_DEC_CONFIG();
    fake_dec_cfg.cb = NULL;
    fake_dec_cfg.name = "dec1";
    fake_dec_cfg.is_shared = true;
    fake_dec_cfg.is_pass = true;

    esp_gmf_element_handle_t fake_dec = NULL;
    fake_dec_init(&fake_dec_cfg, &fake_dec);
    obj_hd = fake_dec;
    TEST_ASSERT_NOT_NULL(fake_dec);
    // Print obj tag
    ESP_LOGE(TAG, "%s-%d,obj_hd:%p", (OBJ_GET_TAG(obj_hd)), __LINE__, obj_hd);
    // Create in/out port
    in_port = NEW_ESP_GMF_PORT_IN_BLOCK(_acquire_read, _release_read, NULL, NULL, TEST_LENGTH, 100);
    out_port = NEW_ESP_GMF_PORT_OUT_BLOCK(_acquire_write_fail, _release_write, NULL, NULL, TEST_LENGTH, 100);
    // Register in/out port
    esp_gmf_element_register_in_port(obj_hd, in_port);
    esp_gmf_element_register_out_port(obj_hd, out_port);
    // Open obj
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_element_process_open(obj_hd, NULL));
    // Run obj
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_FAIL, esp_gmf_element_process_running(obj_hd, NULL));
    // Close obj
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_element_process_close(obj_hd, NULL));
    // Delete obj: port will be deleted when obj handle pool is destroyed
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_obj_delete(obj_hd));
    free(test_buffer);
}
