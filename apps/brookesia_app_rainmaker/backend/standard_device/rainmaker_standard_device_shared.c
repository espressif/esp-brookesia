/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../../ui/ui.h"
#include "rainmaker_standard_device_shared.h"

#include <stdlib.h>
#include <string.h>

esp_err_t rm_standard_device_shared_set_power(struct rm_device_item *device, bool power_on, const char **out_err_reason)
{
    if (device == NULL) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid device pointer");
        }
        return ESP_ERR_INVALID_ARG;
    }
    if (device->online == false) {
        if (out_err_reason) {
            *out_err_reason = strdup("Device is offline");
        }
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_FAIL;
    switch (device->type) {
    case RAINMAKER_APP_DEVICE_TYPE_LIGHT:
        ret = rainmaker_standard_light_set_power(device->node_id, power_on, out_err_reason);
        break;
    case RAINMAKER_APP_DEVICE_TYPE_FAN:
        ret = rainmaker_standard_fan_set_power(device->node_id, power_on, out_err_reason);
        break;
    case RAINMAKER_APP_DEVICE_TYPE_SWITCH:
        ret = rainmaker_standard_switch_set_power(device->node_id, power_on, out_err_reason);
        break;
    default:
        if (out_err_reason) {
            *out_err_reason = strdup("Device type not supported");
        }
        ret = ESP_FAIL;
        break;
    }
    if (ret != ESP_OK) {
        return ret;
    }
    device->power_on = power_on;
    return ESP_OK;
}

const void *rm_standard_device_shared_get_icon_src(rm_app_device_type_t type, bool power_on)
{
    switch (type) {
    case RAINMAKER_APP_DEVICE_TYPE_LIGHT:
        return power_on ? &ui_img_light_3_power_on_png : &ui_img_light_3_power_off_png;
    case RAINMAKER_APP_DEVICE_TYPE_FAN:
        return power_on ? &ui_img_fan_power_on_png : &ui_img_fan_power_off_png;
    case RAINMAKER_APP_DEVICE_TYPE_SWITCH:
        return power_on ? &ui_img_switch_power_on_png : &ui_img_switch_power_off_png;
    default:
        return power_on ? &ui_img_light_3_power_on_png : &ui_img_light_3_power_off_png;
    }
}
