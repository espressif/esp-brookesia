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

enum gsp_component_prop_kind_t { GSP_COMPONENT_PROP_VALUE, GSP_COMPONENT_PROP_TEXT };
enum gsp_value_type_t { GSP_VALUE_BOOL, GSP_VALUE_I32, GSP_VALUE_U32, GSP_VALUE_COLOR };
enum gsp_property_storage_t { GSP_PROPERTY_STORAGE_I32, GSP_PROPERTY_STORAGE_TEXT, GSP_PROPERTY_STORAGE_RESOURCE };
enum gsp_property_codec_t { GSP_PROPERTY_CODEC_DIRECT };
struct gsp_component_prop_t {
    uint8_t prop = 0;
    uint8_t value_type = 0;
    uint8_t storage = 0;
    uint8_t codec = 0;
    uint32_t key = 0;
    uint32_t min_value = 0;
    uint32_t max_value = 0;
};
struct gsp_component_entry_t {
    uint32_t key = 0;
    uint8_t prop_count = 0;
    const gsp_component_prop_t *props = nullptr;
};
struct gsp_component_directory_t {
    const gsp_component_entry_t *entries = nullptr;
    uint16_t count = 0;
};
struct gsp_property_info_t {
    uint32_t key = 0;
    gsp_value_type_t value_type = GSP_VALUE_BOOL;
    gsp_property_storage_t storage = GSP_PROPERTY_STORAGE_I32;
    gsp_property_codec_t codec = GSP_PROPERTY_CODEC_DIRECT;
    int64_t min_value = 0;
    int64_t max_value = 0;
};

struct esp_gsp_config_t {
    const void *bundle = nullptr;
    size_t bundle_size = 0;
    const gsp_component_directory_t *const *directories = nullptr;
    uint16_t directory_count = 0;
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
