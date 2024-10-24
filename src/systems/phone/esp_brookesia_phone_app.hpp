/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include "core/esp_brookesia_core_app.hpp"
#include "widgets/gesture/esp_brookesia_gesture.hpp"
#include "esp_brookesia_phone_type.h"

class ESP_Brookesia_Phone;

// *INDENT-OFF*
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
// *INDENT-OFF*
