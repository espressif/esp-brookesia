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

    #ifdef CONFIG_ESP_BROOKESIA_CONF_FILE_SKIP
        #define ESP_BROOKESIA_CONF_FILE_SKIP
    #endif
#endif

#ifndef ESP_BROOKESIA_CONF_FILE_SKIP
    /* Try to locate the configuration file in different paths */
    #if __has_include("esp_brookesia_conf.h")
        #define ESP_BROOKESIA_INCLUDE_SIMPLE
    #elif __has_include("../../../esp_brookesia_conf.h")
        #define ESP_BROOKESIA_INCLUDE_OUTSIDE
    #else
        #define ESP_BROOKESIA_INCLUDE_INSIDE
    #endif

    /* Include the configuration file based on the path found */
    #ifdef ESP_BROOKESIA_CONF_PATH
        /* Use the custom path if defined */
        #define __TO_STR_AUX(x) #x
        #define __TO_STR(x) __TO_STR_AUX(x)
        #include __TO_STR(ESP_BROOKESIA_PATH)
        #undef __TO_STR_AUX
        #undef __TO_STR
    #elif defined(ESP_BROOKESIA_INCLUDE_SIMPLE)
        #include "esp_brookesia_conf.h"
    #elif defined(ESP_BROOKESIA_INCLUDE_OUTSIDE)
        #include "../../esp_brookesia_conf.h"
    #elif defined(ESP_BROOKESIA_INCLUDE_INSIDE)
        #include "../esp_brookesia_conf.h"
    #endif

    #include "esp_brookesia_versions.h"

    /* Check version compatibility */
    #if ESP_BROOKESIA_CONF_FILE_VER_MAJOR != ESP_BROOKESIA_CONF_VER_MAJOR
        #error "The file `esp_brookesia_conf.h` version is not compatible. Please update it with the file from the library"
    #elif ESP_BROOKESIA_CONF_FILE_VER_MINOR < ESP_BROOKESIA_CONF_VER_MINOR
        #warning "The file `esp_brookesia_conf.h` version is outdated. Some new configurations are missing"
    #elif ESP_BROOKESIA_CONF_FILE_VER_MINOR > ESP_BROOKESIA_CONF_VER_MINOR
        #warning "The file `esp_brookesia_conf.h` version is newer than the library. Some new configurations are not supported"
    #endif
#endif // ESP_BROOKESIA_FILE_SKIP

#ifndef ESP_BROOKESIA_CONF_INCLUDE_INSIDE
    /**
     * There are two purposes to include the this file:
     *  1. Convert configuration items starting with `CONFIG_` to the required configuration items.
     *  2. Define default values for configuration items that are not defined to keep compatibility.
     *
     */
    #include "esp_brookesia_conf_kconfig.h"
#endif

// *INDENT-ON*
