/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_idf_version.h"

/* ESP-IDF 6.0.2 stores dma2d_transfer_ability_t::data_burst_length as a byte
 * count. esp-gsp 1.4.0 still names the pre-6.1 enumerator on that SDK. Supply
 * the missing name at compile time instead of editing the downloaded source.
 */
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 1, 0)
#   ifndef DMA2D_DATA_BURST_LENGTH_16
#       define DMA2D_DATA_BURST_LENGTH_16 16
#   endif
#endif
