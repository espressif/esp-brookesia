/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rm_device_item *rm_device_item_handle_t;

typedef enum {
    RAINMAKER_APP_DEVICE_TYPE_LIGHT = 0,
    RAINMAKER_APP_DEVICE_TYPE_SWITCH = 1,
    RAINMAKER_APP_DEVICE_TYPE_FAN = 2,
    RAINMAKER_APP_DEVICE_TYPE_OTHER = 3,
} rm_app_device_type_t;

typedef struct {
    /* RainMaker node id, it only point to the node id and not allowed to change it */
    const char *node_id;
    /* RainMaker node name, it only point to the name of the node and not allowed to change it */
    const char *name;
    /* Whether the node is online */
    bool online;
    /* Whether the node has a power switch */
    bool whether_has_power;
    /* When the device has power, it will show the current power state */
    bool power_on;
    /* The type of the device */
    rm_app_device_type_t type;
} rm_app_device_t;

typedef struct {
    /* RainMaker group id, it only point to the group id and not allowed to change it */
    const char *group_id;
    /* RainMaker group name, it only point to the name of the group and not allowed to change it */
    const char *group_name;
} rm_app_group_t;

/**
 * @brief RainMaker app backend: login
 *
 * This is a backend hook used by the UI layer. Provide your own (strong) implementation
 * elsewhere to perform real authentication.
 *
 * @param[in] email Email string from textarea
 * @param[in] password Password string from textarea
 * @param[out] out_err_reason Failure reason string (optional).
 *             The returned pointer must remain valid after the function returns
 *             (e.g. a static string or a static buffer).
 *
 * @return true on success, false on failure
 */
bool rm_app_backend_login(const char *email, const char *password, const char **out_err_reason);

/**
 * @brief Check if the user is logged in
 *
 * @return true if the user is logged in, false otherwise
 */
bool rm_app_backend_is_logged_in(void);

/**
 * @brief Initialize Rainmaker backend
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t rm_app_backend_init(void);

/**
 * @brief Get current group name (e.g. "Home")
 *
 * @return Pointer to a persistent string, NULL if no current group is set
 */
const char *rm_app_backend_get_current_group_name(void);

/**
 * @brief Get number of groups (homes) available for the Welcome dropdown
 *
 * @return Number of groups
 */
int rm_app_backend_get_groups_count(void);

/**
 * @brief Get group by index (for dropdown list)
 *
 * @param[in] index Index in [0, get_groups_count())
 * @param[out] group Filled with group_id and group_name (pointers valid until next refresh)
 * @return true on success, false if index out of range
 */
bool rm_app_backend_get_group_by_index(int index, rm_app_group_t *group);

/**
 * @brief Set current group by name
 *
 * @param[in] group_name RainMaker group name of the selected home
 */
void rm_app_backend_set_current_group_by_name(const char *group_name);

/**
 * @brief Get all devices count
 *
 * @return The number of all devices
 */
int rm_app_backend_get_devices_count(void);

/**
 * @brief Get all devices by index
 *
 * @param[in] index The index of the device
 * @param[out] device The device details to be filled
 * @return The device handle pointer, NULL if failed
 */
rm_device_item_handle_t rm_app_backend_get_device_by_index(int index, rm_app_device_t *device);

/**
 * @brief Get current home devices count
 *
 * @return The number of current home devices
 */
int rm_app_backend_get_current_home_devices_count(void);

/**
 * @brief Get current home devices by index
 *
 * @param[in] index The index of the device
 * @param[out] device The device details to be filled
 * @return The current home device handle pointer, NULL if failed
 */
rm_device_item_handle_t rm_app_backend_get_current_home_device_by_index(int index, rm_app_device_t *device);

/**
 * @brief Get device info by handle
 *
 * @param[in] handle Device handle pointer
 * @param[out] device The device details to be filled
 * @return true on success, false on failure
 */
bool rm_app_backend_get_device_info_by_handle(rm_device_item_handle_t handle, rm_app_device_t *device);

/**
 * @brief Get device handle by node_id (current home only)
 *
 * @param[in] node_id RainMaker node ID
 * @return Device handle, or NULL if not found in current home
 */
rm_device_item_handle_t rm_app_backend_get_device_handle_by_node_id(const char *node_id);

/**
 * @brief Get device type
 *
 * @param[in] device Device handle pointer
 * @return The device type
 */
rm_app_device_type_t rm_app_backend_get_device_type(rm_device_item_handle_t device);

/**
 * @brief Refresh/query home screen from backend
 *
 * This is called when the app is started to refresh the home information.
 *
 * @param[in] all Whether need update all(group and device), if set false, it will only update the device and not update the group
 * @return true on success, false on failure.
 */
bool rm_app_backend_refresh_home_screen(bool all);

/**
 * @brief Refresh/query scenes screen from backend
 *
 * This is called when user taps the refresh button on Scenes screen.
 * Provide your own (strong) implementation to fetch scenes from cloud/backend.
 *
 * @return true on success, false on failure.
 */
bool rm_app_backend_refresh_scenes(void);

/**
 * @brief Refresh/query schedules screen from backend
 *
 * This is called when user taps the refresh button on Schedules screen.
 *
 * @return true on success, false on failure.
 */
bool rm_app_backend_refresh_schedules(void);

/**
 * @brief Create/add an automation (stub)
 *
 * This is called when user taps the "+" or "Add Automation" button on Automations screen.
 *
 * @return true on success, false on failure.
 */
bool rm_app_backend_add_automations(void);

/**
 * @brief Get current logged-in user email for UI display
 *
 * @return Pointer to a persistent string.
 */
const char *rm_app_backend_get_current_user_email(void);

/**
 * @brief Logout
 *
 * Called when user taps "Log Out" on User screen.
 *
 * @return true on success, false on failure.
 */
bool rm_app_backend_logout(void);

/**
 * @brief Deinitialize RainMaker app backend
 *
 * This function is called when the app is closed to deinitialize the Rainmaker backend.
 *
 * @return void
 */
void rm_app_backend_deinit(void);
#ifdef __cplusplus
} /* extern "C" */
#endif
