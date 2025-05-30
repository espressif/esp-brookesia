/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

// *INDENT-OFF*

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// AI Framework ///////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(ESP_BROOKESIA_CONF_ENABLE_AI_FRAMEWORK)
    #if defined(CONFIG_ESP_BROOKESIA_CONF_ENABLE_AI_FRAMEWORK)
        #define ESP_BROOKESIA_CONF_ENABLE_AI_FRAMEWORK  CONFIG_ESP_BROOKESIA_CONF_ENABLE_AI_FRAMEWORK
    #else
        #define ESP_BROOKESIA_CONF_ENABLE_AI_FRAMEWORK  (0)
    #endif
#endif

#if ESP_BROOKESIA_CONF_ENABLE_AI_FRAMEWORK
    /**
     * Agents
     */
    #if !defined(ESP_BROOKESIA_CONF_AI_ENABLE_AGENTS)
        #if defined(CONFIG_ESP_BROOKESIA_CONF_AI_ENABLE_AGENTS)
            #define ESP_BROOKESIA_CONF_AI_ENABLE_AGENTS  CONFIG_ESP_BROOKESIA_CONF_AI_ENABLE_AGENTS
        #else
            #define ESP_BROOKESIA_CONF_AI_ENABLE_AGENTS  (0)
        #endif
    #endif

    #if ESP_BROOKESIA_CONF_AI_ENABLE_AGENTS
        /**
         * XiaoZhi Agent
         */
        #if !defined(ESP_BROOKESIA_CONF_AGENTS_ENABLE_XIAOZHI)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_AGENTS_ENABLE_XIAOZHI)
                #define ESP_BROOKESIA_CONF_AGENTS_ENABLE_XIAOZHI  CONFIG_ESP_BROOKESIA_CONF_AGENTS_ENABLE_XIAOZHI
            #else
                #define ESP_BROOKESIA_CONF_AGENTS_ENABLE_XIAOZHI  (0)
            #endif
        #endif
    #endif

    /**
     * HMI
     */
    #if !defined(ESP_BROOKESIA_CONF_AI_ENABLE_HMI)
        #if defined(CONFIG_ESP_BROOKESIA_CONF_AI_ENABLE_HMI)
            #define ESP_BROOKESIA_CONF_AI_ENABLE_HMI  CONFIG_ESP_BROOKESIA_CONF_AI_ENABLE_HMI
        #else
            #define ESP_BROOKESIA_CONF_AI_ENABLE_HMI  (0)
        #endif
    #endif

    #if ESP_BROOKESIA_CONF_AI_ENABLE_HMI
        /**
         * Robot Face
         */
        #if !defined(ESP_BROOKESIA_CONF_HMI_ENABLE_ROBOT_FACE)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_HMI_ENABLE_ROBOT_FACE)
                #define ESP_BROOKESIA_CONF_HMI_ENABLE_ROBOT_FACE  CONFIG_ESP_BROOKESIA_CONF_HMI_ENABLE_ROBOT_FACE
            #else
                #define ESP_BROOKESIA_CONF_HMI_ENABLE_ROBOT_FACE  (0)
            #endif
        #endif
    #endif
#endif // ESP_BROOKESIA_CONF_ENABLE_AI_FRAMEWORK

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////// GUI ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(ESP_BROOKESIA_CONF_ENABLE_GUI)
    #if defined(CONFIG_ESP_BROOKESIA_CONF_ENABLE_GUI)
        #define ESP_BROOKESIA_CONF_ENABLE_GUI  CONFIG_ESP_BROOKESIA_CONF_ENABLE_GUI
    #else
        #define ESP_BROOKESIA_CONF_ENABLE_GUI  (0)
    #endif
#endif

#if ESP_BROOKESIA_CONF_ENABLE_GUI
    /**
     * LVGL
     */
    #if !defined(ESP_BROOKESIA_CONF_LVGL_ENABLE_DEBUG_LOG)
        #if defined(CONFIG_ESP_BROOKESIA_CONF_LVGL_ENABLE_DEBUG_LOG)
            #define ESP_BROOKESIA_CONF_LVGL_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_LVGL_ENABLE_DEBUG_LOG
        #else
            #define ESP_BROOKESIA_CONF_LVGL_ENABLE_DEBUG_LOG  (0)
        #endif
    #endif

    #if ESP_BROOKESIA_CONF_LVGL_ENABLE_DEBUG_LOG
        #if !defined(ESP_BROOKESIA_CONF_LVGL_ANIMATION_ENABLE_DEBUG_LOG)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_LVGL_ANIMATION_ENABLE_DEBUG_LOG)
                #define ESP_BROOKESIA_CONF_LVGL_ANIMATION_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_LVGL_ANIMATION_ENABLE_DEBUG_LOG
            #else
                #define ESP_BROOKESIA_CONF_LVGL_ANIMATION_ENABLE_DEBUG_LOG  (0)
            #endif
        #endif

        #if !defined(ESP_BROOKESIA_CONF_LVGL_CANVAS_ENABLE_DEBUG_LOG)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_LVGL_CANVAS_ENABLE_DEBUG_LOG)
                #define ESP_BROOKESIA_CONF_LVGL_CANVAS_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_LVGL_CANVAS_ENABLE_DEBUG_LOG
            #else
                #define ESP_BROOKESIA_CONF_LVGL_CANVAS_ENABLE_DEBUG_LOG  (0)
            #endif
        #endif

        #if !defined(ESP_BROOKESIA_CONF_LVGL_CONTAINER_ENABLE_DEBUG_LOG)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_LVGL_CONTAINER_ENABLE_DEBUG_LOG)
                #define ESP_BROOKESIA_CONF_LVGL_CONTAINER_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_LVGL_CONTAINER_ENABLE_DEBUG_LOG
            #else
                #define ESP_BROOKESIA_CONF_LVGL_CONTAINER_ENABLE_DEBUG_LOG  (0)
            #endif
        #endif

        #if !defined(ESP_BROOKESIA_CONF_LVGL_OBJECT_ENABLE_DEBUG_LOG)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_LVGL_OBJECT_ENABLE_DEBUG_LOG)
                #define ESP_BROOKESIA_CONF_LVGL_OBJECT_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_LVGL_OBJECT_ENABLE_DEBUG_LOG
            #else
                #define ESP_BROOKESIA_CONF_LVGL_OBJECT_ENABLE_DEBUG_LOG  (0)
            #endif
        #endif

        #if !defined(ESP_BROOKESIA_CONF_LVGL_SCREEN_ENABLE_DEBUG_LOG)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_LVGL_SCREEN_ENABLE_DEBUG_LOG)
                #define ESP_BROOKESIA_CONF_LVGL_SCREEN_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_LVGL_SCREEN_ENABLE_DEBUG_LOG
            #else
                #define ESP_BROOKESIA_CONF_LVGL_SCREEN_ENABLE_DEBUG_LOG  (0)
            #endif
        #endif

        #if !defined(ESP_BROOKESIA_CONF_LVGL_TIMER_ENABLE_DEBUG_LOG)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_LVGL_TIMER_ENABLE_DEBUG_LOG)
                #define ESP_BROOKESIA_CONF_LVGL_TIMER_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_LVGL_TIMER_ENABLE_DEBUG_LOG
            #else
                #define ESP_BROOKESIA_CONF_LVGL_TIMER_ENABLE_DEBUG_LOG  (0)
            #endif
        #endif
    #endif // ESP_BROOKESIA_CONF_LVGL_ENABLE_DEBUG_LOG

    /**
     * Squareline
     */
    #if !defined(ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_HELPERS)
        #if defined(CONFIG_ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_HELPERS)
            #define ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_HELPERS    CONFIG_ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_HELPERS
        #else
            #define ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_HELPERS    (0)
        #endif
    #endif

    #if !defined(ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_COMP)
        #if defined(CONFIG_ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_COMP)
            #define ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_COMP       CONFIG_ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_COMP
        #else
            #define ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_COMP       (0)
        #endif
    #endif
#endif // ESP_BROOKESIA_CONF_ENABLE_GUI

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////// Systems /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(ESP_BROOKESIA_CONF_ENABLE_SYSTEMS)
    #if defined(CONFIG_ESP_BROOKESIA_CONF_ENABLE_SYSTEMS)
        #define ESP_BROOKESIA_CONF_ENABLE_SYSTEMS  CONFIG_ESP_BROOKESIA_CONF_ENABLE_SYSTEMS
    #else
        #define ESP_BROOKESIA_CONF_ENABLE_SYSTEMS  (0)
    #endif
#endif

#if ESP_BROOKESIA_CONF_ENABLE_SYSTEMS
    /**
     * Core related
     */
    #if !defined(ESP_BROOKESIA_CONF_CORE_ENABLE_DEBUG_LOG)
        #if defined(CONFIG_ESP_BROOKESIA_CONF_CORE_ENABLE_DEBUG_LOG)
            #define ESP_BROOKESIA_CONF_CORE_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_CORE_ENABLE_DEBUG_LOG
        #else
            #define ESP_BROOKESIA_CONF_CORE_ENABLE_DEBUG_LOG  (0)
        #endif
    #endif

    #if ESP_BROOKESIA_CONF_CORE_ENABLE_DEBUG_LOG
        #if !defined(ESP_BROOKESIA_CONF_CORE_APP_ENABLE_DEBUG_LOG)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_CORE_APP_ENABLE_DEBUG_LOG)
                #define ESP_BROOKESIA_CONF_CORE_APP_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_CORE_APP_ENABLE_DEBUG_LOG
            #else
                #define ESP_BROOKESIA_CONF_CORE_APP_ENABLE_DEBUG_LOG  (0)
            #endif
        #endif

        #if !defined(ESP_BROOKESIA_CONF_CORE_HOME_ENABLE_DEBUG_LOG)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_CORE_HOME_ENABLE_DEBUG_LOG)
                #define ESP_BROOKESIA_CONF_CORE_HOME_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_CORE_HOME_ENABLE_DEBUG_LOG
            #else
                #define ESP_BROOKESIA_CONF_CORE_HOME_ENABLE_DEBUG_LOG  (0)
            #endif
        #endif

        #if !defined(ESP_BROOKESIA_CONF_CORE_MANAGER_ENABLE_DEBUG_LOG)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_CORE_MANAGER_ENABLE_DEBUG_LOG)
                #define ESP_BROOKESIA_CONF_CORE_MANAGER_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_CORE_MANAGER_ENABLE_DEBUG_LOG
            #else
                #define ESP_BROOKESIA_CONF_CORE_MANAGER_ENABLE_DEBUG_LOG  (0)
            #endif
        #endif

        #if !defined(ESP_BROOKESIA_CONF_CORE_CORE_ENABLE_DEBUG_LOG)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_CORE_CORE_ENABLE_DEBUG_LOG)
                #define ESP_BROOKESIA_CONF_CORE_CORE_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_CORE_CORE_ENABLE_DEBUG_LOG
            #else
                #define ESP_BROOKESIA_CONF_CORE_CORE_ENABLE_DEBUG_LOG  (0)
            #endif
        #endif
    #endif

    /**
     * Phone related
     */
    #if !defined(ESP_BROOKESIA_CONF_SYSTEMS_ENABLE_PHONE)
        #if defined(CONFIG_ESP_BROOKESIA_CONF_SYSTEMS_ENABLE_PHONE)
            #define ESP_BROOKESIA_CONF_SYSTEMS_ENABLE_PHONE  CONFIG_ESP_BROOKESIA_CONF_SYSTEMS_ENABLE_PHONE
        #else
            #define ESP_BROOKESIA_CONF_SYSTEMS_ENABLE_PHONE  (0)
        #endif
    #endif

    #if ESP_BROOKESIA_CONF_SYSTEMS_ENABLE_PHONE
        #if !defined(ESP_BROOKESIA_CONF_PHONE_ENABLE_DEBUG_LOG)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_PHONE_ENABLE_DEBUG_LOG)
                #define ESP_BROOKESIA_CONF_PHONE_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_PHONE_ENABLE_DEBUG_LOG
            #else
                #define ESP_BROOKESIA_CONF_PHONE_ENABLE_DEBUG_LOG  (0)
            #endif
        #endif

        #if ESP_BROOKESIA_CONF_PHONE_ENABLE_DEBUG_LOG
            #if !defined(ESP_BROOKESIA_CONF_PHONE_APP_ENABLE_DEBUG_LOG)
                #if defined(CONFIG_ESP_BROOKESIA_CONF_PHONE_APP_ENABLE_DEBUG_LOG)
                    #define ESP_BROOKESIA_CONF_PHONE_APP_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_PHONE_APP_ENABLE_DEBUG_LOG
                #else
                    #define ESP_BROOKESIA_CONF_PHONE_APP_ENABLE_DEBUG_LOG  (0)
                #endif
            #endif

            #if !defined(ESP_BROOKESIA_CONF_PHONE_DISPLAY_ENABLE_DEBUG_LOG)
                #if defined(CONFIG_ESP_BROOKESIA_CONF_PHONE_DISPLAY_ENABLE_DEBUG_LOG)
                    #define ESP_BROOKESIA_CONF_PHONE_DISPLAY_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_PHONE_DISPLAY_ENABLE_DEBUG_LOG
                #else
                    #define ESP_BROOKESIA_CONF_PHONE_DISPLAY_ENABLE_DEBUG_LOG  (0)
                #endif
            #endif

            #if !defined(ESP_BROOKESIA_CONF_PHONE_MANAGER_ENABLE_DEBUG_LOG)
                #if defined(CONFIG_ESP_BROOKESIA_CONF_PHONE_MANAGER_ENABLE_DEBUG_LOG)
                    #define ESP_BROOKESIA_CONF_PHONE_MANAGER_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_PHONE_MANAGER_ENABLE_DEBUG_LOG
                #else
                    #define ESP_BROOKESIA_CONF_PHONE_MANAGER_ENABLE_DEBUG_LOG  (0)
                #endif
            #endif

            #if !defined(ESP_BROOKESIA_CONF_PHONE_GESTURE_ENABLE_DEBUG_LOG)
                #if defined(CONFIG_ESP_BROOKESIA_CONF_PHONE_GESTURE_ENABLE_DEBUG_LOG)
                    #define ESP_BROOKESIA_CONF_PHONE_GESTURE_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_PHONE_GESTURE_ENABLE_DEBUG_LOG
                #else
                    #define ESP_BROOKESIA_CONF_PHONE_GESTURE_ENABLE_DEBUG_LOG  (0)
                #endif
            #endif

            #if !defined(ESP_BROOKESIA_CONF_PHONE_RECENTS_SCREEN_ENABLE_DEBUG_LOG)
                #if defined(CONFIG_ESP_BROOKESIA_CONF_PHONE_RECENTS_SCREEN_ENABLE_DEBUG_LOG)
                    #define ESP_BROOKESIA_CONF_PHONE_RECENTS_SCREEN_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_PHONE_RECENTS_SCREEN_ENABLE_DEBUG_LOG
                #else
                    #define ESP_BROOKESIA_CONF_PHONE_RECENTS_SCREEN_ENABLE_DEBUG_LOG  (0)
                #endif
            #endif

            #if !defined(ESP_BROOKESIA_CONF_PHONE_NAVIGATION_ENABLE_DEBUG_LOG)
                #if defined(CONFIG_ESP_BROOKESIA_CONF_PHONE_NAVIGATION_ENABLE_DEBUG_LOG)
                    #define ESP_BROOKESIA_CONF_PHONE_NAVIGATION_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_PHONE_NAVIGATION_ENABLE_DEBUG_LOG
                #else
                    #define ESP_BROOKESIA_CONF_PHONE_NAVIGATION_ENABLE_DEBUG_LOG  (0)
                #endif
            #endif

            #if !defined(ESP_BROOKESIA_CONF_PHONE_STATUS_BAR_ENABLE_DEBUG_LOG)
                #if defined(CONFIG_ESP_BROOKESIA_CONF_PHONE_STATUS_BAR_ENABLE_DEBUG_LOG)
                    #define ESP_BROOKESIA_CONF_PHONE_STATUS_BAR_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_PHONE_STATUS_BAR_ENABLE_DEBUG_LOG
                #else
                    #define ESP_BROOKESIA_CONF_PHONE_STATUS_BAR_ENABLE_DEBUG_LOG  (0)
                #endif
            #endif

            #if !defined(ESP_BROOKESIA_CONF_PHONE_PHONE_ENABLE_DEBUG_LOG)
                #if defined(CONFIG_ESP_BROOKESIA_CONF_PHONE_PHONE_ENABLE_DEBUG_LOG)
                    #define ESP_BROOKESIA_CONF_PHONE_PHONE_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_CONF_PHONE_PHONE_ENABLE_DEBUG_LOG
                #else
                    #define ESP_BROOKESIA_CONF_PHONE_PHONE_ENABLE_DEBUG_LOG  (0)
                #endif
            #endif
        #endif // ESP_BROOKESIA_CONF_PHONE_ENABLE_DEBUG_LOG

        #if !defined(ESP_BROOKESIA_CONF_PHONE_ENABLE_APP_EXAMPLES)
            #if defined(CONFIG_ESP_BROOKESIA_CONF_PHONE_ENABLE_APP_EXAMPLES)
                #define ESP_BROOKESIA_CONF_PHONE_ENABLE_APP_EXAMPLES  CONFIG_ESP_BROOKESIA_CONF_PHONE_ENABLE_APP_EXAMPLES
            #else
                #define ESP_BROOKESIA_CONF_PHONE_ENABLE_APP_EXAMPLES  (0)
            #endif
        #endif

        #if ESP_BROOKESIA_CONF_PHONE_ENABLE_APP_EXAMPLES
            #if !defined(ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_COMPLEX_CONF)
                #if defined(CONFIG_ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_COMPLEX_CONF)
                    #define ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_COMPLEX_CONF  CONFIG_ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_COMPLEX_CONF
                #else
                    #define ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_COMPLEX_CONF  (0)
                #endif
            #endif

            #if !defined(ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_SIMPLE_CONF)
                #if defined(CONFIG_ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_SIMPLE_CONF)
                    #define ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_SIMPLE_CONF  CONFIG_ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_SIMPLE_CONF
                #else
                    #define ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_SIMPLE_CONF  (0)
                #endif
            #endif

            #if !defined(ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_SQUARELINE)
                #if defined(CONFIG_ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_SQUARELINE)
                    #define ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_SQUARELINE  CONFIG_ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_SQUARELINE
                #else
                    #define ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_SQUARELINE  (0)
                #endif
            #endif
        #endif // ESP_BROOKESIA_CONF_PHONE_ENABLE_APP_EXAMPLES
    #endif // ESP_BROOKESIA_CONF_SYSTEMS_ENABLE_PHONE

#endif // ESP_BROOKESIA_CONF_ENABLE_SYSTEMS

// *INDENT-ON*
