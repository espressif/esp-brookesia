/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rainmaker_api_handle.h"
#include "rainmaker_device_group_manager.h"
#include "rainmaker_list_manager.h"
#include "esp_log.h"
#include "rainmaker_user_api_hal.h"
#include "rainmaker_app_backend_util.h"
#include "rainmaker_standard_device.h"
#include "cJSON.h"
#include <stdio.h>

/* Free a group item that was never inserted into g_groups_list (clears nodes_list first). */
static void rainmaker_api_free_unlinked_group_item(rm_group_item_t *group_item)
{
    if (group_item == NULL) {
        return;
    }
    if (group_item->nodes_list != NULL) {
        rm_group_node_item_t *node_item, *tmp;
        STAILQ_FOREACH_SAFE(node_item, group_item->nodes_list, next, tmp) {
            STAILQ_REMOVE(group_item->nodes_list, node_item, rm_group_node_item, next);
            rainmaker_app_backend_util_safe_free((void *)node_item->node_id);
            rainmaker_app_backend_util_safe_free(node_item);
        }
        rainmaker_app_backend_util_safe_free(group_item->nodes_list);
    }
    rainmaker_app_backend_util_safe_free((void *)group_item->group_id);
    rainmaker_app_backend_util_safe_free(group_item->group_name);
    rainmaker_app_backend_util_safe_free(group_item);
}

static const char *TAG = "rainmaker_api_handle";
static bool g_is_initialized = false;
static const char *g_api_version = "v1";

#define RM_API_QUERY_PARAMS_GROUP_INFO "node_list=true&sub_groups=false&node_details=true&is_matter=false&matter_node_list=false&fabric_details=false"
#define RM_API_QUERY_PARAMS_DEVICE_INFO "node_details=true&status=true&config=true&params=true"

esp_err_t rainmaker_api_init(const char *base_url, rainmaker_login_failure_cb_t login_failure_cb, rainmaker_login_success_cb_t login_success_cb)
{
    if (g_is_initialized) {
        return ESP_OK;
    }
    app_rmaker_user_api_config_t config = {
        .refresh_token = NULL,
        .base_url = (char *)base_url,
        .api_version = (char *)g_api_version,
        .username = NULL,
        .password = NULL,
    };

    esp_err_t ret = rainmaker_user_api_hal_init(&config);
    /* If already initialized, treat as success */
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "User API init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register callbacks; ignore duplicates */
    if (login_failure_cb) {
        (void)rainmaker_user_api_hal_register_login_failure_callback((app_rmaker_user_api_login_failure_callback_t)login_failure_cb);
    }
    if (login_success_cb) {
        (void)rainmaker_user_api_hal_register_login_success_callback((app_rmaker_user_api_login_success_callback_t)login_success_cb);
    }
    g_is_initialized = true;
    return ESP_OK;
}

esp_err_t rainmaker_api_deinit(void)
{
    esp_err_t ret = rainmaker_user_api_hal_deinit();
    /* Ignore not-initialized */
    if (ret == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    g_is_initialized = false;
    return ret;
}

esp_err_t rainmaker_api_set_refresh_token(const char *refresh_token)
{
    return rainmaker_user_api_hal_set_refresh_token(refresh_token);
}

esp_err_t rainmaker_api_set_username_password(const char *username, const char *password)
{
    return rainmaker_user_api_hal_set_username_password(username, password);
}

esp_err_t rainmaker_api_login(void)
{
    return rainmaker_user_api_hal_login();
}

esp_err_t rainmaker_api_get_refresh_token(char **refresh_token)
{
    return rainmaker_user_api_hal_get_refresh_token(refresh_token);
}

esp_err_t rainmaker_api_get_node_group_json(int *out_status_code, char **out_json)
{
    if (!out_status_code || !out_json) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_status_code = 0;
    *out_json = NULL;

    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = "user/node_group",
        .api_query_params = RM_API_QUERY_PARAMS_GROUP_INFO,
        .api_payload = NULL,
    };
    return rainmaker_user_api_hal_generic(&request_config, out_status_code, out_json);
}

esp_err_t rainmaker_api_get_nodes_json(const char *next_id, int *out_status_code, char **out_json)
{
    if (!out_status_code || !out_json) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_status_code = 0;
    *out_json = NULL;

    char query_params[256] = {0};
    if (next_id && next_id[0]) {
        snprintf(query_params, sizeof(query_params), "%s&start_id=%s", RM_API_QUERY_PARAMS_DEVICE_INFO, next_id);
    } else {
        snprintf(query_params, sizeof(query_params), RM_API_QUERY_PARAMS_DEVICE_INFO);
    }
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = "user/nodes",
        .api_query_params = query_params,
        .api_payload = NULL,
    };
    return rainmaker_user_api_hal_generic(&request_config, out_status_code, out_json);
}

esp_err_t rainmaker_api_parse_node_group(const char *response_data)
{
    if (!response_data) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(response_data);
    if (!root) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    cJSON *groups = cJSON_GetObjectItem(root, "groups");
    if (!groups || !cJSON_IsArray(groups)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    int n_groups = cJSON_GetArraySize(groups);
    if (n_groups <= 0) {
        cJSON_Delete(root);
        return ESP_OK;
    }

    for (int i = 0; i < n_groups; i++) {
        cJSON *g = cJSON_GetArrayItem(groups, i);
        if (!g || !cJSON_IsObject(g)) {
            continue;
        }
        cJSON *gid = cJSON_GetObjectItem(g, "group_id");
        cJSON *gname = cJSON_GetObjectItem(g, "group_name");
        if (!gid || !cJSON_IsString(gid) || !gid->valuestring ||
                !gname || !cJSON_IsString(gname) || !gname->valuestring) {
            continue;
        }

        rm_group_item_t *group_item = (rm_group_item_t *)malloc(sizeof(rm_group_item_t));
        if (!group_item) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        group_item->group_id = strdup(gid->valuestring);
        if (!group_item->group_id) {
            rainmaker_app_backend_util_safe_free(group_item);
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        group_item->group_name = strdup(gname->valuestring);
        if (!group_item->group_name) {
            rainmaker_app_backend_util_safe_free((void *)group_item->group_id);
            rainmaker_app_backend_util_safe_free(group_item);
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        group_item->nodes_list = (struct rm_group_node_list_t *)malloc(sizeof(struct rm_group_node_list_t));
        if (!group_item->nodes_list) {
            rainmaker_app_backend_util_safe_free((void *)group_item->group_id);
            rainmaker_app_backend_util_safe_free(group_item->group_name);
            rainmaker_app_backend_util_safe_free(group_item);
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        STAILQ_INIT(group_item->nodes_list);

        cJSON *nodes = cJSON_GetObjectItem(g, "nodes");
        if (nodes && cJSON_IsArray(nodes)) {
            int nn = cJSON_GetArraySize(nodes);
            if (nn > 0) {
                for (int j = 0; j < nn; j++) {
                    cJSON *nid = cJSON_GetArrayItem(nodes, j);
                    if (nid && cJSON_IsString(nid) && nid->valuestring) {
                        rm_group_node_item_t *node_item = (rm_group_node_item_t *)malloc(sizeof(rm_group_node_item_t));
                        if (!node_item) {
                            rainmaker_api_free_unlinked_group_item(group_item);
                            cJSON_Delete(root);
                            return ESP_ERR_NO_MEM;
                        }
                        node_item->node_id = strdup(nid->valuestring);
                        if (!node_item->node_id) {
                            rainmaker_app_backend_util_safe_free(node_item);
                            rainmaker_api_free_unlinked_group_item(group_item);
                            cJSON_Delete(root);
                            return ESP_ERR_NO_MEM;
                        }
                        STAILQ_INSERT_TAIL(group_item->nodes_list, node_item, next);
                    }
                }
            }
        }
        STAILQ_INSERT_TAIL(g_groups_list, group_item, next);
    }
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t rainmaker_api_parse_nodes(const char *response_data, char **out_next_id)
{
    esp_err_t ret = ESP_OK;
    if (!response_data || !out_next_id) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_next_id = NULL;

    cJSON *root = cJSON_Parse(response_data);
    if (!root) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    cJSON *nodes = cJSON_GetObjectItem(root, "nodes");
    if (!nodes || !cJSON_IsArray(nodes)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    int nn = cJSON_GetArraySize(nodes);
    if (nn == 0) {
        cJSON_Delete(root);
        return ESP_OK;
    }

    cJSON *node_details = cJSON_GetObjectItem(root, "node_details");
    if (!node_details || !cJSON_IsArray(node_details)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    int n = cJSON_GetArraySize(node_details);
    if (n <= 0) {
        cJSON_Delete(root);
        return ESP_OK;
    }

    for (int i = 0; i < n; i++) {
        cJSON *node_obj = cJSON_GetArrayItem(node_details, i);
        if (!node_obj || !cJSON_IsObject(node_obj)) {
            continue;
        }

        cJSON *node_id = cJSON_GetObjectItem(node_obj, "node_id");
        if (!node_id || !cJSON_IsString(node_id) || !node_id->valuestring) {
            continue;
        }
        cJSON *status = cJSON_GetObjectItem(node_obj, "status");
        if (!status || !cJSON_IsObject(status)) {
            continue;
        }
        cJSON *conn = cJSON_GetObjectItem(status, "connectivity");
        if (!conn || !cJSON_IsObject(conn)) {
            continue;
        }
        cJSON *connected = cJSON_GetObjectItem(conn, "connected");
        if (!connected || !cJSON_IsBool(connected)) {
            continue;
        }
        cJSON *last_timestamp = cJSON_GetObjectItem(conn, "timestamp");
        if (!last_timestamp || !cJSON_IsNumber(last_timestamp)) {
            continue;
        }

        cJSON *config = cJSON_GetObjectItem(node_obj, "config");
        if (!config || !cJSON_IsObject(config)) {
            continue;
        }
        cJSON *info = cJSON_GetObjectItem(config, "info");
        if (!info || !cJSON_IsObject(info)) {
            continue;
        }
        cJSON *fw_version = cJSON_GetObjectItem(info, "fw_version");
        if (!fw_version || !cJSON_IsString(fw_version) || !fw_version->valuestring) {
            continue;
        }

        cJSON *params = cJSON_GetObjectItem(node_obj, "params");
        if (!params || !cJSON_IsObject(params)) {
            continue;
        }

        bool whether_has_system_service = false;
        /* Parse the whether_has_system_service in params (optional) */
        cJSON *system_service = cJSON_GetObjectItem(params, "System");
        if (system_service && cJSON_IsObject(system_service)) {
            whether_has_system_service = true;
        } else {
            whether_has_system_service = false;
        }
        /* Parse the Time Zone in params (optional) */
        cJSON *time_zone = cJSON_GetObjectItem(params, "Time");
        cJSON *time_zone_value = NULL;
        if (time_zone && cJSON_IsObject(time_zone)) {
            /* Only get the TZ value here */
            time_zone_value = cJSON_GetObjectItem(time_zone, "TZ");
        }
        cJSON *device = NULL;
        rm_device_item_t *device_item = NULL;
        device = cJSON_GetObjectItem(params, RM_APP_DEVICE_TYPE_STRING_LIGHT);
        if (device && cJSON_IsObject(device)) {
            /* Light device */
            device_item = (rm_device_item_t *)malloc(sizeof(rm_device_item_t));
            if (!device_item) {
                cJSON_Delete(root);
                return ESP_ERR_NO_MEM;
            }
            device_item->node_id = strdup(node_id->valuestring);
            if (!device_item->node_id) {
                rainmaker_app_backend_util_safe_free(device_item);
                cJSON_Delete(root);
                return ESP_ERR_NO_MEM;
            }
            device_item->fw_version = strdup(fw_version->valuestring);
            if (!device_item->fw_version) {
                rainmaker_app_backend_util_safe_free((void *)device_item->node_id);
                rainmaker_app_backend_util_safe_free(device_item);
                cJSON_Delete(root);
                return ESP_ERR_NO_MEM;
            }
            if (time_zone_value && cJSON_IsString(time_zone_value) && time_zone_value->valuestring) {
                device_item->timezone = strdup(time_zone_value->valuestring);
                if (!device_item->timezone) {
                    rainmaker_app_backend_util_safe_free((void *)device_item->node_id);
                    rainmaker_app_backend_util_safe_free((void *)device_item->fw_version);
                    rainmaker_app_backend_util_safe_free(device_item);
                    cJSON_Delete(root);
                    return ESP_ERR_NO_MEM;
                }
            }
            ret = rainmaker_standard_light_parse_parameters(device, device_item);
            if (ret != ESP_OK) {
                rainmaker_app_backend_util_safe_free((void *)device_item->node_id);
                rainmaker_app_backend_util_safe_free((void *)device_item->fw_version);
                rainmaker_app_backend_util_safe_free((void *)device_item->timezone);
                rainmaker_app_backend_util_safe_free(device_item);
                /* Skip invalid light device */
                continue;
            }
            device_item->online = (cJSON_IsTrue(connected) ? true : false);
            device_item->last_timestamp = (int64_t)last_timestamp->valuedouble;
            device_item->whether_has_system_service = whether_has_system_service;
        } else {
            device = cJSON_GetObjectItem(params, RM_APP_DEVICE_TYPE_STRING_SWITCH);
            if (device && cJSON_IsObject(device)) {
                /* Switch device */
                device_item = (rm_device_item_t *)malloc(sizeof(rm_device_item_t));
                if (!device_item) {
                    cJSON_Delete(root);
                    return ESP_ERR_NO_MEM;
                }
                device_item->node_id = strdup(node_id->valuestring);
                if (!device_item->node_id) {
                    rainmaker_app_backend_util_safe_free(device_item);
                    cJSON_Delete(root);
                    return ESP_ERR_NO_MEM;
                }
                device_item->fw_version = strdup(fw_version->valuestring);
                if (!device_item->fw_version) {
                    rainmaker_app_backend_util_safe_free((void *)device_item->node_id);
                    rainmaker_app_backend_util_safe_free(device_item);
                    cJSON_Delete(root);
                    return ESP_ERR_NO_MEM;
                }
                if (time_zone_value && cJSON_IsString(time_zone_value) && time_zone_value->valuestring) {
                    device_item->timezone = strdup(time_zone_value->valuestring);
                    if (!device_item->timezone) {
                        rainmaker_app_backend_util_safe_free((void *)device_item->node_id);
                        rainmaker_app_backend_util_safe_free((void *)device_item->fw_version);
                        rainmaker_app_backend_util_safe_free(device_item);
                        cJSON_Delete(root);
                        return ESP_ERR_NO_MEM;
                    }
                }
                ret = rainmaker_standard_switch_parse_parameters(device, device_item);
                if (ret != ESP_OK) {
                    rainmaker_app_backend_util_safe_free((void *)device_item->node_id);
                    rainmaker_app_backend_util_safe_free((void *)device_item->fw_version);
                    rainmaker_app_backend_util_safe_free((void *)device_item->timezone);
                    rainmaker_app_backend_util_safe_free(device_item);
                    /* Skip invalid switch device */
                    continue;
                }
                device_item->online = (cJSON_IsTrue(connected) ? true : false);
                device_item->last_timestamp = (int64_t)last_timestamp->valuedouble;
                device_item->whether_has_system_service = whether_has_system_service;
            } else {
                device = cJSON_GetObjectItem(params, RM_APP_DEVICE_TYPE_STRING_FAN);
                if (device && cJSON_IsObject(device)) {
                    /* Fan device */
                    device_item = (rm_device_item_t *)malloc(sizeof(rm_device_item_t));
                    if (!device_item) {
                        cJSON_Delete(root);
                        return ESP_ERR_NO_MEM;
                    }
                    device_item->node_id = strdup(node_id->valuestring);
                    if (!device_item->node_id) {
                        rainmaker_app_backend_util_safe_free(device_item);
                        cJSON_Delete(root);
                        return ESP_ERR_NO_MEM;
                    }
                    device_item->fw_version = strdup(fw_version->valuestring);
                    if (!device_item->fw_version) {
                        rainmaker_app_backend_util_safe_free((void *)device_item->node_id);
                        rainmaker_app_backend_util_safe_free(device_item);
                        cJSON_Delete(root);
                        return ESP_ERR_NO_MEM;
                    }
                    if (time_zone_value && cJSON_IsString(time_zone_value) && time_zone_value->valuestring) {
                        device_item->timezone = strdup(time_zone_value->valuestring);
                        if (!device_item->timezone) {
                            rainmaker_app_backend_util_safe_free((void *)device_item->node_id);
                            rainmaker_app_backend_util_safe_free((void *)device_item->fw_version);
                            rainmaker_app_backend_util_safe_free(device_item);
                            cJSON_Delete(root);
                            return ESP_ERR_NO_MEM;
                        }
                    }
                    ret = rainmaker_standard_fan_parse_parameters(device, device_item);
                    if (ret != ESP_OK) {
                        rainmaker_app_backend_util_safe_free((void *)device_item->node_id);
                        rainmaker_app_backend_util_safe_free((void *)device_item->fw_version);
                        rainmaker_app_backend_util_safe_free((void *)device_item->timezone);
                        rainmaker_app_backend_util_safe_free(device_item);
                        /* Skip invalid fan device */
                        continue;
                    }
                    device_item->online = (cJSON_IsTrue(connected) ? true : false);
                    device_item->last_timestamp = (int64_t)last_timestamp->valuedouble;
                    device_item->whether_has_system_service = whether_has_system_service;
                } else {
                    /* Not supported device */
                    continue;
                }
            }
        }
        if (device_item != NULL) {
            STAILQ_INSERT_TAIL(g_devices_list, device_item, next);
            /* Parse the Schedule in params (optional) {"Schedule":{"Schedules":[{"action":{"Switch":{"Power":true}},"enabled":true,"id":"XXXI","name":"Test","triggers":[{"d":21,"m":1071}]},...]}} */
            cJSON *schedule = cJSON_GetObjectItem(params, "Schedule");
            if (schedule && cJSON_IsObject(schedule)) {
                cJSON *schedules = cJSON_GetObjectItem(schedule, "Schedules");
                if (schedules && cJSON_IsArray(schedules)) {
                    int ns = cJSON_GetArraySize(schedules);
                    for (int j = 0; j < ns; j++) {
                        cJSON *s = cJSON_GetArrayItem(schedules, j);
                        if (!s || !cJSON_IsObject(s)) {
                            continue;
                        }
                        cJSON *sid = cJSON_GetObjectItem(s, "id");
                        if (!sid || !cJSON_IsString(sid) || !sid->valuestring) {
                            continue;
                        }
                        cJSON *enabled = cJSON_GetObjectItem(s, "enabled");
                        if (!enabled || !cJSON_IsBool(enabled)) {
                            continue;
                        }
                        cJSON *name = cJSON_GetObjectItem(s, "name");
                        if (!name || !cJSON_IsString(name) || !name->valuestring) {
                            continue;
                        }
                        cJSON *triggers = cJSON_GetObjectItem(s, "triggers");
                        if (!triggers || !cJSON_IsArray(triggers)) {
                            continue;
                        }
                        int nt = cJSON_GetArraySize(triggers);
                        if (nt <= 0) {
                            continue;
                        }
                        uint16_t minutes = 0;
                        uint8_t days_of_week = 0;
                        uint8_t date_of_month = 0;
                        uint16_t month = 0;
                        uint16_t year = 0;
                        bool is_repeating = false;
                        for (int k = 0; k < nt; k++) {
                            cJSON *t = cJSON_GetArrayItem(triggers, k);
                            if (!t || !cJSON_IsObject(t)) {
                                continue;
                            }
                            cJSON *m = cJSON_GetObjectItem(t, "m");
                            if (!m || !cJSON_IsNumber(m)) {
                                continue;
                            }
                            minutes = (uint16_t)m->valuedouble;
                            cJSON *d = cJSON_GetObjectItem(t, "d");
                            if (!d || !cJSON_IsNumber(d)) {
                                continue;
                            }
                            days_of_week = (uint8_t)d->valuedouble;
                            cJSON *dd = cJSON_GetObjectItem(t, "dd");
                            if (!dd || !cJSON_IsNumber(dd)) {
                                continue;
                            }
                            date_of_month = (uint8_t)dd->valuedouble;
                            cJSON *mm = cJSON_GetObjectItem(t, "mm");
                            if (!mm || !cJSON_IsNumber(mm)) {
                                continue;
                            }
                            month = (uint16_t)mm->valuedouble;
                            cJSON *yy = cJSON_GetObjectItem(t, "yy");
                            if (!yy || !cJSON_IsNumber(yy)) {
                                continue;
                            }
                            year = (uint16_t)yy->valuedouble;
                            cJSON *r = cJSON_GetObjectItem(t, "r");
                            if (!r || !cJSON_IsBool(r)) {
                                continue;
                            }
                            is_repeating = r->valueint;
                        }
                        cJSON *action = cJSON_GetObjectItem(s, "action");
                        if (!action || !cJSON_IsObject(action)) {
                            continue;
                        }
                        const char *schedule_id = sid->valuestring;
                        char *action_str = cJSON_PrintUnformatted(action);
                        if (!action_str) {
                            continue;
                        }
                        rm_node_action_item_t *node_action_item = rm_list_manager_create_new_node_action_item(device_item->node_id);
                        if (node_action_item == NULL) {
                            ESP_LOGW(TAG, "Failed to create new schedule node action item node_id=%s", device_item->node_id);
                            cJSON_free(action_str);
                            continue;
                        }
                        node_action_item->action = action_str;
                        rm_list_item_t *existing = rm_list_manager_find_item_by_id(schedule_id);
                        if (existing != NULL) {
                            /* Same schedule id: only update node action for this node */
                            esp_err_t err = rm_list_manager_add_node_action_to_list_item(existing, node_action_item);
                            if (err != ESP_OK) {
                                ESP_LOGW(TAG, "Failed to add schedule node action to schedule item schedule_id=%s", schedule_id);
                                /* Need to free the node action item */
                                rm_list_manager_clean_node_action_item_memory(node_action_item);
                            }
                        } else {
                            rm_list_item_t *new_schedule_item = rm_list_manager_create_new_item_with_name(schedule_id, name->valuestring, RM_LIST_ENTITY_TYPE_SCHEDULE);
                            if (new_schedule_item == NULL) {
                                ESP_LOGW(TAG, "Failed to create new schedule item schedule_id=%s", schedule_id);
                                rm_list_manager_clean_node_action_item_memory(node_action_item);
                                continue;
                            }
                            new_schedule_item->enabled = enabled->valueint;
                            new_schedule_item->trigger_type = RM_SCHEDULE_TRIGGER_TYPE_ABSOLUTE;
                            new_schedule_item->trigger.days_of_week.minutes = minutes;
                            new_schedule_item->trigger.days_of_week.days_of_week = days_of_week;
                            new_schedule_item->trigger.days_of_week.date_of_month = date_of_month;
                            new_schedule_item->trigger.days_of_week.month = month;
                            new_schedule_item->trigger.days_of_week.year = year;
                            new_schedule_item->trigger.days_of_week.is_repeating = is_repeating;
                            new_schedule_item->node_actions_list = NULL;
                            ret = rm_list_manager_add_item_to_list(new_schedule_item);
                            if (ret != ESP_OK) {
                                ESP_LOGW(TAG, "Failed to add schedule to list schedule_id=%s", schedule_id);
                                rm_list_manager_clean_item_memory(new_schedule_item);
                                rm_list_manager_clean_node_action_item_memory(node_action_item);
                                continue;
                            }
                            ret = rm_list_manager_add_node_action_to_list_item(new_schedule_item, node_action_item);
                            if (ret != ESP_OK) {
                                ESP_LOGW(TAG, "Failed to add schedule node action to schedule item schedule_id=%s", schedule_id);
                                /* Need to free the node action item */
                                rm_list_manager_clean_node_action_item_memory(node_action_item);
                            }
                        }
                        /* No need free the node action because it is managed by the schedule manager */
                    }
                }
            }
            /* Parse the Scene in params (optional) {"Scenes":{"Scenes":[{"action":{...},"id":"ABCD","name":"My Scene"}]}} */
            cJSON *scenes = cJSON_GetObjectItem(params, "Scenes");
            if (scenes && cJSON_IsObject(scenes)) {
                cJSON *scenes_array = cJSON_GetObjectItem(scenes, "Scenes");
                if (scenes_array && cJSON_IsArray(scenes_array)) {
                    int ns = cJSON_GetArraySize(scenes_array);
                    for (int j = 0; j < ns; j++) {
                        cJSON *s = cJSON_GetArrayItem(scenes_array, j);
                        if (!s || !cJSON_IsObject(s)) {
                            continue;
                        }
                        cJSON *sid = cJSON_GetObjectItem(s, "id");
                        if (!sid || !cJSON_IsString(sid) || !sid->valuestring) {
                            continue;
                        }
                        cJSON *name = cJSON_GetObjectItem(s, "name");
                        if (!name || !cJSON_IsString(name) || !name->valuestring) {
                            continue;
                        }
                        cJSON *action = cJSON_GetObjectItem(s, "action");
                        if (!action || !cJSON_IsObject(action)) {
                            continue;
                        }
                        const char *scene_id = sid->valuestring;
                        char *action_str = cJSON_PrintUnformatted(action);
                        if (!action_str) {
                            continue;
                        }
                        rm_node_action_item_t *node_action_item = rm_list_manager_create_new_node_action_item(device_item->node_id);
                        if (node_action_item == NULL) {
                            ESP_LOGW(TAG, "Failed to create new scene node action item node_id=%s", device_item->node_id);
                            cJSON_free(action_str);
                            continue;
                        }
                        node_action_item->action = action_str;
                        rm_list_item_t *existing = rm_list_manager_find_item_by_id(scene_id);
                        if (existing != NULL) {
                            esp_err_t err = rm_list_manager_add_node_action_to_list_item(existing, node_action_item);
                            if (err != ESP_OK) {
                                ESP_LOGW(TAG, "Failed to add scene node action to scene item scene_id=%s", scene_id);
                                rm_list_manager_clean_node_action_item_memory(node_action_item);
                            }
                        } else {
                            rm_list_item_t *new_scene_item = rm_list_manager_create_new_item_with_name(scene_id, name->valuestring, RM_LIST_ENTITY_TYPE_SCENE);
                            if (new_scene_item == NULL) {
                                ESP_LOGW(TAG, "Failed to create new scene item scene_id=%s", scene_id);
                                rm_list_manager_clean_node_action_item_memory(node_action_item);
                                continue;
                            }
                            new_scene_item->enabled = true;
                            new_scene_item->node_actions_list = NULL;
                            ret = rm_list_manager_add_item_to_list(new_scene_item);
                            if (ret != ESP_OK) {
                                ESP_LOGW(TAG, "Failed to add scene to list scene_id=%s", scene_id);
                                rm_list_manager_clean_item_memory(new_scene_item);
                                rm_list_manager_clean_node_action_item_memory(node_action_item);
                                continue;
                            }
                            ret = rm_list_manager_add_node_action_to_list_item(new_scene_item, node_action_item);
                            if (ret != ESP_OK) {
                                ESP_LOGW(TAG, "Failed to add scene node action to scene item scene_id=%s", scene_id);
                                rm_list_manager_clean_node_action_item_memory(node_action_item);
                            }
                        }
                    }
                }
            }
        }
    }
    cJSON *next_id = cJSON_GetObjectItem(root, "next_id");
    if (next_id && cJSON_IsString(next_id) && next_id->valuestring) {
        *out_next_id = strdup(next_id->valuestring);
    }
    cJSON_Delete(root);
    return ESP_OK;
}

/*
 * Device parameter writes (HTTP PUT user/nodes/params): synchronous on the calling thread.
 * Success paths accept status 200 or 207; there is no backend-registered lv_async_call or LVGL
 * timer from this module. UI (e.g. ui_Screen_Light_Detail) calls rainmaker_standard_*_set_* then
 * refreshes widgets in the same event handler stack — no deferred callback holds lv_obj_t* from
 * here. Deferred UI work elsewhere uses lv_async_call only for language/theme (ui_common.c) and
 * lv_timer for Home refresh / Login / nav auto-hide; those paths should not capture device pointers
 * across screen delete (see ui_nav / LV_EVENT_DELETE handlers on those screens).
 */
esp_err_t rainmaker_api_set_device_parameters(const char *node_id, const char *payload, const char **out_err_reason)
{
    if (!payload) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid payload");
        }
        return ESP_ERR_INVALID_ARG;
    }

    int actual_payload_size = strlen(payload) + 100;
    char *actual_payload = (char *)calloc(1, actual_payload_size);
    if (!actual_payload) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to allocate memory for payload");
        }
        return ESP_ERR_NO_MEM;
    }
    if (node_id) {
        snprintf(actual_payload, actual_payload_size, "[{\"node_id\":\"%s\",\"payload\":%s}]", node_id, payload);
    } else {
        snprintf(actual_payload, actual_payload_size, "%s", payload);
    }
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .payload_is_json = true,
        .api_type = APP_RMAKER_USER_API_TYPE_PUT,
        .api_name = "user/nodes/params",
        .api_query_params = NULL,
        .api_payload = actual_payload,
    };
    int status_code = 0;
    char *response_data = NULL;
    esp_err_t ret = rainmaker_user_api_hal_generic(&request_config, &status_code, &response_data);
    rainmaker_app_backend_util_safe_free((void *)actual_payload);
    if (ret != ESP_OK) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to set device parameters");
        }
        return ret;
    }
    if (status_code != 200 && status_code != 207) {
        ESP_LOGE(TAG, "Failed to set device parameters: %d, %s", status_code, response_data);
        rainmaker_app_backend_util_safe_free((void *)response_data);
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to set device parameters");
        }
        return ESP_FAIL;
    } else {
        /* Follow json [{"node_id":"744DBD603188","status":"failure","description":"Node is offline"}] to parse the response */
        cJSON *root = cJSON_Parse(response_data);
        rainmaker_app_backend_util_safe_free((void *)response_data);
        if (!root) {
            if (out_err_reason) {
                *out_err_reason = strdup("Invalid response");
            }
            return ESP_ERR_INVALID_RESPONSE;
        }
        /* Is array of objects */
        int nn = cJSON_GetArraySize(root);
        if (nn > 0) {
            for (int i = 0; i < nn; i++) {
                cJSON *n = cJSON_GetArrayItem(root, i);
                if (!n || !cJSON_IsObject(n)) {
                    cJSON_Delete(root);
                    if (out_err_reason) {
                        *out_err_reason = strdup("Invalid response");
                    }
                    return ESP_ERR_INVALID_RESPONSE;
                }
                cJSON *status = cJSON_GetObjectItem(n, "status");
                if (!status || !cJSON_IsString(status) || !status->valuestring) {
                    if (out_err_reason) {
                        *out_err_reason = strdup("Invalid status");
                    }
                    cJSON_Delete(root);
                    return ESP_ERR_INVALID_RESPONSE;
                }
                cJSON *description = cJSON_GetObjectItem(n, "description");
                if (!description || !cJSON_IsString(description) || !description->valuestring) {
                    if (out_err_reason) {
                        *out_err_reason = strdup("Invalid description");
                    }
                    cJSON_Delete(root);
                    return ESP_ERR_INVALID_RESPONSE;
                }
                if (strcmp(status->valuestring, "failure") == 0) {
                    if (out_err_reason) {
                        *out_err_reason = strdup(description->valuestring);
                    }
                    cJSON_Delete(root);
                    return ESP_FAIL;
                }
            }
        } else {
            if (out_err_reason) {
                *out_err_reason = strdup("Invalid response");
            }
            cJSON_Delete(root);
            return ESP_ERR_INVALID_RESPONSE;
        }
        cJSON_Delete(root);
        return ESP_OK;
    }
}

esp_err_t rainmaker_api_remove_device(const char *node_id, const char **out_err_reason)
{
    if (!node_id) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    snprintf(payload, sizeof(payload), "{\"node_id\":\"%s\", \"operation\":\"remove\"}", node_id);
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .payload_is_json = true,
        .api_type = APP_RMAKER_USER_API_TYPE_PUT,
        .api_name = "user/nodes/mapping",
        .api_query_params = NULL,
        .api_payload = payload,
    };
    int status_code = 0;
    char *response_data = NULL;
    esp_err_t ret = rainmaker_user_api_hal_generic(&request_config, &status_code, &response_data);
    if (ret != ESP_OK) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to remove device");
        }
        return ret;
    }
    if (status_code != 200) {
        rainmaker_app_backend_util_safe_free((void *)response_data);
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to remove device");
        }
        return ESP_FAIL;
    } else {
        /* Parse the response {
            "status": "success",
            "description": "Success description"
        }*/
        cJSON *root = cJSON_Parse(response_data);
        rainmaker_app_backend_util_safe_free((void *)response_data);
        if (!root) {
            if (out_err_reason) {
                *out_err_reason = strdup("Invalid response");
            }
            return ESP_ERR_INVALID_RESPONSE;
        }
        cJSON *status = cJSON_GetObjectItem(root, "status");
        if (!status || !cJSON_IsString(status) || !status->valuestring) {
            if (out_err_reason) {
                *out_err_reason = strdup("Invalid status");
            }
            cJSON_Delete(root);
            return ESP_ERR_INVALID_RESPONSE;
        }
        if (strcmp(status->valuestring, "success") != 0) {
            cJSON *description = cJSON_GetObjectItem(root, "description");
            if (!description || !cJSON_IsString(description) || !description->valuestring) {
                if (out_err_reason) {
                    *out_err_reason = strdup("Failed to remove device");
                }
                cJSON_Delete(root);
                return ESP_FAIL;
            }
            if (out_err_reason) {
                *out_err_reason = strdup(description->valuestring);
            }
            cJSON_Delete(root);
            return ESP_FAIL;
        }
        cJSON_Delete(root);
        return ESP_OK;
    }
}

esp_err_t rainmaker_api_check_ota_update(const char *node_id, rainmaker_ota_update_info_t *ota_update_info, const char **out_err_reason)
{
    if (!node_id) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    if (!ota_update_info) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid OTA update info");
        }
        return ESP_ERR_INVALID_ARG;
    }
    /* Initialize the OTA update info */
    ota_update_info->ota_available = false;
    ota_update_info->new_firmware_size = 0;
    memset(ota_update_info->new_firmware_version, 0, sizeof(ota_update_info->new_firmware_version));
    memset(ota_update_info->ota_job_id, 0, sizeof(ota_update_info->ota_job_id));

    /* Check if there is a new firmware update available */
    char query_params[128] = {0};
    snprintf(query_params, sizeof(query_params), "node_id=%s", node_id);
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = "user/nodes/ota_update",
        .api_query_params = (char *)query_params,
        .api_payload = NULL,
    };
    int status_code = 0;
    char *response_data = NULL;
    esp_err_t ret = rainmaker_user_api_hal_generic(&request_config, &status_code, &response_data);
    if (ret != ESP_OK) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to check OTA update");
        }
        return ret;
    }
    if (status_code != 200) {
        rainmaker_app_backend_util_safe_free((void *)response_data);
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to check OTA update");
        }
        return ESP_FAIL;
    }
    /* Parse the response {
    "status": "success",
    "ota_available": true,
    "fw_version": "1.1",
    "ota_job_id": "h5eStwTYcBD5VAtyEqymtt",
    "file_size": 1609728,
    "stream_id": "hzoMFynJ8hoqkVEcqzCLGo"
    }*/
    cJSON *root = cJSON_Parse(response_data);
    rainmaker_app_backend_util_safe_free((void *)response_data);
    if (!root) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid response");
        }
        return ESP_ERR_INVALID_RESPONSE;
    }
    cJSON *status = cJSON_GetObjectItem(root, "status");
    if (!status || !cJSON_IsString(status) || !status->valuestring) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid status");
        }
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (strcmp(status->valuestring, "success") != 0) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to check OTA update");
        }
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    cJSON *ota_available = cJSON_GetObjectItem(root, "ota_available");
    if (!ota_available || !cJSON_IsBool(ota_available)) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid OTA available");
        }
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    ota_update_info->ota_available = cJSON_IsTrue(ota_available) ? true : false;
    if (!ota_update_info->ota_available) {
        cJSON_Delete(root);
        return ESP_OK;
    }
    cJSON *fw_version = cJSON_GetObjectItem(root, "fw_version");
    if (!fw_version || !cJSON_IsString(fw_version) || !fw_version->valuestring) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid firmware version");
        }
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    snprintf(ota_update_info->new_firmware_version, sizeof(ota_update_info->new_firmware_version), "%s",
             fw_version->valuestring);
    cJSON *file_size = cJSON_GetObjectItem(root, "file_size");
    if (!file_size || !cJSON_IsNumber(file_size)) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid file size");
        }
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    ota_update_info->new_firmware_size = (size_t)file_size->valuedouble;
    cJSON *ota_job_id = cJSON_GetObjectItem(root, "ota_job_id");
    if (!ota_job_id || !cJSON_IsString(ota_job_id) || !ota_job_id->valuestring) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid OTA job ID");
        }
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    snprintf(ota_update_info->ota_job_id, sizeof(ota_update_info->ota_job_id), "%s", ota_job_id->valuestring);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t rainmaker_api_start_ota_update(const char *node_id, const char *ota_job_id, const char **out_err_reason)
{
    if (!node_id || !ota_job_id) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID or OTA job ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    snprintf(payload, sizeof(payload), "{\"node_id\":\"%s\",\"ota_job_id\":\"%s\"}", node_id, ota_job_id);
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .payload_is_json = true,
        .api_type = APP_RMAKER_USER_API_TYPE_POST,
        .api_name = "user/nodes/ota_update",
        .api_query_params = NULL,
        .api_payload = payload,
    };
    int status_code = 0;
    char *response_data = NULL;
    esp_err_t ret = rainmaker_user_api_hal_generic(&request_config, &status_code, &response_data);
    if (ret != ESP_OK) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to start OTA update");
        }
        return ret;
    }
    if (status_code != 200) {
        rainmaker_app_backend_util_safe_free((void *)response_data);
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to start OTA update");
        }
        return ESP_FAIL;
    }
    /* Parse the response {
        "status": "success",
        "description": "Success description"
    }*/
    cJSON *root = cJSON_Parse(response_data);
    rainmaker_app_backend_util_safe_free((void *)response_data);
    if (!root) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid response");
        }
        return ESP_ERR_INVALID_RESPONSE;
    }
    cJSON *status = cJSON_GetObjectItem(root, "status");
    if (!status || !cJSON_IsString(status) || !status->valuestring) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid status");
        }
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (strcmp(status->valuestring, "success") != 0) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to start OTA update");
        }
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t rainmaker_api_delete_schedule(const char *node_id, const char *schedule_id, const char **out_err_reason)
{
    if (!node_id || !schedule_id) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID or schedule ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    snprintf(payload, sizeof(payload), "{\"Schedule\":{\"Schedules\":[{\"id\":\"%s\",\"operation\":\"remove\"}]}}", schedule_id);
    return rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
}

esp_err_t rainmaker_api_enable_schedule(const char *node_id, const char *schedule_id, bool enabled, const char **out_err_reason)
{
    if (!node_id || !schedule_id) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID or schedule ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    if (enabled) {
        snprintf(payload, sizeof(payload), "{\"Schedule\":{\"Schedules\":[{\"id\":\"%s\",\"operation\":\"enable\"}]}}", schedule_id);
    } else {
        snprintf(payload, sizeof(payload), "{\"Schedule\":{\"Schedules\":[{\"id\":\"%s\",\"operation\":\"disable\"}]}}", schedule_id);
    }
    return rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
}

esp_err_t rainmaker_api_update_schedule(const char *node_id, const rm_schedule_update_info_t *schedule_update_info, const char **out_err_reason)
{
    if (!node_id || !schedule_update_info) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID or schedule update info");
        }
        return ESP_ERR_INVALID_ARG;
    }

    if (!schedule_update_info->schedule_id || schedule_update_info->schedule_id[0] == '\0') {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid schedule ID");
        }
        return ESP_ERR_INVALID_ARG;
    }

    if (!schedule_update_info->name || schedule_update_info->name[0] == '\0') {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid name");
        }
        return ESP_ERR_INVALID_ARG;
    }
    if (schedule_update_info->node_action_type != RM_NODE_ACTION_EDIT && schedule_update_info->node_action_type != RM_NODE_ACTION_ADD) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node action type");
        }
        return ESP_ERR_INVALID_ARG;
    }

    const char *operation_str = (schedule_update_info->node_action_type == RM_NODE_ACTION_EDIT) ? "edit" : "add";

    cJSON *action_obj = NULL;
    if (schedule_update_info->action_json && schedule_update_info->action_json[0] != '\0') {
        action_obj = cJSON_Parse(schedule_update_info->action_json);
        if (!action_obj || !cJSON_IsObject(action_obj)) {
            if (action_obj) {
                cJSON_Delete(action_obj);
            }
            if (out_err_reason) {
                *out_err_reason = strdup("Invalid action_json");
            }
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid action_json");
        }
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *schedule_item = cJSON_CreateObject();
    if (!schedule_item) {
        cJSON_Delete(action_obj);
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to create schedule item");
        }
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(schedule_item, "action", action_obj);
    cJSON_AddNullToObject(schedule_item, "flags");
    cJSON_AddStringToObject(schedule_item, "id", schedule_update_info->schedule_id);
    cJSON_AddNullToObject(schedule_item, "info");
    cJSON_AddStringToObject(schedule_item, "name", schedule_update_info->name);
    cJSON_AddStringToObject(schedule_item, "operation", operation_str);

    /* triggers: [{"d": days_of_week, "m": minutes}] */
    cJSON *triggers = cJSON_CreateArray();
    if (!triggers) {
        cJSON_Delete(schedule_item);
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to create triggers array");
        }
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(schedule_item, "triggers", triggers);
    if (schedule_update_info->trigger_type == RM_SCHEDULE_TRIGGER_TYPE_ABSOLUTE) {
        cJSON *t = cJSON_CreateObject();
        if (t) {
            cJSON_AddNumberToObject(t, "m", (double)schedule_update_info->trigger.days_of_week.minutes);
            cJSON_AddNumberToObject(t, "d", (double)schedule_update_info->trigger.days_of_week.days_of_week);
            if (schedule_update_info->trigger.days_of_week.date_of_month) {
                cJSON_AddNumberToObject(t, "dd", (double)schedule_update_info->trigger.days_of_week.date_of_month);
            }
            if (schedule_update_info->trigger.days_of_week.month) {
                cJSON_AddNumberToObject(t, "mm", (double)schedule_update_info->trigger.days_of_week.month);
            }
            if (schedule_update_info->trigger.days_of_week.year) {
                cJSON_AddNumberToObject(t, "yy", (double)schedule_update_info->trigger.days_of_week.year);
            }
            if (schedule_update_info->trigger.days_of_week.is_repeating) {
                cJSON_AddBoolToObject(t, "r", true);
            }
            cJSON_AddItemToArray(triggers, t);
        }
    } else if (schedule_update_info->trigger_type == RM_SCHEDULE_TRIGGER_TYPE_RELATIVE) {
        cJSON *t = cJSON_CreateObject();
        if (t) {
            cJSON_AddNumberToObject(t, "rsec", (double)(schedule_update_info->trigger.relative.relative_seconds));
            cJSON_AddItemToArray(triggers, t);
        }
    }
    cJSON_AddNullToObject(schedule_item, "validity");

    cJSON *schedules_arr = cJSON_CreateArray();
    if (!schedules_arr) {
        cJSON_Delete(schedule_item);
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to create schedules array");
        }
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToArray(schedules_arr, schedule_item);

    cJSON *schedule_wrap = cJSON_CreateObject();
    if (!schedule_wrap) {
        cJSON_Delete(schedules_arr);
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to create Schedule wrapper");
        }
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(schedule_wrap, "Schedules", schedules_arr);

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        cJSON_Delete(schedule_wrap);
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to create payload root");
        }
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(root, "Schedule", schedule_wrap);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to print payload");
        }
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
    cJSON_free(payload);
    return ret;
}

esp_err_t rainmaker_api_delete_scene(const char *node_id, const char *scene_id, const char **out_err_reason)
{
    if (!node_id || !scene_id) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID or scene ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    snprintf(payload, sizeof(payload), "{\"Scenes\":{\"Scenes\":[{\"id\":\"%s\",\"operation\":\"remove\"}]}}", scene_id);
    return rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
}

esp_err_t rainmaker_api_update_scene(const char *node_id, const rm_scene_update_info_t *scene_update_info, const char **out_err_reason)
{
    if (!node_id || !scene_update_info) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID or scene update info");
        }
        return ESP_ERR_INVALID_ARG;
    }
    if (!scene_update_info->scene_id || scene_update_info->scene_id[0] == '\0') {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid scene ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    if (!scene_update_info->name || scene_update_info->name[0] == '\0') {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid name");
        }
        return ESP_ERR_INVALID_ARG;
    }

    if (scene_update_info->node_action_type != RM_NODE_ACTION_EDIT && scene_update_info->node_action_type != RM_NODE_ACTION_ADD) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node action type");
        }
        return ESP_ERR_INVALID_ARG;
    }

    const char *operation_str = (scene_update_info->node_action_type == RM_NODE_ACTION_EDIT) ? "edit" : "add";
    cJSON *action_obj = NULL;
    if (scene_update_info->action_json && scene_update_info->action_json[0] != '\0') {
        action_obj = cJSON_Parse(scene_update_info->action_json);
        if (!action_obj || !cJSON_IsObject(action_obj)) {
            if (action_obj) {
                cJSON_Delete(action_obj);
            }
            if (out_err_reason) {
                *out_err_reason = strdup("Invalid action_json");
            }
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid action_json");
        }
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *scene_item = cJSON_CreateObject();
    if (!scene_item) {
        cJSON_Delete(action_obj);
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to create scene item");
        }
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(scene_item, "action", action_obj);
    cJSON_AddStringToObject(scene_item, "id", scene_update_info->scene_id);
    cJSON_AddNullToObject(scene_item, "info");
    cJSON_AddStringToObject(scene_item, "name", scene_update_info->name);
    cJSON_AddStringToObject(scene_item, "operation", operation_str);

    cJSON *scenes_arr = cJSON_CreateArray();
    if (!scenes_arr) {
        cJSON_Delete(scene_item);
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to create scenes array");
        }
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToArray(scenes_arr, scene_item);

    cJSON *scene_wrap = cJSON_CreateObject();
    if (!scene_wrap) {
        cJSON_Delete(scenes_arr);
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to create Scene wrapper");
        }
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(scene_wrap, "Scenes", scenes_arr);

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        cJSON_Delete(scene_wrap);
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to create payload root");
        }
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(root, "Scenes", scene_wrap);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to print payload");
        }
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
    cJSON_free(payload);
    return ret;
}

esp_err_t rainmaker_api_activate_scene(const char *node_id, const char *scene_id, bool activate, const char **out_err_reason)
{
    if (!node_id || !scene_id) {
        if (out_err_reason) {
            *out_err_reason = strdup("Invalid node ID or scene ID");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char payload[128] = {0};
    if (activate) {
        snprintf(payload, sizeof(payload), "{\"Scenes\":{\"Scenes\":[{\"id\":\"%s\",\"operation\":\"activate\"}]}}", scene_id);
    } else {
        snprintf(payload, sizeof(payload), "{\"Scenes\":{\"Scenes\":[{\"id\":\"%s\",\"operation\":\"deactivate\"}]}}", scene_id);
    }
    return rainmaker_api_set_device_parameters(node_id, payload, out_err_reason);
}
