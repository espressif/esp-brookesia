/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_iot_board.h"
#include "esp_iot_settings.h"

#include <esp_log.h>
#include <esp_random.h>
#include <esp_flash.h>
#include <esp_system.h>
#include <esp_check.h>
#if CONFIG_IDF_TARGET_ESP32P4
#include <esp_wifi.h>
#endif
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_chip_info.h>
#include <esp_partition.h>
#include <esp_app_desc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cJSON.h>

static const char *TAG = "esp_iot_board";

// Language constants (matching the auto-generated lang_config.h)
#define LANG_CODE "zh-CN"  // This should be updated based on the actual language configuration

// Helper function to safely copy string
static esp_err_t safe_strncpy(char *dest, const char *src, size_t dest_size)
{
    if (dest == NULL || src == NULL || dest_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
    return ESP_OK;
}

esp_err_t board_init(board_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    settings_handle_t settings;
    esp_err_t ret = ESP_OK;

    memset(handle, 0, sizeof(board_handle_t));

    ret = settings_init(&settings, "board", true);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to initialize settings: %s", esp_err_to_name(ret));

    char uuid_str[64] = {0};
    ret = settings_get_string(&settings, "uuid", "", uuid_str, sizeof(uuid_str));
    if (ret == ESP_OK && strlen(uuid_str) > 0) {
        handle->uuid = malloc(strlen(uuid_str) + 1);
        if (handle->uuid == NULL) {
            settings_deinit(&settings);
            return ESP_ERR_NO_MEM;
        }
        strcpy(handle->uuid, uuid_str);
    } else {
        char new_uuid[37] = {0};
        uint8_t uuid[16] = {0};

        esp_fill_random(uuid, sizeof(uuid));
        uuid[6] = (uuid[6] & 0x0F) | 0x40;
        uuid[8] = (uuid[8] & 0x3F) | 0x80;

        snprintf(new_uuid, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 uuid[0], uuid[1], uuid[2], uuid[3],
                 uuid[4], uuid[5], uuid[6], uuid[7],
                 uuid[8], uuid[9], uuid[10], uuid[11],
                 uuid[12], uuid[13], uuid[14], uuid[15]);

        ret = settings_set_string(&settings, "uuid", new_uuid);
        if (ret != ESP_OK) {
            settings_deinit(&settings);
            return ret;
        }

        handle->uuid = malloc(strlen(new_uuid) + 1);
        if (handle->uuid == NULL) {
            settings_deinit(&settings);
            return ESP_ERR_NO_MEM;
        }
        strcpy(handle->uuid, new_uuid);
    }

    settings_commit(&settings);
    settings_deinit(&settings);

    handle->initialized = true;
    ESP_LOGI(TAG, "Board initialized with UUID: %s", handle->uuid);

    return ESP_OK;
}

void board_cleanup(board_handle_t *handle)
{
    if (handle == NULL) {
        return;
    }

    if (handle->uuid != NULL) {
        free(handle->uuid);
        handle->uuid = NULL;
    }

    handle->initialized = false;
}

esp_err_t board_get_uuid(board_handle_t *handle, char *uuid_buffer, size_t buffer_size)
{
    if (handle == NULL || uuid_buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->initialized || handle->uuid == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return safe_strncpy(uuid_buffer, handle->uuid, buffer_size);
}

esp_err_t board_get_mac_address(char *mac_buffer)
{
    if (mac_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    uint8_t mac[6] = {0};
#if CONFIG_IDF_TARGET_ESP32P4
    ret = esp_wifi_get_mac(WIFI_IF_STA, mac);
#else
    ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
#endif

    if (ret == ESP_OK) {
        snprintf(mac_buffer, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    return ret;
}

esp_err_t board_get_json(board_handle_t *handle, char *json_buffer, size_t buffer_size)
{
    if (handle == NULL || json_buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    uint32_t flash_size = 0;
    char mac_str[18] = {0};
    esp_flash_get_size(NULL, &flash_size);
    board_get_mac_address(mac_str);

    cJSON_AddNumberToObject(root, "version", 2);
    cJSON_AddStringToObject(root, "language", LANG_CODE);
    cJSON_AddNumberToObject(root, "flash_size", flash_size);
    cJSON_AddNumberToObject(root, "minimum_free_heap_size", esp_get_minimum_free_heap_size());
    cJSON_AddStringToObject(root, "mac_address", mac_str);
    cJSON_AddStringToObject(root, "uuid", handle->uuid);
    cJSON_AddStringToObject(root, "chip_model_name", CONFIG_IDF_TARGET);

    cJSON *chip_info_obj = cJSON_CreateObject();
    if (chip_info_obj != NULL) {
        esp_chip_info_t chip_info = {0};
        esp_chip_info(&chip_info);
        cJSON_AddNumberToObject(chip_info_obj, "model", chip_info.model);
        cJSON_AddNumberToObject(chip_info_obj, "cores", chip_info.cores);
        cJSON_AddNumberToObject(chip_info_obj, "revision", chip_info.revision);
        cJSON_AddNumberToObject(chip_info_obj, "features", chip_info.features);
        cJSON_AddItemToObject(root, "chip_info", chip_info_obj);
    }

    cJSON *app_obj = cJSON_CreateObject();
    if (app_obj != NULL) {
        char compile_time[64] = {0};
        char sha256_str[65] = {0};
        const esp_app_desc_t *app_desc = esp_app_get_description();
        if (app_desc != NULL) {
            cJSON_AddStringToObject(app_obj, "name", app_desc->project_name);
            cJSON_AddStringToObject(app_obj, "version", app_desc->version);
            snprintf(compile_time, sizeof(compile_time), "%sT%sZ", app_desc->date, app_desc->time);
            cJSON_AddStringToObject(app_obj, "compile_time", compile_time);
            cJSON_AddStringToObject(app_obj, "idf_version", app_desc->idf_ver);

            for (int i = 0; i < 32; i++) {
                snprintf(sha256_str + i * 2, 3, "%02x", app_desc->app_elf_sha256[i]);
            }
            cJSON_AddStringToObject(app_obj, "elf_sha256", sha256_str);
        } else {
            cJSON_AddStringToObject(app_obj, "name", "Unknown");
            cJSON_AddStringToObject(app_obj, "version", "Unknown");
            cJSON_AddStringToObject(app_obj, "compile_time", "Unknown");
            cJSON_AddStringToObject(app_obj, "idf_version", "Unknown");
            cJSON_AddStringToObject(app_obj, "elf_sha256", "Unknown");
        }
        cJSON_AddItemToObject(root, "application", app_obj);
    }

    cJSON *partition_array = cJSON_CreateArray();
    if (partition_array != NULL) {
        esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
        while (it) {
            const esp_partition_t *partition = esp_partition_get(it);
            cJSON *partition_obj = cJSON_CreateObject();
            if (partition_obj != NULL) {
                cJSON_AddStringToObject(partition_obj, "label", partition->label);
                cJSON_AddNumberToObject(partition_obj, "type", partition->type);
                cJSON_AddNumberToObject(partition_obj, "subtype", partition->subtype);
                cJSON_AddNumberToObject(partition_obj, "address", partition->address);
                cJSON_AddNumberToObject(partition_obj, "size", partition->size);
                cJSON_AddItemToArray(partition_array, partition_obj);
            }
            it = esp_partition_next(it);
        }
        esp_partition_iterator_release(it);
        cJSON_AddItemToObject(root, "partition_table", partition_array);
    }

    cJSON *ota_obj = cJSON_CreateObject();
    if (ota_obj != NULL) {
        const esp_partition_t *ota_partition = esp_ota_get_running_partition();
        if (ota_partition != NULL) {
            cJSON_AddStringToObject(ota_obj, "label", ota_partition->label);
        } else {
            cJSON_AddStringToObject(ota_obj, "label", "Unknown");
        }
        cJSON_AddItemToObject(root, "ota", ota_obj);
    }

    char board_json[512] = {0};
    if (board_get_board_json(handle, board_json, sizeof(board_json)) == ESP_OK) {
        cJSON *board_obj = cJSON_Parse(board_json);
        if (board_obj != NULL) {
            cJSON_AddItemToObject(root, "board", board_obj);
        } else {
            cJSON_AddStringToObject(ota_obj, "board", board_json);
        }
    }

    char *json_string = cJSON_PrintUnformatted(root);
    esp_err_t ret = json_string ? ESP_OK : ESP_ERR_NO_MEM;
    if (json_string) {
        size_t json_len = strlen(json_string);
        if (json_len >= buffer_size) {
            ret = ESP_ERR_INVALID_SIZE;
        } else {
            strcpy(json_buffer, json_string);
        }
        cJSON_free(json_string);
    }

    cJSON_Delete(root);

    return ret;
}

esp_err_t board_get_board_json(board_handle_t *handle, char *json_buffer, size_t buffer_size)
{
    if (handle == NULL || json_buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create board JSON object using cJSON
    cJSON *board = cJSON_CreateObject();
    if (board == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Add basic board information
    cJSON_AddStringToObject(board, "type", "generic");
    cJSON_AddStringToObject(board, "name", "ESP32 Board");
    cJSON_AddStringToObject(board, "version", "1.0");

    // Convert to string
    char *json_string = cJSON_PrintUnformatted(board);
    esp_err_t ret = ESP_OK;

    if (json_string == NULL) {
        ret = ESP_ERR_NO_MEM;
    } else {
        size_t json_len = strlen(json_string);
        if (json_len >= buffer_size) {
            ret = ESP_ERR_INVALID_SIZE;
        } else {
            strcpy(json_buffer, json_string);
        }
        cJSON_free(json_string);
    }

    // Clean up
    cJSON_Delete(board);

    return ret;
}
