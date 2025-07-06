/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_task.h"

static const char *TAG = "TEST_ESP_GMF_TASK";

typedef struct {
    uint32_t prepare : 8;
    uint32_t working : 16;
    uint32_t cleanup : 8;
    esp_gmf_job_err_t prepare_return;
    esp_gmf_job_err_t working_return;
    esp_gmf_job_err_t cleanup_return;
}test_gmf_task_count_t;

test_gmf_task_count_t test_gmf_task1_count = {0};
test_gmf_task_count_t test_gmf_task2_count = {0};
test_gmf_task_count_t test_gmf_task3_count = {0};
test_gmf_task_count_t test_gmf_task4_count = {0};

esp_gmf_job_err_t prepare1(void *self, void *para)
{
    printf("%s,%p,%p\r\n", __func__, self, para);
    test_gmf_task1_count.prepare++;
    return test_gmf_task1_count.prepare_return;
}
esp_gmf_job_err_t prepare2(void *self, void *para)
{
    printf("%s,%p,%p\r\n", __func__, self, para);
    test_gmf_task2_count.prepare++;
    return test_gmf_task2_count.prepare_return;;
}
esp_gmf_job_err_t prepare3(void *self, void *para)
{
    printf("%s,%p,%p\r\n", __func__, self, para);
    test_gmf_task3_count.prepare++;
    return test_gmf_task3_count.prepare_return;;
}
esp_gmf_job_err_t prepare4(void *self, void *para)
{
    printf("%s,%p,%p\r\n", __func__, self, para);
    test_gmf_task4_count.prepare++;
    return test_gmf_task4_count.prepare_return;;
}

esp_gmf_job_err_t working1(void *self, void *para)
{
    vTaskDelay(50 / portTICK_PERIOD_MS);
    printf("%s,%p,%p\r\n", __func__, self, para);
    test_gmf_task1_count.working++;
    return test_gmf_task1_count.working_return;
}
esp_gmf_job_err_t working2(void *self, void *para)
{
    vTaskDelay(150 / portTICK_PERIOD_MS);
    printf("%s,%p,%p\r\n", __func__, self, para);
    test_gmf_task2_count.working++;
    return test_gmf_task2_count.working_return;
}
esp_gmf_job_err_t working3(void *self, void *para)
{
    vTaskDelay(100 / portTICK_PERIOD_MS);
    printf("%s,%p,%p\r\n", __func__, self, para);
    test_gmf_task3_count.working++;
    return test_gmf_task3_count.working_return;
}
esp_gmf_job_err_t working4(void *self, void *para)
{
    vTaskDelay(50 / portTICK_PERIOD_MS);
    printf("%s,%p,%p\r\n", __func__, self, para);
    test_gmf_task4_count.working++;
    return test_gmf_task4_count.working_return;
}

esp_gmf_job_err_t cleanup1(void *self, void *para)
{
    vTaskDelay(200 / portTICK_PERIOD_MS);
    printf("%s,%p,%p\r\n", __func__, self, para);
    test_gmf_task1_count.cleanup++;
    return test_gmf_task1_count.cleanup_return;
}
esp_gmf_job_err_t cleanup2(void *self, void *para)
{
    vTaskDelay(200 / portTICK_PERIOD_MS);
    printf("%s,%p,%p\r\n", __func__, self, para);
    test_gmf_task2_count.cleanup++;
    return test_gmf_task2_count.cleanup_return;
}
esp_gmf_job_err_t cleanup3(void *self, void *para)
{
    vTaskDelay(100 / portTICK_PERIOD_MS);
    printf("%s,%p,%p\r\n", __func__, self, para);
    test_gmf_task3_count.cleanup++;
    return test_gmf_task3_count.cleanup_return;
}
esp_gmf_job_err_t cleanup4(void *self, void *para)
{
    vTaskDelay(100 / portTICK_PERIOD_MS);
    printf("%s,%p,%p\r\n", __func__, self, para);
    test_gmf_task4_count.cleanup++;
    return test_gmf_task4_count.cleanup_return;
}
static void clear_test_gmf_task_count(void)
{
    memset(&test_gmf_task4_count, 0, sizeof(test_gmf_task4_count));
    memset(&test_gmf_task3_count, 0, sizeof(test_gmf_task3_count));
    memset(&test_gmf_task2_count, 0, sizeof(test_gmf_task2_count));
    memset(&test_gmf_task1_count, 0, sizeof(test_gmf_task1_count));
}

static esp_gmf_err_t esp_gmf_task_evt(esp_gmf_event_pkt_t *evt, void *ctx)
{
    esp_gmf_task_handle_t tsk = evt->from;
    ESP_LOGI(TAG, "TASK EVT, tsk:%s-%p, t:%x, sub:%s, pld:%p, sz:%d",
             OBJ_GET_TAG(tsk), evt->from, evt->type, esp_gmf_event_get_state_str(evt->sub), evt->payload, evt->payload_size);
    if (evt->type == ESP_GMF_EVT_TYPE_LOADING_JOB) {
        switch (evt->sub) {
            case ESP_GMF_EVENT_STATE_ERROR:
            case ESP_GMF_EVENT_STATE_STOPPED:
            case ESP_GMF_EVENT_STATE_FINISHED:
                esp_gmf_task_register_ready_job(tsk, NULL, cleanup1, ESP_GMF_JOB_TIMES_ONCE, NULL, true);
                esp_gmf_task_register_ready_job(tsk, NULL, cleanup2, ESP_GMF_JOB_TIMES_ONCE, NULL, true);
                esp_gmf_task_register_ready_job(tsk, NULL, cleanup3, ESP_GMF_JOB_TIMES_ONCE, NULL, true);
                esp_gmf_task_register_ready_job(tsk, NULL, cleanup4, ESP_GMF_JOB_TIMES_ONCE, NULL, true);
                break;
        }
    }
    return ESP_GMF_ERR_OK;
}

TEST_CASE("Working to done with manual register cleanup", "ESP_GMF_TASK")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    clear_test_gmf_task_count();

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t hd = NULL;

    esp_gmf_task_init(&cfg, &hd);
    esp_gmf_task_register_ready_job(hd, NULL, prepare1, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare2, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare3, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare4, ESP_GMF_JOB_TIMES_ONCE, NULL, false);

    esp_gmf_task_register_ready_job(hd, NULL, working1, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working2, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working3, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working4, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_run(hd));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "SET working1-return DONE");
    esp_gmf_task_register_ready_job(hd, NULL, cleanup1, ESP_GMF_JOB_TIMES_ONCE, NULL, true);
    test_gmf_task1_count.working_return = ESP_GMF_JOB_ERR_DONE;
    vTaskDelay(200 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "SET work2ing-return DONE");
    esp_gmf_task_register_ready_job(hd, NULL, cleanup2, ESP_GMF_JOB_TIMES_ONCE, NULL, true);
    test_gmf_task2_count.working_return = ESP_GMF_JOB_ERR_DONE;
    vTaskDelay(300 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "SET working3-return DONE");
    esp_gmf_task_register_ready_job(hd, NULL, cleanup3, ESP_GMF_JOB_TIMES_ONCE, NULL, true);
    test_gmf_task3_count.working_return = ESP_GMF_JOB_ERR_DONE;
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(hd));
    ESP_GMF_MEM_SHOW(TAG);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.cleanup);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.prepare);

    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task1_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task2_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task3_count.working);

    ESP_LOGI(TAG, "task1: %d, task2: %d, task3: %d, task4: %d", test_gmf_task1_count.working,
        test_gmf_task2_count.working, test_gmf_task3_count.working, test_gmf_task4_count.working);
    if ((test_gmf_task1_count.working == test_gmf_task2_count.working) && test_gmf_task1_count.working == test_gmf_task3_count.working) {
        // OK
        TEST_ASSERT_TRUE(true);
    } else {
        TEST_ASSERT_FALSE(true);
    }
}

TEST_CASE("Working to done with auto register cleanup", "ESP_GMF_TASK")
{
    esp_log_level_set("*", ESP_LOG_INFO);

    clear_test_gmf_task_count();
    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t hd = NULL;

    esp_gmf_task_init(&cfg, &hd);
    esp_gmf_task_set_event_func(hd, esp_gmf_task_evt, NULL);
    esp_gmf_task_register_ready_job(hd, NULL, prepare1, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare2, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare3, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare4, ESP_GMF_JOB_TIMES_ONCE, NULL, false);

    esp_gmf_task_register_ready_job(hd, NULL, working1, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working2, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working3, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working4, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_run(hd));
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "SET working1_return DONE");
    test_gmf_task1_count.working_return = ESP_GMF_JOB_ERR_DONE;
    ESP_LOGW(TAG, "SET test_gmf_task2_count.working_return DONE");
    test_gmf_task2_count.working_return = ESP_GMF_JOB_ERR_DONE;
    ESP_LOGW(TAG, "SET test_gmf_task3_count.working_return DONE");
    test_gmf_task3_count.working_return = ESP_GMF_JOB_ERR_DONE;
    ESP_LOGW(TAG, "SET test_gmf_task4_count.working_return DONE");
    test_gmf_task4_count.working_return = ESP_GMF_JOB_ERR_DONE;
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(hd));
    ESP_GMF_MEM_SHOW(TAG);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.cleanup);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.prepare);

    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task1_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task2_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task3_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task4_count.working);

    ESP_LOGI(TAG, "task1: %d, task2: %d, task3: %d, task4: %d", test_gmf_task1_count.working,
        test_gmf_task2_count.working, test_gmf_task3_count.working, test_gmf_task4_count.working);
}

TEST_CASE("Working with CONTINUE", "ESP_GMF_TASK")
{
    esp_log_level_set("*", ESP_LOG_INFO);

/* Test following cases:
*  1. A->B->C->D
*  2. A->B->C
*  3. A->B
*  4. A->B->C->D
*  5. To Done
*/
    clear_test_gmf_task_count();
    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t hd = NULL;

    esp_gmf_task_init(&cfg, &hd);
    esp_gmf_task_set_event_func(hd, esp_gmf_task_evt, NULL);
    esp_gmf_task_register_ready_job(hd, NULL, prepare1, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare2, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare3, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare4, ESP_GMF_JOB_TIMES_ONCE, NULL, false);

    esp_gmf_task_register_ready_job(hd, NULL, working1, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working2, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working3, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working4, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_run(hd));
    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "SET task3 return, ESP_GMF_JOB_ERR_CONTINUE");
    test_gmf_task3_count.working_return = ESP_GMF_JOB_ERR_CONTINUE;
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "SET task2 return, ESP_GMF_JOB_ERR_CONTINUE");
    test_gmf_task2_count.working_return = ESP_GMF_JOB_ERR_CONTINUE;
    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "SET task2 return, ESP_GMF_JOB_ERR_OK");
    test_gmf_task2_count.working_return = ESP_GMF_JOB_ERR_OK;
    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "SET return, ESP_GMF_JOB_ERR_DONE");
    test_gmf_task1_count.working_return = ESP_GMF_JOB_ERR_DONE;
    test_gmf_task2_count.working_return = ESP_GMF_JOB_ERR_DONE;
    test_gmf_task3_count.working_return = ESP_GMF_JOB_ERR_DONE;
    test_gmf_task4_count.working_return = ESP_GMF_JOB_ERR_DONE;
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(hd));
    ESP_GMF_MEM_SHOW(TAG);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.cleanup);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.prepare);

    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task1_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task2_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task3_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task4_count.working);

    ESP_LOGI(TAG, "task1: %d, task2: %d, task3: %d, task4: %d", test_gmf_task1_count.working,
        test_gmf_task2_count.working, test_gmf_task3_count.working, test_gmf_task4_count.working);
    if ((test_gmf_task1_count.working >= test_gmf_task2_count.working)
        && (test_gmf_task2_count.working > test_gmf_task3_count.working)
        && (test_gmf_task3_count.working > test_gmf_task4_count.working)) {
        // OK
        TEST_ASSERT_TRUE(true);
    } else {
        TEST_ASSERT_FALSE(true);
    }
}

TEST_CASE("Working with TRUNCATE", "ESP_GMF_TASK")
{
    esp_log_level_set("*", ESP_LOG_INFO);

/* Test following cases:
*  1. A->B->C->D
*  2. B->C->D
*  3. C->D
*  4. A->B->C->D
*  5. To Done
*/
    clear_test_gmf_task_count();

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t hd = NULL;

    esp_gmf_task_init(&cfg, &hd);
    esp_gmf_task_set_event_func(hd, esp_gmf_task_evt, NULL);
    esp_gmf_task_register_ready_job(hd, NULL, prepare1, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare2, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare3, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare4, ESP_GMF_JOB_TIMES_ONCE, NULL, false);

    esp_gmf_task_register_ready_job(hd, NULL, working1, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working2, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working3, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working4, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_run(hd));
    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "SET task2 return, ESP_GMF_JOB_ERR_TRUNCATE");
    test_gmf_task2_count.working_return = ESP_GMF_JOB_ERR_TRUNCATE;
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "SET task3 return, ESP_GMF_JOB_ERR_TRUNCATE");
    test_gmf_task3_count.working_return = ESP_GMF_JOB_ERR_TRUNCATE;
    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "SET task3 return, ESP_GMF_JOB_ERR_OK");
    test_gmf_task3_count.working_return = ESP_GMF_JOB_ERR_OK;
    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "SET return, ESP_GMF_JOB_ERR_DONE");
    test_gmf_task1_count.working_return = ESP_GMF_JOB_ERR_DONE;
    test_gmf_task2_count.working_return = ESP_GMF_JOB_ERR_DONE;
    test_gmf_task3_count.working_return = ESP_GMF_JOB_ERR_DONE;
    test_gmf_task4_count.working_return = ESP_GMF_JOB_ERR_DONE;
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(hd));
    ESP_GMF_MEM_SHOW(TAG);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.cleanup);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.prepare);

    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task1_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task2_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task3_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task4_count.working);

    ESP_LOGI(TAG, "task1: %d, task2: %d, task3: %d, task4: %d", test_gmf_task1_count.working,
        test_gmf_task2_count.working, test_gmf_task3_count.working, test_gmf_task4_count.working);
    if ((test_gmf_task1_count.working < test_gmf_task2_count.working)
        && (test_gmf_task2_count.working < test_gmf_task3_count.working)
        && (test_gmf_task3_count.working <= test_gmf_task4_count.working)) {
        // OK
        TEST_ASSERT_TRUE(true);
    } else {
        TEST_ASSERT_FALSE(true);
    }

}


TEST_CASE("Stopped by stop API", "ESP_GMF_TASK")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    clear_test_gmf_task_count();
    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t hd = NULL;
    ESP_GMF_MEM_SHOW(TAG);
    esp_gmf_task_init(&cfg, &hd);

    esp_gmf_task_set_event_func(hd, esp_gmf_task_evt, NULL);

    esp_gmf_task_register_ready_job(hd, NULL, prepare1, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare2, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare3, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare4, ESP_GMF_JOB_TIMES_ONCE, NULL, false);

    esp_gmf_task_register_ready_job(hd, NULL, working1, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working2, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working3, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working4, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_run(hd));
    vTaskDelay(200 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_pause(hd));
    vTaskDelay(200 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_resume(hd));
    vTaskDelay(200 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_stop(hd));

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(hd));
    ESP_GMF_MEM_SHOW(TAG);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.cleanup);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.prepare);

    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task1_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task2_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task3_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task4_count.working);

    ESP_LOGI(TAG, "task1: %d, task2: %d, task3: %d, task4: %d", test_gmf_task1_count.working,
        test_gmf_task2_count.working, test_gmf_task3_count.working, test_gmf_task4_count.working);
    // if ((test_gmf_task1_count.working == test_gmf_task2_count.working)
    //     && (test_gmf_task2_count.working > test_gmf_task3_count.working)
    //     && (test_gmf_task3_count.working >= test_gmf_task4_count.working)) {
    //     // OK
    //     TEST_ASSERT_TRUE(true);
    // } else {
    //     TEST_ASSERT_FALSE(true);
    // }
}

TEST_CASE("Return error on the PREPARE stage", "ESP_GMF_TASK")
{
    esp_log_level_set("*", ESP_LOG_INFO);

    clear_test_gmf_task_count();

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t hd = NULL;

    esp_gmf_task_init(&cfg, &hd);
    esp_gmf_task_set_event_func(hd, esp_gmf_task_evt, NULL);

    esp_gmf_task_register_ready_job(hd, NULL, prepare1, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare2, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare3, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare4, ESP_GMF_JOB_TIMES_ONCE, NULL, false);

    esp_gmf_task_register_ready_job(hd, NULL, working1, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working2, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working3, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working4, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);

    test_gmf_task3_count.prepare_return = ESP_GMF_JOB_ERR_FAIL;
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_run(hd));

    vTaskDelay(200 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_pause(hd));
    vTaskDelay(100 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_NOT_SUPPORT, esp_gmf_task_resume(hd));
    vTaskDelay(200 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_stop(hd));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(hd));

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.cleanup);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.prepare);
    TEST_ASSERT_EQUAL(0, test_gmf_task4_count.prepare);

    TEST_ASSERT_EQUAL(0, test_gmf_task1_count.working);
    TEST_ASSERT_EQUAL(0, test_gmf_task2_count.working);
    TEST_ASSERT_EQUAL(0, test_gmf_task3_count.working);
    TEST_ASSERT_EQUAL(0, test_gmf_task4_count.working);

    ESP_LOGI(TAG, "task1: %d, task2: %d, task3: %d, task4: %d", test_gmf_task1_count.working,
        test_gmf_task2_count.working, test_gmf_task3_count.working, test_gmf_task4_count.working);
    ESP_GMF_MEM_SHOW(TAG);
}

TEST_CASE("Return error on the WORKING stage", "ESP_GMF_TASK")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    // esp_log_level_set("ESP_GMF_TASK", ESP_LOG_DEBUG);
    clear_test_gmf_task_count();
    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t hd = NULL;

    esp_gmf_task_init(&cfg, &hd);
    esp_gmf_task_set_event_func(hd, esp_gmf_task_evt, NULL);

    esp_gmf_task_register_ready_job(hd, NULL, prepare1, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare2, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare3, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare4, ESP_GMF_JOB_TIMES_ONCE, NULL, false);

    esp_gmf_task_register_ready_job(hd, NULL, working1, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working2, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working3, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working4, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_run(hd));
    vTaskDelay(200 / portTICK_PERIOD_MS);

    test_gmf_task2_count.working_return = ESP_GMF_JOB_ERR_FAIL;

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_pause(hd));
    vTaskDelay(100 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_NOT_SUPPORT, esp_gmf_task_resume(hd));
    vTaskDelay(200 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_stop(hd));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(hd));


    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.cleanup);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.prepare);

    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task1_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task2_count.working);
    TEST_ASSERT_EQUAL(0, test_gmf_task3_count.working);
    TEST_ASSERT_EQUAL(0, test_gmf_task4_count.working);

    ESP_LOGI(TAG, "task1: %d, task2: %d, task3: %d, task4: %d", test_gmf_task1_count.working,
        test_gmf_task2_count.working, test_gmf_task3_count.working, test_gmf_task4_count.working);

    ESP_GMF_MEM_SHOW(TAG);
}

TEST_CASE("Return error on the CLEANUP stage", "ESP_GMF_TASK")
{
    esp_log_level_set("*", ESP_LOG_INFO);

    clear_test_gmf_task_count();
    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t hd = NULL;

    esp_gmf_task_init(&cfg, &hd);
    esp_gmf_task_set_event_func(hd, esp_gmf_task_evt, NULL);

    esp_gmf_task_register_ready_job(hd, NULL, prepare1, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare2, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare3, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare4, ESP_GMF_JOB_TIMES_ONCE, NULL, false);

    esp_gmf_task_register_ready_job(hd, NULL, working1, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working2, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working3, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working4, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_run(hd));
    vTaskDelay(200 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_pause(hd));
    vTaskDelay(100 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_resume(hd));

    test_gmf_task2_count.cleanup_return = ESP_GMF_JOB_ERR_FAIL;
    vTaskDelay(200 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_stop(hd));
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(hd));

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.cleanup);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.prepare);

    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task1_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task2_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task3_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task4_count.working);

    ESP_LOGI(TAG, "task1: %d, task2: %d, task3: %d, task4: %d", test_gmf_task1_count.working,
        test_gmf_task2_count.working, test_gmf_task3_count.working, test_gmf_task4_count.working);
    if ((test_gmf_task1_count.working > test_gmf_task2_count.working)
        && (test_gmf_task2_count.working == test_gmf_task3_count.working)
        && (test_gmf_task3_count.working == test_gmf_task4_count.working)) {
        // OK
        TEST_ASSERT_TRUE(true);
    } else {
        TEST_ASSERT_FALSE(true);
    }

    ESP_GMF_MEM_SHOW(TAG);
}

TEST_CASE("Return error after call STOP", "ESP_GMF_TASK")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    // esp_log_level_set("ESP_GMF_TASK", ESP_LOG_DEBUG);
    clear_test_gmf_task_count();
    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    esp_gmf_task_handle_t hd = NULL;

    esp_gmf_task_init(&cfg, &hd);
    esp_gmf_task_set_event_func(hd, esp_gmf_task_evt, NULL);

    esp_gmf_task_register_ready_job(hd, NULL, prepare1, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare2, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare3, ESP_GMF_JOB_TIMES_ONCE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, prepare4, ESP_GMF_JOB_TIMES_ONCE, NULL, false);

    esp_gmf_task_register_ready_job(hd, NULL, working1, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working2, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working3, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);
    esp_gmf_task_register_ready_job(hd, NULL, working4, ESP_GMF_JOB_TIMES_INFINITE, NULL, false);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_run(hd));
    vTaskDelay(200 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_pause(hd));
    vTaskDelay(100 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_resume(hd));
    vTaskDelay(200 / portTICK_PERIOD_MS);

    test_gmf_task2_count.cleanup_return = ESP_GMF_JOB_ERR_FAIL;
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_stop(hd));

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_task_deinit(hd));

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.cleanup);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.cleanup);

    TEST_ASSERT_EQUAL(1, test_gmf_task1_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task2_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task3_count.prepare);
    TEST_ASSERT_EQUAL(1, test_gmf_task4_count.prepare);

    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task1_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task2_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task3_count.working);
    TEST_ASSERT_NOT_EQUAL(0, test_gmf_task4_count.working);

    ESP_LOGI(TAG, "task1: %d, task2: %d, task3: %d, task4: %d", test_gmf_task1_count.working,
        test_gmf_task2_count.working, test_gmf_task3_count.working, test_gmf_task4_count.working);
    // if ((test_gmf_task1_count.working > test_gmf_task2_count.working)
    //     && (test_gmf_task2_count.working > test_gmf_task3_count.working)
    //     && (test_gmf_task3_count.working >= test_gmf_task4_count.working)) {
    //     // OK
    //     TEST_ASSERT_TRUE(true);
    // } else {
    //     TEST_ASSERT_FALSE(true);
    // }

    ESP_GMF_MEM_SHOW(TAG);
}
