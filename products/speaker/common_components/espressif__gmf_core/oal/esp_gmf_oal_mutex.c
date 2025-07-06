/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_gmf_oal_mutex.h"
#include "esp_log.h"

void *esp_gmf_oal_mutex_create(void)
{
    void *handle = NULL;
    handle = xSemaphoreCreateMutex();
    return (void *)handle;
}

int esp_gmf_oal_mutex_destroy(void *mutex)
{
    vSemaphoreDelete((QueueHandle_t)mutex);
    return 0;
}

int esp_gmf_oal_mutex_lock(void *mutex)
{
    while (xSemaphoreTake((QueueHandle_t)mutex, portMAX_DELAY) != pdPASS);
    return 0;
}

int esp_gmf_oal_mutex_unlock(void *mutex)
{
    int ret = 0;
    ret = xSemaphoreGive((QueueHandle_t)mutex);
    return ret;
}
