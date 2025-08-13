/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <list>
#include <map>
#include <string>
#include "lvgl.h"
#include "lvgl/esp_brookesia_lv_helper.hpp"
#include "more/esp_utils_plugin_registry.hpp"

namespace esp_brookesia::systems::base {

class Context;

/**
 * @brief The core app class. This serves as the base class for all internal app classes. User-defined app classes
 *        should not inherit from this class directly.
 *
 */
class App {
public:
    friend class Manager;

    struct Config {
        /**
         * @brief The default initializer for core app config structure
         *
         * @note The `enable_recycle_resource` and `enable_resize_visual_area` flags are enabled by default.
         * @note The `screen_size` is set to the full screen by default.
         *
         * @param app_name The name of the app
         * @param icon The icon image of the app
         * @param use_default_screen description
         *
         */
        static constexpr Config SIMPLE_CONSTRUCTOR(const char *name, const void *launcher_icon, bool use_default_screen)
        {
            return {
                .name = name,
                .launcher_icon = gui::StyleImage::IMAGE(launcher_icon),
                .screen_size = gui::StyleSize::RECT_PERCENT(100, 100),
                .flags = {
                    .enable_default_screen = use_default_screen,
                    .enable_recycle_resource = 1,
                    .enable_resize_visual_area = 1,
                }
            };
        }

        const char *name;                           /*!< App name string */
        gui::StyleImage launcher_icon;          /*!< Launcher icon image */
        gui::StyleSize screen_size;             /*!< App screen size */
        struct {
            uint8_t enable_default_screen: 1;       /*!< If this flag is enabled, when app starts, the core will create a
                                                        default screen which will be automatically loaded and cleaned up.
                                                        Otherwise, the app needs to create a new screen and load it
                                                        manually in app's `run()` function */
            uint8_t enable_recycle_resource: 1;     /*!< If this flag is enabled, when app closes, the core will cleaned up
                                                        all recorded resources(screens, timers, and animations) automatically.
                                                        These resources are recorded in app's `run()` and `pause()` functions,
                                                        or between the `startRecordResource()` and `stopRecordResource()`
                                                        functions.  Otherwise, the app needs to call `cleanRecordResource()`
                                                        function to clean manually */
            uint8_t enable_resize_visual_area: 1;   /*!< If this flag is enabled, the core will resize the visual area of
                                                        all recorded screens which are recorded in app's `run()` and `pause()`
                                                        functions, or between the `startRecordResource()` and `stopRecordResource()`
                                                        functions. This is useful when the screen displays floating UIs, such as a
                                                        status bar. Otherwise, the app's screens will be displayed in full screen,
                                                        but some areas might be not visible. The app can call the `getVisualArea()`
                                                        function to retrieve the final visual area */
        } flags;                                    /*!< Core app config flags */
    };

    enum class Status : uint8_t {
        UNINSTALLED = 0,
        RUNNING,
        PAUSED,
        CLOSED,
    };

    using Registry = esp_utils::PluginRegistry<App>;

    static constexpr int APP_ID_MIN = 1;

    /**
     * @brief Delete copy constructor and assignment operator
     */
    App(const App &) = delete;
    App(App &&) = delete;
    App &operator=(const App &) = delete;
    App &operator=(App &&) = delete;

    /**
     * @brief Construct a core app with detailed configuration.
     *
     * @param data The configuration data for the core app.
     *
     */
    App(const Config &data)
        : _init_config(data)
    {
    }

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
    App(const char *name, const void *launcher_icon, bool use_default_screen)
        : _init_config(Config::SIMPLE_CONSTRUCTOR(name, launcher_icon, use_default_screen))
    {
    }

    /**
     * @brief Destructor for the core app, should be defined by the user's app class.
     */
    virtual ~App() = default;

    /**
     * @brief  Check if the app is initialized
     *
     * @return true if the app is initialized, otherwise false
     *
     */
    bool checkInitialized(void) const;

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
        return _active_config.name;
    }

    /**
     * @brief Get the launcher icon
     *
     * @return image: the icon image of the app
     *
     */
    const gui::StyleImage &getLauncherIcon(void) const
    {
        return _active_config.launcher_icon;
    }

    /**
     * @brief Get the visual area
     *
     * @return area: the visual area of the app
     *
     */
    const lv_area_t &getVisualArea(void) const
    {
        return _app_style.calibrate_visual_area;
    }

    /**
     * @brief Get the initial core data which is set during initialization
     *
     * @return data: the core data of the app
     *
     */
    const Config &getCoreInitData(void) const
    {
        return _init_config;
    }

    /**
     * @brief Get the active core data which is calibrated during runtime
     *
     * @return data: the core data of the app
     *
     */
    const Config &getCoreActiveData(void) const
    {
        return _active_config;
    }

    /**
     * @brief Get the system context
     *
     * @return context: the system context of the app
     *
     */
    Context *getSystemContext(void) const
    {
        return _system_context;
    }

    [[deprecated("Use `getSystemContext()` instead")]]
    Context *getCore(void) const
    {
        return getSystemContext();
    }

protected:
    /**
     * @brief Called when the app starts running. This is the entry point for the app, where all UI resources should be
     *        created.
     *
     * @note If the `enable_default_screen` flag in `Config` is set, when app starts, the core will create
     *       a default screen which will be automatically loaded and cleaned up. Then the app should create all UI
     *       resources on it using `lv_scr_act()` in this function. Otherwise, the app needs to create a new screen and
     *       load it manually in this function
     * @note If the `enable_recycle_resource` flag in `Config` is set, when app closes, the core will
     *       automatically cleanup all recorded resources, including screens (`lv_obj_create(NULL)`),
     *       animations (`lv_anim_start()`), and timers (`lv_timer_create()`). The resources created in this function
     *       will be recorded. Otherwise, the app needs to call `cleanRecordResource()` function to clean manually
     * @note If the `enable_resize_visual_area` flag in `Config` is set, the core will resize the visual
     *       area of all recorded screens. The screens created in this function will be recorded. This is useful when
     *       the screen displays floating UIs, such as a status bar. Otherwise, the app's screens will be displayed in
     *       full screen, but some areas might be not visible. The app can call the `getVisualArea()` function to
     *       retrieve the final visual area
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
     * @note  The app should avoid calling the `notifyCoreClosed()` function in this function.
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
     * @note If the `enable_recycle_resource` flag in `Config` is set, when app closes, the core will
     *       automatically cleanup all recorded resources, including screens (`lv_obj_create(NULL)`),
     *       animations (`lv_anim_start()`), and timers (`lv_timer_create()`). The resources created in this function
     *       will be recorded. Otherwise, the app needs to call `cleanRecordResource()` function to clean manually
     * @note If the `enable_resize_visual_area` flag in `Config` is set, the core will resize the visual
     *       area of all recorded screens. The screens created in this function will be recorded. This is useful when
     *       the screen displays floating UIs, such as a status bar. Otherwise, the app's screens will be displayed in
     *       full screen, but some areas might be not visible. The app can call the `getVisualArea()` function to
     *       retrieve the final visual area
     *
     * @return true if successful, otherwise false
     *
     */
    virtual bool resume(void)
    {
        return true;
    }

    /**
     * @brief Called when the app starts to close. The app can perform extra resource cleanup here.
     *
     * @note If there are resources that not recorded by the core (not created in the `run()` and `pause()` functions,
     *       or between the `startRecordResource()` and `stopRecordResource()` functions), the app should call this
     *       function to cleanup these resources manually. This function is not conflicted with the
     *       `cleanRecordResource()` function.
     *
     * @return true if successful, otherwise false
     *
     */
    virtual bool cleanResource(void)
    {
        return true;
    }

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
    void setLauncherIconImage(const gui::StyleImage &icon_image);

    /**
     * @brief Start recording resources(screens, timers, and animations) manually.
     *
     * @note If the `enable_resize_visual_area` flag in `Config` is set, the core will resize the visual
     *       area of all recorded screens which are recorded in this function. This is useful when the screen displays
     *       floating UIs, such as a status bar. Otherwise, the app's screens will be displayed in full screen, but
     *       some areas might be not visible. The final visual area of the app is the intersection of the app's visual
     *       area and the `screen_size`. The app can call the `getVisualArea()` function to retrieve the final visual
     *       area
     * @note This function should be called before creating any resources, including screens (`lv_obj_create(NULL)`),
     *       animations (`lv_anim_start()`), and timers (`lv_timer_create()`)
     * @note This function should not be called in the `run()` and `pause()` functions.
     *
     * @return true if successful, otherwise false
     *
     */
    bool startRecordResource(void);

    /**
     * @brief Stop recording resources(screens, timers, and animations) manually.
     *
     * @note This function should be called after creating any resources, including screens (`lv_obj_create(NULL)`),
     *       animations (`lv_anim_start()`), and timers (`lv_timer_create()`)
     * @note This function should not be called in the `run()` and `pause()` functions.
     *
     * @return true if successful, otherwise false
     *
     */
    bool endRecordResource(void);

    /**
     * @brief Cleanup all recorded resources(screens, timers, and animations) manually. These resources are recorded in
     *        app's `run()` and `pause()` functions, or between the `startRecordResource()` and `stopRecordResource()`
     *        functions.
     *
     * @note If the `enable_recycle_resource` flag in `Config` is set, when app closes, the core will
     *       call this function automatically. So the app doesn't need to call this function manually.
     * @note This function will clear all resources records after finishing the cleanup.
     *
     * @return true if successful, otherwise false
     *
     */
    bool cleanRecordResource(void);

    Context *_system_context = nullptr;

private:
    virtual bool beginExtra(void)
    {
        return true;
    }
    virtual bool delExtra(void)
    {
        return true;
    }
    virtual bool processInstall(Context *system_context, int id);
    virtual bool processUninstall(void);
    virtual bool processRun(void);
    virtual bool processResume(void);
    virtual bool processPause(void);
    virtual bool processClose(bool is_app_active);

    bool setVisualArea(const lv_area_t &area);
    bool calibrateVisualArea(void);
    bool initDefaultScreen(void);
    bool cleanDefaultScreen(void);
    bool saveRecentScreen(bool check_valid);
    bool loadRecentScreen(void);
    bool resetRecordResource(void);
    bool enableAutoClean(void);
    bool saveDisplayTheme(void);
    bool loadDisplayTheme(void);
    bool saveAppTheme(void);
    bool loadAppTheme(void);
    // TODO
    // bool createAndloadTempScreen(void);
    // bool delTempScreen(void);

    static void onCleanResourceEventCallback(lv_event_t *e);
    static void onResizeScreenLoadedEventCallback(lv_event_t *e);

    // Core
    Config _init_config = {};
    Config _active_config = {};
    Status _status = Status::UNINSTALLED;
    // Attributes
    int _id = APP_ID_MIN - 1;
    struct {
        uint8_t is_closing: 1;
        uint8_t is_screen_small: 1;
        uint8_t is_resource_recording: 1;
    } _flags = {};
    struct {
        int w;
        int h;
        lv_theme_t *theme;
    } _display_style = {};
    struct {
        lv_area_t origin_visual_area;
        lv_area_t calibrate_visual_area;
        lv_theme_t *theme;
    } _app_style = {};
    // Resources
    int _resource_timer_count = 0;
    int _resource_anim_count = 0;
    int _resource_head_screen_index = 0;
    int _resource_screen_count = 0;
    lv_obj_t *_last_screen = nullptr;
    lv_obj_t *_active_screen = nullptr;
    // lv_obj_t *_temp_screen;
    lv_timer_t *_resource_head_timer = nullptr;
    lv_anim_t *_resource_head_anim = nullptr;
    std::list <lv_obj_t *> _resource_screens = {};
    std::list <lv_timer_t *> _resource_timers = {};
    std::list <lv_anim_t *> _resource_anims = {};
    // These maps are meant to store additional information about the recorded resources to prevent accidental cleanup
    std::map<lv_obj_t *, std::pair<const lv_obj_class_t *, lv_obj_t *>> _resource_screens_class_parent_map;
    std::map<lv_timer_t *, std::pair<lv_timer_cb_t, void *>> _resource_timers_cb_usr_map;
    std::map<lv_anim_t *, std::pair<void *, lv_anim_exec_xcb_t>> _resource_anims_var_exec_map;
};

}

/**
 * Backward compatibility
 */
using ESP_Brookesia_CoreApp [[deprecated("Use `esp_brookesia::systems::base::App` instead")]] =
    esp_brookesia::systems::base::App;
using ESP_Brookesia_CoreAppData_t [[deprecated("Use `esp_brookesia::systems::base::App::Config` instead")]] =
    esp_brookesia::systems::base::App::Config;
using ESP_Brookesia_CoreAppStatus_t [[deprecated("Use `esp_brookesia::systems::base::App::Status` instead")]] =
    esp_brookesia::systems::base::App::Status;
#define ESP_BROOKESIA_CORE_APP_STATUS_UNINSTALLED ESP_Brookesia_CoreAppStatus_t::UNINSTALLED
#define ESP_BROOKESIA_CORE_APP_STATUS_RUNNING     ESP_Brookesia_CoreAppStatus_t::RUNNING
#define ESP_BROOKESIA_CORE_APP_STATUS_PAUSED      ESP_Brookesia_CoreAppStatus_t::PAUSED
#define ESP_BROOKESIA_CORE_APP_STATUS_CLOSED      ESP_Brookesia_CoreAppStatus_t::CLOSED
#define ESP_BROOKESIA_CORE_APP_ID_MIN             ESP_Brookesia_CoreAppStatus_t::APP_ID_MIN
#define ESP_BROOKESIA_CORE_APP_DATA_DEFAULT(app_name, icon, use_default_screen) \
    esp_brookesia::systems::base::App::Config::SIMPLE_CONSTRUCTOR(app_name, icon, use_default_screen)
