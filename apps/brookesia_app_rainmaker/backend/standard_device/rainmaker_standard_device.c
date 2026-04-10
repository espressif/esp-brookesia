/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rainmaker_standard_device.h"
#include "rainmaker_standard_device_shared.h"
#include "rainmaker_device_group_manager.h"
#include "rainmaker_app_backend_util.h"
#include "rainmaker_api_handle.h"

esp_err_t rainmaker_standard_device_set_power(struct rm_device_item *device, bool power_on, const char **out_err_reason)
{
    return rm_standard_device_shared_set_power(device, power_on, out_err_reason);
}

esp_err_t rainmaker_standard_device_set_name(struct rm_device_item *device, const char *name, const char **out_err_reason)
{
    if (device == NULL || name == NULL || name[0] == '\0') {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid device pointer or name");
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
        ret = rainmaker_standard_light_set_name(device->node_id, name, out_err_reason);
        break;
    case RAINMAKER_APP_DEVICE_TYPE_FAN:
        ret = rainmaker_standard_fan_set_name(device->node_id, name, out_err_reason);
        break;
    case RAINMAKER_APP_DEVICE_TYPE_SWITCH:
        ret = rainmaker_standard_switch_set_name(device->node_id, name, out_err_reason);
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
    rainmaker_app_backend_util_safe_free((void *)device->name);
    device->name = strdup(name);
    if (device->name == NULL) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to allocate memory for name");
        }
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t rainmaker_standard_device_set_timezone(struct rm_device_item *device, const char *tz_name, const char *tz_posix, const char **out_err_reason)
{
    if (device == NULL || tz_name == NULL || tz_name[0] == '\0' || tz_posix == NULL || tz_posix[0] == '\0') {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid device pointer or tz_name or tz_posix");
        }
        return ESP_ERR_INVALID_ARG;
    }
    if (device->online == false) {
        if (out_err_reason) {
            *out_err_reason = strdup("Device is offline");
        }
        return ESP_ERR_INVALID_STATE;
    }
    char payload[128] = {0};
    snprintf(payload, sizeof(payload), "{\"Time\":{\"TZ-POSIX\":\"%s\"}}", tz_posix);
    esp_err_t ret = rainmaker_api_set_device_parameters(device->node_id, payload, out_err_reason);
    if (ret != ESP_OK) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to set timezone");
        }
        return ret;
    }
    rainmaker_app_backend_util_safe_free((void *)device->timezone);
    device->timezone = strdup(tz_name);
    if (device->timezone == NULL) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to allocate memory for timezone");
        }
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t rainmaker_standard_device_do_system_service(struct rm_device_item *device, rm_standard_device_system_service_type_t service_type, const char **out_err_reason)
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
    char payload[128] = {0};
    switch (service_type) {
    case RM_STANDARD_DEVICE_SYSTEM_SERVICE_TYPE_REBOOT:
        snprintf(payload, sizeof(payload), "{\"System\":{\"Reboot\":true}}");
        break;
    case RM_STANDARD_DEVICE_SYSTEM_SERVICE_TYPE_WIFI_RESET:
        snprintf(payload, sizeof(payload), "{\"System\":{\"Wi-Fi-Reset\":true}}");
        break;
    case RM_STANDARD_DEVICE_SYSTEM_SERVICE_TYPE_FACTORY_RESET:
        snprintf(payload, sizeof(payload), "{\"System\":{\"Factory-Reset\":true}}");
        break;
    default:
        if (out_err_reason) {
            *out_err_reason = strdup("Service type not supported");
        }
        return ESP_ERR_INVALID_ARG;
    }
    ret = rainmaker_api_set_device_parameters(device->node_id, payload, out_err_reason);
    if (ret != ESP_OK) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to do system service");
        }
        return ret;
    }
    return ret;
}

const void *rainmaker_standard_device_get_device_icon_src(rm_app_device_type_t type, bool power_on)
{
    return rm_standard_device_shared_get_icon_src(type, power_on);
}
