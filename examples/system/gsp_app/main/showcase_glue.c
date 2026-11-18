/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>

#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include "showcase_gsp.h"

#include "esp_gsp.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "showcase";

static int64_t s_clock_started_us;
static esp_gsp_list_t s_mode_wheel = ESP_GSP_LIST_NONE;
static bool s_lab_components_visible;

static void on_clock(esp_gsp_handle_t ui, void *ctx)
{
    uint32_t elapsed_seconds;
    uint32_t total_seconds;
    (void)ctx;
    if ((ui == NULL) || (s_clock_started_us == 0)) {
        return;
    }
    elapsed_seconds = (uint32_t)((esp_timer_get_time() - s_clock_started_us) / 1000000);
    total_seconds = (19U * 3600U) + (42U * 60U) + elapsed_seconds;
    if ((elapsed_seconds % 5U) == 0U) {
        ESP_LOGI(TAG, "clock tick %u s", (unsigned)elapsed_seconds);
    }
    gsp_showcase_command_hero_clock_set_time(
        ui, (uint8_t)((total_seconds / 3600U) % 24U),
        (uint8_t)((total_seconds / 60U) % 60U), (uint8_t)(total_seconds % 60U)
    );
}

const uint8_t *showcase_bundle_bytes(size_t *size)
{
    esp_gsp_config_t config = gsp_showcase_config();
    if (size != NULL) {
        *size = config.bundle_size;
    }
    return config.bundle;
}

const void *showcase_runtime_config(void)
{
    static esp_gsp_config_t config;
    config = gsp_showcase_config();
    return &config;
}

int showcase_prepare(esp_gsp_handle_t ui)
{
    s_lab_components_visible = false;
    s_mode_wheel = ESP_GSP_LIST_NONE;
    if (esp_gsp_set_swipe_transition(ui, ESP_GSP_SWIPE_SLIDE_FADE_THROUGH_BLACK) != ESP_GSP_OK) {
        return 0;
    }
    s_clock_started_us = esp_timer_get_time();
    return esp_gsp_timer_create(ui, 1000, on_clock, NULL) != NULL;
}

int showcase_set_brightness_text(esp_gsp_handle_t ui, const char *text)
{
    return esp_gsp_component_set_text(ui, GSP_SHOWCASE_COMMAND_OBJ_KEY_BRIGHTNESS_RESULT, text) == ESP_GSP_OK;
}

int showcase_toggle_quick_settings(esp_gsp_handle_t ui)
{
    bool open = false;
    esp_gsp_err_t result = esp_gsp_drawer_is_open(ui, GSP_SHOWCASE_COMMAND_OBJ_KEY_QUICK_SETTINGS, &open);
    if (result == ESP_GSP_OK) {
        result = open
                 ? esp_gsp_drawer_close(ui, GSP_SHOWCASE_COMMAND_OBJ_KEY_QUICK_SETTINGS, true)
                 : esp_gsp_drawer_open(ui, GSP_SHOWCASE_COMMAND_OBJ_KEY_QUICK_SETTINGS, true);
    }
    return result == ESP_GSP_OK;
}

int showcase_scene_is_command(unsigned scene)
{
    return scene == GSP_SHOWCASE_SCENE_SHOWCASE_COMMAND;
}

int showcase_action_is_exit(unsigned id)
{
    return id == GSP_SHOWCASE_COMMAND_ACT_ID_EXIT_APP;
}

int showcase_action_is_brightness(unsigned id)
{
    return id == GSP_SHOWCASE_COMMAND_ACT_ID_DISPLAY_LEVEL;
}

int showcase_action_is_quick(unsigned id)
{
    return id == GSP_SHOWCASE_COMMAND_ACT_ID_QUICK_ACTION;
}

int showcase_scene_is_lab(unsigned scene)
{
    return scene == GSP_SHOWCASE_SCENE_SHOWCASE_LAB;
}

int showcase_action_is_lab_trigger(unsigned id)
{
    return id == GSP_SHOWCASE_LAB_ACT_ID_LAB_TRIGGER;
}

int showcase_action_is_modal_choice(unsigned id)
{
    return id == GSP_SHOWCASE_LAB_ACT_ID_MODAL_CHOICE;
}

static int show_lab_components(esp_gsp_handle_t gsp, bool show)
{
    s_lab_components_visible = show;
    if (esp_gsp_set_visible(gsp, GSP_SHOWCASE_LAB_BIND_LAB_NAVIGATION, !show) != ESP_GSP_OK) {
        return 0;
    }
    if (esp_gsp_set_visible(gsp, GSP_SHOWCASE_LAB_BIND_LAB_COMPONENTS, show) != ESP_GSP_OK) {
        return 0;
    }
    if (show) {
        if (esp_gsp_set_text(gsp, GSP_SHOWCASE_LAB_BIND_KEYBOARD_INPUT, "") != ESP_GSP_OK) {
            return 0;
        }
        if (esp_gsp_set_cursor(gsp, GSP_SHOWCASE_LAB_BIND_KEYBOARD_INPUT) != ESP_GSP_OK) {
            return 0;
        }
        if (esp_gsp_keyboard_attach(
                    gsp, GSP_SHOWCASE_LAB_ACT_ID_FEATURE_KEYBOARD_KEY, GSP_SHOWCASE_LAB_BIND_KEYBOARD_INPUT
                ) != ESP_GSP_OK) {
            return 0;
        }
        return esp_gsp_page_flow_set_page(gsp, GSP_SHOWCASE_LAB_OBJ_KEY_FEATURE_TABS, 0, false) == ESP_GSP_OK;
    }
    if (esp_gsp_set_cursor(gsp, ESP_GSP_NO_CURSOR) != ESP_GSP_OK) {
        return 0;
    }
    return esp_gsp_keyboard_attach(gsp, ESP_GSP_KEYBOARD_NONE, GSP_SHOWCASE_LAB_BIND_KEYBOARD_INPUT) == ESP_GSP_OK;
}

int showcase_enter_scene(void *ui, unsigned scene)
{
    esp_gsp_handle_t gsp = ui;
    if (scene != GSP_SHOWCASE_SCENE_SHOWCASE_LAB) {
        return 1;
    }
    if (!show_lab_components(gsp, false)) {
        return 0;
    }
    if (s_mode_wheel == ESP_GSP_LIST_NONE) {
        s_mode_wheel = gsp_showcase_lab_mode_wheel_bind(gsp, NULL, NULL);
        if (s_mode_wheel == ESP_GSP_LIST_NONE) {
            return 0;
        }
    }
    return esp_gsp_list_snap(gsp, s_mode_wheel, true) == ESP_GSP_OK;
}

int showcase_handle_lab_trigger(void *ui)
{
    return show_lab_components(ui, !s_lab_components_visible);
}

int showcase_handle_modal_choice(void *ui, unsigned arg)
{
    const char *status = (arg == 1)
                         ? "Applied  |  Focus mode  |  Level 64"
                         : "Cancelled  |  Settings remain unchanged";
    return gsp_showcase_lab_confirm_status_set_text(ui, status) == ESP_OK;
}
