/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include "systems/core/esp_brookesia_core_app.hpp"
#include "systems/phone/widgets/gesture/esp_brookesia_gesture.hpp"
#include "widgets/status_bar/esp_brookesia_status_bar.hpp"
#include "widgets/navigation_bar/esp_brookesia_navigation_bar.hpp"
#include "widgets/recents_screen/esp_brookesia_recents_screen.hpp"

// *INDENT-OFF*

typedef struct {
    uint8_t app_launcher_page_index;                    /*!< The index of the app launcher page where the icon is shown */
    uint8_t status_icon_area_index;                     /*!< The index of the status area where the icon is shown */
    ESP_Brookesia_StatusBarIconData_t status_icon_data;        /*!< The status icon data. If the `enable_status_icon_common_size`
                                                             flag is set, the `size` in this value will be ignored */
    ESP_Brookesia_StatusBarVisualMode_t status_bar_visual_mode;            /*!< The visual mode of the status bar */
    ESP_Brookesia_NavigationBarVisualMode_t navigation_bar_visual_mode;    /*!< The visual mode of the navigation bar */
    struct {
        uint8_t enable_status_icon_common_size: 1;      /*!< If set, the size of the status icon will be set to the
                                                             common size in the status bar data */
        uint8_t enable_navigation_gesture: 1;           /*!< If set and the gesture is enabled, the navigation gesture
                                                             will be enabled. */
    } flags;                                            /*!< The flags for the phone app data */
} ESP_Brookesia_PhoneAppData_t;

/**
 * @brief The default initializer for phone app data structure
 *
 * @note  The `app_launcher_page_index` and `status_icon_area_index` are set to 0
 * @note  The `enable_status_icon_common_size` and `enable_navigation_gesture` flags are set by default
 * @note  If the `use_navigation_bar` flag is set, the visual mode of the navigation bar will be set to
 *        `ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_SHOW_FLEX`
 *
 * @param status_icon The status icon image. Set to `NULL` if no icon is needed
 * @param use_status_bar Flag to show the status bar
 * @param use_navigation_bar Flag to show the navigation bar
 *
 */
#define ESP_BROOKESIA_PHONE_APP_DATA_DEFAULT(status_icon, use_status_bar, use_navigation_bar)                 \
    {                                                                                                  \
        .app_launcher_page_index = 0,                                                                  \
        .status_icon_area_index = 0,                                                                   \
        .status_icon_data = {                                                                          \
            .size = {},                                                                                \
            .icon = {                                                                                  \
                .image_num = (uint8_t)((status_icon != NULL) ? 1 : 0),                                 \
                .images = {                                                                            \
                    ESP_BROOKESIA_STYLE_IMAGE(status_icon),                                                   \
                },                                                                                     \
            },                                                                                         \
        },                                                                                             \
        .status_bar_visual_mode = (use_status_bar) ? ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_SHOW_FIXED :        \
                                                     ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_HIDE,               \
        .navigation_bar_visual_mode = (use_navigation_bar) ? ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_SHOW_FLEX : \
                                                             ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_HIDE,       \
        .flags = {                                                                                     \
            .enable_status_icon_common_size = 1,                                                       \
            .enable_navigation_gesture = 1,                                                            \
        },                                                                                             \
    }

class ESP_Brookesia_Phone;

/**
 * @brief The phone app class. This serves as the base class for all phone app classes. User-defined phone app classes
 *        should inherit from this class.
 *
 */
class ESP_Brookesia_PhoneApp: public ESP_Brookesia_CoreApp {
public:
    friend class ESP_Brookesia_PhoneHome;
    friend class ESP_Brookesia_PhoneManager;

    /**
     * @brief Construct a phone app with detailed configuration
     *
     * @param core_data The configuration data for the core
     * @param phone_data The configuration data for the phone
     *
     */
    ESP_Brookesia_PhoneApp(const ESP_Brookesia_CoreAppData_t &core_data, const ESP_Brookesia_PhoneAppData_t &phone_data);

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
    ESP_Brookesia_PhoneApp(const char *name, const void *launcher_icon, bool use_default_screen);

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
    ESP_Brookesia_PhoneApp(
        const char *name, const void *launcher_icon, bool use_default_screen, bool use_status_bar,
        bool use_navigation_bar
    );

    /**
     * @brief Destructor for the phone app
     *
     */
    ~ESP_Brookesia_PhoneApp() override;

    /**
     * @brief Set the status icon state
     *
     * @param state The state of the icon. The value should be less than the maximum number of icons
     *
     * @return true if successful, otherwise false
     *
     */
    bool setStatusIconState(uint8_t state);

    /**
     * @brief Get the initial data of the phone app
     *
     * @return data: phone app data which is set during initialization
     *
     */
    const ESP_Brookesia_PhoneAppData_t &getInitData(void) const
    {
        return _init_data;
    }

    /**
     * @brief Get the active data of the phone app
     *
     * @return data: phone app data which is calibrated during runtime
     *
     */
    const ESP_Brookesia_PhoneAppData_t &getActiveData(void) const
    {
        return _active_data;
    }

    /**
     * @brief Get the pointer of phone object. Users can use this pointer to access other phone widgets like home, manager
     *
     * @return The phone object pointer
     *
     */
    ESP_Brookesia_Phone *getPhone(void);

private:
    bool beginExtra(void) override;
    bool delExtra(void) override;

    bool updateRecentsScreenSnapshotConf(const void *image_resource);

    ESP_Brookesia_PhoneAppData_t _init_data;
    ESP_Brookesia_PhoneAppData_t _active_data;
    ESP_Brookesia_RecentsScreenSnapshotConf_t _recents_screen_snapshot_conf;
};

// *INDENT-ON*
