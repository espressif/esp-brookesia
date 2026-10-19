/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_codec_dev_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void shared_codec_reset(void);
int shared_codec_open(bool playback, uint8_t bits, uint8_t channels, uint32_t rate);
void shared_codec_close(bool playback);
int shared_codec_format_rejections(void);
int shared_codec_last_format_result(void);
esp_codec_dev_sample_info_t shared_codec_driver_format(bool playback);

#ifdef __cplusplus
}
#endif
