/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_gui_internal.h"
#include "esp_brookesia_lv_helper.hpp"
#include "src/misc/lv_anim.h"
#if !ESP_BROOKESIA_LVGL_ANIMATION_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_lv_utils.hpp"
#include "esp_brookesia_lv_animation.hpp"

namespace esp_brookesia::gui {

LvAnimation::LvAnimation():
    _user_data{this, nullptr}
{
    lv_anim_init(&_native);
    lv_anim_set_user_data(&_native, &_user_data);
}

LvAnimation::~LvAnimation()
{
    if (!lv_anim_delete(_native.var, _native.exec_cb)) {
        ESP_UTILS_LOGD("Delete animation failed");
    }
}

void LvAnimation::setStyleAttribute(const StyleAnimation &attribute) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: attribute(%p)", &attribute);

    lv_anim_set_values(&_native, attribute.start_value, attribute.end_value);
    lv_anim_set_duration(&_native, attribute.duration_ms);
    lv_anim_set_path_cb(&_native, getLvAnimPathCb(attribute.path_type));

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
}

void LvAnimation::setVariableExecutionMethod(void *variable, VariableExecutionMethod method) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: variable(%p), method(%p)", variable, method);

    _execution_method = method;
    lv_anim_set_var(&_native, variable);
    lv_anim_set_custom_exec_cb(&_native, [](lv_anim_t *anim, int32_t value) {
        ESP_UTILS_CHECK_NULL_EXIT(anim, "Invalid animation");

        auto user_data = static_cast<UserData *>(anim->user_data);
        ESP_UTILS_CHECK_NULL_EXIT(user_data, "Invalid user data");

        auto animation = user_data->animation;
        ESP_UTILS_CHECK_NULL_EXIT(animation, "Animation is not set");

        auto method = animation->_execution_method;
        if (method) {
            method(animation->_native.var, value);
        }
    });

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
}

void LvAnimation::setCompletedMethod(CompletedMethod method) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: method(%p)", method);

    _completed_method = method;
    lv_anim_set_completed_cb(&_native, [](lv_anim_t *anim) {
        ESP_UTILS_CHECK_NULL_EXIT(anim, "Invalid animation");

        auto user_data = static_cast<UserData *>(anim->user_data);
        ESP_UTILS_CHECK_NULL_EXIT(user_data, "Invalid user data");

        auto animation = user_data->animation;
        ESP_UTILS_CHECK_NULL_EXIT(animation, "Animation is not set");

        auto method = animation->_completed_method;
        if (method) {
            method(animation->_user_data.user_data);
        }
    });
    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
}

void LvAnimation::setUserData(void *user_data) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: user_data(%p)", user_data);

    _user_data.user_data = user_data;

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
}

bool LvAnimation::start(void) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(!isRunning(), false, "Already started");

    auto result = lv_anim_start(&_native);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return result != NULL;
}

bool LvAnimation::stop(void) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(!isRunning(), false, "Already stopped");

    auto result = lv_anim_delete(_native.var, _native.exec_cb);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return result;
}

bool LvAnimation::isRunning(void) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    auto result = lv_anim_get(_native.var, _native.exec_cb) != NULL;

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return result;
}

} // namespace esp_brookesia::gui
