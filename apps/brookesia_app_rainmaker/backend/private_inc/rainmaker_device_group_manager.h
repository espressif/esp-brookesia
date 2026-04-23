/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sys/queue.h"
#include "rainmaker_app_backend.h"
#include "rainmaker_standard_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RainMaker group node item */
typedef struct rm_group_node_item {
    /* RainMaker node id, it will not change after the node is created*/
    const char *node_id;
    /* The next node item in the list */
    STAILQ_ENTRY(rm_group_node_item) next;
} rm_group_node_item_t;

STAILQ_HEAD(rm_group_node_list_t, rm_group_node_item);

/* RainMaker group item */
typedef struct rm_group_item {
    /* RainMaker group id, it will not change after the group is created */
    const char *group_id;
    /* RainMaker group name, it will change after the group is renamed */
    char *group_name;
    /* The nodes in the group */
    struct rm_group_node_list_t *nodes_list;
    /* The next group item in the list */
    STAILQ_ENTRY(rm_group_item) next;
} rm_group_item_t;

STAILQ_HEAD(rm_group_list_t, rm_group_item);

/* RainMaker device item */
typedef struct rm_device_item {
    /* RainMaker node id, it will not change after the node is created */
    const char *node_id;
    /* RainMaker node firmware version, it will not change after the node is created */
    const char *fw_version;
    /* RainMaker node name, it will change after the node is renamed */
    char *name;
    /* RainMaker node timezone, it will change after the node is updated */
    char *timezone;
    /* Whether the node has a system service, like reboot, WiFi reset and factory reset */
    bool whether_has_system_service;
    /* Whether the node is online */
    bool online;
    /* Whether the node has a power switch */
    bool whether_has_power;
    /* When the device has power, it will show the current power state */
    bool power_on;
    /* The type of the device */
    rm_app_device_type_t type;
    /* The last onlinetimestamp of the device */
    int64_t last_timestamp;
    /* The device data */
    union {
        rm_standard_light_device_t light;
        rm_standard_fan_device_t fan;
    };
    /* The next device item in the list */
    STAILQ_ENTRY(rm_device_item) next;
} rm_device_item_t;

STAILQ_HEAD(rm_device_list_t, rm_device_item);

extern struct rm_group_list_t *g_groups_list;
extern struct rm_device_list_t *g_devices_list;
extern rm_group_item_t *g_current_group;

esp_err_t rm_device_group_manager_init(void);
void rm_device_group_manager_deinit(void);
void rm_device_group_manager_clear_device(void);
void rm_device_group_manager_clear_group(void);
esp_err_t rm_device_group_manager_find_group(const char *group_name);
/** Set current group by RainMaker group id (for Welcome dropdown switch). */
esp_err_t rm_device_group_manager_find_group_by_id(const char *group_id);
/** Pick current group by maximum node count (tie: earlier in list). Used after login / when no saved selection. */
esp_err_t rm_device_group_manager_select_default_group(void);
esp_err_t rm_device_group_manager_update_groups_list(void);
esp_err_t rm_device_group_manager_update_devices_list(void);
/** Remove a device from g_devices_list and from all groups' nodes_list by node_id. */
esp_err_t rm_device_group_manager_remove_device_by_node_id(const char *node_id);

/**
 * True if @p item is a current list entry (pointer match). Use before dereferencing a cached
 * rm_device_item_t * after any refresh that may call rm_device_group_manager_clear_device().
 */
bool rm_device_group_manager_is_device_in_list(const rm_device_item_t *item);

#ifdef __cplusplus
}
#endif /* __cplusplus */
