/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "esp_ui.hpp"

/**
 * @brief A template for a phone app with UIs exported from Squareline Studio. Users can modify this template
 *        to design their own app.
 *
 */
class PhoneAppSquareline: public ESP_UI_PhoneApp {
public:
    /**
     * @brief Construct a app with basic configuration
     *
     * @param use_status_bar Flag to show the status bar
     * @param use_navigation_bar Flag to show the navigation bar. If not set, the `enable_navigation_gesture` flag in
     *                           `ESP_UI_PhoneAppData_t` will be set
     *
     */
    PhoneAppSquareline(bool use_status_bar = true, bool use_navigation_bar = true);

    /**
     * @brief Destructor for the phone app
     *
     */
    ~PhoneAppSquareline();

protected:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////// The following functions must be implemented by the user's APP class //////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
    bool run(void) override;

    /**
     * @brief Called when the app receives a back event. To exit, the app can call `notifyCoreClosed()` to notify the
     *        core to close the app.
     *
     * @return true if successful, otherwise false
     *
     */
    bool back(void) override;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following functions can be redefined by the user's APP class ///////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // /**
    //  * @brief Called when the app starts to close. The app can perform necessary operations here.
    //  *
    //  * @note  The app shouldn't call the `notifyCoreClosed()` function in this function.
    //  *
    //  * @return true if successful, otherwise false
    //  *
    //  */
    // bool close(void) override;

    // /**
    //  * @brief Called when the app starts to install. The app can perform initialization here.
    //  *
    //  * @return true if successful, otherwise false
    //  *
    //  */
    // bool init(void) override;

    // /**
    //  * @brief Called when the app is paused. The app can perform necessary operations here.
    //  *
    //  * @return true if successful, otherwise false
    //  *
    //  */
    // bool pause(void) override

    // /**
    //  * @brief Called when the app resumes. The app can perform necessary operations here.
    //  *
    //  * @return true if successful, otherwise false
    //  *
    //  */
    // bool resume(void) override

    // /**
    //  * @brief Called when the app starts to close. If the `enable_recycle_resource` flag in `ESP_UI_CoreAppData_t` is
    //  *        not set, the app should redefine this function to clean up all resources manually. Otherwise, the core
    //  *        will clean up the resources (screens, timers, and animations) automatically.
    //  *
    //  * @return true if successful, otherwise false
    //  *
    //  */
    // bool cleanResource(void) override;
};
