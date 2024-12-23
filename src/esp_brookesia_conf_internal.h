/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

// *INDENT-OFF*
/* Handle special Kconfig options */
#ifndef ESP_BROOKESIA_KCONFIG_IGNORE
    #include "sdkconfig.h"
    #ifdef CONFIG_ESP_BROOKESIA_CONF_SKIP
        #define ESP_BROOKESIA_CONF_SKIP
    #endif
#endif

#include "core/esp_brookesia_core_type.h"

/* If "esp_brookesia_conf.h" is not skipped, include it */
#ifndef ESP_BROOKESIA_CONF_SKIP
    #ifdef __has_include                                /* If "esp_brookesia_conf.h" is available from here, try to use it later */
        #if __has_include("esp_brookesia_conf.h")
            #ifndef ESP_BROOKESIA_CONF_INCLUDE_SIMPLE
                #define ESP_BROOKESIA_CONF_INCLUDE_SIMPLE
            #endif
        #elif __has_include("../../esp_brookesia_conf.h")
            #ifndef ESP_BROOKESIA_CONF_INCLUDE_OUTSIDE
                #define ESP_BROOKESIA_CONF_INCLUDE_OUTSIDE
            #endif
        #else
            #define ESP_BROOKESIA_CONF_INCLUDE_INSIDE
        #endif
    #endif

    #ifdef ESP_BROOKESIA_CONF_PATH                             /* If there is a path defined for "esp_brookesia_conf.h" use it */
        #define __TO_STR_AUX(x) #x
        #define __TO_STR(x) __TO_STR_AUX(x)
        #include __TO_STR(ESP_BROOKESIA_CONF_PATH)
        #undef __TO_STR_AUX
        #undef __TO_STR
    #elif defined(ESP_BROOKESIA_CONF_INCLUDE_SIMPLE)           /* Or simply include if "esp_brookesia_conf.h" is available */
        #include "esp_brookesia_conf.h"
    #elif defined(ESP_BROOKESIA_CONF_INCLUDE_OUTSIDE)          /* Or include if "../../ESP_Panel_Conf.h" is available */
        #include "../../esp_brookesia_conf.h"
    #elif defined(ESP_BROOKESIA_CONF_INCLUDE_INSIDE)           /* Or include the default configuration */
        #include "../esp_brookesia_conf.h"
    #endif
#endif

#ifndef ESP_BROOKESIA_CONF_INCLUDE_INSIDE
    /**
     * There are two purposes to include the this file:
     *  1. Convert configuration items starting with `CONFIG_` to the required configuration items.
     *  2. Define default values for configuration items that are not defined to keep compatibility.
     *
     */
    #include "esp_brookesia_conf_kconfig.h"
#endif

#if ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_HELPERS
// Check if multiple Squareline and LVGL versions are defined
#if defined(ESP_BROOKESIA_SQ1_3_4_LV8_2_0) + defined(ESP_BROOKESIA_SQ1_3_4_LV8_3_3) + defined(ESP_BROOKESIA_SQ1_3_4_LV8_3_4) + \
    defined(ESP_BROOKESIA_SQ1_3_4_LV8_3_6) + defined(ESP_BROOKESIA_SQ1_4_0_LV8_3_6) + defined(ESP_BROOKESIA_SQ1_4_0_LV8_3_11) + \
    defined(ESP_BROOKESIA_SQ1_4_1_LV8_3_6) + defined(ESP_BROOKESIA_SQ1_4_1_LV8_3_11) > 1
    #error "Multiple Squareline and LVGL versions are defined"
#endif
#endif
