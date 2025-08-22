/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include "esp_brookesia_systems_internal.h"
#include "systems/base/esp_brookesia_base_app.hpp"
#include "widgets/gesture/esp_brookesia_gesture.hpp"

namespace esp_brookesia::systems::speaker {

class Speaker;

/**
 * @brief The speaker app class. This serves as the base class for all speaker app classes. User-defined speaker app classes
 *        should inherit from this class.
 *
 */
class App: public base::App {
public:
    friend class Display;
    friend class Manager;

    struct Config {
        static constexpr Config SIMPLE_CONSTRUCTOR(bool enable_navigation_gesture)
        {
            return {
                .app_launcher_page_index = 0,
                .flags = {
                    .enable_navigation_gesture = enable_navigation_gesture,
                },
            };
        }

        uint8_t app_launcher_page_index;                    /*!< The index of the app launcher page where the icon is shown */
        struct {
            uint8_t enable_navigation_gesture: 1;           /*!< If set and the gesture is enabled, the navigation gesture
                                                                will be enabled. */
        } flags;                                            /*!< The flags for the speaker app data */
    };

    App(const App &) = delete;
    App(App &&) = delete;
    App &operator=(const App &) = delete;
    App &operator=(App &&) = delete;

    /**
     * @brief Construct a speaker app with detailed configuration
     *
     * @param core_data The configuration data for the core
     * @param speaker_data The configuration data for the speaker
     *
     */
    App(const base::App::Config &core_data, const Config &speaker_data);

    /**
     * @brief Construct a speaker app with basic configuration
     *
     * @param name The name of the app
     * @param launcher_icon The image of the launcher icon. If set to `nullptr`, it will use the default image
     * @param use_default_screen Flag to enable the default screen. If true, the core will create a default screen, and
     *                           the app should create all UI resources on it using `lv_scr_act()`. The screen will be
     *                           automatically cleaned up
     * @param enable_gesture_navigation Flag to enable the gesture navigation. If true, the gesture navigation will be
     *                                   enabled.
     *
     */
    App(const char *name, const void *launcher_icon, bool use_default_screen, bool enable_gesture_navigation);

    /**
     * @brief Construct a speaker app with basic configuration
     *
     * @param name The name of the app
     * @param launcher_icon The image of the launcher icon. If set to `nullptr`, it will use the default image
     * @param use_default_screen Flag to enable the default screen. If true, the core will create a default screen, and
     *                           the app should create all UI resources on it using `lv_scr_act()`. The screen will be
     *                           automatically cleaned up
     *
     */
    App(const char *name, const void *launcher_icon, bool use_default_screen);

    /**
     * @brief Destructor for the speaker app
     *
     */
    ~App() override;

    /**
     * @brief Get the initial config of the speaker app
     *
     * @return config: speaker app config which is set during initialization
     *
     */
    const Config &getInitConfig(void) const
    {
        return _init_config;
    }

    /**
     * @brief Get the active config of the speaker app
     *
     * @return config: speaker app config which is calibrated during runtime
     *
     */
    const Config &getActiveConfig(void) const
    {
        return _active_config;
    }

    /**
     * @brief Get the pointer of speaker object. Users can use this pointer to access other speaker widgets like display, manager
     *
     * @return The speaker object pointer
     *
     */
    Speaker *getSystem(void);

    [[deprecated("Use `getInitConfig()` instead")]]
    const Config &getInitData(void) const
    {
        return getInitConfig();
    }

    [[deprecated("Use `getActiveConfig()` instead")]]
    const Config &getActiveData(void) const
    {
        return getActiveConfig();
    }

private:
    bool beginExtra(void) override;
    bool delExtra(void) override;

    Config _init_config;
    Config _active_config;
};

/* Keep compatibility with old code */
using AppData_t [[deprecated("Use `esp_brookesia::systems::speaker::App::Config` instead")]] =
    App::Config;
#define ESP_BROOKESIA_SPEAKER_APP_DATA_DEFAULT(use_navigation_gesture) \
    AppData_t::SIMPLE_CONSTRUCTOR(use_navigation_gesture)

} // namespace esp_brookesia::systems::speaker
