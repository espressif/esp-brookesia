/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * PC backend: same data model as rainmaker_app_backend.c — uses rainmaker_api_handle (HAL → libcurl)
 * and rainmaker_device_group_manager for real cloud groups/devices.
 */

#include "rainmaker_app_backend.h"
#include "rainmaker_device_group_manager.h"
#include "rainmaker_list_manager.h"
#include "rainmaker_api_handle.h"
#include "rainmaker_app_backend_util.h"
#include "rainmaker_app_backend_shared.h"
#include "esp_err.h"
#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TAG = "rm_backend_pc";
static char s_cached_email[256];

#define GLOBAL_RM_BASE_URL "https://api.rainmaker.espressif.com"

/* ---------- Cache file helpers (~/.cache/rainmaker_pc/) ---------- */

static int pc_cache_dir(char *dir, size_t dir_sz)
{
    const char *home = getenv("HOME");
    if (!home || !home[0]) {
        return -1;
    }
    snprintf(dir, dir_sz, "%s/.cache/rainmaker_pc", home);
    return 0;
}

static void pc_ensure_cache_dir(void)
{
    char d[512];
    const char *home = getenv("HOME");
    if (!home) {
        return;
    }
    snprintf(d, sizeof(d), "%s/.cache", home);
    (void)mkdir(d, 0755);
    if (pc_cache_dir(d, sizeof(d)) == 0) {
        (void)mkdir(d, 0755);
    }
}

static int pc_token_path(char *out, size_t out_sz)
{
    char dir[512];
    if (pc_cache_dir(dir, sizeof(dir)) != 0) {
        return -1;
    }
    snprintf(out, out_sz, "%s/refresh_token", dir);
    return 0;
}

static int pc_email_path(char *out, size_t out_sz)
{
    char dir[512];
    if (pc_cache_dir(dir, sizeof(dir)) != 0) {
        return -1;
    }
    snprintf(out, out_sz, "%s/email", dir);
    return 0;
}

static bool pc_read_file_nonempty(const char *path, char *buf, size_t cap)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        return false;
    }
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
        buf[--n] = '\0';
    }
    return n > 0;
}

static bool pc_write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        return false;
    }
    fputs(text, f);
    fclose(f);
    return true;
}

static bool pc_refresh_token_cached_nonempty(void)
{
    char path[512];
    char buf[32];
    if (pc_token_path(path, sizeof(path)) != 0) {
        return false;
    }
    return pc_read_file_nonempty(path, buf, sizeof(buf));
}

/* ---------- Backend API (aligned with rainmaker_app_backend.c) ---------- */

esp_err_t rm_app_backend_init(void)
{
    esp_err_t ret = rm_device_group_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "group manager init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = rm_list_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "list manager init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = rainmaker_api_init(GLOBAL_RM_BASE_URL, NULL, NULL);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "rainmaker_api_init failed: %s", esp_err_to_name(ret));
        return ret;
    }
#ifdef RM_PC_HAVE_CURL
    {
        char rt[4096];
        char path_rt[512];
        if (pc_token_path(path_rt, sizeof(path_rt)) == 0 && pc_read_file_nonempty(path_rt, rt, sizeof(rt))) {
            (void)rainmaker_api_set_refresh_token(rt);
        }
        char em[256];
        char path_em[512];
        if (pc_email_path(path_em, sizeof(path_em)) == 0 && pc_read_file_nonempty(path_em, em, sizeof(em))) {
            strncpy(s_cached_email, em, sizeof(s_cached_email) - 1);
            s_cached_email[sizeof(s_cached_email) - 1] = '\0';
        }
    }
#endif
    return ESP_OK;
}

void rm_app_backend_deinit(void)
{
    rm_device_group_manager_deinit();
    rm_list_manager_deinit();
    (void)rainmaker_api_deinit();
}

bool rm_app_backend_is_logged_in(void)
{
#ifdef RM_PC_HAVE_CURL
    return pc_refresh_token_cached_nonempty();
#else
    return false;
#endif
}

bool rm_app_backend_login(const char *email, const char *password, const char **out_err_reason)
{
    if (out_err_reason) {
        *out_err_reason = NULL;
    }
    if (email == NULL || password == NULL) {
        if (out_err_reason) {
            *out_err_reason = strdup("Email or password is empty");
        }
        return false;
    }
#ifndef RM_PC_HAVE_CURL
    if (out_err_reason) {
        *out_err_reason = strdup("PC build without libcurl");
    }
    return false;
#else
    esp_err_t ret = rainmaker_api_set_username_password(email, password);
    if (ret != ESP_OK) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to set credentials");
        }
        return false;
    }
    ret = rainmaker_api_login();
    if (ret != ESP_OK) {
        if (out_err_reason) {
            *out_err_reason = strdup("Login failed");
        }
        return false;
    }

    char *refresh_token = NULL;
    if (rainmaker_api_get_refresh_token(&refresh_token) != ESP_OK || !refresh_token || !refresh_token[0]) {
        rainmaker_app_backend_util_safe_free(refresh_token);
        if (out_err_reason) {
            *out_err_reason = strdup("No refresh token from server");
        }
        return false;
    }

    pc_ensure_cache_dir();
    char path_tok[512];
    char path_email[512];
    if (pc_token_path(path_tok, sizeof(path_tok)) != 0 || pc_email_path(path_email, sizeof(path_email)) != 0) {
        free(refresh_token);
        if (out_err_reason) {
            *out_err_reason = strdup("Cannot resolve cache path");
        }
        return false;
    }
    if (!pc_write_file(path_tok, refresh_token)) {
        free(refresh_token);
        if (out_err_reason) {
            *out_err_reason = strdup("Cannot save refresh token");
        }
        return false;
    }
    free(refresh_token);

    if (!pc_write_file(path_email, email)) {
        if (out_err_reason) {
            *out_err_reason = strdup("Cannot save email");
        }
        return false;
    }

    strncpy(s_cached_email, email, sizeof(s_cached_email) - 1);
    s_cached_email[sizeof(s_cached_email) - 1] = '\0';
    return true;
#endif
}

const char *rm_app_backend_get_current_group_name(void)
{
    return rm_app_backend_shared_get_current_group_name();
}

int rm_app_backend_get_groups_count(void)
{
    return rm_app_backend_shared_get_groups_count();
}

bool rm_app_backend_get_group_by_index(int index, rm_app_group_t *group)
{
    return rm_app_backend_shared_get_group_by_index(index, group);
}

void rm_app_backend_set_current_group_by_name(const char *group_name)
{
    rm_app_backend_shared_set_current_group_by_name(group_name);
}

int rm_app_backend_get_devices_count(void)
{
    return rm_app_backend_shared_get_devices_count();
}

rm_device_item_handle_t rm_app_backend_get_device_by_index(int index, rm_app_device_t *device)
{
    return rm_app_backend_shared_get_device_by_index(index, device);
}

int rm_app_backend_get_current_home_devices_count(void)
{
    return rm_app_backend_shared_get_current_home_devices_count();
}

rm_device_item_handle_t rm_app_backend_get_current_home_device_by_index(int index, rm_app_device_t *device)
{
    return rm_app_backend_shared_get_current_home_device_by_index(index, device);
}

bool rm_app_backend_get_device_info_by_handle(rm_device_item_handle_t handle, rm_app_device_t *device)
{
    return rm_app_backend_shared_get_device_info_by_handle(handle, device);
}

rm_device_item_handle_t rm_app_backend_get_device_handle_by_node_id(const char *node_id)
{
    return rm_app_backend_shared_get_device_handle_by_node_id(node_id);
}

rm_app_device_type_t rm_app_backend_get_device_type(rm_device_item_handle_t device)
{
    return rm_app_backend_shared_get_device_type(device);
}

bool rm_app_backend_refresh_home_screen(bool all)
{
#ifdef RM_PC_HAVE_CURL
    esp_err_t ret = ESP_FAIL;
    char *saved_group_name = NULL;
    if (all && g_current_group && g_current_group->group_name && g_current_group->group_name[0]) {
        saved_group_name = strdup(g_current_group->group_name);
    }
    if (all) {
        rm_device_group_manager_clear_group();
        ret = rm_device_group_manager_update_groups_list();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "update groups: %s", esp_err_to_name(ret));
            free(saved_group_name);
            return false;
        }
        if (saved_group_name) {
            ret = rm_device_group_manager_find_group(saved_group_name);
            if (ret != ESP_OK) {
                (void)rm_device_group_manager_select_default_group();
            }
        } else {
            (void)rm_device_group_manager_select_default_group();
        }
        free(saved_group_name);
    }
    rm_device_group_manager_clear_device();
    rm_list_manager_clear_item();
    ret = rm_device_group_manager_update_devices_list();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "update devices: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
#else
    (void)all;
    return true;
#endif
}

bool rm_app_backend_refresh_scenes(void)
{
    return rm_app_backend_refresh_home_screen(false);
}

bool rm_app_backend_refresh_schedules(void)
{
    return rm_app_backend_refresh_home_screen(false);
}

bool rm_app_backend_add_automations(void)
{
    return true;
}

const char *rm_app_backend_get_current_user_email(void)
{
#ifdef RM_PC_HAVE_CURL
    if (s_cached_email[0]) {
        return s_cached_email;
    }
    char path[512];
    static char buf[256];
    if (pc_email_path(path, sizeof(path)) == 0 && pc_read_file_nonempty(path, buf, sizeof(buf))) {
        strncpy(s_cached_email, buf, sizeof(s_cached_email) - 1);
        s_cached_email[sizeof(s_cached_email) - 1] = '\0';
        return s_cached_email;
    }
#endif
    return "pc@local";
}

bool rm_app_backend_logout(void)
{
#ifdef RM_PC_HAVE_CURL
    char p[512];
    if (pc_token_path(p, sizeof(p)) == 0) {
        (void)unlink(p);
    }
    if (pc_email_path(p, sizeof(p)) == 0) {
        (void)unlink(p);
    }
    s_cached_email[0] = '\0';
#endif
    rm_device_group_manager_deinit();
    rm_list_manager_deinit();
    (void)rainmaker_api_deinit();
    (void)rm_device_group_manager_init();
    (void)rm_list_manager_init();
    (void)rainmaker_api_init(GLOBAL_RM_BASE_URL, NULL, NULL);
    return true;
}
