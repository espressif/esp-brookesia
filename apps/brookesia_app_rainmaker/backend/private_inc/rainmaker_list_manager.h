/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file rainmaker_list_manager.h
 * @brief RainMaker list in-memory manager: list and item CRUD.
 */

#pragma once

#include <stdbool.h>
#include <time.h>
#include "sys/queue.h"
#include "rainmaker_app_backend.h"
#include "rainmaker_standard_device.h"
#include "rainmaker_api_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * List item
 * -----------------------------------------------------------------------------
 */

/** One node action under a schedule, e.g. {"Switch":{"Power":true}} */
typedef struct rm_node_action_item {
    /** Node action type */
    rm_node_action_type_t type;
    /** Node ID (read-only) */
    const char *node_id;
    /** Action JSON, e.g. {"Switch":{"Power":true}} or {"Light":{"Brightness":50}} */
    char *action;
    STAILQ_ENTRY(rm_node_action_item) next;
} rm_node_action_item_t;

STAILQ_HEAD(rm_node_action_list_t, rm_node_action_item);

typedef enum {
    RM_LIST_ENTITY_TYPE_SCHEDULE = 0,
    RM_LIST_ENTITY_TYPE_SCENE = 1,
} rm_list_entity_type_t;

/* -----------------------------------------------------------------------------
 * List item
 * -----------------------------------------------------------------------------
 */

/** One list item */
typedef struct rm_list_item {
    /** List entity type */
    rm_list_entity_type_t entity_type;
    /** List ID; immutable and unique after creation */
    const char *id;
    /** List name; may be renamed */
    char *name;
    /** Whether the schedule is enabled, this field is only valid for schedule list */
    bool enabled;
    /** Trigger type, this field is only valid for schedule list */
    rm_schedule_trigger_type_t trigger_type;
    /** Trigger payload (use relative or days_of_week according to trigger_type), this field is only valid for schedule list */
    union {
        rm_schedule_trigger_relative_t relative;
        rm_schedule_trigger_days_of_week_t days_of_week;
    } trigger;
    /** List of node actions for this list */
    struct rm_node_action_list_t *node_actions_list;
    /** Start time (UTC timestamp), this field is only valid for schedule list */
    time_t start_time;
    /** End time (UTC timestamp), this field is only valid for schedule list */
    time_t end_time;
    STAILQ_ENTRY(rm_list_item) next;
} rm_list_item_t;

STAILQ_HEAD(rm_list_t, rm_list_item);


/* -----------------------------------------------------------------------------
 * Lifecycle
 * -----------------------------------------------------------------------------
 */

/**
 * @brief Initialize the list manager, currently support schedule and scene list
 *
 * Allocates and initializes g_item_list. If the list already exists, it is cleared first.
 *
 * @return ESP_OK on success, ESP_FAIL on allocation failure
 */
esp_err_t rm_list_manager_init(void);

/**
 * @brief Deinitialize the list manager
 *
 * Frees all list entries and g_item_list.
 */
void rm_list_manager_deinit(void);

/**
 * @brief Clear all list entries
 *
 * Does not free the list head; g_item_list remains valid.
 */
void rm_list_manager_clear_item(void);

/**
 * @brief Free a list item and its node action items (caller-allocated or copied item)
 *
 * Frees name, id, all node action items in node_actions_list, and the list_item struct.
 * Does not free the node_actions_list head pointer.
 *
 * @param list_item List item to free (may be NULL)
 */
void rm_list_manager_clean_item_memory(rm_list_item_t *list_item);

/**
 * @brief Free a list node action item and its associated memory
 *
 * @param node_action_item Node action item to free (may be NULL)
 */
void rm_list_manager_clean_node_action_item_memory(rm_node_action_item_t *node_action_item);

/* -----------------------------------------------------------------------------
 * List CRUD
 * -----------------------------------------------------------------------------
 */

/**
 * @brief Remove a list item by ID
 *
 * Frees the list item, its node actions, and associated memory.
 *
 * @param[in] list_id List ID to remove
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if list/schedule_id is NULL or schedule not found
 */
esp_err_t rm_list_manager_remove_item_by_id(const char *id);

/**
 * @brief Find a list item by ID
 *
 * @param[in] id List ID to look up
 * @return Pointer to the list item, or NULL if not found
 */
rm_list_item_t *rm_list_manager_find_item_by_id(const char *id);

/**
 * @brief After a successful PUT and rm_app_backend_refresh_home_screen(), overlay display name from the
 * edited working copy onto the canonical list item for the same id.
 *
 * Use when GET /nodes may still return stale scene/schedule metadata while the cloud console already shows
 * the new name, so the UI would otherwise show the old name.
 *
 * @param[in] saved_copy Working copy (same id as canonical); name field is the user-confirmed string.
 * @return ESP_OK if merged or nothing to do; ESP_ERR_INVALID_ARG; ESP_ERR_NOT_FOUND if id missing in list.
 */
esp_err_t rm_list_manager_merge_saved_item_into_canonical(const rm_list_item_t *saved_copy);

/**
 * @brief Get a list item by index
 *
 * @param[in] index Index of the list item
 * @return Pointer to the list item, or NULL if not found
 */
rm_list_item_t *rm_list_manager_get_item_by_index(int index);

/**
 * @brief Create a new list item with the given id, name and entity type (id is immutable and unique after creation)
 *
 * Caller must free with rm_list_manager_clean_item_memory.
 * If name is NULL, returns NULL.
 * If id is NULL, a new unique id is generated.
 *
 * @param[in] id List id (may be NULL, generated if NULL)
 * @param[in] name List name, cannot be NULL
 * @param[in] entity_type List entity type
 * @return New list item, or NULL on allocation failure or name is NULL
 */
rm_list_item_t *rm_list_manager_create_new_item_with_name(const char *id, const char *name, rm_list_entity_type_t entity_type);

/**
 * @brief Create a new node action item with the given node id
 *
 * @param[in] node_id Node id
 * @return New node action item, or NULL on allocation failure
 */
rm_node_action_item_t *rm_list_manager_create_new_node_action_item(const char *node_id);

/**
 * @brief Deep-copy a list item (and its node_actions_list) by ID
 *
 * Caller must free with rm_list_manager_clean_item_memory.
 *
 * @param[in] id List ID to copy
 * @return New list item copy, or NULL if not found or allocation failure
 */
rm_list_item_t *rm_list_manager_get_copied_item_by_id(const char *id);

/**
 * @brief Add or update a list item in the list
 *
 * If a list item with the same ID exists, updates name, enabled, trigger, and times.
 * Otherwise inserts a new item (node_actions_list is not copied).
 *
 * @param[in] list_item List item data to add or merge
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on NULL, ESP_FAIL on allocation failure
 */
esp_err_t rm_list_manager_add_item_to_list(rm_list_item_t *list_item);

/* -----------------------------------------------------------------------------
 * Node action CRUD
 * -----------------------------------------------------------------------------
 */

/**
 * @brief Find a node action by list item and node ID
 *
 * @param[in] list_item List item (with node_actions_list)
 * @param[in] node_id Node ID
 * @return Pointer to the node action item, or NULL if not found
 */
rm_node_action_item_t *rm_list_manager_find_node_action_item_by_id(const rm_list_item_t *list_item, const char *node_id);

/**
 * @brief Get a node action by list item and index
 *
 * @param[in] list_item List item (with node_actions_list)
 * @param[in] index Index of the node action
 * @return Pointer to the node action item, or NULL if not found
 */
rm_node_action_item_t *rm_list_manager_get_node_action_item_by_index(const rm_list_item_t *list_item, int index);

/**
 * @brief Add or update a node action on a list item (by node_id; same node_id updates in place)
 *
 * @param[in] list_item List item to add to (node_actions_list created if NULL)
 * @param[in] node_action_item Node action data (node_id, name, type, action)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on NULL, ESP_FAIL on allocation failure
 */
esp_err_t rm_list_manager_add_node_action_to_list_item(rm_list_item_t *list_item, rm_node_action_item_t *node_action_item);

/**
 * @brief Remove a node action by node ID from a list item
 *
 * @param[in] list_item List item
 * @param[in] node_id Node ID to remove
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on NULL, ESP_ERR_NOT_FOUND if node action not found
 */
esp_err_t rm_list_manager_remove_node_action_from_list_item(const rm_list_item_t *list_item, const char *node_id);

/**
 * @brief Get the number of lists by entity type
 *
 * @param[in] entity_type List entity type
 * @return Number of lists, 0 if lists list is NULL
 */
int rm_list_manager_get_list_count_by_type(rm_list_entity_type_t entity_type);

/**
 * @brief Get the number of lists
 *
 * @return Number of lists, 0 if lists list is NULL
 */
int rm_list_manager_get_lists_count(void);

/**
 * @brief Get the number of node actions for a list item
 *
 * @param[in] list_item List item
 * @return Number of node actions, 0 if list item or node actions list is NULL
 */
int rm_list_manager_get_node_actions_count(const rm_list_item_t *list_item);

/**
 * @brief Convert action JSON string to human-readable summary (e.g. "Power to On\nBrightness to 61")
 *
 * Buffer is allocated inside; caller must free(*buf).
 *
 * @param[in] action_json Action JSON, e.g. {"Switch":{"Power":true}} or {"Light":{"Brightness":61,"Power":true}}
 * @param[out] buf On success, set to allocated buffer; on failure or empty input, set to NULL. Caller frees.
 * @param[out] buf_size On success, set to allocated buffer size; on failure, set to 0
 */
void rm_list_manager_action_to_summary(const char *action_json, char **buf, size_t *buf_size);

/**
 * @brief True if action JSON is empty or resolves to the backend "no actions" placeholder summary.
 */
bool rm_list_manager_action_is_placeholder(const char *action_json);

/**
 * @brief Convert human-readable summary to action JSON string
 *
 * Buffer is allocated inside; caller must free(*action_json).
 *
 * @param[in] device_type Device type
 * @param[in] summary Summary string, e.g. "Power to On\nBrightness to 61"
 * @param[out] action_json On success, set to allocated buffer; on failure, set to NULL. Caller frees.
 * @param[out] action_json_size On success, set to allocated buffer size; on failure, set to 0
 */
void rm_list_manager_summary_to_action_json(rm_app_device_type_t device_type, const char *summary, char **action_json, size_t *action_json_size);

/**
 * @brief Update a list item to the backend. This include create and edit.
 * Depending on each node action item's type to determine the operation to the backend.
 * @param[in] list_item List item to update
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on NULL, ESP_FAIL on allocation failure
 */
esp_err_t rm_list_manager_update_item_to_backend(const rm_list_item_t *list_item);

/**
 * @brief Delete a list item from the backend.
 * @param[in] list_item List item to delete
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on NULL, ESP_FAIL on allocation failure
 */
esp_err_t rm_list_manager_delete_item_from_backend(const rm_list_item_t *list_item);

/**
 * @brief Enable a list item from the backend. For schedule list, it means enable and disable the schedule. For scene list, it means activate and deactivate the scene.
 * @param[in] list_item List item to enable
 * @param[in] enabled Whether to enable the list item
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on NULL, ESP_FAIL on allocation failure
 */
esp_err_t rm_list_manager_enable_item_to_backend(const rm_list_item_t *list_item, bool enabled);

/**
 * @brief Set the type of a node action item
 *
 * @param[in] node_action_item Node action item to set the type
 * @param[in] type Node action type
 */
void rm_list_manager_set_node_action_item_type(rm_node_action_item_t *node_action_item, rm_node_action_type_t type);

/**
 * @brief Revert a node action item to the original list item
 *
 * @param[in] list_item List item to revert
 */
void rm_list_manager_revert_node_action_item(rm_list_item_t *list_item);

/**
 * @brief Print a list item
 *
 * @param[in] list_item List item to print
 */
void rm_list_manager_print_item(const rm_list_item_t *list_item);
#ifdef __cplusplus
}
#endif /* __cplusplus */
