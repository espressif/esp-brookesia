/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_PHONE_NAVIGATION_BAR_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "phone/private/esp_brookesia_phone_utils.hpp"
#include "esp_brookesia_navigation_bar.hpp"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::phone {

#define VISUAL_FLEX_SHOW_ANIM_PERIOD_MS     200
#define VISUAL_FLEX_SHOW_DURATION_MS        2000
#define VISUAL_FLEX_HIDE_ANIM_PERIOD_MS     200

NavigationBar::NavigationBar(base::Context &core, const NavigationBar::Data &data):
    _system_context(core),
    _data(data),
    _flags{},
    _visual_flex_show_anim(nullptr),
    _visual_flex_hide_anim(nullptr),
    _visual_flex_hide_timer(nullptr),
    _visual_mode(NavigationBar::VisualMode::SHOW_FIXED),
    _main_obj(nullptr)
{
}

NavigationBar::~NavigationBar()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);
    if (!del()) {
        ESP_UTILS_LOGE("Delete failed");
    }
}

bool NavigationBar::begin(lv_obj_t *parent)
{
    ESP_Brookesia_LvObj_t main_obj = nullptr;
    ESP_Brookesia_LvObj_t button_obj = nullptr;
    ESP_Brookesia_LvObj_t icon_main_obj = nullptr;
    ESP_Brookesia_LvObj_t icon_image_obj = nullptr;
    ESP_Brookesia_LvAnim_t visual_flex_show_anim = nullptr;
    ESP_Brookesia_LvAnim_t visual_flex_hide_anim = nullptr;
    ESP_Brookesia_LvTimer_t visual_flex_hide_timer = nullptr;
    vector<ESP_Brookesia_LvObj_t> button_objs;
    vector<ESP_Brookesia_LvObj_t> icon_main_objs;
    vector<ESP_Brookesia_LvObj_t> icon_image_objs;

    ESP_UTILS_LOGD("Begin(0x%p)", this);
    ESP_UTILS_CHECK_NULL_RETURN(parent, false, "Invalid parent");
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    /* Create objects */
    // Main
    main_obj = ESP_BROOKESIA_LV_OBJ(obj, parent);
    ESP_UTILS_CHECK_NULL_RETURN(main_obj, false, "Create main object failed");
    // Button
    for (int i = 0; i < BUTTON_NUM; i++) {
        button_obj = ESP_BROOKESIA_LV_OBJ(obj, main_obj.get());
        ESP_UTILS_CHECK_NULL_RETURN(button_obj, false, "Create button failed");
        button_objs.push_back(button_obj);

        icon_main_obj = ESP_BROOKESIA_LV_OBJ(obj, button_obj.get());
        ESP_UTILS_CHECK_NULL_RETURN(icon_main_obj, false, "Create icon main failed");
        icon_main_objs.push_back(icon_main_obj);

        icon_image_obj = ESP_BROOKESIA_LV_OBJ(img, icon_main_obj.get());
        ESP_UTILS_CHECK_NULL_RETURN(icon_image_obj, false, "Create icon image failed");
        icon_image_objs.push_back(icon_image_obj);
    }
    ESP_UTILS_CHECK_FALSE_RETURN(_system_context.registerDateUpdateEventCallback(onDataUpdateEventCallback, this), false,
                                 "Register data update event callback failed");
    // Flex hide Timer
    visual_flex_show_anim = ESP_BROOKESIA_LV_ANIM();
    ESP_UTILS_CHECK_NULL_RETURN(visual_flex_show_anim, false, "Create flex show anim failed");
    visual_flex_hide_anim = ESP_BROOKESIA_LV_ANIM();
    ESP_UTILS_CHECK_NULL_RETURN(visual_flex_hide_anim, false, "Create flex hide anim failed");
    visual_flex_hide_timer = ESP_BROOKESIA_LV_TIMER(onVisualFlexHideTimerCallback, 3000, this);
    ESP_UTILS_CHECK_NULL_RETURN(visual_flex_hide_timer, false, "Create flex hide timer failed");

    /* Setup objects style */
    // Main
    lv_obj_add_style(main_obj.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
    lv_obj_align(main_obj.get(), LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(main_obj.get(), LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_obj.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(main_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Button
    for (int i = 0; i < BUTTON_NUM; i++) {
        lv_obj_add_style(button_objs[i].get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
        lv_obj_set_style_bg_opa(button_objs[i].get(), LV_OPA_TRANSP, 0);
        lv_obj_add_flag(button_objs[i].get(), LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(button_objs[i].get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_add_event_cb(button_objs[i].get(), onIconTouchEventCallback, LV_EVENT_PRESSED, this);
        lv_obj_add_event_cb(button_objs[i].get(), onIconTouchEventCallback, LV_EVENT_PRESSING, this);
        lv_obj_add_event_cb(button_objs[i].get(), onIconTouchEventCallback, LV_EVENT_PRESS_LOST, this);
        lv_obj_add_event_cb(button_objs[i].get(), onIconTouchEventCallback, LV_EVENT_RELEASED, this);
        lv_obj_add_event_cb(button_objs[i].get(), onIconTouchEventCallback, LV_EVENT_CLICKED, this);
        // Icon object
        lv_obj_add_style(icon_main_objs[i].get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
        lv_obj_align(icon_main_objs[i].get(), LV_ALIGN_CENTER, 0, 0);
        lv_obj_clear_flag(icon_main_objs[i].get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        // Icon image
        lv_obj_add_style(icon_image_objs[i].get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
        lv_obj_align(icon_image_objs[i].get(), LV_ALIGN_CENTER, 0, 0);
        // lv_obj_set_size(icon_image_objs[i].get(), LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_image_set_inner_align(icon_image_objs[i].get(), LV_IMAGE_ALIGN_CENTER);
        lv_obj_clear_flag(icon_image_objs[i].get(), LV_OBJ_FLAG_CLICKABLE);
    }
    /* Visual flex */
    // Show animation
    lv_anim_init(visual_flex_show_anim.get());
    lv_anim_set_var(visual_flex_show_anim.get(), this);
    lv_anim_set_early_apply(visual_flex_show_anim.get(), false);
    lv_anim_set_exec_cb(visual_flex_show_anim.get(), onVisualFlexAnimationExecuteCallback);
    lv_anim_set_ready_cb(visual_flex_show_anim.get(), onVisualFlexShowAnimationReadyCallback);
    // Hide animation
    lv_anim_init(visual_flex_hide_anim.get());
    lv_anim_set_var(visual_flex_hide_anim.get(), this);
    lv_anim_set_early_apply(visual_flex_hide_anim.get(), false);
    lv_anim_set_exec_cb(visual_flex_hide_anim.get(), onVisualFlexAnimationExecuteCallback);
    lv_anim_set_ready_cb(visual_flex_hide_anim.get(), onVisualFlexHideAnimationReadyCallback);
    // Hide timer
    lv_timer_pause(visual_flex_hide_timer.get());

    /* Save objects */
    _main_obj = main_obj;
    _button_objs = button_objs;
    _icon_main_objs = icon_main_objs;
    _icon_image_objs = icon_image_objs;
    _visual_flex_hide_timer = visual_flex_hide_timer;
    _visual_flex_show_anim = visual_flex_show_anim;
    _visual_flex_hide_anim = visual_flex_hide_anim;

    /* Update */
    ESP_UTILS_CHECK_FALSE_GOTO(updateByNewData(), err, "Update by new data failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool NavigationBar::del(void)
{
    bool ret = true;

    ESP_UTILS_LOGD("Delete(0x%p)", this);

    if (!checkInitialized()) {
        return true;
    }

    if (_system_context.checkCoreInitialized() && !_system_context.unregisterDateUpdateEventCallback(onDataUpdateEventCallback, this)) {
        ESP_UTILS_LOGE("Unregister data update event callback failed");
        ret = false;
    }

    _main_obj.reset();
    _button_objs.clear();
    _icon_main_objs.clear();
    _icon_image_objs.clear();
    _visual_flex_show_anim.reset();
    _visual_flex_hide_anim.reset();
    _visual_flex_hide_timer.reset();

    return ret;
}

bool NavigationBar::setVisualMode(NavigationBar::VisualMode mode)
{
    /* Used for test */
    // bool is_cur_hide = false;
    // bool is_cur_show_fixed = false;
    // bool is_cur_show_flex = false;
    // bool is_target_hide = false;
    // bool is_target_show_fixed = false;
    // bool is_target_show_flex = false;

    ESP_UTILS_LOGD("Set Visual Mode(%d)", mode);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    /* Used for test */
    // is_cur_hide = (_visual_mode == NavigationBar::VisualMode::HIDE);
    // is_cur_show_fixed = (_visual_mode == NavigationBar::VisualMode::SHOW_FIXED);
    // is_cur_show_flex = (_visual_mode == NavigationBar::VisualMode::SHOW_FLEX);
    // is_target_hide = (mode == NavigationBar::VisualMode::HIDE);
    // is_target_show_fixed = (mode == NavigationBar::VisualMode::SHOW_FIXED);
    // is_target_show_flex = (mode == NavigationBar::VisualMode::SHOW_FLEX);
    // ESP_UTILS_LOGD("Current: Hide(%d) Show Fixed(%d) Show Flex(%d)", is_cur_hide, is_cur_show_fixed, is_cur_show_flex);
    // ESP_UTILS_LOGD("Target: Hide(%d) Show Fixed(%d) Show Flex(%d)", is_target_hide, is_target_show_fixed,
    // is_target_show_flex);

    if (mode == NavigationBar::VisualMode::SHOW_FIXED) {
        ESP_UTILS_LOGD("Force to show");
        ESP_UTILS_CHECK_FALSE_RETURN(stopFlexHideTimer(), false, "Stop flex hide timer failed");
        ESP_UTILS_CHECK_FALSE_RETURN(stopFlexHideAnimation(), false, "Stop flex hide animation failed");
        ESP_UTILS_CHECK_FALSE_RETURN(stopFlexShowAnimation(), false, "Stop flex show animation failed");
        ESP_UTILS_CHECK_FALSE_RETURN(show(), false, "Show failed");
    } else if (mode == NavigationBar::VisualMode::HIDE) {
        ESP_UTILS_LOGD("Force to hide");
        ESP_UTILS_CHECK_FALSE_RETURN(stopFlexHideTimer(), false, "Stop flex hide timer failed");
        ESP_UTILS_CHECK_FALSE_RETURN(stopFlexHideAnimation(), false, "Stop flex hide animation failed");
        ESP_UTILS_CHECK_FALSE_RETURN(stopFlexShowAnimation(), false, "Stop flex show animation failed");
        ESP_UTILS_CHECK_FALSE_RETURN(hide(), false, "Hide failed");
    } else if (_visual_mode != NavigationBar::VisualMode::HIDE) {
        ESP_UTILS_LOGD("Force to start hide animation");
        // In this case, we should force to end the show animation
        ESP_UTILS_CHECK_FALSE_RETURN(stopFlexHideTimer(), false, "Stop flex hide timer failed");
        ESP_UTILS_CHECK_FALSE_RETURN(stopFlexShowAnimation(), false, "Stop flex show animation failed");
        ESP_UTILS_CHECK_FALSE_RETURN(startFlexHideAnimation(), false, "Start flex hide animation failed");
    }

    _visual_mode = mode;

    return true;
}

bool NavigationBar::triggerVisualFlexShow(void)
{
    ESP_UTILS_LOGD("Trigger visual flex show animation");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(_visual_mode == NavigationBar::VisualMode::SHOW_FLEX, false,
                                 "Invalid visual mode");

    if (checkVisualFlexHideTimerRunning()) {
        ESP_UTILS_CHECK_FALSE_RETURN(resetFlexHideTimer(), false, "Reset flex hide timer failed");
    } else {
        ESP_UTILS_CHECK_FALSE_RETURN(stopFlexHideAnimation(), false, "Stop flex hide animation failed");
        ESP_UTILS_CHECK_FALSE_RETURN(startFlexShowAnimation(true), false, "Start flex show animation failed");
    }

    return true;
}

bool NavigationBar::show(void)
{
    ESP_UTILS_LOGD("Show");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    lv_obj_clear_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(_main_obj.get(), LV_ALIGN_BOTTOM_MID, 0, 0);

    return true;
}

bool NavigationBar::hide(void)
{
    ESP_UTILS_LOGD("Hide");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    lv_obj_add_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(_main_obj.get(), LV_ALIGN_BOTTOM_MID, 0, _data.main.size.height);

    return true;
}

bool NavigationBar::checkVisible(void) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    return !lv_obj_has_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
}

int NavigationBar::getCurrentOffset(void) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), 0, "Not initialized");

    lv_obj_update_layout(_main_obj.get());
    lv_obj_refr_pos(_main_obj.get());

    return lv_obj_get_y_aligned(_main_obj.get());
}

bool NavigationBar::calibrateData(const gui::StyleSize &screen_size, const base::Display &display,
                                  NavigationBar::Data &data)
{
    base::Manager::NavigateType navigate_type = base::Manager::NavigateType::MAX;
    const gui::StyleSize *parent_size = nullptr;

    ESP_UTILS_LOGD("Calibrate data");

    // Calibrate the min and max size
    if (data.flags.enable_main_size_min) {
        ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(screen_size, data.main.size_min), false,
                                     "Calibrate data main size min failed");
    }
    if (data.flags.enable_main_size_max) {
        ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(screen_size, data.main.size_max), false,
                                     "Calibrate data main size max failed");
    }

    /* Main */
    parent_size = &screen_size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, data.main.size), false,
                                 "Invalid main size");
    // Adjust the size according to the min and max size
    if (data.flags.enable_main_size_min) {
        data.main.size.width = max(data.main.size.width, data.main.size_min.width);
        data.main.size.height = max(data.main.size.height, data.main.size_min.height);
    }
    if (data.flags.enable_main_size_max) {
        data.main.size.width = min(data.main.size.width, data.main.size_max.width);
        data.main.size.height = min(data.main.size.height, data.main.size_max.height);
    }
    // Button
    parent_size = &data.main.size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, data.button.icon_size), false,
                                 "Invalid button icon size");
    for (int i = 0; i < BUTTON_NUM; i++) {
        navigate_type = data.button.navigate_types[i];
        ESP_UTILS_CHECK_VALUE_RETURN(
            static_cast<int>(navigate_type), 0, static_cast<int>(base::Manager::NavigateType::MAX) - 1, false,
            "Invalid button navigate type"
        );
        ESP_UTILS_CHECK_NULL_RETURN(data.button.icon_images[i].resource, false, "Invalid button icon image resources");
    }
    // Visual flex
    if (data.visual_flex.show_animation.duration_ms == 0) {
        data.visual_flex.show_animation.duration_ms = VISUAL_FLEX_SHOW_ANIM_PERIOD_MS;
    }
    if (data.visual_flex.hide_animation.duration_ms == 0) {
        data.visual_flex.hide_animation.duration_ms = VISUAL_FLEX_HIDE_ANIM_PERIOD_MS; // TODO:
    }
    ESP_UTILS_CHECK_FALSE_RETURN(data.visual_flex.show_animation.path_type < gui::StyleAnimation::ANIM_PATH_TYPE_MAX, false,
                                 "Invalid visual flex show animation path type");
    ESP_UTILS_CHECK_FALSE_RETURN(data.visual_flex.hide_animation.path_type < gui::StyleAnimation::ANIM_PATH_TYPE_MAX, false,
                                 "Invalid visual flex hide animation path type");

    return true;
}

bool NavigationBar::updateByNewData(void)
{
    float h_factor = 0;
    float w_factor = 0;
    lv_img_dsc_t *icon_image_resource = nullptr;

    ESP_UTILS_LOGD("Update(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Main
    lv_obj_set_size(_main_obj.get(), _data.main.size.width, _data.main.size.height);
    lv_obj_set_style_bg_color(_main_obj.get(), lv_color_hex(_data.main.background_color.color), 0);
    lv_obj_set_style_bg_opa(_main_obj.get(), _data.main.background_color.opacity, 0);

    for (int i = 0; i < BUTTON_NUM; i++) {
        // Button
        lv_obj_set_size(_button_objs[i].get(), _data.main.size.width / BUTTON_NUM,
                        _data.main.size.height);
        lv_obj_set_style_bg_color(_button_objs[i].get(), lv_color_hex(_data.button.active_background_color.color),
                                  LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(_button_objs[i].get(), _data.button.active_background_color.opacity, LV_STATE_PRESSED);
        // Icon main
        lv_obj_set_size(_icon_main_objs[i].get(), _data.button.icon_size.width, _data.button.icon_size.height);
        // Icon image
        icon_image_resource = (lv_img_dsc_t *)_data.button.icon_images[i].resource;
        lv_img_set_src(_icon_image_objs[i].get(), icon_image_resource);
        lv_obj_set_style_img_recolor(_icon_image_objs[i].get(),
                                     lv_color_hex(_data.button.icon_images[i].recolor.color), 0);
        lv_obj_set_style_img_recolor_opa(_icon_image_objs[i].get(),
                                         _data.button.icon_images[i].recolor.opacity, 0);
        // Calculate the multiple of the size between the target and the image.
        h_factor = (float)(_data.button.icon_size.height) / icon_image_resource->header.h;
        w_factor = (float)(_data.button.icon_size.width) / icon_image_resource->header.w;
        // Scale the image to a suitable size.
        // So you don’t have to consider the size of the source image.
        if (h_factor < w_factor) {
            lv_image_set_scale(_icon_image_objs[i].get(), (int)(h_factor * LV_SCALE_NONE));
        } else {
            lv_image_set_scale(_icon_image_objs[i].get(), (int)(w_factor * LV_SCALE_NONE));
        }
        lv_obj_set_size(_icon_image_objs[i].get(), _data.button.icon_size.width, _data.button.icon_size.height);
        lv_obj_refr_size(_icon_image_objs[i].get());
    }

    /* Visual flex */
    // Show animation
    lv_anim_set_values(_visual_flex_show_anim.get(), _data.main.size.height, 0);
    lv_anim_set_time(_visual_flex_show_anim.get(), _data.visual_flex.show_animation.duration_ms);
    lv_anim_set_delay(_visual_flex_show_anim.get(), _data.visual_flex.show_animation.delay_ms);
    lv_anim_set_path_cb(_visual_flex_show_anim.get(), gui::getLvAnimPathCb(_data.visual_flex.show_animation.path_type));
    // Hide animation
    lv_anim_set_values(_visual_flex_hide_anim.get(), 0, _data.main.size.height);
    lv_anim_set_time(_visual_flex_hide_anim.get(), _data.visual_flex.hide_animation.duration_ms);
    lv_anim_set_delay(_visual_flex_show_anim.get(), _data.visual_flex.hide_animation.delay_ms);
    lv_anim_set_path_cb(_visual_flex_hide_anim.get(), gui::getLvAnimPathCb(_data.visual_flex.hide_animation.path_type));
    // Hide timer
    lv_timer_set_period(_visual_flex_hide_timer.get(), _data.visual_flex.hide_timer_period_ms);

    return true;
}

bool NavigationBar::startFlexShowAnimation(bool enable_auto_hide)
{
    ESP_UTILS_LOGD("Start flex show animation");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (_flags.is_visual_flex_show_anim_running || (getCurrentOffset() == 0)) {
        ESP_UTILS_LOGD("Skip");
        return true;
    }

    _flags.enable_visual_flex_auto_hide = enable_auto_hide;
    lv_obj_clear_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_main_obj.get());
    lv_anim_set_values(_visual_flex_show_anim.get(), getCurrentOffset(), 0);
    ESP_UTILS_CHECK_NULL_RETURN(lv_anim_start(_visual_flex_show_anim.get()), false, "Start animation failed");
    _flags.is_visual_flex_show_anim_running = true;

    return true;
}

bool NavigationBar::stopFlexShowAnimation(void)
{
    ESP_UTILS_LOGD("Stop flex show animation");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!_flags.is_visual_flex_show_anim_running) {
        ESP_UTILS_LOGD("Skip");
        return true;
    }

    ESP_UTILS_CHECK_FALSE_RETURN(lv_anim_del(_visual_flex_show_anim->var, nullptr), false, "Delete animation failed");
    _flags.is_visual_flex_show_anim_running = false;

    return true;
}

bool NavigationBar::startFlexHideAnimation(void)
{
    ESP_UTILS_LOGD("Start flex hide animation");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (_flags.is_visual_flex_hide_anim_running || (getCurrentOffset() == _data.main.size.height)) {
        ESP_UTILS_LOGD("Skip");
        return true;
    }

    lv_anim_set_values(_visual_flex_hide_anim.get(), getCurrentOffset(), _data.main.size.height);
    ESP_UTILS_CHECK_NULL_RETURN(lv_anim_start(_visual_flex_hide_anim.get()), false, "Start animation failed");
    _flags.is_visual_flex_hide_anim_running = true;

    return true;
}

bool NavigationBar::stopFlexHideAnimation(void)
{
    ESP_UTILS_LOGD("Stop flex hide animation");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!_flags.is_visual_flex_hide_anim_running) {
        ESP_UTILS_LOGD("Skip");
        return true;
    }

    ESP_UTILS_CHECK_FALSE_RETURN(lv_anim_del(_visual_flex_hide_anim->var, nullptr), false, "Delete animation failed");
    _flags.is_visual_flex_hide_anim_running = false;

    return true;
}

bool NavigationBar::startFlexHideTimer(void)
{
    ESP_UTILS_LOGD("Start flex hide timer");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (_flags.is_visual_flex_hide_timer_running || (getCurrentOffset() == _data.main.size.height)) {
        ESP_UTILS_LOGD("Skip");
        return true;
    }

    lv_timer_reset(_visual_flex_hide_timer.get());
    lv_timer_resume(_visual_flex_hide_timer.get());
    _flags.is_visual_flex_hide_timer_running = true;

    return true;
}

bool NavigationBar::stopFlexHideTimer(void)
{
    ESP_UTILS_LOGD("Stop flex hide timer");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!_flags.is_visual_flex_hide_timer_running) {
        ESP_UTILS_LOGD("Skip");
        return true;
    }

    lv_timer_pause(_visual_flex_hide_timer.get());
    lv_timer_reset(_visual_flex_hide_timer.get());
    _flags.is_visual_flex_hide_timer_running = false;

    return true;
}

bool NavigationBar::resetFlexHideTimer(void)
{
    ESP_UTILS_LOGD("Reset flex hide timer");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!_flags.is_visual_flex_hide_timer_running) {
        ESP_UTILS_LOGD("Skip");
        return true;
    }

    lv_timer_reset(_visual_flex_hide_timer.get());

    return true;
}

void NavigationBar::onDataUpdateEventCallback(lv_event_t *event)
{
    NavigationBar *navigation_bar = nullptr;

    ESP_UTILS_LOGD("Data update event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event object");

    navigation_bar = (NavigationBar *)lv_event_get_user_data(event);
    ESP_UTILS_CHECK_NULL_EXIT(navigation_bar, "Invalid navigation bar object");

    ESP_UTILS_CHECK_FALSE_EXIT(navigation_bar->updateByNewData(), "Update failed");
}

void NavigationBar::onIconTouchEventCallback(lv_event_t *event)
{
    lv_obj_t *button_obj = nullptr;
    lv_event_code_t event_code = _LV_EVENT_LAST;
    NavigationBar *navigation_bar = nullptr;
    base::Manager::NavigateType navigate_type = base::Manager::NavigateType::MAX;

    ESP_UTILS_LOGD("Icon touch event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event object");

    event_code = lv_event_get_code(event);
    button_obj = (lv_obj_t *)lv_event_get_current_target(event);
    navigation_bar = (NavigationBar *)lv_event_get_user_data(event);
    ESP_UTILS_CHECK_FALSE_EXIT(event_code < _LV_EVENT_LAST, "Invalid event code");
    ESP_UTILS_CHECK_NULL_EXIT(button_obj, "Invalid button object");
    ESP_UTILS_CHECK_NULL_EXIT(navigation_bar, "Invalid navigation bar");

    switch (event_code) {
    case LV_EVENT_CLICKED:
        ESP_UTILS_LOGD("Clicked");
        if (navigation_bar->_flags.is_icon_pressed_losted) {
            break;
        }
        for (size_t i = 0; i < navigation_bar->_button_objs.size(); i++) {
            if (button_obj == navigation_bar->_button_objs[i].get()) {
                navigate_type = navigation_bar->_data.button.navigate_types[i];
                break;
            }
        }
        ESP_UTILS_CHECK_VALUE_EXIT(
            static_cast<int>(navigate_type), 0, static_cast<int>(base::Manager::NavigateType::MAX) - 1,
            "Invalid navigate type"
        );
        ESP_UTILS_CHECK_FALSE_EXIT(navigation_bar->_system_context.sendNavigateEvent(navigate_type), "Send navigate event failed");
        break;
    case LV_EVENT_PRESSED:
        ESP_UTILS_LOGD("Pressed");
        navigation_bar->_flags.is_icon_pressed_losted = false;
        lv_obj_set_style_bg_opa(button_obj, navigation_bar->_data.button.active_background_color.opacity, 0);
        break;
    case LV_EVENT_PRESS_LOST:
        ESP_UTILS_LOGD("Press lost");
        navigation_bar->_flags.is_icon_pressed_losted = true;
        [[fallthrough]];
    case LV_EVENT_RELEASED:
        ESP_UTILS_LOGD("Release");
        lv_obj_set_style_bg_opa(button_obj, LV_OPA_TRANSP, 0);
        break;
    case LV_EVENT_PRESSING:
        if (navigation_bar->_visual_mode == NavigationBar::VisualMode::SHOW_FLEX) {
            ESP_UTILS_CHECK_FALSE_EXIT(navigation_bar->resetFlexHideTimer(), "Reset flex hide timer failed");
        }
        break;
    default:
        ESP_UTILS_CHECK_FALSE_EXIT(false, "Invalid event code(%d)", event_code);
        break;
    }
}

void NavigationBar::onVisualFlexAnimationExecuteCallback(void *var, int32_t value)
{
    NavigationBar *navigation_bar = static_cast<NavigationBar *>(var);
    ESP_UTILS_CHECK_NULL_EXIT(navigation_bar, "Invalid var");

    lv_obj_align(navigation_bar->_main_obj.get(), LV_ALIGN_BOTTOM_MID, 0, value);
}

void NavigationBar::onVisualFlexShowAnimationReadyCallback(lv_anim_t *anim)
{
    NavigationBar *navigation_bar = static_cast<NavigationBar *>(anim->var);
    ESP_UTILS_CHECK_NULL_EXIT(navigation_bar, "Invalid var");

    ESP_UTILS_LOGD("Flex show animation ready");
    if (navigation_bar->_flags.enable_visual_flex_auto_hide) {
        ESP_UTILS_CHECK_FALSE_EXIT(navigation_bar->startFlexHideTimer(), "Navigation bar start flex hide timer failed");
    }
    navigation_bar->_flags.is_visual_flex_show_anim_running = false;
}

void NavigationBar::onVisualFlexHideAnimationReadyCallback(lv_anim_t *anim)
{
    NavigationBar *navigation_bar = static_cast<NavigationBar *>(anim->var);
    ESP_UTILS_CHECK_NULL_EXIT(navigation_bar, "Invalid var");

    ESP_UTILS_LOGD("Flex hide animation ready");
    navigation_bar->_flags.is_visual_flex_hide_anim_running = false;
    lv_obj_add_flag(navigation_bar->_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
}

void NavigationBar::onVisualFlexHideTimerCallback(lv_timer_t *timer)
{
    NavigationBar *navigation_bar = static_cast<NavigationBar *>(timer->user_data);

    ESP_UTILS_LOGD("Flex hide timer callback");
    ESP_UTILS_CHECK_NULL_EXIT(navigation_bar, "Invalid var");

    ESP_UTILS_CHECK_FALSE_EXIT(navigation_bar->startFlexHideAnimation(), "Navigation bar start flex hide animation failed");

    lv_timer_pause(timer);
    navigation_bar->_flags.is_visual_flex_hide_timer_running = false;
}

} // namespace esp_brookesia::systems::phone
