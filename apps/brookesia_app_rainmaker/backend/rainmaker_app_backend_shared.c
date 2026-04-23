/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "rainmaker_app_backend_shared.h"
#include "rainmaker_device_group_manager.h"

static void rm_app_backend_fill_device(rm_device_item_t *item, rm_app_device_t *device)
{
    if (!item || !device) {
        return;
    }
    device->node_id = item->node_id;
    device->name = item->name;
    device->online = item->online;
    device->whether_has_power = item->whether_has_power;
    device->power_on = item->power_on;
    device->type = item->type;
}

const char *rm_app_backend_shared_get_current_group_name(void)
{
    if (g_current_group && g_current_group->group_name && g_current_group->group_name[0]) {
        return g_current_group->group_name;
    }
    return "All Devices";
}

int rm_app_backend_shared_get_groups_count(void)
{
    if (g_groups_list == NULL) {
        return 0;
    }
    int count = 0;
    rm_group_item_t *group_item = NULL;
    STAILQ_FOREACH(group_item, g_groups_list, next) {
        count++;
    }
    return count;
}

bool rm_app_backend_shared_get_group_by_index(int index, rm_app_group_t *group)
{
    if (index < 0 || group == NULL || g_groups_list == NULL) {
        return false;
    }
    int i = 0;
    rm_group_item_t *group_item = NULL;
    STAILQ_FOREACH(group_item, g_groups_list, next) {
        if (i == index) {
            group->group_id = group_item->group_id;
            group->group_name = group_item->group_name;
            return true;
        }
        i++;
    }
    return false;
}

void rm_app_backend_shared_set_current_group_by_name(const char *group_name)
{
    if (group_name == NULL || group_name[0] == '\0' || g_groups_list == NULL) {
        return;
    }
    rm_group_item_t *group_item = NULL;
    STAILQ_FOREACH(group_item, g_groups_list, next) {
        if (group_item->group_name && strncmp(group_item->group_name, group_name, strlen(group_name)) == 0) {
            g_current_group = group_item;
            return;
        }
    }
}

int rm_app_backend_shared_get_devices_count(void)
{
    if (g_devices_list == NULL) {
        return 0;
    }
    int count = 0;
    rm_device_item_t *device_item = NULL;
    STAILQ_FOREACH(device_item, g_devices_list, next) {
        count++;
    }
    return count;
}

rm_device_item_handle_t rm_app_backend_shared_get_device_by_index(int index, rm_app_device_t *device)
{
    if (index < 0 || device == NULL || g_devices_list == NULL) {
        return NULL;
    }
    rm_device_item_t *device_item = NULL;
    int i = 0;
    STAILQ_FOREACH(device_item, g_devices_list, next) {
        if (i == index) {
            rm_app_backend_fill_device(device_item, device);
            return device_item;
        }
        i++;
    }
    return NULL;
}

int rm_app_backend_shared_get_current_home_devices_count(void)
{
    if (g_current_group == NULL || g_current_group->nodes_list == NULL) {
        return 0;
    }
    int count = 0;
    rm_group_node_item_t *group_node_item = NULL;
    STAILQ_FOREACH(group_node_item, g_current_group->nodes_list, next) {
        count++;
    }
    return count;
}

rm_device_item_handle_t rm_app_backend_shared_get_current_home_device_by_index(int index, rm_app_device_t *device)
{
    if (index < 0 || device == NULL || g_current_group == NULL || g_current_group->nodes_list == NULL || g_devices_list == NULL) {
        return NULL;
    }
    rm_group_node_item_t *group_node_item = NULL;
    int i = 0;
    STAILQ_FOREACH(group_node_item, g_current_group->nodes_list, next) {
        if (i == index && group_node_item->node_id) {
            rm_device_item_t *device_item = NULL;
            STAILQ_FOREACH(device_item, g_devices_list, next) {
                if (device_item->node_id
                        && strncmp(device_item->node_id, group_node_item->node_id, strlen(group_node_item->node_id)) == 0) {
                    rm_app_backend_fill_device(device_item, device);
                    return device_item;
                }
            }
        }
        i++;
    }
    return NULL;
}

bool rm_app_backend_shared_get_device_info_by_handle(rm_device_item_handle_t handle, rm_app_device_t *device)
{
    if (handle == NULL || device == NULL) {
        return false;
    }
    rm_app_backend_fill_device((rm_device_item_t *)handle, device);
    return true;
}

rm_device_item_handle_t rm_app_backend_shared_get_device_handle_by_node_id(const char *node_id)
{
    if (node_id == NULL) {
        return NULL;
    }
    int n = rm_app_backend_shared_get_current_home_devices_count();
    for (int i = 0; i < n; i++) {
        rm_app_device_t dev;
        rm_device_item_handle_t handle = rm_app_backend_shared_get_current_home_device_by_index(i, &dev);
        if (handle && dev.node_id && strcmp(dev.node_id, node_id) == 0) {
            return handle;
        }
    }
    return NULL;
}

rm_app_device_type_t rm_app_backend_shared_get_device_type(rm_device_item_handle_t device)
{
    if (device == NULL) {
        return RAINMAKER_APP_DEVICE_TYPE_OTHER;
    }
    return device->type;
}
