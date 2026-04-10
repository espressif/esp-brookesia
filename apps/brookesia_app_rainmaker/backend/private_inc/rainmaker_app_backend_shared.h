/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "rainmaker_app_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *rm_app_backend_shared_get_current_group_name(void);
int rm_app_backend_shared_get_groups_count(void);
bool rm_app_backend_shared_get_group_by_index(int index, rm_app_group_t *group);
void rm_app_backend_shared_set_current_group_by_name(const char *group_name);
int rm_app_backend_shared_get_devices_count(void);
rm_device_item_handle_t rm_app_backend_shared_get_device_by_index(int index, rm_app_device_t *device);
int rm_app_backend_shared_get_current_home_devices_count(void);
rm_device_item_handle_t rm_app_backend_shared_get_current_home_device_by_index(int index, rm_app_device_t *device);
bool rm_app_backend_shared_get_device_info_by_handle(rm_device_item_handle_t handle, rm_app_device_t *device);
rm_device_item_handle_t rm_app_backend_shared_get_device_handle_by_node_id(const char *node_id);
rm_app_device_type_t rm_app_backend_shared_get_device_type(rm_device_item_handle_t device);

#ifdef __cplusplus
}
#endif
