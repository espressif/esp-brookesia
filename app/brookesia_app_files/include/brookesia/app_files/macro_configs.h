/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#if defined(ESP_PLATFORM)
#   include "sdkconfig.h"
#endif

#if !defined(BROOKESIA_APP_FILES_RESOURCE_DIR)
#   define BROOKESIA_APP_FILES_RESOURCE_DIR "brookesia.general.files"
#endif

#if !defined(BROOKESIA_APP_FILES_PACKAGE_DIR)
#   define BROOKESIA_APP_FILES_PACKAGE_DIR ""
#endif

#if !defined(BROOKESIA_APP_FILES_ENABLE_PRELOAD_DOM)
#   if defined(CONFIG_BROOKESIA_APP_FILES_ENABLE_PRELOAD_DOM)
#       define BROOKESIA_APP_FILES_ENABLE_PRELOAD_DOM CONFIG_BROOKESIA_APP_FILES_ENABLE_PRELOAD_DOM
#   elif defined(ESP_PLATFORM)
#       define BROOKESIA_APP_FILES_ENABLE_PRELOAD_DOM (0)
#   else
#       define BROOKESIA_APP_FILES_ENABLE_PRELOAD_DOM (1)
#   endif
#endif

#if !defined(BROOKESIA_APP_FILES_LOG_TAG)
#   define BROOKESIA_APP_FILES_LOG_TAG "FileMgr"
#endif
