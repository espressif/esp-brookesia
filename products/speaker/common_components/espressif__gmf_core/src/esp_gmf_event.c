/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "stdlib.h"
#include "esp_gmf_event.h"

static const char *event_state_string[] = {
    "ESP_GMF_EVENT_STATE_NONE",
    "ESP_GMF_EVENT_STATE_INITIALIZED",
    "ESP_GMF_EVENT_STATE_OPENING",
    "ESP_GMF_EVENT_STATE_RUNNING",
    "ESP_GMF_EVENT_STATE_PAUSED",
    "ESP_GMF_EVENT_STATE_STOPPED",
    "ESP_GMF_EVENT_STATE_FINISHED",
    "ESP_GMF_EVENT_STATE_ERROR",
    "",
};

const char *esp_gmf_event_get_state_str(esp_gmf_event_state_t st)
{
    return event_state_string[st];
}
