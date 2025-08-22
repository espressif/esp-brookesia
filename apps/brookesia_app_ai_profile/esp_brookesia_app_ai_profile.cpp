/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lvgl.h"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:App:AI_Profile"
#include "esp_lib_utils.h"
#include "ui/ui.h"
#include "esp_brookesia_app_ai_profile.hpp"

#define MAX_ROBOT_NUM 2

#define UI_SCREEN_AI_PROFILE_PANEL_INDICATOR_OPA_ACTIVE     128
#define UI_SCREEN_AI_PROFILE_PANEL_INDICATOR_OPA_INACTIVE   50

#define RESTART_CHAT_THREAD_NAME              "restart_chat"
#define RESTART_CHAT_THREAD_STACK_SIZE        (10 * 1024)
#define RESTART_CHAT_THREAD_STACK_CAPS_EXT    (true)

#define APP_NAME "AI_Profile"

using namespace std;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems::speaker;
using namespace esp_brookesia::ai_framework;

LV_IMG_DECLARE(esp_brookesia_app_icon_launcher_ai_profile_112_112);

namespace esp_brookesia::apps {

AI_Profile *AI_Profile::requestInstance()
{
    if (_instance == nullptr) {
        ESP_UTILS_CHECK_EXCEPTION_RETURN(
            _instance = new AI_Profile(), nullptr, "Failed to create instance"
        );
    }
    return _instance;
}

AI_Profile::AI_Profile():
    App(APP_NAME, &esp_brookesia_app_icon_launcher_ai_profile_112_112, true)
{
}

AI_Profile::~AI_Profile()
{
}

bool AI_Profile::run(void)
{
    ESP_UTILS_LOGD("Run");

    if (Agent::requestInstance()->hasChatState(Agent::ChatState::ChatStateStarted)) {
        goto next;
    }

    {
        auto screen = lv_obj_create(nullptr);
        ESP_UTILS_CHECK_NULL_RETURN(screen, false, "Failed to create screen");
        auto label = lv_label_create(screen);
        ESP_UTILS_CHECK_NULL_RETURN(label, false, "Failed to create label");
        lv_label_set_text(label, "Chat server is not connected, please exit and restart the app after the server is connected");
        lv_obj_center(label);
        lv_obj_set_size(label, lv_pct(80), LV_SIZE_CONTENT);
        lv_scr_load(screen);
        goto end;
    }

next: {
        std::vector<CozeChatRobotInfo> robot_infos;
        ESP_UTILS_CHECK_FALSE_RETURN(
            Agent::requestInstance()->getRobotInfo(robot_infos), false, "Failed to get robot infos"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            Agent::requestInstance()->getCurrentRobotIndex(_robot_current_index),
            false,
            "Failed to get current robot index"
        );
        _robot_next_index = _robot_current_index;

        // Create all UI resources here
        speaker_ai_profile_ui_init();

        lv_obj_add_flag(ui_ScreenAIProfileTabpageTabPageRole1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ScreenAIProfilePanelPanelIndicator1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ScreenAIProfileTabpageTabPageRole2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ScreenAIProfilePanelPanelIndicator2, LV_OBJ_FLAG_HIDDEN);

        // Update robot information
        for (int i = 0; i < robot_infos.size() && (i < MAX_ROBOT_NUM); i++) {
            if (i == 0) {
                lv_label_set_text(ui_ScreenAIProfileLabelLabelRole1Name, robot_infos[i].name.c_str());
                lv_label_set_text(ui_ScreenAIProfileLabelLabelRole1Description, robot_infos[i].description.c_str());
                lv_obj_remove_flag(ui_ScreenAIProfileTabpageTabPageRole1, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(ui_ScreenAIProfilePanelPanelIndicator1, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_label_set_text(ui_ScreenAIProfileLabelLabelRole2Name, robot_infos[i].name.c_str());
                lv_label_set_text(ui_ScreenAIProfileLabelLabelRole2Description, robot_infos[i].description.c_str());
                lv_obj_remove_flag(ui_ScreenAIProfileTabpageTabPageRole2, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(ui_ScreenAIProfilePanelPanelIndicator2, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    // Update indicator
    lv_obj_set_style_bg_opa(ui_ScreenAIProfilePanelPanelIndicator1, UI_SCREEN_AI_PROFILE_PANEL_INDICATOR_OPA_ACTIVE, 0);
    lv_obj_set_style_bg_opa(ui_ScreenAIProfilePanelPanelIndicator2, UI_SCREEN_AI_PROFILE_PANEL_INDICATOR_OPA_INACTIVE, 0);

    lv_obj_add_event_cb(ui_ScreenAIProfileTabviewTabView, [](lv_event_t *e) {
        auto app = (AI_Profile *)lv_event_get_user_data(e);
        ESP_UTILS_CHECK_NULL_EXIT(app, "App is NULL");

        lv_obj_t *tab_view = (lv_obj_t *)lv_event_get_target(e);
        ESP_UTILS_CHECK_NULL_EXIT(tab_view, "Tab view is NULL");

        int index = lv_tabview_get_tab_act(tab_view);
        ESP_UTILS_LOGD("Tab changed to index: %d", index);

        if (index == 0) {
            lv_obj_set_style_bg_opa(
                ui_ScreenAIProfilePanelPanelIndicator1, UI_SCREEN_AI_PROFILE_PANEL_INDICATOR_OPA_ACTIVE, 0
            );
            lv_obj_set_style_bg_opa(
                ui_ScreenAIProfilePanelPanelIndicator2, UI_SCREEN_AI_PROFILE_PANEL_INDICATOR_OPA_INACTIVE, 0
            );
        } else if (index == 1) {
            lv_obj_set_style_bg_opa(
                ui_ScreenAIProfilePanelPanelIndicator1, UI_SCREEN_AI_PROFILE_PANEL_INDICATOR_OPA_INACTIVE, 0
            );
            lv_obj_set_style_bg_opa(
                ui_ScreenAIProfilePanelPanelIndicator2, UI_SCREEN_AI_PROFILE_PANEL_INDICATOR_OPA_ACTIVE, 0
            );
        }
    }, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_add_event_cb(ui_ScreenAIProfileButtonButtonRole1Select, [](lv_event_t *e) {
        auto app = (AI_Profile *)lv_event_get_user_data(e);
        ESP_UTILS_CHECK_NULL_EXIT(app, "App is NULL");

        lv_obj_remove_flag(ui_ScreenAIProfileImageImageRole1Select, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(ui_ScreenAIProfileButtonButtonRole2Select, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ScreenAIProfileButtonButtonRole1Select, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ScreenAIProfileImageImageRole2Select, LV_OBJ_FLAG_HIDDEN);

        app->_robot_next_index = 0;
    }, LV_EVENT_CLICKED, this);

    lv_obj_add_event_cb(ui_ScreenAIProfileButtonButtonRole2Select, [](lv_event_t *e) {
        auto app = (AI_Profile *)lv_event_get_user_data(e);
        ESP_UTILS_CHECK_NULL_EXIT(app, "App is NULL");

        lv_obj_add_flag(ui_ScreenAIProfileImageImageRole1Select, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ScreenAIProfileButtonButtonRole2Select, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(ui_ScreenAIProfileButtonButtonRole1Select, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(ui_ScreenAIProfileImageImageRole2Select, LV_OBJ_FLAG_HIDDEN);

        app->_robot_next_index = 1;
    }, LV_EVENT_CLICKED, this);

    lv_tabview_set_active(ui_ScreenAIProfileTabviewTabView, _robot_next_index, LV_ANIM_OFF);
    if (_robot_current_index == 0) {
        lv_obj_remove_flag(ui_ScreenAIProfileImageImageRole1Select, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(ui_ScreenAIProfileButtonButtonRole2Select, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ScreenAIProfileButtonButtonRole1Select, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ScreenAIProfileImageImageRole2Select, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui_ScreenAIProfileImageImageRole1Select, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ScreenAIProfileButtonButtonRole2Select, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(ui_ScreenAIProfileButtonButtonRole1Select, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(ui_ScreenAIProfileImageImageRole2Select, LV_OBJ_FLAG_HIDDEN);
    }

end:
    return true;
}

bool AI_Profile::back(void)
{
    ESP_UTILS_LOGD("Back");

    // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

bool AI_Profile::close(void)
{
    ESP_UTILS_LOGD("Close");

    if (_robot_current_index == _robot_next_index) {
        goto end;
    }

    if (!Agent::requestInstance()->setCurrentRobotIndex(_robot_next_index)) {
        ESP_UTILS_LOGE("Set current robot failed");
    } else {
        Agent::requestInstance()->sendChatEvent(Agent::ChatEvent::Stop);
        Agent::requestInstance()->sendChatEvent(Agent::ChatEvent::Start, false);
    }

end:
    return true;
}

// bool AI_Profile::init()
// {
//     ESP_UTILS_LOGD("Init");

//     /* Do some initialization here if needed */

//     return true;
// }

// bool AI_Profile::deinit()
// {
//     ESP_UTILS_LOGD("Deinit");

//     /* Do some deinitialization here if needed */

//     return true;
// }

// bool AI_Profile::pause()
// {
//     ESP_UTILS_LOGD("Pause");

//     /* Do some operations here if needed */

//     return true;
// }

// bool AI_Profile::resume()
// {
//     ESP_UTILS_LOGD("Resume");

//     /* Do some operations here if needed */

//     return true;
// }

// bool AI_Profile::cleanResource()
// {
//     ESP_UTILS_LOGD("Clean resource");

//     /* Do some cleanup here if needed */

//     return true;
// }

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, AI_Profile, APP_NAME, []()
{
    return std::shared_ptr<AI_Profile>(AI_Profile::requestInstance(), [](AI_Profile * p) {});
})

} // namespace esp_brookesia::apps
