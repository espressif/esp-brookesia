/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_brookesia_conf_internal.h"

/* Library Version */
#if !defined(ESP_BROOKESIA_VER_MAJOR) && !defined(ESP_BROOKESIA_VER_MINOR) && !defined(ESP_BROOKESIA_VER_PATCH)
#define ESP_BROOKESIA_VER_MAJOR 0
#define ESP_BROOKESIA_VER_MINOR 4
#define ESP_BROOKESIA_VER_PATCH 1
#endif

/* File `esp_brookesia_conf.h` */
#define ESP_BROOKESIA_CONF_VER_MAJOR 0
#define ESP_BROOKESIA_CONF_VER_MINOR 2
#define ESP_BROOKESIA_CONF_VER_PATCH 0

#ifndef ESP_BROOKESIA_CONF_SKIP
/* Check if the current configuration file version is compatible with the library version */
// File `esp_brookesia_conf.h`
// If the version is not defined, set it to `0.1.0`
#if !defined(ESP_BROOKESIA_CONF_FILE_VER_MAJOR) && !defined(ESP_BROOKESIA_CONF_FILE_VER_MINOR) && \
        !defined(ESP_BROOKESIA_CONF_FILE_VER_PATCH)
#define ESP_BROOKESIA_CONF_FILE_VER_MAJOR 0
#define ESP_BROOKESIA_CONF_FILE_VER_MINOR 1
#define ESP_BROOKESIA_CONF_FILE_VER_PATCH 0
#endif
// Check if the current configuration file version is compatible with the library version
#if ESP_BROOKESIA_CONF_FILE_VER_MAJOR != ESP_BROOKESIA_CONF_VER_MAJOR
#error "The file `esp_brookesia_conf.h` version is not compatible. Please update it with the file from the library"
#elif ESP_BROOKESIA_CONF_FILE_VER_MINOR < ESP_BROOKESIA_CONF_VER_MINOR
#warning "The file `esp_brookesia_conf.h` version is outdated. Some new configurations are missing"
#elif ESP_BROOKESIA_CONF_FILE_VER_MINOR > ESP_BROOKESIA_CONF_VER_MINOR
#warning "The file `esp_brookesia_conf.h` version is newer than the library. Some new configurations are not supported"
#endif /* ESP_BROOKESIA_CONF_INCLUDE_INSIDE */
#endif /* ESP_BROOKESIA_CONF_SKIP */
