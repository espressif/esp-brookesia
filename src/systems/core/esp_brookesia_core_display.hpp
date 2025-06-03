/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <array>
#include "lvgl.h"
#include "gui/lvgl/esp_brookesia_lv.hpp"
#include "esp_brookesia_core_app.hpp"

// *INDENT-OFF*

#define ESP_BROOKESIA_CORE_DISPLAY_DEFAULT_FONTS_NUM_MAX  \
    ((ESP_BROOKESIA_STYLE_FONT_SIZE_MAX - ESP_BROOKESIA_STYLE_FONT_SIZE_MIN) / 2 + 1)
#define ESP_BROOKESIA_CORE_DISPLAY_DEBUG_STYLES_NUM   6

struct ESP_Brookesia_CoreDisplayFonts {
    uint8_t fonts_num = 0;
    esp_brookesia::gui::StyleFont fonts[esp_brookesia::gui::StyleFont::FONT_SIZE_NUM]{};
};

struct ESP_Brookesia_CoreDisplayDebugStyles {
    uint8_t outline_width{0};
    esp_brookesia::gui::StyleColor outline_color{};
};

struct ESP_Brookesia_CoreDisplayData {
    struct {
        ESP_Brookesia_StyleColor_t color;
        ESP_Brookesia_StyleImage_t wallpaper_image_resource;
    } background;
    struct {
        uint8_t default_fonts_num;
        esp_brookesia::gui::StyleFont default_fonts[esp_brookesia::gui::StyleFont::FONT_SIZE_NUM];
    } text;
    struct {
        struct {
            uint8_t outline_width;
            ESP_Brookesia_StyleColor_t outline_color;
        } styles[ESP_BROOKESIA_CORE_DISPLAY_DEBUG_STYLES_NUM];
    } container;
    // std::array<ESP_Brookesia_CoreDisplayFonts, esp_brookesia::gui::STYLE_FONT_TYPE_MAX> fonts{};
    // std::array<ESP_Brookesia_CoreDisplayDebugStyles, ESP_BROOKESIA_CORE_DISPLAY_DEBUG_STYLES_NUM> debug_styles{};
};

class ESP_Brookesia_Core;

class ESP_Brookesia_CoreDisplay {
public:
    friend class ESP_Brookesia_CoreManager;
    friend class ESP_Brookesia_Core;

    ESP_Brookesia_CoreDisplay(ESP_Brookesia_Core &core, const ESP_Brookesia_CoreDisplayData &data);
    ~ESP_Brookesia_CoreDisplay();

    bool showContainerBorder(void);
    bool hideContainerBorder(void);

    bool checkCoreInitialized(void) const       { return (_main_screen != nullptr); }
    lv_obj_t *getMainScreen(void) const         { return _main_screen->getNativeHandle(); }
    lv_obj_t *getSystemScreen(void) const       { return _system_screen->getNativeHandle(); }
    lv_obj_t *getMainScreenObject(void) const   { return _main_screen_obj->getNativeHandle(); }
    lv_obj_t *getSystemScreenObject(void) const { return _system_screen_obj->getNativeHandle(); }
    const esp_brookesia::gui::LvObject *getMainScreenPtr(void) const       { return _main_screen.get(); }
    const esp_brookesia::gui::LvObject *getSystemScreenPtr(void) const     { return _system_screen.get(); }
    const esp_brookesia::gui::LvObject *getMainScreenObjectPtr(void) const       { return _main_screen_obj.get(); }
    const esp_brookesia::gui::LvObject *getSystemScreenObjectPtr(void) const     { return _system_screen_obj.get(); }
    lv_style_t *getCoreContainerStyle(void);

    bool calibrateCoreObjectSize(
        const esp_brookesia::gui::StyleSize &parent, esp_brookesia::gui::StyleSize &target
    ) const;
    bool calibrateCoreObjectSize(
        const esp_brookesia::gui::StyleSize &parent, esp_brookesia::gui::StyleSize &target,
        bool check_width, bool check_height
    ) const;
    bool calibrateCoreObjectSize(
        const esp_brookesia::gui::StyleSize &parent, esp_brookesia::gui::StyleSize &target, bool allow_zero
    ) const;
    bool calibrateCoreFont(const esp_brookesia::gui::StyleSize *parent, esp_brookesia::gui::StyleFont &target) const;
    bool calibrateCoreIconImage(const esp_brookesia::gui::StyleImage &target) const;

protected:
    ESP_Brookesia_Core &_core;
    const ESP_Brookesia_CoreDisplayData &_core_data;

private:
    virtual bool processAppInstall(ESP_Brookesia_CoreApp *app) = 0;
    virtual bool processAppUninstall(ESP_Brookesia_CoreApp *app) = 0;
    virtual bool processAppRun(ESP_Brookesia_CoreApp *app) = 0;
    virtual bool processAppResume(ESP_Brookesia_CoreApp *app) { return true; }
    virtual bool processAppPause(ESP_Brookesia_CoreApp *app)  { return true; }
    virtual bool processAppClose(ESP_Brookesia_CoreApp *app)  { return true; }
    virtual bool processMainScreenLoad(void);
    virtual bool getAppVisualArea(ESP_Brookesia_CoreApp *app, lv_area_t &app_visual_area) const { return true; }

    bool beginCore(void);
    bool delCore(void);
    bool updateByNewData(void);
    bool calibrateCoreData(ESP_Brookesia_CoreDisplayData &data);
    void saveLvScreens(void);
    void loadLvScreens(void);

    bool calibrateStyleSizeInternal(esp_brookesia::gui::StyleSize &target) const;
    const lv_font_t *getFontBySize(int size) const;
    const lv_font_t *getFontByHeight(int height, int *size_px) const;

    lv_obj_t *_lv_main_screen = nullptr;
    lv_obj_t *_lv_system_screen = nullptr;

    esp_brookesia::gui::LvScreenUniquePtr _main_screen;
    esp_brookesia::gui::LvScreenUniquePtr _system_screen;
    esp_brookesia::gui::LvContainerUniquePtr _main_screen_obj;
    esp_brookesia::gui::LvContainerUniquePtr _system_screen_obj;

    uint8_t _container_style_index;
    std::array<lv_style_t, ESP_BROOKESIA_CORE_DISPLAY_DEBUG_STYLES_NUM> _container_styles;
    std::map<uint8_t, const lv_font_t *> _default_size_font_map;
    std::map<uint8_t, const lv_font_t *> _default_height_font_map;
    std::map<uint8_t, const lv_font_t *> _update_size_font_map;
    std::map<uint8_t, const lv_font_t *> _update_height_font_map;
};

/**
 * Backward compatibility
 */
#define ESP_BROOKESIA_CORE_HOME_DATA_DEFAULT_FONTS_NUM_MAX  ESP_BROOKESIA_CORE_DISPLAY_DEFAULT_FONTS_NUM_MAX
#define ESP_BROOKESIA_CORE_HOME_DATA_CONTAINER_STYLES_NUM   ESP_BROOKESIA_CORE_DISPLAY_DEBUG_STYLES_NUM
typedef ESP_Brookesia_CoreDisplay ESP_Brookesia_CoreHome;
typedef ESP_Brookesia_CoreDisplayData ESP_Brookesia_CoreHomeData_t;

// *INDENT-ON*
