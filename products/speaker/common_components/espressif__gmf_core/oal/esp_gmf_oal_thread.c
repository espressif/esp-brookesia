/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_thread.h"
#include "esp_memory_utils.h"

static const char *TAG = "ESP_GMF_THREAD";

esp_gmf_err_t esp_gmf_oal_thread_create(esp_gmf_oal_thread_t *p_handle, const char *name, void (*main_func)(void *arg), void *arg,
                                        uint32_t stack, int prio, bool stack_in_ext, int core_id)
{

    TaskHandle_t task_handle = NULL;

    // When only support one core force no affinity
 #if CONFIG_FREERTOS_UNICORE
    core_id = tskNO_AFFINITY;
#endif

    BaseType_t ret;
    if (stack_in_ext && esp_gmf_oal_mem_spiram_stack_is_enabled()) {
       ret = xTaskCreatePinnedToCoreWithCaps(main_func, name, stack, arg, prio, &task_handle,
                                                         core_id, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    } else {
        if (stack_in_ext) {
            ESP_LOGW(TAG, "Make sure selected the `CONFIG_SPIRAM_BOOT_INIT` and `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` by `make menuconfig`");
        }
        ret = xTaskCreatePinnedToCore(main_func, name, stack, arg, prio, &task_handle, core_id);
    }
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Error create task  %s in %s", name, stack_in_ext ? "PSRAM" : "RAM");
        return ESP_GMF_ERR_FAIL;
    }
    if (p_handle) {
        *p_handle = task_handle;
    }
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_oal_thread_delete(esp_gmf_oal_thread_t p_handle)
{
    ESP_GMF_NULL_CHECK(TAG, p_handle, return ESP_GMF_ERR_INVALID_ARG);
    TaskHandle_t task_handle = (TaskHandle_t) p_handle;
    uint8_t *task_stack = pxTaskGetStackStart(task_handle);
    ESP_GMF_NULL_CHECK(TAG, task_stack, return ESP_GMF_ERR_INVALID_ARG);
    if (esp_ptr_internal(task_stack) == false) {
        vTaskDeleteWithCaps(task_handle);
    } else {
        vTaskDelete(task_handle);
    }
    return ESP_GMF_ERR_OK;  /* Control never reach here if this is self delete */
}
