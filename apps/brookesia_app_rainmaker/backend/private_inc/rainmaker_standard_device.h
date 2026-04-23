/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <esp_err.h>
#include "rainmaker_app_backend.h"

/* Forward declaration to avoid circular include with rainmaker_app_backend.h */
struct rm_device_item;

#ifdef __cplusplus
extern "C" {
#endif

#define RM_STANDARD_DEVICE_POWER "Power"
/* -------------------------------------------------------------------------
 * Standard Device System Service
 * ------------------------------------------------------------------------- */
typedef enum {
    RM_STANDARD_DEVICE_SYSTEM_SERVICE_TYPE_REBOOT = 0,
    RM_STANDARD_DEVICE_SYSTEM_SERVICE_TYPE_WIFI_RESET = 1,
    RM_STANDARD_DEVICE_SYSTEM_SERVICE_TYPE_FACTORY_RESET = 2,
} rm_standard_device_system_service_type_t;

/* -------------------------------------------------------------------------
 * Standard Light Device
 * ------------------------------------------------------------------------- */
#define RM_APP_DEVICE_TYPE_STRING_LIGHT "Light"
#define RM_STANDARD_LIGHT_DEVICE_BRIGHTNESS "Brightness"
#define RM_STANDARD_LIGHT_DEVICE_HUE "Hue"
#define RM_STANDARD_LIGHT_DEVICE_SATURATION "Saturation"

typedef struct rm_standard_light_device {
    int brightness;
    int hue;
    int saturation;
} rm_standard_light_device_t;

esp_err_t rainmaker_standard_light_parse_parameters(cJSON *device, struct rm_device_item *device_item);
esp_err_t rainmaker_standard_light_set_power(const char *node_id, bool power_on, const char **out_err_reason);
esp_err_t rainmaker_standard_light_set_brightness(const char *node_id, int brightness, const char **out_err_reason);
esp_err_t rainmaker_standard_light_set_hue(const char *node_id, int hue, const char **out_err_reason);
esp_err_t rainmaker_standard_light_set_saturation(const char *node_id, int saturation, const char **out_err_reason);
esp_err_t rainmaker_standard_light_set_name(const char *node_id, const char *name, const char **out_err_reason);

/* -------------------------------------------------------------------------
 * Standard Fan Device
 * ------------------------------------------------------------------------- */
#define RM_APP_DEVICE_TYPE_STRING_FAN "Fan"
#define RM_STANDARD_FAN_DEVICE_SPEED "Speed"

typedef struct rm_standard_fan_device {
    int speed;
} rm_standard_fan_device_t;

esp_err_t rainmaker_standard_fan_parse_parameters(cJSON *device, struct rm_device_item *device_item);
esp_err_t rainmaker_standard_fan_set_power(const char *node_id, bool power_on, const char **out_err_reason);
esp_err_t rainmaker_standard_fan_set_speed(const char *node_id, int speed, const char **out_err_reason);
esp_err_t rainmaker_standard_fan_set_name(const char *node_id, const char *name, const char **out_err_reason);

/* -------------------------------------------------------------------------
 * Standard Switch Device
 * ------------------------------------------------------------------------- */
#define RM_APP_DEVICE_TYPE_STRING_SWITCH "Switch"

esp_err_t rainmaker_standard_switch_parse_parameters(cJSON *device, struct rm_device_item *device_item);
esp_err_t rainmaker_standard_switch_set_power(const char *node_id, bool power_on, const char **out_err_reason);
esp_err_t rainmaker_standard_switch_set_name(const char *node_id, const char *name, const char **out_err_reason);

/* -------------------------------------------------------------------------
 * Standard Device (common)
 * ------------------------------------------------------------------------- */

/**
 * @brief Set device power state.
 *
 * UI calls this when user toggles the power switch on Home screen.
 * Provide your own (strong) implementation to control real device power.
 *
 * @param[in]  device        Device handle pointer
 * @param[in]  power_on      Desired power state
 * @param[out] out_err_reason Optional failure reason string (persistent)
 * @return ESP_OK on success, other on failure
 */
esp_err_t rainmaker_standard_device_set_power(struct rm_device_item *device, bool power_on,
        const char **out_err_reason);

/**
 * @brief Set device name.
 *
 * @param[in]  device        Device handle pointer
 * @param[in]  name         New name (non-empty)
 * @param[out] out_err_reason Optional failure reason string (persistent)
 * @return ESP_OK on success, other on failure
 */
esp_err_t rainmaker_standard_device_set_name(struct rm_device_item *device, const char *name,
        const char **out_err_reason);

/**
 * @brief Set device timezone.
 *
 * @param[in]  device        Device handle pointer
 * @param[in]  tz_name       New timezone name (non-empty)
 * @param[in]  tz_posix      New POSIX timezone string (non-empty)
 * @param[out] out_err_reason Optional failure reason string (persistent)
 * @return ESP_OK on success, other on failure
 */
esp_err_t rainmaker_standard_device_set_timezone(struct rm_device_item *device, const char *tz_name,
        const char *tz_posix, const char **out_err_reason);

/**
 * @brief Do system service.
 *
 * @param[in]  device        Device handle pointer
 * @param[in]  service_type  System service type
 * @param[out] out_err_reason Optional failure reason string (persistent)
 * @return ESP_OK on success, other on failure
 */
esp_err_t rainmaker_standard_device_do_system_service(struct rm_device_item *device,
        rm_standard_device_system_service_type_t service_type, const char **out_err_reason);

/**
 * @brief Get device icon source.
 *
 * @param[in]  type        Device type
 * @param[in]  power_on    Whether the device is powered on
 * @return Device icon source
 */
const void *rainmaker_standard_device_get_device_icon_src(rm_app_device_type_t type, bool power_on);
#ifdef __cplusplus
}
#endif
