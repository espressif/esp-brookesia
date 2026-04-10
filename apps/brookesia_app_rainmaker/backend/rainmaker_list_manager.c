/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rainmaker_list_manager.h"
#include "rainmaker_api_handle.h"
#include "rainmaker_app_backend_util.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include "esp_random.h"
#include "esp_log.h"

static const char *TAG = "rainmaker_list_manager";

struct rm_list_t *g_item_list = NULL;

void rm_list_manager_clean_node_action_item_memory(rm_node_action_item_t *node_action_item)
{
    if (!node_action_item) {
        return;
    }
    rainmaker_app_backend_util_safe_free((void *)node_action_item->action);
    rainmaker_app_backend_util_safe_free((void *)node_action_item->node_id);
    rainmaker_app_backend_util_safe_free(node_action_item);
}

static void delete_all_node_action_items_in_list(struct rm_node_action_list_t *node_actions_list)
{
    if (node_actions_list == NULL) {
        return;
    }
    rm_node_action_item_t *node_action_item, *tmp;
    STAILQ_FOREACH_SAFE(node_action_item, node_actions_list, next, tmp) {
        STAILQ_REMOVE(node_actions_list, node_action_item, rm_node_action_item, next);
        rm_list_manager_clean_node_action_item_memory(node_action_item);
    }
    rainmaker_app_backend_util_safe_free(node_actions_list);
}

void rm_list_manager_clean_item_memory(rm_list_item_t *list_item)
{
    if (!list_item) {
        return;
    }
    rainmaker_app_backend_util_safe_free((void *)list_item->name);
    rainmaker_app_backend_util_safe_free((void *)list_item->id);
    if (list_item->node_actions_list) {
        delete_all_node_action_items_in_list(list_item->node_actions_list);
    }
    rainmaker_app_backend_util_safe_free(list_item);
}

static void delete_all_items_in_list(void)
{
    rm_list_item_t *list_item, *tmp;
    STAILQ_FOREACH_SAFE(list_item, g_item_list, next, tmp) {
        STAILQ_REMOVE(g_item_list, list_item, rm_list_item, next);
        rm_list_manager_clean_item_memory(list_item);
    }
}

esp_err_t rm_list_manager_remove_item_by_id(const char *id)
{
    if (g_item_list == NULL || id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    rm_list_item_t *list_item, *tmp;
    STAILQ_FOREACH_SAFE(list_item, g_item_list, next, tmp) {
        if (list_item->id && strcmp(list_item->id, id) == 0) {
            STAILQ_REMOVE(g_item_list, list_item, rm_list_item, next);
            rm_list_manager_clean_item_memory(list_item);
            return ESP_OK;
        }
    }
    return ESP_ERR_INVALID_ARG;
}

rm_list_item_t *rm_list_manager_find_item_by_id(const char *id)
{
    if (g_item_list == NULL || id == NULL) {
        return NULL;
    }
    rm_list_item_t *list_item;
    STAILQ_FOREACH(list_item, g_item_list, next) {
        if (list_item->id && strcmp(list_item->id, id) == 0) {
            return list_item;
        }
    }
    return NULL;
}

esp_err_t rm_list_manager_merge_saved_item_into_canonical(const rm_list_item_t *saved_copy)
{
    if (!saved_copy || !saved_copy->id || saved_copy->id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (!saved_copy->name) {
        return ESP_OK;
    }
    rm_list_item_t *canon = rm_list_manager_find_item_by_id(saved_copy->id);
    if (!canon) {
        ESP_LOGW(TAG, "merge_saved: no canonical item for id=%s", saved_copy->id);
        return ESP_ERR_NOT_FOUND;
    }
    if (strcmp(canon->name ? canon->name : "", saved_copy->name) == 0) {
        return ESP_OK;
    }
    char *n = strdup(saved_copy->name);
    if (!n) {
        return ESP_ERR_NO_MEM;
    }
    rainmaker_app_backend_util_safe_free((void *)canon->name);
    canon->name = n;
    return ESP_OK;
}

rm_list_item_t *rm_list_manager_get_item_by_index(int index)
{
    if (g_item_list == NULL || index < 0) {
        return NULL;
    }
    rm_list_item_t *list_item = NULL;
    int count = 0;
    STAILQ_FOREACH(list_item, g_item_list, next) {
        if (count == index) {
            return list_item;
        }
        count++;
    }
    return NULL;
}

esp_err_t rm_list_manager_init(void)
{
    /* Initialize the list */
    if (g_item_list != NULL) {
        delete_all_items_in_list();
        rainmaker_app_backend_util_safe_free_and_set_null((void **)&g_item_list);
    }
    g_item_list = (struct rm_list_t *)calloc(1, sizeof(struct rm_list_t));
    if (g_item_list == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for lists list");
        return ESP_FAIL;
    }
    STAILQ_INIT(g_item_list);

    return ESP_OK;
}

void rm_list_manager_deinit(void)
{
    if (g_item_list) {
        delete_all_items_in_list();
        rainmaker_app_backend_util_safe_free_and_set_null((void **)&g_item_list);
    }
}

void rm_list_manager_clear_item(void)
{
    if (g_item_list) {
        delete_all_items_in_list();
    }
}

static rm_node_action_item_t *rm_list_manager_find_node_action_item_by_id_internal(struct rm_node_action_list_t *node_actions_list, const char *node_id)
{
    if (node_actions_list == NULL || node_id == NULL) {
        return NULL;
    }
    rm_node_action_item_t *node_action_item;
    STAILQ_FOREACH(node_action_item, node_actions_list, next) {
        if (node_action_item->node_id && strcmp(node_action_item->node_id, node_id) == 0) {
            return node_action_item;
        }
    }
    return NULL;
}

static char *generate_item_id_internal(void)
{
    /* Generate a unique schedule id, like X75d, 4 characters random letters and numbers */
    char random_id[5] = {0};
    rm_list_item_t *existing_item = NULL;
    do {
        for (int i = 0; i < 4; i++) {
            random_id[i] = 'A' + esp_random() % 26;
        }
        random_id[4] = '\0';
        existing_item = rm_list_manager_find_item_by_id(random_id);
    } while (existing_item != NULL);

    return strdup(random_id);
}

rm_list_item_t *rm_list_manager_create_new_item_with_name(const char *id, const char *name, rm_list_entity_type_t entity_type)
{
    if (name == NULL) {
        return NULL;
    }

    rm_list_item_t *list_item = (rm_list_item_t *)calloc(1, sizeof(rm_list_item_t));
    if (list_item == NULL) {
        return NULL;
    }

    if (id == NULL) {
        list_item->id = generate_item_id_internal();
        if (list_item->id == NULL) {
            rainmaker_app_backend_util_safe_free(list_item);
            return NULL;
        }
    } else {
        list_item->id = strdup(id);
        if (list_item->id == NULL) {
            rainmaker_app_backend_util_safe_free(list_item);
            return NULL;
        }
    }

    list_item->name = strdup(name);
    if (list_item->name == NULL) {
        rainmaker_app_backend_util_safe_free((void *)list_item->id);
        rainmaker_app_backend_util_safe_free(list_item);
        return NULL;
    }

    list_item->entity_type = entity_type;
    list_item->node_actions_list = NULL;
    list_item->start_time = 0;
    list_item->end_time = 0;
    list_item->enabled = true;
    list_item->trigger_type = RM_SCHEDULE_TRIGGER_TYPE_ABSOLUTE;
    list_item->trigger.days_of_week.minutes = 0;
    list_item->trigger.days_of_week.days_of_week = 0;
    list_item->trigger.days_of_week.date_of_month = 0;
    list_item->trigger.days_of_week.month = 0;
    list_item->trigger.days_of_week.year = 0;
    list_item->trigger.days_of_week.is_repeating = false;
    return list_item;
}

rm_node_action_item_t *rm_list_manager_create_new_node_action_item(const char *node_id)
{
    if (node_id == NULL) {
        return NULL;
    }
    rm_node_action_item_t *node_action_item = (rm_node_action_item_t *)calloc(1, sizeof(rm_node_action_item_t));
    if (node_action_item == NULL) {
        return NULL;
    }
    memset(node_action_item, 0, sizeof(rm_node_action_item_t));
    node_action_item->node_id = strdup(node_id);
    if (node_action_item->node_id == NULL) {
        rainmaker_app_backend_util_safe_free(node_action_item);
        return NULL;
    }
    node_action_item->action = NULL;
    node_action_item->type = RM_NODE_ACTION_ADD;
    return node_action_item;
}

rm_list_item_t *rm_list_manager_get_copied_item_by_id(const char *id)
{
    if (id == NULL) {
        return NULL;
    }
    rm_list_item_t *list_item = rm_list_manager_find_item_by_id(id);
    if (list_item == NULL) {
        return NULL;
    }
    rm_list_item_t *copied_item = (rm_list_item_t *)calloc(1, sizeof(rm_list_item_t));
    if (copied_item == NULL) {
        return NULL;
    }
    memset(copied_item, 0, sizeof(rm_list_item_t));

    copied_item->id = strdup(list_item->id ? list_item->id : "");
    if (copied_item->id == NULL) {
        rainmaker_app_backend_util_safe_free(copied_item);
        return NULL;
    }
    copied_item->name = strdup(list_item->name ? list_item->name : "");
    if (copied_item->name == NULL) {
        rainmaker_app_backend_util_safe_free((void *)copied_item->id);
        rainmaker_app_backend_util_safe_free(copied_item);
        return NULL;
    }
    copied_item->enabled = list_item->enabled;
    copied_item->entity_type = list_item->entity_type;
    copied_item->trigger_type = list_item->trigger_type;
    copied_item->trigger = list_item->trigger;
    copied_item->start_time = list_item->start_time;
    copied_item->end_time = list_item->end_time;
    copied_item->node_actions_list = NULL;

    if (list_item->node_actions_list) {
        copied_item->node_actions_list = (struct rm_node_action_list_t *)calloc(1, sizeof(struct rm_node_action_list_t));
        if (copied_item->node_actions_list == NULL) {
            rainmaker_app_backend_util_safe_free((void *)copied_item->id);
            rainmaker_app_backend_util_safe_free((void *)copied_item->name);
            rainmaker_app_backend_util_safe_free(copied_item);
            return NULL;
        }
        STAILQ_INIT(copied_item->node_actions_list);

        rm_node_action_item_t *node_action_item;
        STAILQ_FOREACH(node_action_item, list_item->node_actions_list, next) {
            rm_node_action_item_t *new_node_action_item = (rm_node_action_item_t *)calloc(1, sizeof(rm_node_action_item_t));
            if (new_node_action_item == NULL) {
                delete_all_node_action_items_in_list(copied_item->node_actions_list);
                free(copied_item->node_actions_list);
                rainmaker_app_backend_util_safe_free((void *)copied_item->id);
                rainmaker_app_backend_util_safe_free((void *)copied_item->name);
                rainmaker_app_backend_util_safe_free(copied_item);
                return NULL;
            }
            memset(new_node_action_item, 0, sizeof(rm_node_action_item_t));

            new_node_action_item->node_id = strdup(node_action_item->node_id ? node_action_item->node_id : "");
            if (new_node_action_item->node_id == NULL) {
                rainmaker_app_backend_util_safe_free(new_node_action_item);
                continue;
            }
            new_node_action_item->action = strdup(node_action_item->action ? node_action_item->action : "");
            if (new_node_action_item->action == NULL) {
                rainmaker_app_backend_util_safe_free((void *)new_node_action_item->node_id);
                rainmaker_app_backend_util_safe_free(new_node_action_item);
                continue;
            }
            /* If the node action item is from the original list item, set the type to no change */
            new_node_action_item->type = RM_NODE_ACTION_NO_CHANGE;
            STAILQ_INSERT_TAIL(copied_item->node_actions_list, new_node_action_item, next);
        }
    }

    return copied_item;
}

void rm_list_manager_revert_node_action_item(rm_list_item_t *list_item)
{
    if (list_item == NULL) {
        return;
    }
    rm_list_item_t *original_item = rm_list_manager_find_item_by_id(list_item->id);
    if (original_item == NULL) {
        return;
    }
    /* Reverting the canonical item to "itself" would wipe node_actions with no restore path. */
    if (original_item == list_item) {
        return;
    }
    /* First remove all node action items from the list item */
    delete_all_node_action_items_in_list(list_item->node_actions_list);
    list_item->node_actions_list = NULL;

    /* Second copy all node action items from the original list item to the list item */
    if (original_item->node_actions_list) {
        struct rm_node_action_list_t *new_node_actions_list = (struct rm_node_action_list_t *)calloc(1, sizeof(struct rm_node_action_list_t));
        if (new_node_actions_list == NULL) {
            return;
        }
        STAILQ_INIT(new_node_actions_list);
        rm_node_action_item_t *node_action_item;
        STAILQ_FOREACH(node_action_item, original_item->node_actions_list, next) {
            rm_node_action_item_t *new_node_action_item = (rm_node_action_item_t *)calloc(1, sizeof(rm_node_action_item_t));
            if (new_node_action_item == NULL) {
                delete_all_node_action_items_in_list(new_node_actions_list);
                rainmaker_app_backend_util_safe_free(new_node_actions_list);
                return;
            }
            new_node_action_item->node_id = strdup(node_action_item->node_id ? node_action_item->node_id : "");
            if (new_node_action_item->node_id == NULL) {
                rainmaker_app_backend_util_safe_free(new_node_action_item);
                continue;
            }
            new_node_action_item->action = strdup(node_action_item->action ? node_action_item->action : "");
            if (new_node_action_item->action == NULL) {
                rainmaker_app_backend_util_safe_free((void *)new_node_action_item->node_id);
                rainmaker_app_backend_util_safe_free(new_node_action_item);
                continue;
            }
            new_node_action_item->type = RM_NODE_ACTION_NO_CHANGE;
            STAILQ_INSERT_TAIL(new_node_actions_list, new_node_action_item, next);
        }
        list_item->node_actions_list = new_node_actions_list;
    }
}

esp_err_t rm_list_manager_remove_node_action_from_list_item(const rm_list_item_t *list_item, const char *node_id)
{
    if (list_item == NULL || node_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    rm_node_action_item_t *node_action_item = rm_list_manager_find_node_action_item_by_id_internal(list_item->node_actions_list, node_id);
    if (node_action_item == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    STAILQ_REMOVE(list_item->node_actions_list, node_action_item, rm_node_action_item, next);
    rm_list_manager_clean_node_action_item_memory(node_action_item);
    return ESP_OK;
}

rm_node_action_item_t *rm_list_manager_find_node_action_item_by_id(const rm_list_item_t *list_item, const char *node_id)
{
    if (list_item == NULL || node_id == NULL) {
        return NULL;
    }
    return rm_list_manager_find_node_action_item_by_id_internal(list_item->node_actions_list, node_id);
}

rm_node_action_item_t *rm_list_manager_get_node_action_item_by_index(const rm_list_item_t *list_item, int index)
{
    if (list_item == NULL || index < 0 || list_item->node_actions_list == NULL) {
        return NULL;
    }
    int count = 0;
    rm_node_action_item_t *node_action_item = NULL;
    STAILQ_FOREACH(node_action_item, list_item->node_actions_list, next) {
        if (count == index) {
            return node_action_item;
        }
        count++;
    }
    return NULL;
}

int rm_list_manager_get_list_count_by_type(rm_list_entity_type_t entity_type)
{
    if (g_item_list == NULL) {
        return 0;
    }
    int count = 0;
    rm_list_item_t *item = NULL;
    STAILQ_FOREACH(item, g_item_list, next) {
        if (item->entity_type == entity_type) {
            count++;
        }
    }
    return count;
}

int rm_list_manager_get_lists_count(void)
{
    if (g_item_list == NULL) {
        return 0;
    }
    int count = 0;
    rm_list_item_t *item = NULL;
    STAILQ_FOREACH(item, g_item_list, next) {
        count++;
    }
    return count;
}

int rm_list_manager_get_node_actions_count(const rm_list_item_t *list_item)
{
    if (list_item == NULL || list_item->node_actions_list == NULL) {
        return 0;
    }
    int count = 0;
    rm_node_action_item_t *item = NULL;
    STAILQ_FOREACH(item, list_item->node_actions_list, next) {
        count++;
    }
    return count;
}

esp_err_t rm_list_manager_add_node_action_to_list_item(rm_list_item_t *list_item, rm_node_action_item_t *node_action_item)
{
    if (list_item == NULL || node_action_item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Allow modifying the list head when it is NULL (cast away const for that field only) */
    if (list_item->node_actions_list == NULL) {
        list_item->node_actions_list = (struct rm_node_action_list_t *)calloc(1, sizeof(struct rm_node_action_list_t));
        if (list_item->node_actions_list == NULL) {
            return ESP_FAIL;
        }
        STAILQ_INIT(list_item->node_actions_list);
    }

    /* If same node_id already exists, clean it and remove it from the list */
    rm_list_manager_remove_node_action_from_list_item(list_item, node_action_item->node_id);

    /* Add the new node action item to the list */
    STAILQ_INSERT_TAIL(list_item->node_actions_list, node_action_item, next);
    return ESP_OK;
}

esp_err_t rm_list_manager_add_item_to_list(rm_list_item_t *list_item)
{
    if (list_item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* First find whether id+entity_type exists in the list, if yes, replace it */
    rm_list_item_t *existing = NULL;
    STAILQ_FOREACH(existing, g_item_list, next) {
        if (existing->id && list_item->id &&
                strcmp(existing->id, list_item->id) == 0 &&
                existing->entity_type == list_item->entity_type) {
            break;
        }
    }
    if (existing != NULL) {
        STAILQ_REMOVE(g_item_list, existing, rm_list_item, next);
        rm_list_manager_clean_item_memory(existing);
    }
    STAILQ_INSERT_TAIL(g_item_list, list_item, next);
    return ESP_OK;
}

/* -----------------------------------------------------------------------------
 * Action summary format: "Key to Value" per line, shared by action_to_summary
 * and summary_to_action. Param keys in display order.
 * ----------------------------------------------------------------------------- */
#define RM_ITEM_SUMMARY_SEP      " to "
#define RM_ITEM_SUMMARY_ON       "On"
#define RM_ITEM_SUMMARY_OFF      "Off"
#define RM_ITEM_NO_ACTIONS       "No Actions Selected"
#define RM_ITEM_SUMMARY_KEY_MAX  64

static const char *const s_action_param_order[] = {
    RM_STANDARD_DEVICE_POWER,
    RM_STANDARD_LIGHT_DEVICE_BRIGHTNESS,
    RM_STANDARD_LIGHT_DEVICE_HUE,
    RM_STANDARD_LIGHT_DEVICE_SATURATION,
    RM_STANDARD_FAN_DEVICE_SPEED,
    NULL
};

/** True if key is one of the known ordered param keys. */
static bool is_ordered_param_key(const char *key)
{
    for (int i = 0; s_action_param_order[i] != NULL; i++) {
        if (strcmp(key, s_action_param_order[i]) == 0) {
            return true;
        }
    }
    return false;
}

/** Format one param for summary: "Key to Value". Returns length (excluding NUL), 0 on skip. */
static size_t param_line_length(const char *key, cJSON *val, bool is_first)
{
    const char *prefix = is_first ? "" : "\n";
    const char *power_str;
    int n = 0;
    if (strcmp(key, RM_STANDARD_DEVICE_POWER) == 0 && cJSON_IsBool(val)) {
        power_str = cJSON_IsTrue(val) ? RM_ITEM_SUMMARY_ON : RM_ITEM_SUMMARY_OFF;
        n = snprintf(NULL, 0, "%s" RM_STANDARD_DEVICE_POWER RM_ITEM_SUMMARY_SEP "%s", prefix, power_str);
    } else if (cJSON_IsNumber(val)) {
        n = snprintf(NULL, 0, "%s%s" RM_ITEM_SUMMARY_SEP "%d", prefix, key, (int)cJSON_GetNumberValue(val));
    } else if (cJSON_IsString(val) && val->valuestring) {
        n = snprintf(NULL, 0, "%s%s" RM_ITEM_SUMMARY_SEP "%s", prefix, key, val->valuestring);
    } else if (cJSON_IsBool(val)) {
        power_str = cJSON_IsTrue(val) ? RM_ITEM_SUMMARY_ON : RM_ITEM_SUMMARY_OFF;
        n = snprintf(NULL, 0, "%s%s" RM_ITEM_SUMMARY_SEP "%s", prefix, key, power_str);
    }
    return (n > 0) ? (size_t)n : 0;
}

static void append_param_line(char *buf, size_t buf_size, size_t *len, const char *key, cJSON *val)
{
    if (*len >= buf_size) {
        return;
    }
    size_t space = buf_size - *len;
    char *dst = buf + *len;
    bool is_first = (*len == 0);
    const char *prefix = is_first ? "" : "\n";
    const char *power_str;
    int n = 0;
    if (strcmp(key, RM_STANDARD_DEVICE_POWER) == 0 && cJSON_IsBool(val)) {
        power_str = cJSON_IsTrue(val) ? RM_ITEM_SUMMARY_ON : RM_ITEM_SUMMARY_OFF;
        n = snprintf(dst, space, "%s" RM_STANDARD_DEVICE_POWER RM_ITEM_SUMMARY_SEP "%s", prefix, power_str);
    } else if (cJSON_IsNumber(val)) {
        n = snprintf(dst, space, "%s%s" RM_ITEM_SUMMARY_SEP "%d", prefix, key, (int)cJSON_GetNumberValue(val));
    } else if (cJSON_IsString(val) && val->valuestring) {
        n = snprintf(dst, space, "%s%s" RM_ITEM_SUMMARY_SEP "%s", prefix, key, val->valuestring);
    } else if (cJSON_IsBool(val)) {
        power_str = cJSON_IsTrue(val) ? RM_ITEM_SUMMARY_ON : RM_ITEM_SUMMARY_OFF;
        n = snprintf(dst, space, "%s%s" RM_ITEM_SUMMARY_SEP "%s", prefix, key, power_str);
    }
    if (n > 0 && (size_t)n < space) {
        *len += (size_t)n;
    }
}

/** Count total length for params object (same order as append_param_line). */
static size_t compute_summary_length(cJSON *params)
{
    size_t len = 0;
    bool is_first = true;
    for (int i = 0; s_action_param_order[i] != NULL; i++) {
        cJSON *val = cJSON_GetObjectItem(params, s_action_param_order[i]);
        if (val) {
            len += param_line_length(s_action_param_order[i], val, is_first);
            is_first = false;
        }
    }
    for (cJSON *p = params->child; p != NULL; p = p->next) {
        if (!p->string || is_ordered_param_key(p->string)) {
            continue;
        }
        len += param_line_length(p->string, p, is_first);
        is_first = false;
    }
    return len;
}

/** Set *buf and *buf_size to "No Actions Selected" and return. Caller ensures buf/buf_size non-NULL. */
static void set_buf_no_actions(char **buf, size_t *buf_size)
{
    size_t need = strlen(RM_ITEM_NO_ACTIONS) + 1;
    char *out = (char *)calloc(1, need);
    if (out) {
        memcpy(out, RM_ITEM_NO_ACTIONS, need);
        *buf = out;
        *buf_size = need;
    }
}

void rm_list_manager_action_to_summary(const char *action_json, char **buf, size_t *buf_size)
{
    if (!buf || !buf_size) {
        return;
    }
    *buf = NULL;
    *buf_size = 0;

    if (!action_json || action_json[0] == '\0') {
        set_buf_no_actions(buf, buf_size);
        return;
    }
    cJSON *root = cJSON_Parse(action_json);
    if (!root || !cJSON_IsObject(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        set_buf_no_actions(buf, buf_size);
        return;
    }
    cJSON *params = NULL;
    for (cJSON *dev = root->child; dev != NULL; dev = dev->next) {
        if (cJSON_IsObject(dev)) {
            params = dev;
            break;
        }
    }
    if (!params) {
        cJSON_Delete(root);
        set_buf_no_actions(buf, buf_size);
        return;
    }

    size_t content_len = compute_summary_length(params);
    if (content_len == 0) {
        content_len = strlen(RM_ITEM_NO_ACTIONS);
    }
    size_t need = content_len + 1;
    char *out = (char *)calloc(1, need);
    if (!out) {
        cJSON_Delete(root);
        return;
    }
    out[0] = '\0';
    size_t len = 0;
    for (int i = 0; s_action_param_order[i] != NULL; i++) {
        cJSON *val = cJSON_GetObjectItem(params, s_action_param_order[i]);
        if (val) {
            append_param_line(out, need, &len, s_action_param_order[i], val);
        }
    }
    for (cJSON *p = params->child; p != NULL; p = p->next) {
        if (!p->string || is_ordered_param_key(p->string)) {
            continue;
        }
        append_param_line(out, need, &len, p->string, p);
    }
    cJSON_Delete(root);
    if (len == 0 && need > 0) {
        memcpy(out, RM_ITEM_NO_ACTIONS, strlen(RM_ITEM_NO_ACTIONS) + 1);
    }
    *buf = out;
    *buf_size = need;
}

bool rm_list_manager_action_is_placeholder(const char *action_json)
{
    if (action_json == NULL || action_json[0] == '\0') {
        return true;
    }
    char *buf = NULL;
    size_t buf_size = 0;
    rm_list_manager_action_to_summary(action_json, &buf, &buf_size);
    if (buf == NULL || buf_size == 0) {
        rainmaker_app_backend_util_safe_free(buf);
        return true;
    }
    bool ph = (strcmp(buf, RM_ITEM_NO_ACTIONS) == 0);
    rainmaker_app_backend_util_safe_free(buf);
    return ph;
}

/** Infer device type string from summary keys (Light / Fan / Switch). */
static const char *infer_device_type_string(rm_app_device_type_t device_type)
{
    switch (device_type) {
    case RAINMAKER_APP_DEVICE_TYPE_LIGHT:
        return RM_APP_DEVICE_TYPE_STRING_LIGHT;
    case RAINMAKER_APP_DEVICE_TYPE_FAN:
        return RM_APP_DEVICE_TYPE_STRING_FAN;
    case RAINMAKER_APP_DEVICE_TYPE_SWITCH:
        return RM_APP_DEVICE_TYPE_STRING_SWITCH;
    case RAINMAKER_APP_DEVICE_TYPE_OTHER:
        return RM_APP_DEVICE_TYPE_STRING_LIGHT;
    default:
        return RM_APP_DEVICE_TYPE_STRING_LIGHT;
    }
}

/** True if key is a numeric param (Brightness, Hue, Saturation, Speed). */
static bool is_numeric_param_key(const char *key)
{
    return strcmp(key, RM_STANDARD_LIGHT_DEVICE_BRIGHTNESS) == 0 || strcmp(key, RM_STANDARD_LIGHT_DEVICE_HUE) == 0
           || strcmp(key, RM_STANDARD_LIGHT_DEVICE_SATURATION) == 0 || strcmp(key, RM_STANDARD_FAN_DEVICE_SPEED) == 0;
}

/** Parse one line "Key to Value" and add to cJSON object. Returns true if added. */
static bool parse_summary_line_to_cjson(const char *line, cJSON *params)
{
    const char *p = strstr(line, RM_ITEM_SUMMARY_SEP);
    if (!p || p == line) {
        return false;
    }
    size_t key_len = (size_t)(p - line);
    if (key_len == 0 || key_len > RM_ITEM_SUMMARY_KEY_MAX) {
        return false;
    }
    char key_buf[RM_ITEM_SUMMARY_KEY_MAX + 1];
    memcpy(key_buf, line, key_len);
    key_buf[key_len] = '\0';
    const char *value = p + strlen(RM_ITEM_SUMMARY_SEP);
    if (value[0] == '\0') {
        return false;
    }

    cJSON *val = NULL;
    if (strcmp(key_buf, RM_STANDARD_DEVICE_POWER) == 0) {
        if (strcmp(value, RM_ITEM_SUMMARY_ON) == 0) {
            val = cJSON_CreateTrue();
        } else if (strcmp(value, RM_ITEM_SUMMARY_OFF) == 0) {
            val = cJSON_CreateFalse();
        }
    } else if (is_numeric_param_key(key_buf)) {
        val = cJSON_CreateNumber((double)atoi(value));
    } else {
        val = cJSON_CreateString(value);
    }
    if (!val) {
        return false;
    }
    cJSON_AddItemToObject(params, key_buf, val);
    return true;
}

void rm_list_manager_summary_to_action_json(rm_app_device_type_t device_type, const char *summary, char **action_json, size_t *action_json_size)
{
    if (!action_json || !action_json_size) {
        return;
    }
    *action_json = NULL;
    *action_json_size = 0;

    if (!summary || summary[0] == '\0') {
        return;
    }

    const char *device_type_str = infer_device_type_string(device_type);
    cJSON *params = cJSON_CreateObject();
    if (!params) {
        return;
    }

    const char *line_start = summary;
    for (;;) {
        const char *next = strchr(line_start, '\n');
        size_t line_len = next ? (size_t)(next - line_start) : strlen(line_start);
        if (line_len > 0) {
            char *line = (char *)malloc(line_len + 1);
            if (line) {
                memcpy(line, line_start, line_len);
                line[line_len] = '\0';
                parse_summary_line_to_cjson(line, params);
                free(line);
            }
        }
        if (!next) {
            break;
        }
        line_start = next + 1;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        cJSON_Delete(params);
        return;
    }
    cJSON_AddItemToObject(root, device_type_str, params);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) {
        return;
    }
    *action_json = json_str;
    *action_json_size = strlen(json_str) + 1;
}

static esp_err_t rm_list_manager_update_item_to_backend_internal(const rm_list_item_t *list_item, const rm_node_action_item_t *node_action_item)
{
    if (list_item == NULL || node_action_item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (node_action_item->type == RM_NODE_ACTION_ADD || node_action_item->type == RM_NODE_ACTION_EDIT) {
        if (list_item->entity_type == RM_LIST_ENTITY_TYPE_SCHEDULE) {
            rm_schedule_update_info_t schedule_update_info = {0};
            schedule_update_info.node_action_type = node_action_item->type;
            schedule_update_info.trigger_type = list_item->trigger_type;
            if (list_item->trigger_type == RM_SCHEDULE_TRIGGER_TYPE_RELATIVE) {
                schedule_update_info.trigger.relative.relative_seconds = list_item->trigger.relative.relative_seconds;
            } else if (list_item->trigger_type == RM_SCHEDULE_TRIGGER_TYPE_ABSOLUTE) {
                schedule_update_info.trigger.days_of_week.minutes = list_item->trigger.days_of_week.minutes;
                schedule_update_info.trigger.days_of_week.days_of_week = list_item->trigger.days_of_week.days_of_week;
                schedule_update_info.trigger.days_of_week.date_of_month = list_item->trigger.days_of_week.date_of_month;
                schedule_update_info.trigger.days_of_week.month = list_item->trigger.days_of_week.month;
                schedule_update_info.trigger.days_of_week.year = list_item->trigger.days_of_week.year;
                schedule_update_info.trigger.days_of_week.is_repeating = list_item->trigger.days_of_week.is_repeating;
            }
            schedule_update_info.action_json = node_action_item->action;
            schedule_update_info.schedule_id = list_item->id;
            schedule_update_info.name = list_item->name;
            schedule_update_info.flag = NULL;
            schedule_update_info.info = NULL;
            return rainmaker_api_update_schedule(node_action_item->node_id, &schedule_update_info, NULL);
        } else if (list_item->entity_type == RM_LIST_ENTITY_TYPE_SCENE) {
            rm_scene_update_info_t scene_update_info = {0};
            scene_update_info.node_action_type = node_action_item->type;
            scene_update_info.action_json = node_action_item->action;
            scene_update_info.scene_id = list_item->id;
            scene_update_info.name = list_item->name;
            return rainmaker_api_update_scene(node_action_item->node_id, &scene_update_info, NULL);
        } else {
            ESP_LOGE(TAG, "Invalid entity type: %d", list_item->entity_type);
            return ESP_ERR_INVALID_ARG;
        }
    } else if (node_action_item->type == RM_NODE_ACTION_DELETE) {
        if (list_item->entity_type == RM_LIST_ENTITY_TYPE_SCHEDULE) {
            return rainmaker_api_delete_schedule(node_action_item->node_id, list_item->id, NULL);
        } else if (list_item->entity_type == RM_LIST_ENTITY_TYPE_SCENE) {
            return rainmaker_api_delete_scene(node_action_item->node_id, list_item->id, NULL);
        } else {
            ESP_LOGE(TAG, "Invalid entity type: %d", list_item->entity_type);
            return ESP_ERR_INVALID_ARG;
        }
    } else if (node_action_item->type == RM_NODE_ACTION_ENABLE || node_action_item->type == RM_NODE_ACTION_DISABLE) {
        if (list_item->entity_type == RM_LIST_ENTITY_TYPE_SCHEDULE) {
            return rainmaker_api_enable_schedule(node_action_item->node_id, list_item->id, node_action_item->type == RM_NODE_ACTION_ENABLE ? true : false, NULL);
        } else if (list_item->entity_type == RM_LIST_ENTITY_TYPE_SCENE) {
            return rainmaker_api_activate_scene(node_action_item->node_id, list_item->id, node_action_item->type == RM_NODE_ACTION_ENABLE ? true : false, NULL);
        } else {
            ESP_LOGE(TAG, "Invalid entity type: %d", list_item->entity_type);
            return ESP_ERR_INVALID_ARG;
        }
    } else if (node_action_item->type == RM_NODE_ACTION_NO_CHANGE) {
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

/** True if scene node_actions differ from original (name already matched). */
static bool rm_scene_node_actions_differ_from_original(const rm_list_item_t *current, const rm_list_item_t *original)
{
    if (current == NULL || original == NULL) {
        return true;
    }
    int n_cur = rm_list_manager_get_node_actions_count(current);
    int n_orig = rm_list_manager_get_node_actions_count(original);
    if (n_cur != n_orig) {
        return true;
    }
    if (n_cur == 0) {
        return false;
    }
    rm_node_action_item_t *cur = NULL;
    STAILQ_FOREACH(cur, current->node_actions_list, next) {
        if (cur->node_id == NULL) {
            return true;
        }
        rm_node_action_item_t *orig = rm_list_manager_find_node_action_item_by_id(original, cur->node_id);
        if (orig == NULL) {
            return true;
        }
        const char *a = cur->action;
        const char *b = orig->action;
        if (a == NULL && b == NULL) {
            continue;
        }
        if (a == NULL || b == NULL) {
            return true;
        }
        if (strcmp(a, b) != 0) {
            return true;
        }
    }
    return false;
}

static bool rm_list_manager_is_item_edited(const rm_list_item_t *list_item)
{
    if (list_item == NULL) {
        return false;
    }

    /* Get the original list item */
    rm_list_item_t *original_list_item = rm_list_manager_find_item_by_id(list_item->id);
    if (original_list_item == NULL) {
        return false;
    }

    /* Compare the schedule item with the original schedule item */
    if (strcmp(list_item->name, original_list_item->name) != 0) {
        return true;
    }

    if (list_item->entity_type == RM_LIST_ENTITY_TYPE_SCHEDULE) {
        if (list_item->trigger_type != original_list_item->trigger_type) {
            return true;
        }
        if (list_item->trigger_type == RM_SCHEDULE_TRIGGER_TYPE_RELATIVE) {
            if (list_item->trigger.relative.relative_seconds != original_list_item->trigger.relative.relative_seconds) {
                return true;
            }
        } else if (list_item->trigger_type == RM_SCHEDULE_TRIGGER_TYPE_ABSOLUTE) {
            if (list_item->trigger.days_of_week.minutes != original_list_item->trigger.days_of_week.minutes) {
                return true;
            }
            if (list_item->trigger.days_of_week.days_of_week != original_list_item->trigger.days_of_week.days_of_week) {
                return true;
            }
            if (list_item->trigger.days_of_week.date_of_month != original_list_item->trigger.days_of_week.date_of_month) {
                return true;
            }
            if (list_item->trigger.days_of_week.month != original_list_item->trigger.days_of_week.month) {
                return true;
            }
            if (list_item->trigger.days_of_week.year != original_list_item->trigger.days_of_week.year) {
                return true;
            }
            if (list_item->trigger.days_of_week.is_repeating != original_list_item->trigger.days_of_week.is_repeating) {
                return true;
            }
        }
        if (list_item->enabled != original_list_item->enabled) {
            return true;
        }
        if (rm_scene_node_actions_differ_from_original(list_item, original_list_item)) {
            return true;
        }
    }

    if (list_item->entity_type == RM_LIST_ENTITY_TYPE_SCENE) {
        if (rm_scene_node_actions_differ_from_original(list_item, original_list_item)) {
            return true;
        }
        return false;
    }

    return false;
}

esp_err_t rm_list_manager_update_item_to_backend(const rm_list_item_t *list_item)
{
    if (list_item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    bool is_item_edited = rm_list_manager_is_item_edited(list_item);
    if (list_item->node_actions_list == NULL) {
        rm_list_manager_print_item(list_item);
        return ESP_OK;
    }
    rm_node_action_item_t *node_action_item = NULL;
    STAILQ_FOREACH(node_action_item, list_item->node_actions_list, next) {
        if (is_item_edited) {
            if (node_action_item->type == RM_NODE_ACTION_NO_CHANGE) {
                node_action_item->type = RM_NODE_ACTION_EDIT;
            }
        }
        esp_err_t err = rm_list_manager_update_item_to_backend_internal(list_item, node_action_item);
        if (err != ESP_OK) {
            return err;
        }
    }
    rm_list_manager_print_item(list_item);
    return ESP_OK;
}

esp_err_t rm_list_manager_delete_item_from_backend(const rm_list_item_t *list_item)
{
    if (list_item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (list_item->node_actions_list == NULL) {
        rm_list_manager_print_item(list_item);
        return ESP_OK;
    }
    rm_node_action_item_t *node_action_item = NULL;
    STAILQ_FOREACH(node_action_item, list_item->node_actions_list, next) {
        node_action_item->type = RM_NODE_ACTION_DELETE;
        esp_err_t err = rm_list_manager_update_item_to_backend_internal(list_item, node_action_item);
        if (err != ESP_OK) {
            return err;
        }
    }
    rm_list_manager_print_item(list_item);
    return ESP_OK;
}

esp_err_t rm_list_manager_enable_item_to_backend(const rm_list_item_t *list_item, bool enabled)
{
    if (list_item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (list_item->node_actions_list == NULL) {
        rm_list_manager_print_item(list_item);
        return ESP_OK;
    }
    rm_node_action_item_t *node_action_item = NULL;
    STAILQ_FOREACH(node_action_item, list_item->node_actions_list, next) {
        node_action_item->type = enabled ? RM_NODE_ACTION_ENABLE : RM_NODE_ACTION_DISABLE;
        esp_err_t err = rm_list_manager_update_item_to_backend_internal(list_item, node_action_item);
        if (err != ESP_OK) {
            return err;
        }
    }
    rm_list_manager_print_item(list_item);
    return ESP_OK;
}

void rm_list_manager_set_node_action_item_type(rm_node_action_item_t *node_action_item, rm_node_action_type_t type)
{
    if (node_action_item == NULL) {
        return;
    }
    node_action_item->type = type;
}

void rm_list_manager_print_item(const rm_list_item_t *list_item)
{
    if (list_item == NULL) {
        return;
    }
    printf("==============================================\n");
    printf("List item entity type: %s\n",
           list_item->entity_type == RM_LIST_ENTITY_TYPE_SCENE ? "scene" : "schedule");
    printf("List item name: %s\n", list_item->name);
    printf("List item id: %s\n", list_item->id);
    printf("List item enabled: %d\n", list_item->enabled);
    if (list_item->entity_type == RM_LIST_ENTITY_TYPE_SCHEDULE) {
        printf("List item trigger type: %d\n", list_item->trigger_type);
        if (list_item->trigger_type == RM_SCHEDULE_TRIGGER_TYPE_RELATIVE) {
            printf("List item relative seconds: %ld\n", (long)list_item->trigger.relative.relative_seconds);
        } else if (list_item->trigger_type == RM_SCHEDULE_TRIGGER_TYPE_ABSOLUTE) {
            printf("List item minutes: %d\n", list_item->trigger.days_of_week.minutes);
            printf("List item days of week: %d\n", list_item->trigger.days_of_week.days_of_week);
            printf("List item date of month: %d\n", list_item->trigger.days_of_week.date_of_month);
            printf("List item month: %d\n", list_item->trigger.days_of_week.month);
            printf("List item year: %d\n", list_item->trigger.days_of_week.year);
            printf("List item is repeating: %d\n", list_item->trigger.days_of_week.is_repeating);
        }
        printf("List item start time: %lld\n", (long long)list_item->start_time);
        printf("List item end time: %lld\n", (long long)list_item->end_time);
    }
    if (list_item->node_actions_list != NULL) {
        rm_node_action_item_t *node_action_item = NULL;
        STAILQ_FOREACH(node_action_item, list_item->node_actions_list, next) {
            printf("==============================================\n");
            printf("Node action item type: %d\n", node_action_item->type);
            printf("Node action item node id: %s\n", node_action_item->node_id);
            printf("Node action item action: %s\n", node_action_item->action);
            printf("==============================================\n");
        }
    }
    printf("==============================================\n");
}
