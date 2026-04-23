/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui_i18n.h"
#include "rm_platform.h"
#include <stdio.h>
#include <string.h>
#if RM_PLATFORM_IS_HOST
#include <stdlib.h>
#else
#include "nvs_flash.h"
#include "nvs.h"
#endif

#ifndef UI_I18N_DEFAULT_LANG
#define UI_I18N_DEFAULT_LANG UI_LANG_EN
#endif

// NVS keys for persisting UI language preference
static const char *UI_I18N_NVS_NAMESPACE = "ui";
static const char *UI_I18N_NVS_LANG_KEY = "lang";

static const char *const s_strings_en[UI_STR_COUNT] = {
    [UI_STR_NAV_HOME] = "Home",
    [UI_STR_NAV_SCHED] = "Sched",
    [UI_STR_NAV_SCENES] = "Scenes",
    [UI_STR_NAV_USER] = "User",
    [UI_STR_MSGBOX_TITLE_INFO] = "Info",
    [UI_STR_MSGBOX_TEXT_NA] = "N/A",
    [UI_STR_TITLE_MY_PROFILE] = "My Profile",
    [UI_STR_SETTINGS] = "Settings",
    [UI_STR_SETTINGS_PLACEHOLDER] = "Settings",
    [UI_STR_MENU] = "Menu",
    [UI_STR_LOGOUT] = "Log Out",
    [UI_STR_LOGOUT_FAILED] = "Logout failed",
    [UI_STR_UNKNOWN] = "Unknown",
    [UI_STR_NOTIFICATION_CENTER] = "Notification Center",
    [UI_STR_PRIVACY_POLICY] = "Privacy Policy",
    [UI_STR_TERMS_OF_USE] = "Terms of Use",
    [UI_STR_LANGUAGE] = "Language",
    [UI_STR_LANGUAGE_BTN_EN] = "English",
    [UI_STR_LANGUAGE_BTN_ZH] = "中文",
    [UI_STR_LANGUAGE_PICK_HINT] = "Choose interface language",
    [UI_STR_LANGUAGE_RESTART_HINT] = "The app will restart to apply the new language.",
    [UI_STR_MSGBOX_ERROR] = "Error",
    [UI_STR_MSGBOX_SUCCESS] = "Success",
    [UI_STR_ON] = "On",
    [UI_STR_OFF] = "Off",
    [UI_STR_DEVICE] = "Device",
    [UI_STR_SWITCH] = "Switch",
    [UI_STR_LIGHT] = "Light",
    [UI_STR_FAN] = "Fan",
    [UI_STR_DEVICE_OFFLINE] = "Device is offline",
    [UI_STR_FAILED_SET_POWER] = "Failed to set power",
    [UI_STR_FAILED_LIST_ACTION] = "Failed to add list node action",
    [UI_STR_SELECT_DEVICES] = "Select Devices",
    [UI_STR_SELECT_MORE] = "Select More",
    [UI_STR_SELECTED_DEVICES] = "Selected Devices",
    [UI_STR_DONE] = "Done",
    [UI_STR_NO_ACTIONS_SELECTED] = "No Actions Selected",
    [UI_STR_SELECT_ACTIONS] = "Select Actions",
    [UI_STR_POWER] = "Power",
    [UI_STR_BRIGHTNESS] = "Brightness",
    [UI_STR_HUE] = "Hue",
    [UI_STR_SATURATION] = "Saturation",
    [UI_STR_SPEED] = "Speed",
    [UI_STR_SAVE] = "Save",
    [UI_STR_CANCEL] = "Cancel",
    [UI_STR_DELETE] = "Delete",
    [UI_STR_CREATE_SCHEDULE] = "Create Schedule",
    [UI_STR_SCHEDULE_NAME] = "Schedule Name",
    [UI_STR_NEXT] = "Next",
    [UI_STR_SCHEDULE_EMPTY] = "No Schedules Added Yet",
    [UI_STR_SCHEDULE_EMPTY_SUB] = "Create your first schedule to automate your devices",
    [UI_STR_SCHEDULES_TITLE] = "Schedules",
    [UI_STR_REPEAT] = "Repeat",
    [UI_STR_ONCE] = "Once",
    [UI_STR_DEVICE_SINGULAR] = "device",
    [UI_STR_DEVICE_PLURAL] = "devices",
    [UI_STR_EDIT] = "Edit",
    [UI_STR_ADD_SCHEDULE] = "Add Schedule",
    [UI_STR_SCENES_TITLE] = "Scenes",
    [UI_STR_SCENE_EMPTY] = "No Scenes Added Yet",
    [UI_STR_SCENE_EMPTY_SUB] = "Create your first scene to quickly apply device actions",
    [UI_STR_ACTIVATE] = "Activate",
    [UI_STR_ACTION_SINGULAR] = "device",
    [UI_STR_ACTION_PLURAL] = "devices",
    [UI_STR_ADD_SCENE] = "Add Scene",
    [UI_STR_NEW_SCENE] = "New Scene",
    [UI_STR_LOGIN] = "Login",
    [UI_STR_LOGIN_PLEASE_WAIT] = "Signing in...",
    [UI_STR_EMAIL] = "Email",
    [UI_STR_PASSWORD] = "Password",
    [UI_STR_MY_DEVICES] = "My Devices",
    [UI_STR_WELCOME_TO] = "Welcome to",
    [UI_STR_HOME_DEFAULT] = "Home",
    [UI_STR_COMMON] = "Common",
    [UI_STR_HOME_PLUS_PLACEHOLDER] = "Plus clicked (placeholder)",
    [UI_STR_FAILED_REFRESH_HOME] = "Failed to refresh home, please retry later",
    [UI_STR_HOME_REFRESH_OK] = "Home refreshed successfully",
    [UI_STR_HOME_REFRESHING] = "Refreshing...",
    [UI_STR_UNSUPPORTED_DEVICE] = "This device type is not supported",
    [UI_STR_WHITE] = "White",
    [UI_STR_COLOR] = "Color",
    [UI_STR_DEVICE_SETTINGS] = "Device Settings",
    [UI_STR_CONFIRM] = "Confirm",
    [UI_STR_CONTINUE] = "Continue?",
    [UI_STR_SELECT_TIMEZONE] = "Select Timezone",
    [UI_STR_SEARCH_TIMEZONE] = "Search timezone...",
    [UI_STR_NO_UPDATES] = "No updates available",
    [UI_STR_REMOVE_DEVICE] = "Remove Device",
    [UI_STR_REMOVE_DEVICE_CONFIRM] =
    "Are you sure you want to remove this device? This action cannot be undone.",
    [UI_STR_REBOOT] = "Reboot",
    [UI_STR_REBOOT_CONFIRM] =
    "Are you sure you want to reboot this device? The device will restart and may be temporarily unavailable.",
    [UI_STR_WIFI_RESET] = "Wi-Fi Reset",
    [UI_STR_WIFI_RESET_CONFIRM] =
    "Are you sure you want to reset Wi-Fi settings? The device will disconnect the device from the current network.",
    [UI_STR_FACTORY_RESET] = "Factory Reset",
    [UI_STR_FACTORY_RESET_CONFIRM] =
    "Are you sure you want to factory reset this device? All settings will be erased.",
    [UI_STR_FAILED_INIT_RM] = "Failed to initialize RainMaker backend",
    [UI_STR_AM] = "am",
    [UI_STR_PM] = "pm",
    [UI_STR_SCHEDULE] = "Schedule",
    [UI_STR_SCENE] = "Scene",
    [UI_STR_FAILED_DELETE_SCHEDULE] = "Failed to delete schedule in backend",
    [UI_STR_FAILED_DELETE_SCENE] = "Failed to delete scene in backend",
    [UI_STR_FAILED_FIND_SCENE] = "Failed to find scene",
    [UI_STR_FAILED_ACTIVATE_SCENE] = "Failed to activate scene",
    [UI_STR_SCENE_ACTIVATING] = "Activating...",
    [UI_STR_SCENE_ACTIVATE_SUCCESS] = "Activated successfully",
    [UI_STR_SCHEDULE_NAME_EMPTY] = "Schedule name cannot be empty",
    [UI_STR_FAILED_UPDATE_SCHEDULE] = "Failed to update schedule enabled state to backend",
    [UI_STR_TIME_FORMAT_PM] = "PM",
    [UI_STR_TIME_FORMAT_AM] = "AM",
    [UI_STR_FAILED_SET_TZ] = "Failed to set timezone",
    [UI_STR_TZ_SET_OK] = "Timezone set successfully",
    [UI_STR_FAILED_OTA] = "Failed to start OTA update",
    [UI_STR_OTA_STARTED] = "OTA update started successfully",
    [UI_STR_FAILED_FACTORY] = "Failed to factory reset device",
    [UI_STR_FAILED_REMOVE_DEV] = "Failed to remove device",
    [UI_STR_DEV_REMOVED_OK] = "Device removed successfully",
    [UI_STR_FAILED_DEV_NAME] = "Failed to set device name",
    [UI_STR_DEV_NAME_OK] = "Device name updated successfully",
    [UI_STR_INVALID_DEV_NAME] = "Invalid device name",
    [UI_STR_FAILED_REBOOT] = "Failed to reboot device",
    [UI_STR_REBOOT_OK] = "Device rebooted successfully",
    [UI_STR_FAILED_WIFI_RESET] = "Failed to reset Wi-Fi",
    [UI_STR_WIFI_RESET_OK] = "Wi-Fi reset successfully",
    [UI_STR_FAILED_FACTORY_RESET] = "Failed to factory reset device",
    [UI_STR_FACTORY_RESET_OK] = "Device factory reset successfully",
    [UI_STR_FAILED_CHECK_UPDATE] = "Failed to check for updates",
    [UI_STR_FAILED_SET_BRIGHTNESS] = "Failed to set brightness",
    [UI_STR_FAILED_SET_HUE] = "Failed to set hue",
    [UI_STR_FAILED_SET_SAT] = "Failed to set saturation",
    [UI_STR_FAILED_SET_SPEED] = "Failed to set speed",
    [UI_STR_EDIT_SCHEDULE] = "Edit Schedule",
    [UI_STR_EDIT_SCENE] = "Edit Scene",
    [UI_STR_CREATE_SCENE_SCREEN] = "Create Scene",
    [UI_STR_UPDATE] = "Update",
    [UI_STR_TIME] = "Time",
    [UI_STR_ACTIONS] = "Actions",
    [UI_STR_SCHEDULE_ACTIONS_HINT] =
    "Add device actions to create your schedule. Tap the + button to get started.",
    [UI_STR_SCHEDULE_ITEM_EMPTY] = "Schedule item is empty",
    [UI_STR_FAILED_SCHEDULE_SAVE] = "Failed to update schedule to backend",
    [UI_STR_FAILED_SCHEDULE_DELETE_FROM] = "Failed to delete schedule from backend",
    [UI_STR_FAILED_SCHEDULE_CREATE_NEW] = "Failed to create new schedule item",
    [UI_STR_SCHEDULE_COPY_FAILED] = "Schedule not found or failed to copy",
    [UI_STR_SCENE_NAME] = "Scene Name",
    [UI_STR_SCENE_DATA_NOT_READY] = "Scene data not ready",
    [UI_STR_NO_MEMORY] = "No memory",
    [UI_STR_SCENE_NAME_EMPTY] = "Scene name cannot be empty",
    [UI_STR_ADD_ONE_ACTION] = "Please add at least one action",
    [UI_STR_FAILED_SAVE_SCENE_BACKEND] = "Failed to save scene to backend",
    [UI_STR_FAILED_CREATE_SCENE_NEW] = "Failed to create scene",
    [UI_STR_SCENE_NOT_FOUND] = "Scene not found",
    [UI_STR_NODE_ID] = "Node ID:",
    [UI_STR_VERSION_SHORT] = "Version:",
    [UI_STR_TIMEZONE_COLON] = "Timezone:",
    [UI_STR_CURRENT_VERSION_COLON] = "Current Version:",
    [UI_STR_NEW_VERSION_COLON] = "New Version:",
    [UI_STR_CHECK_UPDATES] = "Check for Updates",
    [UI_STR_UPDATE_NOW] = "Update Now",
    [UI_STR_DEVICE_NAME_SECTION] = "Device Name",
    [UI_STR_NODE_INFORMATION] = "Node Information",
    [UI_STR_OPERATIONS] = "Operations",
    [UI_STR_OTA_SECTION] = "OTA",
    [UI_STR_AUTOMATIONS_TITLE] = "Automations",
    [UI_STR_AUTOMATIONS_EMPTY] = "No automations yet",
    [UI_STR_AUTOMATIONS_EMPTY_SUB] =
    "Create scenes, schedules, and\nother automations to control your devices.",
    [UI_STR_ADD_AUTOMATION] = "Add Automation",
    [UI_STR_OK] = "OK",
    [UI_STR_NOT_IMPLEMENTED] = "Not implemented yet",
    [UI_STR_OPERATION_FAILED] = "Operation failed",
    [UI_STR_FAILED_ADD_AUTOMATION] = "Failed to add automation",
    [UI_STR_AUTOMATIONS_ADD_STUB] =
    "Automation added (stub).\n\nThis will connect to the cloud API later.",
    [UI_STR_HOME_MANAGEMENT] = "Home management",
    /* U+00B0 DEGREE SIGN — must be present in the Latin font (same as on-device UI). */
    [UI_STR_UNIT_DEGREE] = "°",
    [UI_STR_SCHEDULE_ERR_NO_DEVICE] = "No device selected.",
    [UI_STR_SCHEDULE_ERR_NO_VALID_ACTION] =
    "No valid device action (configure actions for each device).",
    [UI_STR_SCHEDULE_ERR_DEVICE_UNAVAILABLE] =
    "One or more devices are unavailable (offline or not in this home).",
    [UI_STR_DELETING] = "Deleting...",
};

static const char *const s_strings_zh[UI_STR_COUNT] = {
    [UI_STR_NAV_HOME] = "首页",
    [UI_STR_NAV_SCHED] = "定时任务",
    [UI_STR_NAV_SCENES] = "场景",
    [UI_STR_NAV_USER] = "我的",
    [UI_STR_MSGBOX_TITLE_INFO] = "提示",
    [UI_STR_MSGBOX_TEXT_NA] = "无",
    [UI_STR_TITLE_MY_PROFILE] = "用户中心",
    [UI_STR_SETTINGS] = "设置",
    [UI_STR_SETTINGS_PLACEHOLDER] = "设置",
    [UI_STR_MENU] = "菜单",
    [UI_STR_LOGOUT] = "退出登录",
    [UI_STR_LOGOUT_FAILED] = "退出失败",
    [UI_STR_UNKNOWN] = "未知",
    [UI_STR_NOTIFICATION_CENTER] = "通知中心",
    [UI_STR_PRIVACY_POLICY] = "隐私政策",
    [UI_STR_TERMS_OF_USE] = "使用条款",
    [UI_STR_LANGUAGE] = "语言",
    [UI_STR_LANGUAGE_BTN_EN] = "English",
    [UI_STR_LANGUAGE_BTN_ZH] = "中文",
    [UI_STR_LANGUAGE_PICK_HINT] = "选择界面语言",
    [UI_STR_LANGUAGE_RESTART_HINT] = "界面语言已更改，应用将重启以生效。",
    [UI_STR_MSGBOX_ERROR] = "错误",
    [UI_STR_MSGBOX_SUCCESS] = "成功",
    [UI_STR_ON] = "开",
    [UI_STR_OFF] = "关",
    [UI_STR_DEVICE] = "设备",
    [UI_STR_SWITCH] = "开关",
    [UI_STR_LIGHT] = "灯",
    [UI_STR_FAN] = "风扇",
    [UI_STR_DEVICE_OFFLINE] = "设备离线",
    [UI_STR_FAILED_SET_POWER] = "开关控制失败",
    [UI_STR_FAILED_LIST_ACTION] = "添加动作失败",
    [UI_STR_SELECT_DEVICES] = "选择设备",
    [UI_STR_SELECT_MORE] = "再选择",
    [UI_STR_SELECTED_DEVICES] = "已选设备",
    [UI_STR_DONE] = "完成",
    [UI_STR_NO_ACTIONS_SELECTED] = "未选动作",
    [UI_STR_SELECT_ACTIONS] = "选择动作",
    [UI_STR_POWER] = "电源",
    [UI_STR_BRIGHTNESS] = "亮度",
    [UI_STR_HUE] = "色调",
    [UI_STR_SATURATION] = "饱和度",
    [UI_STR_SPEED] = "速度",
    [UI_STR_SAVE] = "保存",
    [UI_STR_CANCEL] = "取消",
    [UI_STR_DELETE] = "删除",
    [UI_STR_CREATE_SCHEDULE] = "创建定时任务",
    [UI_STR_SCHEDULE_NAME] = "定时任务名称",
    [UI_STR_NEXT] = "下一步",
    [UI_STR_SCHEDULE_EMPTY] = "暂无定时任务",
    [UI_STR_SCHEDULE_EMPTY_SUB] = "创建第一条定时任务以定时控制设备",
    [UI_STR_SCHEDULES_TITLE] = "定时任务",
    [UI_STR_REPEAT] = "重复",
    [UI_STR_ONCE] = "仅一次",
    [UI_STR_DEVICE_SINGULAR] = "设备",
    [UI_STR_DEVICE_PLURAL] = "设备们",
    [UI_STR_EDIT] = "编辑",
    [UI_STR_ADD_SCHEDULE] = "添加定时任务",
    [UI_STR_SCENES_TITLE] = "场景",
    [UI_STR_SCENE_EMPTY] = "暂无场景",
    [UI_STR_SCENE_EMPTY_SUB] = "创建场景以快速控制设备",
    [UI_STR_ACTIVATE] = "激活",
    [UI_STR_ACTION_SINGULAR] = "设备",
    [UI_STR_ACTION_PLURAL] = "设备们",
    [UI_STR_ADD_SCENE] = "添加场景",
    [UI_STR_NEW_SCENE] = "新场景",
    [UI_STR_LOGIN] = "登录",
    [UI_STR_LOGIN_PLEASE_WAIT] = "正在登录...",
    [UI_STR_EMAIL] = "邮箱",
    [UI_STR_PASSWORD] = "密码",
    [UI_STR_MY_DEVICES] = "我的设备",
    [UI_STR_WELCOME_TO] = "欢迎",
    [UI_STR_HOME_DEFAULT] = "家",
    [UI_STR_COMMON] = "常用",
    [UI_STR_HOME_PLUS_PLACEHOLDER] = "加号",
    [UI_STR_FAILED_REFRESH_HOME] = "刷新失败，请稍后重试",
    [UI_STR_HOME_REFRESH_OK] = "刷新成功",
    [UI_STR_HOME_REFRESHING] = "刷新中",
    [UI_STR_UNSUPPORTED_DEVICE] = "不支持的设备类型",
    [UI_STR_WHITE] = "白光",
    [UI_STR_COLOR] = "彩光",
    [UI_STR_DEVICE_SETTINGS] = "设备设置",
    [UI_STR_CONFIRM] = "确认",
    [UI_STR_CONTINUE] = "继续？",
    [UI_STR_SELECT_TIMEZONE] = "选择时区",
    [UI_STR_SEARCH_TIMEZONE] = "搜索时区...",
    [UI_STR_NO_UPDATES] = "无可用更新",
    [UI_STR_REMOVE_DEVICE] = "移除设备",
    [UI_STR_REMOVE_DEVICE_CONFIRM] =
    "确定移除该设备？此操作不可撤销。",
    [UI_STR_REBOOT] = "重启",
    [UI_STR_REBOOT_CONFIRM] =
    "确定重启该设备？重启期间可能暂时不可用。",
    [UI_STR_WIFI_RESET] = "重置 Wi-Fi",
    [UI_STR_WIFI_RESET_CONFIRM] =
    "确定重置 Wi-Fi？设备将断开当前网络。",
    [UI_STR_FACTORY_RESET] = "恢复出厂",
    [UI_STR_FACTORY_RESET_CONFIRM] =
    "确定恢复出厂？所有设置将被清除。",
    [UI_STR_FAILED_INIT_RM] = "RainMaker 后端初始化失败",
    [UI_STR_AM] = "am",
    [UI_STR_PM] = "pm",
    [UI_STR_SCHEDULE] = "定时任务",
    [UI_STR_SCENE] = "场景",
    [UI_STR_FAILED_DELETE_SCHEDULE] = "删除定时任务失败",
    [UI_STR_FAILED_DELETE_SCENE] = "删除场景失败",
    [UI_STR_FAILED_FIND_SCENE] = "未找到场景",
    [UI_STR_FAILED_ACTIVATE_SCENE] = "激活场景失败",
    [UI_STR_SCENE_ACTIVATING] = "激活中",
    [UI_STR_SCENE_ACTIVATE_SUCCESS] = "激活成功",
    [UI_STR_SCHEDULE_NAME_EMPTY] = "定时任务名不能为空",
    [UI_STR_FAILED_UPDATE_SCHEDULE] = "更新定时任务状态失败",
    [UI_STR_TIME_FORMAT_PM] = "下午",
    [UI_STR_TIME_FORMAT_AM] = "上午",
    [UI_STR_FAILED_SET_TZ] = "设置时区失败",
    [UI_STR_TZ_SET_OK] = "时区设置成功",
    [UI_STR_FAILED_OTA] = "OTA 升级启动失败",
    [UI_STR_OTA_STARTED] = "OTA 已开始",
    [UI_STR_FAILED_FACTORY] = "出厂恢复失败",
    [UI_STR_FAILED_REMOVE_DEV] = "移除设备失败",
    [UI_STR_DEV_REMOVED_OK] = "设备已移除",
    [UI_STR_FAILED_DEV_NAME] = "修改设备名称失败",
    [UI_STR_DEV_NAME_OK] = "设备名称已更新",
    [UI_STR_INVALID_DEV_NAME] = "无效的设备名称",
    [UI_STR_FAILED_REBOOT] = "重启失败",
    [UI_STR_REBOOT_OK] = "设备已重启",
    [UI_STR_FAILED_WIFI_RESET] = "Wi-Fi 重置失败",
    [UI_STR_WIFI_RESET_OK] = "Wi-Fi 已重置",
    [UI_STR_FAILED_FACTORY_RESET] = "出厂恢复失败",
    [UI_STR_FACTORY_RESET_OK] = "已恢复出厂",
    [UI_STR_FAILED_CHECK_UPDATE] = "检查更新失败",
    [UI_STR_FAILED_SET_BRIGHTNESS] = "设置亮度失败",
    [UI_STR_FAILED_SET_HUE] = "设置色调失败",
    [UI_STR_FAILED_SET_SAT] = "设置饱和度失败",
    [UI_STR_FAILED_SET_SPEED] = "设置速度失败",
    [UI_STR_EDIT_SCHEDULE] = "编辑定时任务",
    [UI_STR_EDIT_SCENE] = "编辑场景",
    [UI_STR_CREATE_SCENE_SCREEN] = "创建场景",
    [UI_STR_UPDATE] = "更新",
    [UI_STR_TIME] = "时间",
    [UI_STR_ACTIONS] = "动作",
    [UI_STR_SCHEDULE_ACTIONS_HINT] =
    "添加设备动作以创建定时任务。点击 + 开始。",
    [UI_STR_SCHEDULE_ITEM_EMPTY] = "定时任务项为空",
    [UI_STR_FAILED_SCHEDULE_SAVE] = "保存定时任务失败",
    [UI_STR_FAILED_SCHEDULE_DELETE_FROM] = "从后端删除定时任务失败",
    [UI_STR_FAILED_SCHEDULE_CREATE_NEW] = "创建定时任务失败",
    [UI_STR_SCHEDULE_COPY_FAILED] = "未找到定时任务或复制失败",
    [UI_STR_SCENE_NAME] = "场景名称",
    [UI_STR_SCENE_DATA_NOT_READY] = "场景数据未就绪",
    [UI_STR_NO_MEMORY] = "内存不足",
    [UI_STR_SCENE_NAME_EMPTY] = "场景名不能为空",
    [UI_STR_ADD_ONE_ACTION] = "请至少添加一个动作",
    [UI_STR_FAILED_SAVE_SCENE_BACKEND] = "保存场景失败",
    [UI_STR_FAILED_CREATE_SCENE_NEW] = "创建场景失败",
    [UI_STR_SCENE_NOT_FOUND] = "未找到场景",
    [UI_STR_NODE_ID] = "节点 ID：",
    [UI_STR_VERSION_SHORT] = "版本：",
    [UI_STR_TIMEZONE_COLON] = "时区：",
    [UI_STR_CURRENT_VERSION_COLON] = "当前版本：",
    [UI_STR_NEW_VERSION_COLON] = "新版本：",
    [UI_STR_CHECK_UPDATES] = "检查更新",
    [UI_STR_UPDATE_NOW] = "立即更新",
    [UI_STR_DEVICE_NAME_SECTION] = "设备名称",
    [UI_STR_NODE_INFORMATION] = "节点信息",
    [UI_STR_OPERATIONS] = "操作",
    [UI_STR_OTA_SECTION] = "OTA",
    [UI_STR_AUTOMATIONS_TITLE] = "自动化",
    [UI_STR_AUTOMATIONS_EMPTY] = "暂无自动化",
    [UI_STR_AUTOMATIONS_EMPTY_SUB] =
    "创建场景、定时任务等\n以自动化控制设备",
    [UI_STR_ADD_AUTOMATION] = "添加自动化",
    [UI_STR_OK] = "确定",
    [UI_STR_NOT_IMPLEMENTED] = "暂未实现",
    [UI_STR_OPERATION_FAILED] = "操作失败",
    [UI_STR_FAILED_ADD_AUTOMATION] = "添加自动化失败",
    [UI_STR_AUTOMATIONS_ADD_STUB] =
    "已添加。\n\n后续将连接云端 API。",
    [UI_STR_HOME_MANAGEMENT] = "家庭管理",
    [UI_STR_UNIT_DEGREE] = "度",
    [UI_STR_SCHEDULE_ERR_NO_DEVICE] = "未选择任何设备。",
    [UI_STR_SCHEDULE_ERR_NO_VALID_ACTION] =
    "没有有效的设备动作（请为每台设备配置动作）。",
    [UI_STR_SCHEDULE_ERR_DEVICE_UNAVAILABLE] =
    "部分设备不可用（离线或不在当前家庭）。",
    [UI_STR_DELETING] = "删除中...",
};

static const char *const *s_tables[UI_LANG_MAX] = {
    [UI_LANG_EN] = s_strings_en,
    [UI_LANG_ZH] = s_strings_zh,
};

static ui_lang_t s_lang = UI_I18N_DEFAULT_LANG;

#if RM_PLATFORM_IS_HOST
static const char *ui_i18n_host_lang_file_path(void)
{
    const char *home = getenv("HOME");
    static char path[512];
    if (!home || !home[0]) {
        home = ".";
    }
    snprintf(path, sizeof(path), "%s/.cache/rainmaker_app_lang", home);
    return path;
}
#endif

void ui_i18n_init(void)
{
    s_lang = UI_I18N_DEFAULT_LANG;
#if RM_PLATFORM_IS_HOST
    FILE *f = fopen(ui_i18n_host_lang_file_path(), "rb");
    if (f) {
        int c = fgetc(f);
        fclose(f);
        if (c >= 0 && c < (int)UI_LANG_MAX) {
            s_lang = (ui_lang_t)c;
        }
    }
#else
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(UI_I18N_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK && handle != 0) {
        uint8_t lang = (uint8_t)UI_LANG_EN;
        if (nvs_get_u8(handle, UI_I18N_NVS_LANG_KEY, &lang) == ESP_OK) {
            if (lang < UI_LANG_MAX) {
                s_lang = (ui_lang_t)lang;
            }
        }
        nvs_close(handle);
    }
#endif
}

void ui_i18n_set_lang(ui_lang_t lang)
{
    if (lang >= UI_LANG_MAX) {
        return;
    }
    s_lang = lang;
#if RM_PLATFORM_IS_HOST
    {
        const char *p = ui_i18n_host_lang_file_path();
        FILE *f = fopen(p, "wb");
        if (f) {
            fputc((int)lang, f);
            fclose(f);
        }
    }
#else
    {
        nvs_handle_t handle = 0;
        esp_err_t err = nvs_open(UI_I18N_NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err == ESP_OK) {
            nvs_set_u8(handle, UI_I18N_NVS_LANG_KEY, (uint8_t)lang);
            nvs_commit(handle);
            nvs_close(handle);
        }
    }
#endif
}

ui_lang_t ui_i18n_get_lang(void)
{
    return s_lang;
}

const char *ui_str(ui_str_id_t id)
{
    if (id >= UI_STR_COUNT) {
        return "";
    }
    const char *const *table = s_tables[s_lang];
    if (table == NULL || table[id] == NULL) {
        table = s_tables[UI_LANG_EN];
    }
    return table[id] ? table[id] : "";
}

bool ui_i18n_uses_cjk(void)
{
    return s_lang == UI_LANG_ZH;
}

const char *ui_device_name_for_display(const char *backend_name)
{
    if (!backend_name || !backend_name[0]) {
        return ui_str(UI_STR_DEVICE);
    }
    if (ui_i18n_uses_cjk()) {
        if (strcmp(backend_name, "Light") == 0) {
            return ui_str(UI_STR_LIGHT);
        }
        if (strcmp(backend_name, "Fan") == 0) {
            return ui_str(UI_STR_FAN);
        }
        if (strcmp(backend_name, "Switch") == 0) {
            return ui_str(UI_STR_SWITCH);
        }
    }
    return backend_name;
}

void ui_format_hue_value_string(int hue, char *buf, size_t buf_sz)
{
    if (!buf || buf_sz == 0) {
        return;
    }
    snprintf(buf, buf_sz, "%d%s", hue, ui_str(UI_STR_UNIT_DEGREE));
}

void ui_format_action_summary_for_display(const char *summary_en, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) {
        return;
    }
    out[0] = '\0';
    if (!summary_en || !summary_en[0]) {
        return;
    }
    if (!ui_i18n_uses_cjk()) {
        snprintf(out, out_sz, "%s", summary_en);
        return;
    }
    if (strcmp(summary_en, "No Actions Selected") == 0) {
        snprintf(out, out_sz, "%s", ui_str(UI_STR_NO_ACTIONS_SELECTED));
        return;
    }

    static const char sep[] = " to ";
    size_t w = 0;
    const char *line_start = summary_en;
    bool first_line = true;

    while (*line_start && w + 1 < out_sz) {
        const char *line_end = strchr(line_start, '\n');
        size_t line_len = line_end ? (size_t)(line_end - line_start) : strlen(line_start);
        char line[192];
        if (line_len >= sizeof(line)) {
            line_len = sizeof(line) - 1;
        }
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';

        if (!first_line && w < out_sz - 1) {
            out[w++] = '\n';
            out[w] = '\0';
        }
        first_line = false;

        char *psep = strstr(line, sep);
        if (psep && psep != line) {
            *psep = '\0';
            const char *key = line;
            const char *val = psep + strlen(sep);
            const char *kzh = key;
            if (strcmp(key, "Power") == 0) {
                kzh = ui_str(UI_STR_POWER);
            } else if (strcmp(key, "Brightness") == 0) {
                kzh = ui_str(UI_STR_BRIGHTNESS);
            } else if (strcmp(key, "Hue") == 0) {
                kzh = ui_str(UI_STR_HUE);
            } else if (strcmp(key, "Saturation") == 0) {
                kzh = ui_str(UI_STR_SATURATION);
            } else if (strcmp(key, "Speed") == 0) {
                kzh = ui_str(UI_STR_SPEED);
            }
            const char *vzh = val;
            if (strcmp(key, "Power") == 0) {
                if (strcmp(val, "On") == 0) {
                    vzh = ui_str(UI_STR_ON);
                } else if (strcmp(val, "Off") == 0) {
                    vzh = ui_str(UI_STR_OFF);
                }
            }
            int n = snprintf(out + w, out_sz - w, "%s: %s", kzh, vzh);
            if (n <= 0 || (size_t)n >= out_sz - w) {
                break;
            }
            w += (size_t)n;
        } else {
            int n = snprintf(out + w, out_sz - w, "%s", line);
            if (n <= 0 || (size_t)n >= out_sz - w) {
                break;
            }
            w += (size_t)n;
        }

        if (line_end) {
            line_start = line_end + 1;
        } else {
            break;
        }
    }
}
