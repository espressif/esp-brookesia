/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "esp_brookesia.hpp"

namespace esp_brookesia::apps {

/**
 * @brief A template for a phone app with UIs exported from AI_Profile Studio. Users can modify this template
 *        to design their own app.
 *
 */
class AI_Profile: public systems::speaker::App {
public:
    /**
     * @brief Destructor for the phone app
     *
     */
    ~AI_Profile();

    /**
     * @brief Get the singleton instance of AI_Profile
     *
     * @return Pointer to the singleton instance
     */
    static AI_Profile *requestInstance();

protected:
    /**
     * @brief Private constructor to enforce singleton pattern
     *
     */
    AI_Profile();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////// The following functions must be implemented by the user's app class. /////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Called when the app starts running. This is the entry point for the app, where all UI resources should be
     *        created.
     *
     * @note If the `enable_default_screen` flag in `systems::base::App::Config` is set, when app starts, the core will create
     *       a default screen which will be automatically loaded and cleaned up. Then the app should create all UI
     *       resources on it using `lv_scr_act()` in this function. Otherwise, the app needs to create a new screen and
     *       load it manually in this function
     * @note If the `enable_recycle_resource` flag in `systems::base::App::Config` is set, when app closes, the core will
     *       automatically cleanup all recorded resources, including screens (`lv_obj_create(NULL)`),
     *       animations (`lv_anim_start()`), and timers (`lv_timer_create()`). The resources created in this function
     *       will be recorded. Otherwise, the app needs to call `cleanRecordResource()` function to clean manually
     * @note If the `enable_resize_visual_area` flag in `systems::base::App::Config` is set, the core will resize the visual
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

    bool close(void) override;

private:
    int _robot_current_index = 0;
    int _robot_next_index = 0;

    inline static AI_Profile *_instance = nullptr; // Singleton instance
};

} // namespace esp_brookesia::apps
