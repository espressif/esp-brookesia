/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <array>
#include "lvgl.h"
#include "lvgl/esp_brookesia_lv.hpp"
#include "esp_brookesia_base_app.hpp"

namespace esp_brookesia::systems::base {

class Context;

// struct DisplayFonts {
//     uint8_t fonts_num = 0;
//     gui::StyleFont fonts[esp_brookesia::gui::StyleFont::FONT_SIZE_NUM]{};
// };

// struct DisplayDebugStyles {
//     uint8_t outline_width{0};
//     gui::StyleColor outline_color{};
// };

class Display {
public:
    friend class Manager;
    friend class Context;

    static constexpr int DEBUG_STYLES_NUM = 6;

    struct Data {
        struct {
            gui::StyleColor color;
            gui::StyleImage wallpaper_image_resource;
        } background;
        struct {
            uint8_t default_fonts_num;
            gui::StyleFont default_fonts[gui::StyleFont::FONT_SIZE_NUM];
        } text;
        struct {
            struct {
                uint8_t outline_width;
                gui::StyleColor outline_color;
            } styles[DEBUG_STYLES_NUM];
        } container;
        // std::array<DisplayFonts, gui::STYLE_FONT_TYPE_MAX> fonts{};
        // std::array<DisplayDebugStyles, DEBUG_STYLES_NUM> debug_styles{};
    };

    /**
     * @brief Delete copy constructor and assignment operator
     */
    Display(const Display &) = delete;
    Display(Display &&) = delete;
    Display &operator=(const Display &) = delete;
    Display &operator=(Display &&) = delete;

    Display(Context &core, const Data &data);
    ~Display();

    bool showContainerBorder(void);
    bool hideContainerBorder(void);

    bool checkCoreInitialized(void) const
    {
        return (_main_screen != nullptr);
    }
    lv_obj_t *getMainScreen(void) const
    {
        return _main_screen->getNativeHandle();
    }
    lv_obj_t *getSystemScreen(void) const
    {
        return _system_screen->getNativeHandle();
    }
    lv_obj_t *getMainScreenObject(void) const
    {
        return _main_screen_obj->getNativeHandle();
    }
    lv_obj_t *getSystemScreenObject(void) const
    {
        return _system_screen_obj->getNativeHandle();
    }
    const esp_brookesia::gui::LvObject *getMainScreenPtr(void) const
    {
        return _main_screen.get();
    }
    const esp_brookesia::gui::LvObject *getSystemScreenPtr(void) const
    {
        return _system_screen.get();
    }
    const esp_brookesia::gui::LvObject *getMainScreenObjectPtr(void) const
    {
        return _main_screen_obj.get();
    }
    const esp_brookesia::gui::LvObject *getSystemScreenObjectPtr(void) const
    {
        return _system_screen_obj.get();
    }
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
    Context &_system_context;
    const Data &_core_data;

private:
    virtual bool processAppInstall(App *app) = 0;
    virtual bool processAppUninstall(App *app) = 0;
    virtual bool processAppRun(App *app) = 0;
    virtual bool processAppResume(App *app)
    {
        return true;
    }
    virtual bool processAppPause(App *app)
    {
        return true;
    }
    virtual bool processAppClose(App *app)
    {
        return true;
    }
    virtual bool processMainScreenLoad(void);
    virtual bool getAppVisualArea(App *app, lv_area_t &app_visual_area) const
    {
        return true;
    }

    bool begin(void);
    bool del(void);
    bool updateByNewData(void);
    bool calibrateCoreData(Data &data);
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

    uint8_t _container_style_index = 0;
    std::array<lv_style_t, DEBUG_STYLES_NUM> _container_styles;
    std::map<uint8_t, const lv_font_t *> _default_size_font_map;
    std::map<uint8_t, const lv_font_t *> _default_height_font_map;
    std::map<uint8_t, const lv_font_t *> _update_size_font_map;
    std::map<uint8_t, const lv_font_t *> _update_height_font_map;
};

} // namespace esp_brookesia::systems::base

/**
 * Backward compatibility
 */
using ESP_Brookesia_CoreDisplayData_t [[deprecated("Use `esp_brookesia::systems::base::Display::Data` instead")]] =
    esp_brookesia::systems::base::Display::Data;
using ESP_Brookesia_CoreDisplay [[deprecated("Use `esp_brookesia::systems::base::Display` instead")]] =
    esp_brookesia::systems::base::Display;
#define ESP_BROOKESIA_BASE_DISPLAY_DEBUG_STYLES_NUM   esp_brookesia::systems::base::Display::DEBUG_STYLES_NUM
