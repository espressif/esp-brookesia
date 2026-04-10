/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rainmaker_standard_device.h"
#include "rainmaker_standard_device_shared.h"
#include "rainmaker_device_group_manager.h"
#include "rainmaker_api_handle.h"
#include "rainmaker_app_backend_util.h"

#include <stdlib.h>
#include <string.h>

esp_err_t rainmaker_standard_device_set_power(struct rm_device_item *device, bool power_on, const char **out_err_reason)
{
    return rm_standard_device_shared_set_power(device, power_on, out_err_reason);
}

esp_err_t rainmaker_standard_device_set_name(struct rm_device_item *device, const char *name, const char **out_err_reason)
{
    (void)device;
    (void)name;
    if (out_err_reason) {
        *out_err_reason = strdup("Not supported on PC build");
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t rainmaker_standard_device_set_timezone(struct rm_device_item *device, const char *tz_name, const char *tz_posix,
        const char **out_err_reason)
{
    (void)device;
    (void)tz_name;
    (void)tz_posix;
    if (out_err_reason) {
        *out_err_reason = strdup("Not supported on PC build");
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t rainmaker_standard_device_do_system_service(struct rm_device_item *device,
        rm_standard_device_system_service_type_t service_type,
        const char **out_err_reason)
{
    (void)device;
    (void)service_type;
    if (out_err_reason) {
        *out_err_reason = strdup("Not supported on PC build");
    }
    return ESP_ERR_NOT_SUPPORTED;
}

const void *rainmaker_standard_device_get_device_icon_src(rm_app_device_type_t type, bool power_on)
{
    return rm_standard_device_shared_get_icon_src(type, power_on);
}
