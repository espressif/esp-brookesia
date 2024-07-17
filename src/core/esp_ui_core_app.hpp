/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <list>
#include <string>
#include "lvgl.h"
#include "esp_ui_core_type.h"

class ESP_UI_Core;

// *INDENT-OFF*
/**
 * @brief The core app class. This serves as the base class for all internal app classes. User-defined app classes
 *        should not inherit from this class directly.
 *
 */
class ESP_UI_CoreApp {
public:
    friend class ESP_UI_CoreManager;

    /**
     * @brief Construct a core app with detailed configuration.
     *
     * @param data The configuration data for the core app.
     *
     */
    ESP_UI_CoreApp(const ESP_UI_CoreAppData_t &data);

    /**
     * @brief Construct a core app with basic configuration
     *
     * @param name The name of the app.
     * @param launcher_icon The image of the launcher icon.
     * @param use_default_screen Flag to enable the default screen. If true, the core will create a default screen, and
     *                           the app should create all UI resources on it using `lv_scr_act()`. The screen will be
     *                           automatically cleaned up
     *
     */
    ESP_UI_CoreApp(const char *name, const void *launcher_icon, bool use_default_screen);

    /**
     * @brief Destructor for the core app, should be defined by the user's app class.
     */
    virtual ~ESP_UI_CoreApp() = default;

    /**
     * @brief  Check if the app is initialized
     *
     * @return true if the app is initialized, otherwise false
     *
     */
    bool checkInitialized(void) const
    {
        return (_core != nullptr);
    }

    /**
     * @brief Get the id. The id is assigned by the core when installed and is unique for each app.
     *
     * @return id: the unique ID of the app
     *
     */
    int getId(void) const
    {
        return _id;
    }

    /**
     * @brief Get the app name
     *
     * @return name: the string pointer of the app name
     *
     */
    const char *getName(void) const
    {
        return _core_active_data.name;
    }

    /**
     * @brief Get the launcher icon
     *
     * @return image: the icon image of the app
     *
     */
    const ESP_UI_StyleImage_t &getLauncherIcon(void) const
    {
        return _core_active_data.launcher_icon;
    }

    /**
     * @brief Get the visual area
     *
     * @return area: the visual area of the app
     *
     */
    const lv_area_t & getVisualArea(void) const
    {
        return _app_style.visual_area;
    }

    /**
     * @brief Get the initial core data which is set during initialization
     *
     * @return data: the core data of the app
     *
     */
    const ESP_UI_CoreAppData_t &getCoreInitData(void) const
    {
        return _core_init_data;
    }

    /**
     * @brief Get the active core data which is calibrated during runtime
     *
     * @return data: the core data of the app
     *
     */
    const ESP_UI_CoreAppData_t &getCoreActiveData(void) const
    {
        return _core_active_data;
    }

protected:
    /**
     * @brief Called when the app starts running. This is the entry point for the app, where all UI resources should be
     *        created.
     *
     * @note If the `enable_default_screen` flag in `ESP_UI_CoreAppData_t` is set, the core will create a default
     *       screen, and the app should create all UI resources on it using `lv_scr_act()` in this function. Otherwise,
     *       the app needs to create a new screen and load it manually.
     * @note If the `enable_recycle_resource` flag in `ESP_UI_CoreAppData_t` is set, the core will record all resources
     *       (screens, timers, and animations) created in this function. These resources will be cleaned up
     *       automatically when the app is closed by calling the `cleanResource()` function. Otherwise, the app should
     *       clean up all resources manually.
     * @note If the `enable_resize_visual_area` flag in `ESP_UI_CoreAppData_t` is set, the core will resize the visual
     *       area of the screens created in this function. This is useful when the screen displays floating UIs, such
     *       as a status bar. Otherwise, the app's screens will be displayed in full screen, but some areas might be
     *       not visible. The final visual area of the app is the intersection of the app's visual area and the
     *       `screen_size` in `ESP_UI_CoreAppData_t`. The app can call the `getVisualArea()` function to retrieve the
     *       final visual area.
     *
     * @return true if successful, otherwise false
     *
     */
    virtual bool run(void) = 0;

    /**
     * @brief Called when the app receives a back event. To exit, the app can call `notifyCoreClosed()` to notify the
     *        core to close the app.
     *
     * @return true if successful, otherwise false
     *
     */
    virtual bool back(void) = 0;

    /**
     * @brief Called when the app starts to close. The app can perform necessary operations here.
     *
     * @note  The app shouldn't call the `notifyCoreClosed()` function in this function.
     *
     * @return true if successful, otherwise false
     *
     */
    virtual bool close(void)
    {
        return true;
    }

    /**
     * @brief Called when the app starts to install. The app can perform initialization here.
     *
     * @return true if successful, otherwise false
     *
     */
    virtual bool init(void)
    {
        return true;
    }

    /**
     * @brief Called when the app starts to uninstall. The app can perform deinitialization here.
     *
     * @return true if successful, otherwise false
     *
     */
    virtual bool deinit(void)
    {
        return true;
    }

    /**
     * @brief Called when the app is paused. The app can perform necessary operations here.
     *
     * @return true if successful, otherwise false
     *
     */
    virtual bool pause(void)
    {
        return true;
    }

    /**
     * @brief Called when the app resumes. The app can perform necessary operations here.
     *
     * @return true if successful, otherwise false
     *
     */
    virtual bool resume(void)
    {
        return true;
    }

    /**
     * @brief Called when the app starts to close. If the `enable_recycle_resource` flag in `ESP_UI_CoreAppData_t` is
     *        not set, the app should redefine this function to clean up all resources manually. Otherwise, the core
     *        will clean up the resources (screens, timers, and animations) automatically.
     *
     * @return true if successful, otherwise false
     *
     */
    virtual bool cleanResource(void);

    /**
     * @brief Notify the core to close the app, and the core will eventually call the `close()` function.
     *
     * @note  This function should be called in the `back()` function and should not be called in the `close()` function.
     *
     * @return true if successful, otherwise false
     *
     */
    bool notifyCoreClosed(void) const;

    /**
     * @brief Set the icon image of the app.
     *
     * @param icon_image The icon image of the app.
     *
     */
    void setLauncherIconImage(const ESP_UI_StyleImage_t &icon_image);

    /**
     * @brief Get the core object.
     *
     * @return core: used to access the core functions
     *
     */
    ESP_UI_Core *getCore(void) const
    {
        return _core;
    }

private:
    virtual bool beginExtra(void) { return true; }
    virtual bool delExtra(void)   { return true; }
    virtual bool processInstall(ESP_UI_Core *core, int id);
    virtual bool processUninstall(void);
    virtual bool processRun(lv_area_t area);
    virtual bool processResume(void);
    virtual bool processPause(void);
    virtual bool processClose(bool is_app_active);

    bool initActiveScreen(void);
    bool startResizeVisualArea(void);
    bool endResizeVisualArea(void);
    bool startRecordResource(void);
    bool endRecordResource(void);
    bool saveRecentScreen(void);
    bool loadRecentScreen(void);
    bool setVisualArea(const lv_area_t &area);
    bool cleanActiveScreen(void);
    bool enableAutoClean(void);
    bool saveDisplayTheme(void);
    bool loadDisplayTheme(void);
    bool saveAppTheme(void);
    bool loadAppTheme(void);
    bool createAndloadTempScreen(void);
    bool delTempScreen(void);

    static void onCleanResourceEventCallback(lv_event_t *e);
    static void onResizeScreenLoadedEventCallback(lv_event_t *e);

    // Core
    ESP_UI_Core *_core;
    ESP_UI_CoreAppData_t _core_init_data;
    ESP_UI_CoreAppData_t _core_active_data;
    ESP_UI_CoreAppStatus_t _status;
    // Attributes
    int _id;
    struct {
        uint8_t is_closing: 1;
        uint8_t is_screen_small: 1;
    } _flags;
    struct {
        uint16_t w;
        uint16_t h;
        lv_theme_t *theme;
    } _display_style;
    struct {
        lv_area_t visual_area;
        lv_theme_t *theme;
    } _app_style;
    // Resources
    int _resource_timer_count;
    int _resource_anim_count;
    int _resource_head_screen_index;
    int _resource_screen_count;
    lv_obj_t *_active_screen;
    lv_obj_t *_temp_screen;
    lv_timer_t *_resource_head_timer;
    lv_anim_t *_resource_head_anim;
    std::list <lv_obj_t *> _resource_screens;
    std::list <lv_timer_t *> _resource_timers;
    std::list <lv_anim_t *> _resource_anims;
};
// *INDENT-OFF*
