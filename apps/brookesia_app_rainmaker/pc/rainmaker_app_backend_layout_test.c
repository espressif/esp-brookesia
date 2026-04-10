/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rainmaker_app_backend_layout_test.h"
#include "rainmaker_device_group_manager.h"
#include "rainmaker_app_backend_util.h"
#include "rainmaker_app_backend.h"

#include <stdlib.h>
#include <string.h>

void rm_app_backend_layout_test_inject_minimal_home(void)
{
    (void)rm_device_group_manager_init();

    if (g_groups_list == NULL || g_devices_list == NULL) {
        return;
    }

    rm_group_item_t *gi = (rm_group_item_t *)calloc(1, sizeof(rm_group_item_t));
    if (!gi) {
        return;
    }
    gi->group_id = strdup("rm_layout_test_gid");
    gi->group_name = strdup("Layout Test Home");
    if (!gi->group_id || !gi->group_name) {
        rainmaker_app_backend_util_safe_free((void *)gi->group_id);
        rainmaker_app_backend_util_safe_free(gi->group_name);
        rainmaker_app_backend_util_safe_free(gi);
        return;
    }

    struct rm_group_node_list_t *nl = (struct rm_group_node_list_t *)malloc(sizeof(struct rm_group_node_list_t));
    if (!nl) {
        rainmaker_app_backend_util_safe_free((void *)gi->group_id);
        rainmaker_app_backend_util_safe_free(gi->group_name);
        rainmaker_app_backend_util_safe_free(gi);
        return;
    }
    STAILQ_INIT(nl);

    rm_group_node_item_t *gni = (rm_group_node_item_t *)calloc(1, sizeof(rm_group_node_item_t));
    if (!gni) {
        rainmaker_app_backend_util_safe_free(nl);
        rainmaker_app_backend_util_safe_free((void *)gi->group_id);
        rainmaker_app_backend_util_safe_free(gi->group_name);
        rainmaker_app_backend_util_safe_free(gi);
        return;
    }
    gni->node_id = strdup("rm_layout_test_node1");
    if (!gni->node_id) {
        rainmaker_app_backend_util_safe_free(gni);
        rainmaker_app_backend_util_safe_free(nl);
        rainmaker_app_backend_util_safe_free((void *)gi->group_id);
        rainmaker_app_backend_util_safe_free(gi->group_name);
        rainmaker_app_backend_util_safe_free(gi);
        return;
    }
    STAILQ_INSERT_TAIL(nl, gni, next);
    gi->nodes_list = nl;

    rm_device_item_t *di = (rm_device_item_t *)calloc(1, sizeof(rm_device_item_t));
    if (!di) {
        rainmaker_app_backend_util_safe_free((void *)gni->node_id);
        rainmaker_app_backend_util_safe_free(gni);
        rainmaker_app_backend_util_safe_free(nl);
        rainmaker_app_backend_util_safe_free((void *)gi->group_id);
        rainmaker_app_backend_util_safe_free(gi->group_name);
        rainmaker_app_backend_util_safe_free(gi);
        return;
    }
    di->node_id = strdup("rm_layout_test_node1");
    di->name = strdup("Light");
    di->fw_version = strdup("1.0.0");
    if (!di->node_id || !di->name || !di->fw_version) {
        rainmaker_app_backend_util_safe_free((void *)di->node_id);
        rainmaker_app_backend_util_safe_free((void *)di->name);
        rainmaker_app_backend_util_safe_free((void *)di->fw_version);
        rainmaker_app_backend_util_safe_free(di);
        rainmaker_app_backend_util_safe_free((void *)gni->node_id);
        rainmaker_app_backend_util_safe_free(gni);
        rainmaker_app_backend_util_safe_free(nl);
        rainmaker_app_backend_util_safe_free((void *)gi->group_id);
        rainmaker_app_backend_util_safe_free(gi->group_name);
        rainmaker_app_backend_util_safe_free(gi);
        return;
    }

    di->whether_has_system_service = false;
    di->online = true;
    di->whether_has_power = true;
    di->power_on = true;
    di->type = RAINMAKER_APP_DEVICE_TYPE_LIGHT;
    di->timezone = NULL;
    di->last_timestamp = 0;

    STAILQ_INSERT_TAIL(g_groups_list, gi, next);
    STAILQ_INSERT_TAIL(g_devices_list, di, next);
    g_current_group = gi;
}
