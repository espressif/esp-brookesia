/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include "systems/base/esp_brookesia_base_app.hpp"
#include "systems/phone/widgets/gesture/esp_brookesia_gesture.hpp"
#include "widgets/status_bar/esp_brookesia_status_bar.hpp"
#include "widgets/navigation_bar/esp_brookesia_navigation_bar.hpp"
#include "widgets/recents_screen/esp_brookesia_recents_screen.hpp"

namespace esp_brookesia::systems::phone {

class Phone;

/**
 * @brief The phone app class. This serves as the base class for all phone app classes. User-defined phone app classes
 *        should inherit from this class.
 *
 */
class App: public base::App {
public:
    friend class Display;
    friend class Manager;

    struct Config {
        /**
         * @brief The default initializer for phone app data structure
         *
         * @note  The `app_launcher_page_index` and `status_icon_area_index` are set to 0
         * @note  The `enable_status_icon_common_size` and `enable_navigation_gesture` flags are set by default
         * @note  If the `use_navigation_bar` flag is set, the visual mode of the navigation bar will be set to
         *        `NAVIGATION_BAR_VISUAL_MODE_SHOW_FLEX`
         *
         * @param status_icon The status icon image. Set to `NULL` if no icon is needed
         * @param use_status_bar Flag to show the status bar
         * @param use_navigation_bar Flag to show the navigation bar
         *
         */
        static constexpr Config SIMPLE_CONSTRUCTOR(const void *status_icon, bool use_status_bar, bool use_navigation_bar)
        {
            return {
                .app_launcher_page_index = 0,
                .status_icon_area_index = 0,
                .status_icon_data = {
                    .size = {},
                    .icon = {
                        .image_num = (uint8_t)((status_icon != NULL) ? 1 : 0),
                        .images = {
                            gui::StyleImage::IMAGE(status_icon),
                        },
                    },
                },
                .status_bar_visual_mode = (use_status_bar) ? StatusBar::VisualMode::SHOW_FIXED : StatusBar::VisualMode::HIDE,
                .navigation_bar_visual_mode = (use_navigation_bar) ? NavigationBar::VisualMode::SHOW_FLEX : NavigationBar::VisualMode::HIDE,
                .flags = {
                    .enable_status_icon_common_size = 1,
                    .enable_navigation_gesture = 1,
                },
            };
        }

        uint8_t app_launcher_page_index;                    /*!< The index of the app launcher page where the icon is shown */
        uint8_t status_icon_area_index;                     /*!< The index of the status area where the icon is shown */
        StatusBarIcon::Data status_icon_data;        /*!< The status icon data. If the `enable_status_icon_common_size`
                                                                flag is set, the `size` in this value will be ignored */
        StatusBar::VisualMode status_bar_visual_mode;            /*!< The visual mode of the status bar */
        NavigationBar::VisualMode navigation_bar_visual_mode;    /*!< The visual mode of the navigation bar */
        struct {
            uint8_t enable_status_icon_common_size: 1;      /*!< If set, the size of the status icon will be set to the
                                                                common size in the status bar data */
            uint8_t enable_navigation_gesture: 1;           /*!< If set and the gesture is enabled, the navigation gesture
                                                                will be enabled. */
        } flags;                                            /*!< The flags for the phone app data */
    };

    /**
     * @brief Construct a phone app with detailed configuration
     *
     * @param core_data The configuration data for the core
     * @param phone_data The configuration data for the phone
     *
     */
    App(const base::App::Config &core_data, const Config &phone_data);

    /**
     * @brief Construct a phone app with basic configuration
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
     * @brief Construct a phone app with basic configuration
     *
     * @param name The name of the app
     * @param launcher_icon The image of the launcher icon. If set to `nullptr`, it will use the default image
     * @param use_default_screen Flag to enable the default screen. If true, the core will create a default screen, and
     *                           the app should create all UI resources on it using `lv_scr_act()`. The screen will be
     *                           automatically cleaned up
     * @param use_status_bar Flag to show the status bar
     * @param use_navigation_bar Flag to show the navigation bar
     *
     */
    App(
        const char *name, const void *launcher_icon, bool use_default_screen, bool use_status_bar,
        bool use_navigation_bar
    );

    /**
     * @brief Destructor for the phone app
     *
     */
    ~App() override;

    /**
     * @brief Set the status icon state
     *
     * @param state The state of the icon. The value should be less than the maximum number of icons
     *
     * @return true if successful, otherwise false
     *
     */
    bool setStatusIconState(int state);

    /**
     * @brief Get the initial config of the phone app
     *
     * @return config: phone app config which is set during initialization
     *
     */
    const Config &getInitConfig(void) const
    {
        return _init_config;
    }

    /**
     * @brief Get the active config of the phone app
     *
     * @return config: phone app config which is calibrated during runtime
     *
     */
    const Config &getActiveConfig(void) const
    {
        return _active_config;
    }

    /**
     * @brief Get the phone object
     *
     * @return The phone object
     *
     */
    Phone *getSystem(void);

    [[deprecated("Use `getSystem()` instead")]]
    Phone *getPhone(void)
    {
        return getSystem();
    }

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

    bool updateRecentsScreenSnapshotConf(const void *image_resource);

    Config _init_config;
    Config _active_config;
    RecentsScreenSnapshot::Conf _recents_screen_snapshot_conf;
};

} // namespace esp_brookesia::systems::phone

using ESP_Brookesia_PhoneAppData_t [[deprecated("Use `esp_brookesia::systems::phone::App::Config` instead")]] =
    esp_brookesia::systems::phone::App::Config;
#define ESP_BROOKESIA_PHONE_APP_DATA_DEFAULT(status_icon, use_status_bar, use_navigation_bar) \
    esp_brookesia::systems::phone::App::Config::SIMPLE_CONSTRUCTOR(status_icon, use_status_bar, use_navigation_bar)
using ESP_Brookesia_PhoneApp [[deprecated("Use `esp_brookesia::systems::phone::App` instead")]] =
    esp_brookesia::systems::phone::App;
