/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "esp_brookesia.hpp"

/**
 * @brief A template for a phone app with simple configuration. Users can modify this template to design their own app.
 *
 */
class PhoneAppSimpleConf: public ESP_Brookesia_PhoneApp {
public:
    /**
     * @brief Construct a app with basic configuration
     *
     * @param use_status_bar Flag to show the status bar
     * @param use_navigation_bar Flag to show the navigation bar
     *
     */
    PhoneAppSimpleConf(bool use_status_bar, bool use_navigation_bar);

    /**
     * @brief Construct a app with basic configuration
     *
     */
    PhoneAppSimpleConf();

    /**
     * @brief Destructor for the phone app
     *
     */
    ~PhoneAppSimpleConf();

protected:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////// The following functions must be implemented by the user's app class. /////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Called when the app starts running. This is the entry point for the app, where all UI resources should be
     *        created.
     *
     * @note If the `enable_default_screen` flag in `ESP_Brookesia_CoreAppData_t` is set, when app starts, the core will create
     *       a default screen which will be automatically loaded and cleaned up. Then the app should create all UI
     *       resources on it using `lv_scr_act()` in this function. Otherwise, the app needs to create a new screen and
     *       load it manually in this function
     * @note If the `enable_recycle_resource` flag in `ESP_Brookesia_CoreAppData_t` is set, when app closes, the core will
     *       automatically cleanup all recorded resources, including screens (`lv_obj_create(NULL)`),
     *       animations (`lv_anim_start()`), and timers (`lv_timer_create()`). The resources created in this function
     *       will be recorded. Otherwise, the app needs to call `cleanRecordResource()` function to clean manually
     * @note If the `enable_resize_visual_area` flag in `ESP_Brookesia_CoreAppData_t` is set, the core will resize the visual
     *       area of all recorded screens. The screens created in this function will be recorded. This is useful when
     *       the screen displays floating UIs, such as a status bar. Otherwise, the app's screens will be displayed in
     *       full screen, but some areas might be not visible. The app can call the `getVisualArea()` function to
     *       retrieve the final visual area
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
/////////////////////////// The following functions can be redefined by the user's app class. //////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Called when the app starts to close. The app can perform necessary operations here.
     *
     * @note  The app shouldn't call the `notifyCoreClosed()` function in this function.
     *
     * @return true if successful, otherwise false
     *
     */
    // bool close(void) override;

    /**
     * @brief Called when the app starts to install. The app can perform initialization here.
     *
     * @return true if successful, otherwise false
     *
     */
    // bool init(void) override;

    /**
     * @brief Called when the app starts to install. The app can perform initialization here.
     *
     * @return true if successful, otherwise false
     *
     */
    // bool init(void) override;

    /**
     * @brief Called when the app is paused. The app can perform necessary operations here.
     *
     * @return true if successful, otherwise false
     *
     */
    // bool pause(void) override;

    /**
     * @brief Called when the app resumes. The app can perform necessary operations here.
     *
     * @note If the `enable_recycle_resource` flag in `ESP_Brookesia_CoreAppData_t` is set, when app closes, the core will
     *       automatically cleanup all recorded resources, including screens (`lv_obj_create(NULL)`),
     *       animations (`lv_anim_start()`), and timers (`lv_timer_create()`). The resources created in this function
     *       will be recorded. Otherwise, the app needs to call `cleanRecordResource()` function to clean manually
     * @note If the `enable_resize_visual_area` flag in `ESP_Brookesia_CoreAppData_t` is set, the core will resize the visual
     *       area of all recorded screens. The screens created in this function will be recorded. This is useful when
     *       the screen displays floating UIs, such as a status bar. Otherwise, the app's screens will be displayed in
     *       full screen, but some areas might be not visible. The app can call the `getVisualArea()` function to
     *       retrieve the final visual area
     *
     * @return true if successful, otherwise false
     *
     */
    // bool resume(void) override;

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
    // bool cleanResource(void) override;
};
