/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <memory>
#include <array>
#include "lvgl.h"
#include "esp_ui_core_utils.h"
#include "esp_ui_core_app.hpp"
#include "esp_ui_lv.hpp"

class ESP_UI_Core;

// *INDENT-OFF*
class ESP_UI_CoreHome {
public:
    friend class ESP_UI_CoreManager;
    friend class ESP_UI_Core;

    ESP_UI_CoreHome(ESP_UI_Core &core, const ESP_UI_CoreHomeData_t &data);
    ~ESP_UI_CoreHome();

    bool showContainerBorder(void);
    bool hideContainerBorder(void);
    bool calibrateCoreObjectSize(const ESP_UI_StyleSize_t &parent, ESP_UI_StyleSize_t &target) const;
    bool calibrateCoreObjectSize(const ESP_UI_StyleSize_t &parent, ESP_UI_StyleSize_t &target,
                                 bool check_width, bool check_height) const;
    bool calibrateCoreObjectSize(const ESP_UI_StyleSize_t &parent, ESP_UI_StyleSize_t &target, bool allow_zero) const;
    bool calibrateCoreFont(const ESP_UI_StyleSize_t *parent, ESP_UI_StyleFont_t &target) const;
    bool calibrateCoreIconImage(const ESP_UI_StyleImage_t &target) const;

    bool checkCoreInitialized(void) const       { return (_main_screen != nullptr); }
    lv_obj_t *getMainScreen(void) const         { return _main_screen; }
    lv_obj_t *getSystemScreen(void) const       { return _system_screen; }
    lv_obj_t *getMainScreenObject(void) const   { return _main_screen_obj.get(); }
    lv_obj_t *getSystemScreenObject(void) const { return _system_screen_obj.get(); }
    lv_style_t *getCoreContainerStyle(void);
    const lv_font_t *getCoreDefaultFontBySize(uint8_t size) const;
    const lv_font_t *getCoreDefaultFontByHeight(uint8_t height, uint8_t *size_px) const;

protected:
    ESP_UI_Core &_core;
    const ESP_UI_CoreHomeData_t &_core_data;

private:
    virtual bool processAppInstall(ESP_UI_CoreApp *app) = 0;
    virtual bool processAppUninstall(ESP_UI_CoreApp *app) = 0;
    virtual bool processAppRun(ESP_UI_CoreApp *app) = 0;
    virtual bool processAppResume(ESP_UI_CoreApp *app) { return true; }
    virtual bool processAppPause(ESP_UI_CoreApp *app)  { return true; }
    virtual bool processAppClose(ESP_UI_CoreApp *app)  { return true; }
    virtual bool processMainScreenLoad(void);
    virtual bool getAppVisualArea(ESP_UI_CoreApp *app, lv_area_t &app_visual_area) const { return true; }

    bool beginCore(void);
    bool delCore(void);
    bool updateByNewData(void);
    bool calibrateCoreData(ESP_UI_CoreHomeData_t &data);

    const lv_font_t *getCoreUpdateFontBySize(uint8_t size) const;
    const lv_font_t *getCoreUpdateFontByHeight(uint8_t height, uint8_t *size_px) const;

    lv_obj_t *_main_screen;
    lv_obj_t *_system_screen;
    ESP_UI_LvObj_t _main_screen_obj;
    ESP_UI_LvObj_t _system_screen_obj;

    uint8_t _container_style_index;
    std::array<lv_style_t, ESP_UI_CORE_HOME_DATA_CONTAINER_STYLES_NUM> _container_styles;
    std::map<uint8_t, const lv_font_t *> _default_size_font_map;
    std::map<uint8_t, const lv_font_t *> _default_height_font_map;
    std::map<uint8_t, const lv_font_t *> _update_size_font_map;
    std::map<uint8_t, const lv_font_t *> _update_height_font_map;
};
// *INDENT-OFF*
