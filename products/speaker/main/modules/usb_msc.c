/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "diskio.h"
#include "tinyusb.h"
#include "tinyusb_msc.h"
#include "tinyusb_default_config.h"
#include "esp_check.h"
#include "usb_msc.h"
#include "bsp/echoear.h"

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum {
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN  = 0x80,
    EDPT_MSC_OUT  = 0x01,
    EDPT_MSC_IN   = 0x81,
};

static const char *TAG = "usb_msc";

static tusb_desc_device_t msc_device_descriptor = {
    .bLength = sizeof(msc_device_descriptor),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A,             // This is Espressif VID. This needs to be changed according to Users / Customers
    .idProduct = 0x4002,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

static char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: is supported language is English (0x0409)
    "TinyUSB",                      // 1: Manufacturer
    "TinyUSB Device",               // 2: Product
    "123456",                       // 3: Serials
    "MSC",                          // 4. MSC
};

static uint8_t const msc_fs_configuration_desc[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, 64),
};

static tinyusb_msc_storage_handle_t msc_handle = NULL;

static void storage_event_cb(tinyusb_msc_storage_handle_t handle, tinyusb_msc_event_t *event, void *arg)
{
    switch (event->id) {
    case TINYUSB_MSC_EVENT_MOUNT_START:
        f_setlabel("SPEAKER");
        ESP_LOGI(TAG, "Storage mount start");
        break;
    case TINYUSB_MSC_EVENT_MOUNT_COMPLETE:
        ESP_LOGI(TAG, "Storage mount complete");
        break;
    case TINYUSB_MSC_EVENT_MOUNT_FAILED:
        ESP_LOGI(TAG, "Storage mount failed");
        break;
    case TINYUSB_MSC_EVENT_FORMAT_REQUIRED:
        ESP_LOGI(TAG, "Storage format required");
        break;
    default:
        ESP_LOGI(TAG, "Storage event unknown: %d", event->id);
        break;
    }
}


esp_err_t usb_msc_mount(void)
{
    ESP_LOGI(TAG, "USB MSC initialization");

    tinyusb_msc_driver_config_t driver_cfg = {
        .callback = storage_event_cb,                  // Register the callback for mount changed events
        .callback_arg = NULL,                          // No additional argument for the callback
    };

    ESP_RETURN_ON_ERROR(tinyusb_msc_install_driver(&driver_cfg), TAG, "tinyusb_msc_driver_install failed");

    tinyusb_msc_storage_config_t config = {
        .medium.card = bsp_sdcard_get_handle(),        // Set the context to the SDMMC card handle
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP,  // Initial mount point to APP
        .fat_fs = {
            .base_path = "/data",                      // Use default base path
            .config.max_files = 5,                     // Maximum number of files that can be opened simultaneously
            .format_flags = 0,                         // No special format flags
        },
    };
    ESP_RETURN_ON_ERROR(tinyusb_msc_new_storage_sdmmc(&config, &msc_handle), TAG, "tinyusb_msc_storage_init failed");

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = &msc_device_descriptor;
    tusb_cfg.descriptor.full_speed_config = msc_fs_configuration_desc;
    tusb_cfg.descriptor.string = string_desc_arr;
    tusb_cfg.descriptor.string_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]);
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG, "tinyusb_driver_install failed");
    ESP_LOGI(TAG, "USB MSC initialization DONE");

    return ESP_OK;
}
