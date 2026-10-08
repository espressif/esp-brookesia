/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>

using esp_gsp_err_t = int;

constexpr int ESP_GSP_OK = 0;
constexpr int ESP_GSP_FAIL = -1;
constexpr int ESP_GSP_ERR_INVALID_SIZE = 0x104;
constexpr int ESP_GSP_ERR_NOT_FOUND = 0x105;
constexpr int ESP_GSP_ERR_TIMEOUT = 0x107;

struct esp_gsp_config_t {
    const void *bundle = nullptr;
    size_t bundle_size = 0;
    const void *directories = nullptr;
    const void *ttf = nullptr;
    size_t ttf_size = 0;
};

struct esp_gsp_deployable_info_t {
    uint16_t scene_count = 0;
};

struct esp_gsp_deployable_bundle {
    const void *data;
    size_t size;
};
using esp_gsp_deployable_bundle_t = esp_gsp_deployable_bundle;

#define ESP_GSP_CONFIG_INIT() esp_gsp_config_t {}

#ifdef __cplusplus
extern "C" {
#endif

esp_gsp_err_t esp_gsp_deployable_bundle_open(
    const void *data, size_t size, bool verify_crc, esp_gsp_deployable_bundle_t **out_bundle
);
esp_gsp_err_t esp_gsp_deployable_bundle_get_info(
    const esp_gsp_deployable_bundle_t *bundle, esp_gsp_deployable_info_t *out_info
);
esp_gsp_err_t esp_gsp_deployable_bundle_make_config(
    const esp_gsp_deployable_bundle_t *bundle, esp_gsp_config_t *out_config
);
esp_gsp_err_t esp_gsp_deployable_bundle_action_key(
    const esp_gsp_deployable_bundle_t *bundle, uint16_t scene_id, uint16_t action_id, uint32_t *out_key
);
void esp_gsp_deployable_bundle_close(esp_gsp_deployable_bundle_t *bundle);

#ifdef __cplusplus
}
#endif
