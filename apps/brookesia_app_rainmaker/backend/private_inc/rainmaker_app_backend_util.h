/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t rainmaker_app_backend_util_nvs_init(void);
void rainmaker_app_backend_util_safe_free_and_set_null(void **p);
void rainmaker_app_backend_util_safe_free(void *p);
esp_err_t rainmaker_app_backend_util_nvs_read_str(const char *key, char **out_str);
esp_err_t rainmaker_app_backend_util_nvs_write_str(const char *key, const char *val);
esp_err_t rainmaker_app_backend_util_nvs_erase_key(const char *key);
void rainmaker_app_backend_util_vTaskDelay(uint32_t ms);
typedef struct {
    const char *name;       /* TZ region string, e.g. "Asia/Shanghai" */
    const char *posix_str;  /* TZ-POSIX string, e.g. "CST-8" */
} rainmaker_app_backend_util_tz_db_pair_t;

const char *rainmaker_app_backend_util_get_tz_posix_str(const char *tz_name);
void rainmaker_app_backend_util_get_tz_db(const rainmaker_app_backend_util_tz_db_pair_t **out_db, size_t *out_count);
#ifdef __cplusplus
}
#endif /* __cplusplus */
