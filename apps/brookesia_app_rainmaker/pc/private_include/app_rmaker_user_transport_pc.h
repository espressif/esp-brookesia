/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include "app_rmaker_user_api.h"

/**
 * PC/libcurl transport: mirrors app_rmaker_user_transport_esp_execute() contract.
 *
 * @param persist_curl  Optional persistent CURL handle for reuse_session (may be NULL).
 */
esp_err_t app_rmaker_user_transport_pc_execute(void **persist_curl, const char *url,
        app_rmaker_user_api_request_config_t *request_config,
        const char *access_token, bool need_authorize, int *status_code,
        char **response_data, bool *out_recoverable);

/**
 * Cleanup persistent PC transport session handle.
 */
void app_rmaker_user_transport_pc_cleanup(void **persist_curl);
