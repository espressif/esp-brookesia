/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include "esp_partition.h"
#include "esp_check.h"
#include "mbedtls/aes.h"
#include "cJSON.h"
#include "bsp/esp-bsp.h"
#include "coze_agent_config.h"

#define BASE_PATH                  BSP_SD_MOUNT_POINT
#define PRIVATE_KEY_PATH           BASE_PATH "/private_key.pem"
#define BOT_SETTING_PATH           BASE_PATH "/bot_setting.json"

static const char *TAG = "coze_agent_config";

static char *strdup_or_empty(const char *s)
{
    return s ? strdup(s) : strdup("");
}

static esp_err_t parse_bot_json(const char *filename, coze_agent_config_t *cfg)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        return ESP_ERR_NOT_FOUND;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);

    char *json_data = malloc(len + 1);
    if (!json_data) {
        fclose(fp);
        return ESP_ERR_NOT_FOUND;
    }
    fread(json_data, 1, len, fp);
    fclose(fp);

    json_data[len] = '\0';

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);

    if (!root) {
        fprintf(stderr, "JSON parse error: %s\n", cJSON_GetErrorPtr());
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *public_key = cJSON_GetObjectItem(root, "public_key");
    cfg->public_key = strdup_or_empty(cJSON_IsString(public_key) ? public_key->valuestring : "");

    cJSON *appid = cJSON_GetObjectItem(root, "appid");
    cfg->appid = strdup_or_empty(cJSON_IsString(appid) ? appid->valuestring : "");
    cJSON *bots = cJSON_GetObjectItem(root, "bots");
    if (!cJSON_IsArray(bots)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    int count = cJSON_GetArraySize(bots);
    if (count > MAX_BOT_NUM) {
        count = MAX_BOT_NUM;
        ESP_LOGW(TAG, "Too many bots, only %d bots will be used", count);
    }
    cfg->bot_num = count;

    for (int i = 0; i < count; i++) {
        cJSON *bot = cJSON_GetArrayItem(bots, i);
        if (!cJSON_IsObject(bot)) {
            continue;
        }

        cJSON *id    = cJSON_GetObjectItem(bot, "bot_id");
        cJSON *vid   = cJSON_GetObjectItem(bot, "voice_id");
        cJSON *name  = cJSON_GetObjectItem(bot, "bot_name");
        cJSON *desc  = cJSON_GetObjectItem(bot, "description");

        cfg->bot[i].bot_id         = strdup_or_empty(cJSON_IsString(id)   ? id->valuestring   : "");
        cfg->bot[i].voice_id       = strdup_or_empty(cJSON_IsString(vid)  ? vid->valuestring  : "");
        cfg->bot[i].bot_name       = strdup_or_empty(cJSON_IsString(name) ? name->valuestring : "");
        cfg->bot[i].bot_description = strdup_or_empty(cJSON_IsString(desc) ? desc->valuestring : "");
    }

    cJSON_Delete(root);

    return ESP_OK;
}

static esp_err_t parse_private_key(const char *file_path, coze_agent_config_t *config)
{
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open private key file: %s", file_path);
        return ESP_ERR_NOT_FOUND;
    }
    int private_key_len = 0;
    fseek(fp, 0, SEEK_END);
    private_key_len = ftell(fp);

    rewind(fp);
    char *private_key = malloc(private_key_len + 1);
    if (!private_key) {
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }
    fread(private_key, 1, private_key_len, fp);
    fclose(fp);

    private_key[private_key_len] = '\0';
    config->private_key = strdup_or_empty(private_key);
    free(private_key);

    return ESP_OK;
}

static bool check_if_file_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return false;
    }
    fclose(fp);
    return true;
}

esp_err_t coze_agent_config_read(coze_agent_config_t *config)
{
    const char *bot_setting_path = NULL;
    const char *private_key_path = NULL;

    if (check_if_file_exists(BOT_SETTING_PATH) && check_if_file_exists(PRIVATE_KEY_PATH)) {
        bot_setting_path = BOT_SETTING_PATH;
        private_key_path = PRIVATE_KEY_PATH;
        ESP_LOGI(TAG, "Using bot setting and private key files");
    } else {
        ESP_LOGE(TAG, "Missing bot setting or private key file");
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = parse_bot_json(bot_setting_path, config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse bot setting: %s", bot_setting_path);
        return err;
    }

    if (config->bot_num == 0) {
        ESP_LOGW(TAG, "No bot config found");
        return ESP_ERR_NOT_FOUND;
    }

    err = parse_private_key(private_key_path, config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse private key: %s", private_key_path);
        return err;
    }

    // ESP_LOGI(TAG, "appid: %s\n", config->appid);
    // ESP_LOGI(TAG, "public_key: %s\n", config->public_key);
    // ESP_LOGI(TAG, "bot_num: %d\n", config->bot_num);
    // for (int i = 0; i < config->bot_num; i++) {
    //     ESP_LOGI(
    //         TAG, "bot[%d]:bot_id %s, voice_id %s, bot_name %s\n", i, config->bot[i].bot_id, config->bot[i].voice_id,
    //         config->bot[i].bot_name
    //     );
    //     ESP_LOGI(TAG, "bot[%d]:bot_description %s\n", i, config->bot[i].bot_description);
    // }

    return ESP_OK;
}

esp_err_t coze_agent_config_release(coze_agent_config_t *config)
{
    for (int i = 0; i < config->bot_num; i++) {
        if (config->bot[i].bot_id) {
            free(config->bot[i].bot_id);
            config->bot[i].bot_id = NULL;
        }
        if (config->bot[i].voice_id) {
            free(config->bot[i].voice_id);
            config->bot[i].voice_id = NULL;
        }
        if (config->bot[i].bot_name) {
            free(config->bot[i].bot_name);
            config->bot[i].bot_name = NULL;
        }
        if (config->bot[i].bot_description) {
            free(config->bot[i].bot_description);
            config->bot[i].bot_description = NULL;
        }
    }
    if (config->public_key) {
        free(config->public_key);
        config->public_key = NULL;
    }
    if (config->private_key) {
        free(config->private_key);
        config->private_key = NULL;
    }
    return ESP_OK;
}
