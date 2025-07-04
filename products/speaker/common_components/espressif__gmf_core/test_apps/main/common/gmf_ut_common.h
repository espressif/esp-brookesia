/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/sdmmc_host.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

int verify_two_files(const char *src_path, const char *dest_path);

void esp_gmf_ut_setup_sdmmc(sdmmc_card_t **out_card);

void esp_gmf_ut_teardown_sdmmc(sdmmc_card_t *card);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
