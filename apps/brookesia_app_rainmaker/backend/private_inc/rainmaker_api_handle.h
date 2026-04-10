/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool ota_available;
    size_t new_firmware_size;
    char new_firmware_version[32];
    char ota_job_id[32];
} rainmaker_ota_update_info_t;

/** Schedule trigger type */
typedef enum {
    RM_SCHEDULE_TRIGGER_TYPE_INVALID = 0,
    RM_SCHEDULE_TRIGGER_TYPE_RELATIVE = 1,
    RM_SCHEDULE_TRIGGER_TYPE_ABSOLUTE = 2,
} rm_schedule_trigger_type_t;

typedef enum {
    RM_NODE_ACTION_NO_CHANGE = 0,
    RM_NODE_ACTION_ADD = 1,
    RM_NODE_ACTION_EDIT = 2,
    RM_NODE_ACTION_DELETE = 3,
    RM_NODE_ACTION_ENABLE = 4,
    RM_NODE_ACTION_DISABLE = 5,
} rm_node_action_type_t;

/** Relative-time trigger (offset in seconds) */
typedef struct rm_schedule_trigger_relative {
    /** Relative seconds */
    int32_t relative_seconds;
} rm_schedule_trigger_relative_t;

/** Day-of-week / date trigger parameters */
typedef struct rm_schedule_trigger_days_of_week {
    /** Minutes since midnight (0–1439), e.g. 18:30 = 18*60+30 = 1110 */
    uint16_t minutes;
    /** Day-of-week bitmap, LSB = Monday, e.g. 0x7F = every day, 0x1F = weekdays */
    uint8_t days_of_week;
    /** Day of month (1–31) for "every month on day N" */
    uint8_t date_of_month;
    /** Month bitmap, LSB = January, e.g. 0xFFFF = every month, 0x0001 = January */
    uint16_t month;
    /** Year (e.g. 2026) for "on a specific date" */
    uint16_t year;
    /** Whether to repeat yearly (used with date_of_month and optionally month) */
    bool is_repeating;
} rm_schedule_trigger_days_of_week_t;

typedef struct {
    rm_node_action_type_t node_action_type;
    rm_schedule_trigger_type_t trigger_type;
    union {
        rm_schedule_trigger_relative_t relative;
        rm_schedule_trigger_days_of_week_t days_of_week;
    } trigger;
    const char *schedule_id;
    const char *action_json;
    const char *flag;
    const char *info;
    const char *name;
} rm_schedule_update_info_t;

typedef struct {
    rm_node_action_type_t node_action_type;
    const char *scene_id;
    const char *action_json;
    const char *info;
    const char *name;
} rm_scene_update_info_t;

typedef void (*rainmaker_login_failure_cb_t)(int error_code, const char *failed_reason);
typedef void (*rainmaker_login_success_cb_t)(void);

/**
 * @brief Initialize RainMaker User API and register login callbacks.
 *
 * This is a thin wrapper around the User API (C API). Calling multiple times is safe.
 */
esp_err_t rainmaker_api_init(const char *base_url, rainmaker_login_failure_cb_t login_failure_cb, rainmaker_login_success_cb_t login_success_cb);

esp_err_t rainmaker_api_deinit(void);

esp_err_t rainmaker_api_set_refresh_token(const char *refresh_token);
esp_err_t rainmaker_api_set_username_password(const char *username, const char *password);
esp_err_t rainmaker_api_login(void);
esp_err_t rainmaker_api_get_refresh_token(char **refresh_token);

/**
 * @brief Get raw JSON for `user/node_group`.
 * Caller must free(*out_json) when return ESP_OK and *out_json != NULL.
 */
esp_err_t rainmaker_api_get_node_group_json(int *out_status_code, char **out_json);

/**
 * @brief Get raw JSON for `user/nodes`.
 * Caller must free(*out_json) when return ESP_OK and *out_json != NULL.
 */
esp_err_t rainmaker_api_get_nodes_json(const char *next_id, int *out_status_code, char **out_json);

/**
 * @brief Parse RainMaker `user/node_group` response.
 */
esp_err_t rainmaker_api_parse_node_group(const char *response_data);

/**
 * @brief Parse RainMaker `user/nodes` response.
 *
 */
esp_err_t rainmaker_api_parse_nodes(const char *response_data, char **out_next_id);

/**
 * @brief Set the parameters of a device.
 */
esp_err_t rainmaker_api_set_device_parameters(const char *node_id, const char *payload, const char **out_err_reason);

/**
 * @brief Remove a device.
 */
esp_err_t rainmaker_api_remove_device(const char *node_id, const char **out_err_reason);

/**
 * @brief Check if there is a new firmware update available.
 */
esp_err_t rainmaker_api_check_ota_update(const char *node_id, rainmaker_ota_update_info_t *ota_update_info, const char **out_err_reason);

/**
 * @brief Start OTA update.
 */
esp_err_t rainmaker_api_start_ota_update(const char *node_id, const char *ota_job_id, const char **out_err_reason);

/**
 * @brief Update a schedule. This include create and edit.
 */
esp_err_t rainmaker_api_update_schedule(const char *node_id, const rm_schedule_update_info_t *schedule_update_info, const char **out_err_reason);

/**
 * @brief Delete a schedule.
 */
esp_err_t rainmaker_api_delete_schedule(const char *node_id, const char *schedule_id, const char **out_err_reason);

/**
 * @brief Enable or disable a schedule.
 */
esp_err_t rainmaker_api_enable_schedule(const char *node_id, const char *schedule_id, bool enabled, const char **out_err_reason);

/**
 * @brief Update a scene. This include create and edit.
 */
esp_err_t rainmaker_api_update_scene(const char *node_id, const rm_scene_update_info_t *scene_update_info, const char **out_err_reason);

/**
 * @brief Delete a scene.
 */
esp_err_t rainmaker_api_delete_scene(const char *node_id, const char *scene_id, const char **out_err_reason);

/**
 * @brief Activate or deactivate a scene.
 */
esp_err_t rainmaker_api_activate_scene(const char *node_id, const char *scene_id, bool activate, const char **out_err_reason);
#ifdef __cplusplus
}
#endif /* __cplusplus */
