/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "rainmaker_device_group_manager.h"
#include "rainmaker_api_handle.h"
#include "rainmaker_app_backend_util.h"
#include "esp_log.h"

static const char *TAG = "rainmaker_device_group_manager";

struct rm_group_list_t *g_groups_list = NULL;
struct rm_device_list_t *g_devices_list = NULL;
rm_group_item_t *g_current_group = NULL;

static void delete_all_group_node_items_in_list(rm_group_item_t *group_item)
{
    if (group_item == NULL || group_item->nodes_list == NULL) {
        ESP_LOGW(TAG, "Group item or nodes list is NULL");
        return;
    }
    rm_group_node_item_t *node_item, *tmp;
    STAILQ_FOREACH_SAFE(node_item, group_item->nodes_list, next, tmp) {
        STAILQ_REMOVE(group_item->nodes_list, node_item, rm_group_node_item, next);
        rainmaker_app_backend_util_safe_free((void *)node_item->node_id);
        rainmaker_app_backend_util_safe_free(node_item);
    }
}

static void delete_all_group_items_in_list(void)
{
    rm_group_item_t *group_item, *tmp;
    STAILQ_FOREACH_SAFE(group_item, g_groups_list, next, tmp) {
        STAILQ_REMOVE(g_groups_list, group_item, rm_group_item, next);
        rainmaker_app_backend_util_safe_free(group_item->group_name);
        rainmaker_app_backend_util_safe_free((void *)group_item->group_id);
        delete_all_group_node_items_in_list(group_item);
        rainmaker_app_backend_util_safe_free(group_item);
    }
}

static void delete_all_device_items_in_list(void)
{
    rm_device_item_t *device_item, *tmp;
    STAILQ_FOREACH_SAFE(device_item, g_devices_list, next, tmp) {
        STAILQ_REMOVE(g_devices_list, device_item, rm_device_item, next);
        rainmaker_app_backend_util_safe_free((void *)device_item->node_id);
        rainmaker_app_backend_util_safe_free((void *)device_item->name);
        rainmaker_app_backend_util_safe_free((void *)device_item->timezone);
        rainmaker_app_backend_util_safe_free((void *)device_item->fw_version);
        rainmaker_app_backend_util_safe_free(device_item);
    }
}

esp_err_t rm_device_group_manager_remove_device_by_node_id(const char *node_id)
{
    if (g_devices_list == NULL || node_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    rm_device_item_t *device_item, *tmp;
    STAILQ_FOREACH_SAFE(device_item, g_devices_list, next, tmp) {
        if (device_item->node_id && strcmp(device_item->node_id, node_id) == 0) {
            /* Remove this node from every group's nodes_list */
            if (g_groups_list != NULL) {
                rm_group_item_t *group_item;
                STAILQ_FOREACH(group_item, g_groups_list, next) {
                    if (group_item->nodes_list == NULL) {
                        continue;
                    }
                    rm_group_node_item_t *node_item, *n_tmp;
                    STAILQ_FOREACH_SAFE(node_item, group_item->nodes_list, next, n_tmp) {
                        if (node_item->node_id && strcmp(node_item->node_id, node_id) == 0) {
                            STAILQ_REMOVE(group_item->nodes_list, node_item, rm_group_node_item, next);
                            rainmaker_app_backend_util_safe_free((void *)node_item->node_id);
                            rainmaker_app_backend_util_safe_free(node_item);
                            break;
                        }
                    }
                }
            }
            STAILQ_REMOVE(g_devices_list, device_item, rm_device_item, next);
            rainmaker_app_backend_util_safe_free((void *)device_item->node_id);
            rainmaker_app_backend_util_safe_free((void *)device_item->name);
            rainmaker_app_backend_util_safe_free((void *)device_item->timezone);
            rainmaker_app_backend_util_safe_free((void *)device_item->fw_version);
            rainmaker_app_backend_util_safe_free(device_item);
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

static int rm_group_node_count(const rm_group_item_t *group_item)
{
    if (group_item == NULL || group_item->nodes_list == NULL) {
        return 0;
    }
    int c = 0;
    const rm_group_node_item_t *n;
    STAILQ_FOREACH(n, group_item->nodes_list, next) {
        (void)n;
        c++;
    }
    return c;
}

esp_err_t rm_device_group_manager_select_default_group(void)
{
    if (g_groups_list == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    rm_group_item_t *group_item;
    rm_group_item_t *best = NULL;
    int best_count = -1;
    STAILQ_FOREACH(group_item, g_groups_list, next) {
        int cnt = rm_group_node_count(group_item);
        if (cnt > best_count) {
            best_count = cnt;
            best = group_item;
        }
    }
    if (best != NULL) {
        g_current_group = best;
        return ESP_OK;
    }
    g_current_group = STAILQ_FIRST(g_groups_list);
    return ESP_OK;
}

esp_err_t rm_device_group_manager_find_group(const char *group_name)
{
    if (g_groups_list == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    rm_group_item_t *group_item;
    if (group_name == NULL) {
        return rm_device_group_manager_select_default_group();
    }
    STAILQ_FOREACH(group_item, g_groups_list, next) {
        if (group_item->group_name != NULL && strcmp(group_item->group_name, group_name) == 0) {
            g_current_group = group_item;
            return ESP_OK;
        }
    }
    ESP_LOGE(TAG, "Group %s not found", group_name);
    g_current_group = NULL;
    return ESP_ERR_NOT_FOUND;
}

esp_err_t rm_device_group_manager_find_group_by_id(const char *group_id)
{
    if (g_groups_list == NULL || group_id == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    rm_group_item_t *group_item;
    STAILQ_FOREACH(group_item, g_groups_list, next) {
        if (group_item->group_id && strcmp(group_item->group_id, group_id) == 0) {
            g_current_group = group_item;
            return ESP_OK;
        }
    }
    ESP_LOGE(TAG, "Group id %s not found", group_id);
    g_current_group = NULL;
    return ESP_ERR_NOT_FOUND;
}

esp_err_t rm_device_group_manager_init(void)
{
    /* Initialize the groups list */
    if (g_groups_list != NULL) {
        delete_all_group_items_in_list();
        rainmaker_app_backend_util_safe_free_and_set_null((void **)&g_groups_list);
    }
    g_groups_list = (struct rm_group_list_t *)malloc(sizeof(struct rm_group_list_t));
    if (g_groups_list == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for groups list");
        return ESP_FAIL;
    }
    STAILQ_INIT(g_groups_list);

    /* Initialize the devices list */
    if (g_devices_list != NULL) {
        delete_all_device_items_in_list();
        rainmaker_app_backend_util_safe_free_and_set_null((void **)&g_devices_list);
    }
    g_devices_list = (struct rm_device_list_t *)malloc(sizeof(struct rm_device_list_t));
    if (g_devices_list == NULL) {
        rainmaker_app_backend_util_safe_free_and_set_null((void **)&g_groups_list);
        ESP_LOGE(TAG, "Failed to allocate memory for devices list");
        return ESP_FAIL;
    }
    STAILQ_INIT(g_devices_list);

    /* Initialize the current group */
    g_current_group = NULL;

    return ESP_OK;
}

void rm_device_group_manager_deinit(void)
{
    if (g_groups_list) {
        delete_all_group_items_in_list();
        rainmaker_app_backend_util_safe_free_and_set_null((void **)&g_groups_list);
    }
    if (g_devices_list) {
        delete_all_device_items_in_list();
        rainmaker_app_backend_util_safe_free_and_set_null((void **)&g_devices_list);
    }
    g_current_group = NULL;
}

void rm_device_group_manager_clear_device(void)
{
    if (g_devices_list) {
        delete_all_device_items_in_list();
    }
}

bool rm_device_group_manager_is_device_in_list(const rm_device_item_t *item)
{
    if (item == NULL || g_devices_list == NULL) {
        return false;
    }
    rm_device_item_t *walk;
    STAILQ_FOREACH(walk, g_devices_list, next) {
        if (walk == item) {
            return true;
        }
    }
    return false;
}

void rm_device_group_manager_clear_group(void)
{
    if (g_groups_list) {
        delete_all_group_items_in_list();
    }
    g_current_group = NULL;
}

esp_err_t rm_device_group_manager_update_groups_list(void)
{
    char *response_data = NULL;
    int status_code = 0;
    esp_err_t ret = rainmaker_api_get_node_group_json(&status_code, &response_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get group information: %s", esp_err_to_name(ret));
        return ret;
    }
    if (status_code != 200 || response_data == NULL) {
        ESP_LOGE(TAG, "Get group info failed, status: %d", status_code);
        rainmaker_app_backend_util_safe_free(response_data);
        return ESP_FAIL;
    }
    ret = rainmaker_api_parse_node_group(response_data);
    rainmaker_app_backend_util_safe_free(response_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse group information: %s", esp_err_to_name(ret));
        rm_device_group_manager_clear_group();
        return ret;
    }
    /* Caller sets current group: restore previous name after refresh, or select_default_group() on first load */
    return ESP_OK;
}

esp_err_t rm_device_group_manager_update_devices_list(void)
{
    /* First, get the devices information */
    char *response_data = NULL;
    int status_code = 0;
    esp_err_t ret = rainmaker_api_get_nodes_json(NULL, &status_code, &response_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get devices information: %s", esp_err_to_name(ret));
        return ret;
    }
    if (status_code != 200 || response_data == NULL) {
        ESP_LOGE(TAG, "Get devices info failed, status: %d", status_code);
        rainmaker_app_backend_util_safe_free(response_data);
        return ESP_FAIL;
    }
    char *next_id = NULL;
    ret = rainmaker_api_parse_nodes(response_data, &next_id);
    rainmaker_app_backend_util_safe_free(response_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse devices information: %s", esp_err_to_name(ret));
        rm_device_group_manager_clear_device();
        return ret;
    }
    /* If the next_id is not NULL, there are more devices to get */
    while (next_id != NULL) {
        response_data = NULL;
        status_code = 0;
        ret = rainmaker_api_get_nodes_json(next_id, &status_code, &response_data);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get devices information: %s after getting %s", esp_err_to_name(ret), next_id);
            rainmaker_app_backend_util_safe_free(next_id);
            rm_device_group_manager_clear_device();
            return ret;
        }
        if (status_code != 200 || response_data == NULL) {
            ESP_LOGE(TAG, "Get devices info failed, status: %d after getting %s", status_code, next_id);
            rainmaker_app_backend_util_safe_free(next_id);
            rm_device_group_manager_clear_device();
            rainmaker_app_backend_util_safe_free(response_data);
            return ESP_FAIL;
        }
        /* Free the next_id */
        rainmaker_app_backend_util_safe_free_and_set_null((void **)&next_id);
        ret = rainmaker_api_parse_nodes(response_data, &next_id);
        rainmaker_app_backend_util_safe_free(response_data);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to parse devices information: %s after getting %s", esp_err_to_name(ret), next_id);
            rm_device_group_manager_clear_device();
            return ret;
        }
    }
    return ESP_OK;
}
