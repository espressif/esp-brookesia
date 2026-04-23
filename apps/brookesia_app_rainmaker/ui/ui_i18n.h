/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_LANG_EN = 0,
    UI_LANG_ZH,
    UI_LANG_MAX,
} ui_lang_t;

typedef enum {
    UI_STR_NAV_HOME = 0,
    UI_STR_NAV_SCHED,
    UI_STR_NAV_SCENES,
    UI_STR_NAV_USER,
    UI_STR_MSGBOX_TITLE_INFO,
    UI_STR_MSGBOX_TEXT_NA,
    UI_STR_TITLE_MY_PROFILE,
    UI_STR_SETTINGS,
    UI_STR_SETTINGS_PLACEHOLDER,
    UI_STR_MENU,
    UI_STR_LOGOUT,
    UI_STR_LOGOUT_FAILED,
    UI_STR_UNKNOWN,
    UI_STR_NOTIFICATION_CENTER,
    UI_STR_PRIVACY_POLICY,
    UI_STR_TERMS_OF_USE,
    UI_STR_LANGUAGE,
    UI_STR_LANGUAGE_BTN_EN,
    UI_STR_LANGUAGE_BTN_ZH,
    UI_STR_LANGUAGE_PICK_HINT,
    /** Shown after picking a language on device: app will soft-restart (no chip reset). */
    UI_STR_LANGUAGE_RESTART_HINT,
    UI_STR_MSGBOX_ERROR,
    UI_STR_MSGBOX_SUCCESS,
    UI_STR_ON,
    UI_STR_OFF,
    UI_STR_DEVICE,
    UI_STR_SWITCH,
    UI_STR_LIGHT,
    UI_STR_FAN,
    UI_STR_DEVICE_OFFLINE,
    UI_STR_FAILED_SET_POWER,
    UI_STR_FAILED_LIST_ACTION,
    UI_STR_SELECT_DEVICES,
    UI_STR_SELECT_MORE,
    UI_STR_SELECTED_DEVICES,
    UI_STR_DONE,
    UI_STR_NO_ACTIONS_SELECTED,
    UI_STR_SELECT_ACTIONS,
    UI_STR_POWER,
    UI_STR_BRIGHTNESS,
    UI_STR_HUE,
    UI_STR_SATURATION,
    UI_STR_SPEED,
    UI_STR_SAVE,
    UI_STR_CANCEL,
    UI_STR_DELETE,
    UI_STR_CREATE_SCHEDULE,
    UI_STR_SCHEDULE_NAME,
    UI_STR_NEXT,
    UI_STR_SCHEDULE_EMPTY,
    UI_STR_SCHEDULE_EMPTY_SUB,
    UI_STR_SCHEDULES_TITLE,
    UI_STR_REPEAT,
    UI_STR_ONCE,
    UI_STR_DEVICE_SINGULAR,
    UI_STR_DEVICE_PLURAL,
    UI_STR_EDIT,
    UI_STR_ADD_SCHEDULE,
    UI_STR_SCENES_TITLE,
    UI_STR_SCENE_EMPTY,
    UI_STR_SCENE_EMPTY_SUB,
    UI_STR_ACTIVATE,
    UI_STR_ACTION_SINGULAR,
    UI_STR_ACTION_PLURAL,
    UI_STR_ADD_SCENE,
    UI_STR_NEW_SCENE,
    UI_STR_LOGIN,
    /** Shown while backend init/login is in progress */
    UI_STR_LOGIN_PLEASE_WAIT,
    UI_STR_EMAIL,
    UI_STR_PASSWORD,
    UI_STR_MY_DEVICES,
    UI_STR_WELCOME_TO,
    UI_STR_HOME_DEFAULT,
    UI_STR_COMMON,
    UI_STR_HOME_PLUS_PLACEHOLDER,
    UI_STR_FAILED_REFRESH_HOME,
    UI_STR_HOME_REFRESH_OK,
    /** Shown on full-screen busy overlay while home list refresh is in progress */
    UI_STR_HOME_REFRESHING,
    UI_STR_UNSUPPORTED_DEVICE,
    UI_STR_WHITE,
    UI_STR_COLOR,
    UI_STR_DEVICE_SETTINGS,
    UI_STR_CONFIRM,
    UI_STR_CONTINUE,
    UI_STR_SELECT_TIMEZONE,
    UI_STR_SEARCH_TIMEZONE,
    UI_STR_NO_UPDATES,
    UI_STR_REMOVE_DEVICE,
    UI_STR_REMOVE_DEVICE_CONFIRM,
    UI_STR_REBOOT,
    UI_STR_REBOOT_CONFIRM,
    UI_STR_WIFI_RESET,
    UI_STR_WIFI_RESET_CONFIRM,
    UI_STR_FACTORY_RESET,
    UI_STR_FACTORY_RESET_CONFIRM,
    UI_STR_FAILED_INIT_RM,
    UI_STR_AM,
    UI_STR_PM,
    UI_STR_SCHEDULE,
    UI_STR_SCENE,
    UI_STR_FAILED_DELETE_SCHEDULE,
    UI_STR_FAILED_DELETE_SCENE,
    UI_STR_FAILED_FIND_SCENE,
    UI_STR_FAILED_ACTIVATE_SCENE,
    /** Full-screen busy while scene activation is in progress */
    UI_STR_SCENE_ACTIVATING,
    /** Shown when scene activation completes successfully */
    UI_STR_SCENE_ACTIVATE_SUCCESS,
    UI_STR_SCHEDULE_NAME_EMPTY,
    UI_STR_FAILED_UPDATE_SCHEDULE,
    UI_STR_TIME_FORMAT_PM,
    UI_STR_TIME_FORMAT_AM,
    UI_STR_FAILED_SET_TZ,
    UI_STR_TZ_SET_OK,
    UI_STR_FAILED_OTA,
    UI_STR_OTA_STARTED,
    UI_STR_FAILED_FACTORY,
    UI_STR_FAILED_REMOVE_DEV,
    UI_STR_DEV_REMOVED_OK,
    UI_STR_FAILED_DEV_NAME,
    UI_STR_DEV_NAME_OK,
    UI_STR_INVALID_DEV_NAME,
    UI_STR_FAILED_REBOOT,
    UI_STR_REBOOT_OK,
    UI_STR_FAILED_WIFI_RESET,
    UI_STR_WIFI_RESET_OK,
    UI_STR_FAILED_FACTORY_RESET,
    UI_STR_FACTORY_RESET_OK,
    UI_STR_FAILED_CHECK_UPDATE,
    UI_STR_FAILED_SET_BRIGHTNESS,
    UI_STR_FAILED_SET_HUE,
    UI_STR_FAILED_SET_SAT,
    UI_STR_FAILED_SET_SPEED,
    UI_STR_EDIT_SCHEDULE,
    UI_STR_EDIT_SCENE,
    UI_STR_CREATE_SCENE_SCREEN,
    UI_STR_UPDATE,
    UI_STR_TIME,
    UI_STR_ACTIONS,
    UI_STR_SCHEDULE_ACTIONS_HINT,
    UI_STR_SCHEDULE_ITEM_EMPTY,
    UI_STR_FAILED_SCHEDULE_SAVE,
    UI_STR_FAILED_SCHEDULE_DELETE_FROM,
    UI_STR_FAILED_SCHEDULE_CREATE_NEW,
    UI_STR_SCHEDULE_COPY_FAILED,
    UI_STR_SCENE_NAME,
    UI_STR_SCENE_DATA_NOT_READY,
    UI_STR_NO_MEMORY,
    UI_STR_SCENE_NAME_EMPTY,
    UI_STR_ADD_ONE_ACTION,
    UI_STR_FAILED_SAVE_SCENE_BACKEND,
    UI_STR_FAILED_CREATE_SCENE_NEW,
    UI_STR_SCENE_NOT_FOUND,
    UI_STR_NODE_ID,
    UI_STR_VERSION_SHORT,
    UI_STR_TIMEZONE_COLON,
    UI_STR_CURRENT_VERSION_COLON,
    UI_STR_NEW_VERSION_COLON,
    UI_STR_CHECK_UPDATES,
    UI_STR_UPDATE_NOW,
    UI_STR_DEVICE_NAME_SECTION,
    UI_STR_NODE_INFORMATION,
    UI_STR_OPERATIONS,
    UI_STR_OTA_SECTION,
    UI_STR_AUTOMATIONS_TITLE,
    UI_STR_AUTOMATIONS_EMPTY,
    UI_STR_AUTOMATIONS_EMPTY_SUB,
    UI_STR_ADD_AUTOMATION,
    UI_STR_OK,
    UI_STR_NOT_IMPLEMENTED,
    UI_STR_OPERATION_FAILED,
    UI_STR_FAILED_ADD_AUTOMATION,
    UI_STR_AUTOMATIONS_ADD_STUB,
    /** Home group dropdown: last row (e.g. manage homes in RainMaker) */
    UI_STR_HOME_MANAGEMENT,
    /** Hue value suffix: "°" (EN) / "度" (ZH). */
    UI_STR_UNIT_DEGREE,
    UI_STR_SCHEDULE_ERR_NO_DEVICE,
    UI_STR_SCHEDULE_ERR_NO_VALID_ACTION,
    UI_STR_SCHEDULE_ERR_DEVICE_UNAVAILABLE,
    /** Full-screen busy while a schedule or scene is being deleted */
    UI_STR_DELETING,
    UI_STR_COUNT
} ui_str_id_t;

void ui_i18n_init(void);
void ui_i18n_set_lang(ui_lang_t lang);
ui_lang_t ui_i18n_get_lang(void);
const char *ui_str(ui_str_id_t id);
bool ui_i18n_uses_cjk(void);

/** Hue value as "303°" (EN) / "303度" (ZH). */
void ui_format_hue_value_string(int hue, char *buf, size_t buf_sz);

/**
 * Backend action summary is English ("Brightness to 85\\nHue to 303"). For display only
 * (storage stays English). When UI is Chinese, maps keys/values to localized strings.
 */
void ui_format_action_summary_for_display(const char *summary_en, char *out, size_t out_sz);

/** When UI is Chinese, map default RainMaker names (Light/Fan/Switch) to localized strings. */
const char *ui_device_name_for_display(const char *backend_name);

// Convenience wrapper to fetch translated string by id (inline for zero-cost)
static inline const char *ui_str_by_id(ui_str_id_t id)
{
    return ui_str(id);
}

#ifdef __cplusplus
}
#endif
