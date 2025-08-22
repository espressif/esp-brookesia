/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include "systems/base/esp_brookesia_base_context.hpp"
#include "lvgl/esp_brookesia_lv_helper.hpp"
#include "esp_brookesia_navigation_bar.hpp"


namespace esp_brookesia::systems::phone {

class NavigationBar {
public:
    static constexpr uint8_t BUTTON_NUM = static_cast<uint8_t>(base::Manager::NavigateType::MAX);

    struct Data {
        struct {
            gui::StyleSize size;
            gui::StyleSize size_min;
            gui::StyleSize size_max;
            gui::StyleColor background_color;
        } main;
        struct {
            gui::StyleSize icon_size;
            gui::StyleImage icon_images[BUTTON_NUM];
            base::Manager::NavigateType navigate_types[BUTTON_NUM];
            gui::StyleColor active_background_color;
        } button;
        struct {
            gui::StyleAnimation show_animation;
            gui::StyleAnimation hide_animation;
            int hide_timer_period_ms;
        } visual_flex;
        struct {
            uint8_t enable_main_size_min: 1;
            uint8_t enable_main_size_max: 1;
        } flags;
    };

    enum class VisualMode : uint8_t {
        HIDE,
        SHOW_FIXED,
        SHOW_FLEX,
        MAX,
    };

    NavigationBar(base::Context &core, const Data &data);
    ~NavigationBar();

    bool begin(lv_obj_t *parent);
    bool del(void);
    bool setVisualMode(VisualMode mode);
    bool triggerVisualFlexShow(void);
    bool show(void);
    bool hide(void);

    bool checkInitialized(void) const
    {
        return (_main_obj != nullptr);
    }
    bool checkVisible(void) const;
    bool checkVisualFlexShowAnimRunning(void) const
    {
        return _flags.is_visual_flex_show_anim_running;
    }
    bool checkVisualFlexHideAnimRunning(void) const
    {
        return _flags.is_visual_flex_hide_anim_running;
    }
    bool checkVisualFlexHideTimerRunning(void) const
    {
        return _flags.is_visual_flex_hide_timer_running;
    }
    const Data &getData(void)
    {
        return _data;
    }
    int getCurrentOffset(void) const;

    static bool calibrateData(const gui::StyleSize &screen_size, const base::Display &display,
                              Data &data);

private:
    bool updateByNewData(void);
    bool startFlexShowAnimation(bool enable_auto_hide);
    bool stopFlexShowAnimation(void);
    bool startFlexHideAnimation(void);
    bool stopFlexHideAnimation(void);
    bool startFlexHideTimer(void);
    bool stopFlexHideTimer(void);
    bool resetFlexHideTimer(void);

    static void onDataUpdateEventCallback(lv_event_t *event);
    static void onIconTouchEventCallback(lv_event_t *event);
    static void onVisualFlexAnimationExecuteCallback(void *var, int32_t value);
    static void onVisualFlexShowAnimationReadyCallback(lv_anim_t *anim);
    static void onVisualFlexHideAnimationReadyCallback(lv_anim_t *anim);
    static void onVisualFlexHideTimerCallback(lv_timer_t *timer);

    base::Context &_system_context;
    const Data &_data;

    struct {
        uint8_t is_icon_pressed_losted: 1;
        uint8_t is_visual_flex_show_anim_running: 1;
        uint8_t is_visual_flex_hide_anim_running: 1;
        uint8_t is_visual_flex_hide_timer_running: 1;
        uint8_t enable_visual_flex_auto_hide: 1;
    } _flags;
    ESP_Brookesia_LvAnim_t _visual_flex_show_anim;
    ESP_Brookesia_LvAnim_t _visual_flex_hide_anim;
    ESP_Brookesia_LvTimer_t _visual_flex_hide_timer;
    VisualMode _visual_mode;
    ESP_Brookesia_LvObj_t _main_obj;
    std::vector<ESP_Brookesia_LvObj_t> _button_objs;
    std::vector<ESP_Brookesia_LvObj_t> _icon_main_objs;
    std::vector<ESP_Brookesia_LvObj_t> _icon_image_objs;
};

} // namespace esp_brookesia::systems::phone

using ESP_Brookesia_NavigationBarData_t [[deprecated("Use `esp_brookesia::systems::phone::NavigationBar::Data` instead")]] =
    esp_brookesia::systems::phone::NavigationBar::Data;
using ESP_Brookesia_NavigationBarVisualMode_t [[deprecated("Use `esp_brookesia::systems::phone::NavigationBar::VisualMode` instead")]] =
    esp_brookesia::systems::phone::NavigationBar::VisualMode;
#define ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_HIDE       ESP_Brookesia_NavigationBarVisualMode_t::HIDE
#define ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_SHOW_FIXED ESP_Brookesia_NavigationBarVisualMode_t::SHOW_FIXED
#define ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_SHOW_FLEX  ESP_Brookesia_NavigationBarVisualMode_t::SHOW_FLEX
#define ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_MAX        ESP_Brookesia_NavigationBarVisualMode_t::MAX
using ESP_Brookesia_NavigationBar [[deprecated("Use `esp_brookesia::systems::phone::NavigationBar` instead")]] =
    esp_brookesia::systems::phone::NavigationBar;
