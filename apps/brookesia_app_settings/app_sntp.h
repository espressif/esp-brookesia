/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifndef ESP_BROOKESIA_APP_SPEAKER_SETTINGS_SIMULATOR

#ifdef __cplusplus
extern "C" {
#endif

bool app_sntp_init(void);

bool app_sntp_start(void);

bool app_sntp_is_time_synced(void);

#ifdef __cplusplus
}
#endif

#endif
