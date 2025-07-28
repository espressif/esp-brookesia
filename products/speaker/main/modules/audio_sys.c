/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <sys/time.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_sys.h"
#include "soc/soc_memory_layout.h"

static const char *TAG = "AUDIO_SYS";

#define ARRAY_SIZE_OFFSET                   8   // Increase this if audio_sys_get_real_time_stats returns ESP_ERR_INVALID_SIZE
#define AUDIO_SYS_TASKS_ELAPSED_TIME_MS  1000   // Period of stats measurement

#define audio_malloc    malloc
#define audio_free      free
#define AUDIO_MEM_CHECK(tag, ptr, check) if (ptr == NULL) { ESP_LOGE(tag, "Failed to allocate memory"); check; }

const char *task_state[] = {
    "Running",
    "Ready",
    "Blocked",
    "Suspended",
    "Deleted"
};

/** @brief
 * "Extr": Allocated task stack from psram, "Intr": Allocated task stack from internal
 */
const char *task_stack[] = {"Extr", "Intr"};

int audio_sys_get_tick_by_time_ms(int ms)
{
    return (ms / portTICK_PERIOD_MS);
}

int64_t audio_sys_get_time_ms(void)
{
    struct timeval te;
    gettimeofday(&te, NULL);
    int64_t milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    return milliseconds;
}

esp_err_t audio_sys_get_real_time_stats(void)
{
#if (CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID && CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS)
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    uint32_t start_run_time, end_run_time;
    uint32_t task_elapsed_time, percentage_time;
    esp_err_t ret;

    // 分配数组存储当前任务状态
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = audio_malloc(sizeof(TaskStatus_t) * start_array_size);
    AUDIO_MEM_CHECK(TAG, start_array, {
        ret = ESP_FAIL;
        goto exit;
    });
    // 获取当前任务状态
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ESP_LOGE(TAG, "Insufficient array size for uxTaskGetSystemState. Trying increasing ARRAY_SIZE_OFFSET");
        ret = ESP_FAIL;
        goto exit;
    }

    vTaskDelay(pdMS_TO_TICKS(AUDIO_SYS_TASKS_ELAPSED_TIME_MS));

    // 分配数组存储延迟后的任务状态
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = audio_malloc(sizeof(TaskStatus_t) * end_array_size);
    AUDIO_MEM_CHECK(TAG, start_array, {
        ret = ESP_FAIL;
        goto exit;
    });

    // 获取延迟后的任务状态
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ESP_LOGE(TAG, "Insufficient array size for uxTaskGetSystemState. Trying increasing ARRAY_SIZE_OFFSET");
        ret = ESP_FAIL;
        goto exit;
    }

    // 计算总运行时间
    uint32_t total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ESP_LOGE(TAG, "Delay duration too short. Trying increasing AUDIO_SYS_TASKS_ELAPSED_TIME_MS");
        ret = ESP_FAIL;
        goto exit;
    }

    // 创建一个结构体来存储任务信息和计算的百分比时间
    typedef struct {
        TaskStatus_t task;
        uint32_t elapsed_time;
        uint32_t percentage;
        BaseType_t core_id;
        bool is_deleted;
        bool is_created;
    } TaskInfoExt_t;

    TaskInfoExt_t *task_info = audio_malloc(sizeof(TaskInfoExt_t) * (start_array_size + end_array_size));
    AUDIO_MEM_CHECK(TAG, task_info, {
        ret = ESP_FAIL;
        goto exit;
    });

    int task_count = 0;

    // 匹配开始和结束数组中的任务
    for (int i = 0; i < start_array_size; i++) {
        bool found = false;
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                task_elapsed_time = end_array[j].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
                percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * portNUM_PROCESSORS);

                task_info[task_count].task = start_array[i];
                task_info[task_count].elapsed_time = task_elapsed_time;
                task_info[task_count].percentage = percentage_time;
                task_info[task_count].core_id = start_array[i].xCoreID;
                task_info[task_count].is_deleted = false;
                task_info[task_count].is_created = false;

                task_count++;

                // 标记任务已匹配
                start_array[i].xHandle = NULL;
                end_array[j].xHandle = NULL;
                found = true;
                break;
            }
        }

        // 处理已删除的任务
        if (!found && start_array[i].xHandle != NULL) {
            task_info[task_count].task = start_array[i];
            task_info[task_count].elapsed_time = 0;
            task_info[task_count].percentage = 0;
            task_info[task_count].core_id = start_array[i].xCoreID;
            task_info[task_count].is_deleted = true;
            task_info[task_count].is_created = false;
            task_count++;
        }
    }

    // 处理新创建的任务
    for (int i = 0; i < end_array_size; i++) {
        if (end_array[i].xHandle != NULL) {
            task_info[task_count].task = end_array[i];
            task_info[task_count].elapsed_time = 0;
            task_info[task_count].percentage = 0;
            task_info[task_count].core_id = end_array[i].xCoreID;
            task_info[task_count].is_deleted = false;
            task_info[task_count].is_created = true;
            task_count++;
        }
    }

    // 首先按照CoreId排序，然后按照百分比时间降序排序
    for (int i = 0; i < task_count - 1; i++) {
        for (int j = 0; j < task_count - i - 1; j++) {
            // 首先按CoreId排序
            if (task_info[j].core_id > task_info[j + 1].core_id) {
                TaskInfoExt_t temp = task_info[j];
                task_info[j] = task_info[j + 1];
                task_info[j + 1] = temp;
            }
            // 如果CoreId相同，则按百分比降序排序
            else if (task_info[j].core_id == task_info[j + 1].core_id &&
                     task_info[j].percentage < task_info[j + 1].percentage) {
                TaskInfoExt_t temp = task_info[j];
                task_info[j] = task_info[j + 1];
                task_info[j + 1] = temp;
            }
        }
    }

    ESP_LOGI(TAG, "| Task              | Run Time    | Per | Prio | HWM       | State   | CoreId   | Stack ");

    // 打印排序后的任务信息
    for (int i = 0; i < task_count; i++) {
        if (task_info[i].is_deleted) {
            ESP_LOGI(TAG, "| %s | Deleted", task_info[i].task.pcTaskName);
        } else if (task_info[i].is_created) {
            ESP_LOGI(TAG, "| %s | Created", task_info[i].task.pcTaskName);
        } else {
            ESP_LOGI(TAG, "| %-17s | %-11d |%2d%%  | %-4u | %-9u | %-7s | %-8x | %s",
                     task_info[i].task.pcTaskName, (int)task_info[i].elapsed_time, (int)task_info[i].percentage,
                     task_info[i].task.uxCurrentPriority, (int)task_info[i].task.usStackHighWaterMark,
                     task_state[(task_info[i].task.eCurrentState)], task_info[i].core_id,
                     task_stack[esp_ptr_internal(pxTaskGetStackStart(task_info[i].task.xHandle))]);
        }
    }

    printf("\n");
    ret = ESP_OK;

    if (task_info) {
        audio_free(task_info);
        task_info = NULL;
    }

exit:    // 通用返回路径
    if (start_array) {
        audio_free(start_array);
        start_array = NULL;
    }
    if (end_array) {
        audio_free(end_array);
        end_array = NULL;
    }
    return ret;
#else
    ESP_LOGW(TAG, "请在menuconfig中启用 `CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID` 和 `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS`");
    return ESP_FAIL;
#endif
}
