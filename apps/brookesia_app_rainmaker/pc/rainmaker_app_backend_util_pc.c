/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rainmaker_app_backend_util.h"
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

void rainmaker_app_backend_util_safe_free(void *p)
{
    free(p);
}

void rainmaker_app_backend_util_safe_free_and_set_null(void **p)
{
    if (p && *p) {
        free(*p);
        *p = NULL;
    }
}

esp_err_t rainmaker_app_backend_util_nvs_init(void)
{
    return ESP_OK;
}

esp_err_t rainmaker_app_backend_util_nvs_read_str(const char *key, char **out_str)
{
    (void)key;
    (void)out_str;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t rainmaker_app_backend_util_nvs_write_str(const char *key, const char *val)
{
    (void)key;
    (void)val;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t rainmaker_app_backend_util_nvs_erase_key(const char *key)
{
    (void)key;
    return ESP_ERR_NOT_SUPPORTED;
}

void rainmaker_app_backend_util_vTaskDelay(uint32_t ms)
{
    if (ms == 0) {
        return;
    }
    usleep((useconds_t)ms * 1000U);
}

const char *rainmaker_app_backend_util_get_tz_posix_str(const char *tz_name)
{
    (void)tz_name;
    return NULL;
}

void rainmaker_app_backend_util_get_tz_db(const rainmaker_app_backend_util_tz_db_pair_t **out_db, size_t *out_count)
{
    if (out_db) {
        *out_db = NULL;
    }
    if (out_count) {
        *out_count = 0;
    }
}
