/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <sys/time.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_sys.h"
#include "esp_memory_utils.h"

static const char *TAG = "ESP_GMF_OAL_SYS";

#define ARRAY_SIZE_OFFSET               8     // Increase this if esp_gmf_oal_sys_get_real_time_stats returns ESP_GMF_ERR_NOT_ENOUGH

#ifndef configRUN_TIME_COUNTER_TYPE
#define configRUN_TIME_COUNTER_TYPE uint32_t
#endif

const char *task_state[] = {
    "Running",
    "Ready",
    "Blocked",
    "Suspended",
    "Deleted",
    "Invalid state"
};

/** @brief  Task stack location
 *       - "Extr": Allocated task stack from psram
 *       - "Intr": Allocated task stack from internal
 */
const char *task_stack[] = {"Extr", "Intr"};

int esp_gmf_oal_sys_get_tick_by_time_ms(int ms)
{
    return (ms / portTICK_PERIOD_MS);
}

int64_t esp_gmf_oal_sys_get_time_ms(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    int64_t milliseconds = t.tv_sec * 1000LL + t.tv_usec / 1000;
    return milliseconds;
}

#if (CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID && CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS)
static TaskStatus_t *matched_status;

static esp_gmf_err_t esp_gmf_get_cur_task_status(TaskStatus_t **status, UBaseType_t *num, configRUN_TIME_COUNTER_TYPE *cur_time)
{
    // Reserve some space for task created during get state running
    *num = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    TaskStatus_t *task_status = esp_gmf_oal_malloc(sizeof(TaskStatus_t) * (*num));
    if (task_status == NULL) {
        return ESP_GMF_ERR_MEMORY_LACK;
    }
    *num = uxTaskGetSystemState(task_status, *num, cur_time);
    if (*num == 0) {
        ESP_LOGE(TAG, "Insufficient array size for uxTaskGetSystemState. Trying increasing ARRAY_SIZE_OFFSET");
        esp_gmf_oal_free(task_status);
        return ESP_GMF_ERR_NOT_ENOUGH;
    }
    *status = task_status;
    return ESP_GMF_ERR_OK;
}

static int compare_tasks(const void *a, const void *b)
{
    const TaskStatus_t *task_a = &matched_status[*(uint8_t *)a];
    const TaskStatus_t *task_b = &matched_status[*(uint8_t *)b];

    if (task_a->xCoreID != task_b->xCoreID) {
        return task_a->xCoreID - task_b->xCoreID;
    }
    return (task_b->ulRunTimeCounter > task_a->ulRunTimeCounter) ? 1 : -1;
}

esp_gmf_err_t esp_gmf_oal_sys_get_real_time_stats(int elapsed_time_ms, bool markdown)
{
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    uint8_t *matched_arr = NULL;
    UBaseType_t start_array_size = 0, end_array_size = 0;
    configRUN_TIME_COUNTER_TYPE start_run_time, end_run_time;

    esp_gmf_err_t ret = esp_gmf_get_cur_task_status(&start_array, &start_array_size, &start_run_time);
    ESP_GMF_MEM_CHECK(TAG, start_array, { goto exit; });

    // Delay some time to get cpu usage
    vTaskDelay(pdMS_TO_TICKS(elapsed_time_ms));

    ret = esp_gmf_get_cur_task_status(&end_array, &end_array_size, &end_run_time);
    ESP_GMF_MEM_CHECK(TAG, end_array, { goto exit; });

    // Match each task in start_array to those in the end_array
    int min_count = start_array_size < end_array_size ? start_array_size : end_array_size;
    matched_arr = malloc(min_count * sizeof(uint8_t));
    ESP_GMF_MEM_CHECK(TAG, matched_arr, { goto exit; });

    uint8_t matched_count = 0;
    for (int i = 0; i < start_array_size; i++) {
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle != end_array[j].xHandle) {
                continue;
            }
            matched_arr[matched_count++] = j;
            start_array[i].xHandle = NULL;
            end_array[j].xHandle = NULL;
            end_array[j].ulRunTimeCounter -= start_array[i].ulRunTimeCounter;
            break;
        }
    }

    // Sort tasks by core ID and CPU usage
    matched_status = end_array;
    qsort(matched_arr, matched_count, sizeof(uint8_t), compare_tasks);
    if (markdown) {
        printf("|       Task        |  Core ID |  Run Time   |  CPU    | Priority | Stack HWM |   State    | Stack |\n");
        printf("|-------------------|----------|-------------|---------|----------|-----------|------------|-------|\n");
    } else {
        ESP_LOGI("", "┌───────────────────┬──────────┬─────────────┬─────────┬──────────┬───────────┬────────────┬───────┐");
        ESP_LOGI("", "│ Task              │ Core ID  │ Run Time    │ CPU     │ Priority │ Stack HWM │ State      │ Stack │");
    }
    BaseType_t current_core = -1;
    // Calculate total_elapsed_time in units of run time stats clock period.
    int total_elapsed_time = (int)(end_run_time - start_run_time) * portNUM_PROCESSORS;
    for (int i = 0; i < matched_count; i++) {
        TaskStatus_t *cur = &matched_status[matched_arr[i]];
        float percentage_time = ((float)cur->ulRunTimeCounter * 100UL) / total_elapsed_time;

        if (current_core != cur->xCoreID && markdown == false) {
            current_core = cur->xCoreID;
            ESP_LOGI("", "├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤");
        }
        if (markdown) {
            printf("| %-17s | %-8x | %-11d | %6.2f%% | %-8u | %-9u | %-10s | %-5s |\n",
                cur->pcTaskName,
                cur->xCoreID,
                (int)cur->ulRunTimeCounter,
                percentage_time,
                cur->uxCurrentPriority,
                (int)cur->usStackHighWaterMark,
                task_state[(cur->eCurrentState)],
                task_stack[esp_ptr_internal(pxTaskGetStackStart(cur->xHandle))]);
        } else {
            ESP_LOGI("", "│ %-17s │ %-8x │ %-11d │ %6.2f%% │ %-8u │ %-9u │ %-10s │ %-5s │",
                cur->pcTaskName,
                cur->xCoreID,
                (int)cur->ulRunTimeCounter,
                percentage_time,
                cur->uxCurrentPriority,
                (int)cur->usStackHighWaterMark,
                task_state[(cur->eCurrentState)],
                task_stack[esp_ptr_internal(pxTaskGetStackStart(cur->xHandle))]);
        }
    }
    if (markdown == false) {
        ESP_LOGI("", "└───────────────────┴──────────┴─────────────┴─────────┴──────────┴───────────┴────────────┴───────┘");
    }
    if (matched_count != start_array_size) {
        ESP_LOGI("", "Deleted Tasks:");
        for (int i = 0; i < start_array_size; i++) {
            if (start_array[i].xHandle != NULL) {
                ESP_LOGI(TAG, "    %s", start_array[i].pcTaskName);
            }
        }
    }
    if (matched_count != end_array_size) {
        ESP_LOGI("", "Created Tasks:");
        for (int i = 0; i < end_array_size; i++) {
            if (end_array[i].xHandle != NULL) {
                ESP_LOGI("", "    %s", end_array[i].pcTaskName);
            }
        }
    }
    ret = ESP_GMF_ERR_OK;

exit:  // Common return path
    if (matched_arr) {
        esp_gmf_oal_free(matched_arr);
    }
    if (start_array) {
        esp_gmf_oal_free(start_array);
        start_array = NULL;
    }
    if (end_array) {
        esp_gmf_oal_free(end_array);
        end_array = NULL;
    }
    return ret;
}

#else

esp_gmf_err_t esp_gmf_oal_sys_get_real_time_stats(int elapsed_time_ms, bool markdown)
{
    ESP_LOGW(TAG, "Please enable `CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID` and `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` in menuconfig");
    return ESP_GMF_ERR_FAIL;
}

#endif
