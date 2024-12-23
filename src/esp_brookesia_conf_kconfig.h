/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

// *INDENT-OFF*
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////// Debug /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef ESP_BROOKESIA_CHECK_RESULT_ASSERT
    #ifdef CONFIG_ESP_BROOKESIA_CHECK_RESULT_ASSERT
        #define ESP_BROOKESIA_CHECK_RESULT_ASSERT     (CONFIG_ESP_BROOKESIA_CHECK_RESULT_ASSERT)
    #else
        #define ESP_BROOKESIA_CHECK_RESULT_ASSERT     (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_STYLE
    #ifdef CONFIG_ESP_BROOKESIA_LOG_STYLE
        #define ESP_BROOKESIA_LOG_STYLE        (CONFIG_ESP_BROOKESIA_LOG_STYLE)
    #else
        #define ESP_BROOKESIA_LOG_STYLE        (ESP_BROOKESIA_LOG_STYLE_STD)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_LEVEL
    #ifdef CONFIG_ESP_BROOKESIA_LOG_LEVEL
        #define ESP_BROOKESIA_LOG_LEVEL        (CONFIG_ESP_BROOKESIA_LOG_LEVEL)
    #else
        #define ESP_BROOKESIA_LOG_LEVEL        (ESP_BROOKESIA_LOG_LEVEL_INFO)
    #endif
#endif

/* Enable debug logs for modules */
#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE        (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE        (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS     (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS     (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE       (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE       (0)
    #endif
#endif

// Core
#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_APP
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_APP
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_APP            (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_APP)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_APP            (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_HOME
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_HOME
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_HOME           (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_HOME)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_HOME           (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_MANAGER
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_MANAGER
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_MANAGER        (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_MANAGER)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_MANAGER        (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_CORE
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_CORE
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_CORE           (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_CORE)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_CORE           (0)
    #endif
#endif

// Widgets
#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_APP_LAUNCHER
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_APP_LAUNCHER
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_APP_LAUNCHER   (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_APP_LAUNCHER)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_APP_LAUNCHER   (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_RECENTS_SCREEN
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_RECENTS_SCREEN
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_RECENTS_SCREEN   (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_RECENTS_SCREEN)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_RECENTS_SCREEN   (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_GESTURE
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_GESTURE
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_GESTURE     (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_GESTURE)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_GESTURE     (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_NAVIGATION
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_NAVIGATION
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_NAVIGATION  (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_NAVIGATION)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_NAVIGATION  (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_STATUS_BAR
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_STATUS_BAR
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_STATUS_BAR  (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_STATUS_BAR)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_STATUS_BAR  (0)
    #endif
#endif

// Phone
#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_APP
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_APP
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_APP           (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_APP)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_APP           (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_HOME
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_HOME
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_HOME          (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_HOME)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_HOME          (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_MANAGER
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_MANAGER
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_MANAGER       (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_MANAGER)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_MANAGER       (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_PHONE
    #ifdef CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_PHONE
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_PHONE         (CONFIG_ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_PHONE)
    #else
        #define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_PHONE         (0)
    #endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////// Memory /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef ESP_BROOKESIA_MEMORY_USE_CUSTOM
    #ifdef CONFIG_ESP_BROOKESIA_MEMORY_USE_CUSTOM
        #define ESP_BROOKESIA_MEMORY_USE_CUSTOM     (CONFIG_ESP_BROOKESIA_MEMORY_USE_CUSTOM)
    #else
        #define ESP_BROOKESIA_MEMORY_USE_CUSTOM     (0)
    #endif
#endif

#if ESP_BROOKESIA_MEMORY_USE_CUSTOM
    #ifdef CONFIG_ESP_BROOKESIA_MEMORY_INCLUDE
        #define ESP_BROOKESIA_MEMORY_INCLUDE   CONFIG_ESP_BROOKESIA_MEMORY_INCLUDE
    #else
        #error "Unknown memory include"
    #endif

    #ifdef ESP_BROOKESIA_MEMORY_CUSTOM_MALLOC
        #define ESP_BROOKESIA_MEMORY_MALLOC   ESP_BROOKESIA_MEMORY_CUSTOM_MALLOC
    #else
        #error "Unknown memory malloc"
    #endif

    #ifdef ESP_BROOKESIA_MEMORY_CUSTOM_FREE
        #define ESP_BROOKESIA_MEMORY_FREE       ESP_BROOKESIA_MEMORY_CUSTOM_FREE
    #else
        // #error "Unknown memory free"
        #define ESP_BROOKESIA_MEMORY_FREE       free
    #endif
#else
    #ifndef ESP_BROOKESIA_MEMORY_INCLUDE
        #define ESP_BROOKESIA_MEMORY_INCLUDE   <stdlib.h>
    #endif
    #ifndef ESP_BROOKESIA_MEMORY_MALLOC
        #define ESP_BROOKESIA_MEMORY_MALLOC    malloc
    #endif
    #ifndef ESP_BROOKESIA_MEMORY_FREE
        #define ESP_BROOKESIA_MEMORY_FREE      free
    #endif
#endif /* ESP_BROOKESIA_MEMORY_USE_CUSTOM */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// Squareline ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_COMP
    #ifdef CONFIG_ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_COMP
        #define ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_COMP   (CONFIG_ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_COMP)
    #else
        #define ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_COMP   (0)
    #endif
#endif

#ifndef ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_HELPERS
    #ifdef CONFIG_ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_HELPERS
        #define ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_HELPERS   (CONFIG_ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_HELPERS)
    #else
        #define ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_HELPERS   (0)
    #endif
#endif

#if !defined(ESP_BROOKESIA_SQ1_3_4_LV8_2_0) && defined(CONFIG_ESP_BROOKESIA_SQ1_3_4_LV8_2_0)
    #define ESP_BROOKESIA_SQ1_3_4_LV8_2_0
#endif

#if !defined(ESP_BROOKESIA_SQ1_3_4_LV8_3_3) && defined(CONFIG_ESP_BROOKESIA_SQ1_3_4_LV8_3_3)
    #define ESP_BROOKESIA_SQ1_3_4_LV8_3_3
#endif

#if !defined(ESP_BROOKESIA_SQ1_3_4_LV8_3_4) && defined(CONFIG_ESP_BROOKESIA_SQ1_3_4_LV8_3_4)
    #define ESP_BROOKESIA_SQ1_3_4_LV8_3_4
#endif

#if !defined(ESP_BROOKESIA_SQ1_3_4_LV8_3_6) && defined(CONFIG_ESP_BROOKESIA_SQ1_3_4_LV8_3_6)
    #define ESP_BROOKESIA_SQ1_3_4_LV8_3_6
#endif

#if !defined(ESP_BROOKESIA_SQ1_4_0_LV8_3_6) && defined(CONFIG_ESP_BROOKESIA_SQ1_4_0_LV8_3_6)
    #define ESP_BROOKESIA_SQ1_4_0_LV8_3_6
#endif

#if !defined(ESP_BROOKESIA_SQ1_4_0_LV8_3_11) && defined(CONFIG_ESP_BROOKESIA_SQ1_4_0_LV8_3_11)
    #define ESP_BROOKESIA_SQ1_4_0_LV8_3_11
#endif

#if !defined(ESP_BROOKESIA_SQ1_4_1_LV8_3_6) && defined(CONFIG_ESP_BROOKESIA_SQ1_4_1_LV8_3_6)
    #define ESP_BROOKESIA_SQ1_4_1_LV8_3_6
#endif

#if !defined(ESP_BROOKESIA_SQ1_4_1_LV8_3_11) && defined(CONFIG_ESP_BROOKESIA_SQ1_4_1_LV8_3_11)
    #define ESP_BROOKESIA_SQ1_4_1_LV8_3_11
#endif
