/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_partition.h"
#include "esp_check.h"
#include "cJSON.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include "tusb_cdc_acm.h"
#include "tusb_console.h"
#include "tusb_composite.h"

#define BASE_PATH "/usb" // base path to mount the partition

static const char *TAG = "tusb_composite";

static wl_handle_t s_wl_basic_handle = WL_INVALID_HANDLE;
/**
 * @brief CDC device line change callback
 *
 * CDC device signals, that the DTR, RTS states changed
 *
 * @param[in] itf   CDC device index
 * @param[in] event CDC event type
 */
static void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event)
{
    int dtr = event->line_state_changed_data.dtr;
    int rts = event->line_state_changed_data.rts;
    ESP_LOGI(TAG, "Line state changed on channel %d: DTR:%d, RTS:%d", itf, dtr, rts);
}

static esp_err_t storage_init_spiflash(wl_handle_t *wl_handle)
{
    ESP_LOGI(TAG, "Initializing wear levelling");

    const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
    if (data_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find FATFS partition. Check the partition table.");
        return ESP_ERR_NOT_FOUND;
    }

    return wl_mount(data_partition, wl_handle);
}

static esp_err_t mount_wl_basic(void)
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .use_one_fat = false,
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl("/usb", "storage", &mount_config, &s_wl_basic_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

static esp_err_t unmount_wl_basic(void)
{
    esp_err_t err = esp_vfs_fat_spiflash_unmount_rw_wl("/usb", s_wl_basic_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount FATFS (%s)", esp_err_to_name(err));
    }
    return err;
}

static char *strdup_or_empty(const char *s)
{
    return s ? strdup(s) : strdup("");
}

static  int parse_bot_json(const char *filename, coze_agent_config_t *cfg)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);

    char *json_data = malloc(len + 1);
    if (!json_data) {
        fclose(fp);
        return -1;
    }
    fread(json_data, 1, len, fp);
    json_data[len] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);

    if (!root) {
        fprintf(stderr, "JSON parse error: %s\n", cJSON_GetErrorPtr());
        return -1;
    }

    cJSON *public_key = cJSON_GetObjectItem(root, "public_key");
    cfg->public_key = strdup_or_empty(cJSON_IsString(public_key) ? public_key->valuestring : "");

    cJSON *appid = cJSON_GetObjectItem(root, "appid");
    cfg->appid = strdup_or_empty(cJSON_IsString(appid) ? appid->valuestring : "");
    cJSON *bots = cJSON_GetObjectItem(root, "bots");
    if (!cJSON_IsArray(bots)) {
        cJSON_Delete(root);
        return -1;
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
    return 0;
}

esp_err_t read_bot_config_from_flash(coze_agent_config_t *config)
{
    mount_wl_basic();
    parse_bot_json("/usb/bot_setting.json", config);
    if (config->bot_num == 0) {
        ESP_LOGW(TAG, "No bot config found");
        return ESP_ERR_NOT_FOUND;
    }

    FILE *fp = fopen("/usb/private_key.pem", "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open private key file");
        release_bot_config(config);
        return ESP_ERR_NOT_FOUND;
    }
    int private_key_len = 0;
    fseek(fp, 0, SEEK_END);
    private_key_len = ftell(fp);
    rewind(fp);
    char *private_key = malloc(private_key_len + 1);
    fread(private_key, 1, private_key_len, fp);
    private_key[private_key_len] = '\0';
    fclose(fp);
    config->private_key = private_key;

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
    unmount_wl_basic();
    return ESP_OK;
}

esp_err_t release_bot_config(coze_agent_config_t *config)
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

esp_err_t mount_wl_basic_and_tusb(void)
{

    ESP_LOGI(TAG, "Initializing storage...");

    static wl_handle_t wl_handle = WL_INVALID_HANDLE;
    ESP_ERROR_CHECK(storage_init_spiflash(&wl_handle));

    const tinyusb_msc_spiflash_config_t config_spi = {
        .wl_handle = wl_handle
    };
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_spiflash(&config_spi));
    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(BASE_PATH));

    ESP_LOGI(TAG, "USB Composite initialization");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .string_descriptor_count = 0,
        .external_phy = false,
#if (TUD_OPT_HIGH_SPEED)
        .fs_configuration_descriptor = NULL,
        .hs_configuration_descriptor = NULL,
        .qualifier_descriptor = NULL,
#else
        .configuration_descriptor = NULL,
#endif // TUD_OPT_HIGH_SPEED
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 64,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
    /* the second way to register a callback */
    ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(
                        TINYUSB_CDC_ACM_0,
                        CDC_EVENT_LINE_STATE_CHANGED,
                        &tinyusb_cdc_line_state_changed_callback));
    esp_tusb_init_console(TINYUSB_CDC_ACM_0); // log to usb

    return ESP_OK;
}
