/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/system_core/macro_configs.h"

#if !defined(BROOKESIA_APP_SETTINGS_RESOURCE_DIR)
#   define BROOKESIA_APP_SETTINGS_RESOURCE_DIR "brookesia.general.settings"
#endif

#if !defined(BROOKESIA_APP_SETTINGS_PACKAGE_DIR)
#   define BROOKESIA_APP_SETTINGS_PACKAGE_DIR ""
#endif

#if !defined(BROOKESIA_APP_SETTINGS_WIFI_MAX_VISIBLE_APS)
#   if defined(CONFIG_BROOKESIA_APP_SETTINGS_WIFI_MAX_VISIBLE_APS)
#       define BROOKESIA_APP_SETTINGS_WIFI_MAX_VISIBLE_APS CONFIG_BROOKESIA_APP_SETTINGS_WIFI_MAX_VISIBLE_APS
#   else
#       define BROOKESIA_APP_SETTINGS_WIFI_MAX_VISIBLE_APS (5)
#   endif
#endif

#if !defined(BROOKESIA_APP_SETTINGS_LOG_TAG)
#   define BROOKESIA_APP_SETTINGS_LOG_TAG "AppSettings"
#endif

#if !defined(BROOKESIA_APP_SETTINGS_VER_MAJOR)
#   define BROOKESIA_APP_SETTINGS_VER_MAJOR (0)
#endif

#if !defined(BROOKESIA_APP_SETTINGS_VER_MINOR)
#   define BROOKESIA_APP_SETTINGS_VER_MINOR (8)
#endif

#if !defined(BROOKESIA_APP_SETTINGS_VER_PATCH)
#   define BROOKESIA_APP_SETTINGS_VER_PATCH (0)
#endif

/**
 * @brief Enable the Settings app memory/heap trace instrumentation.
 *
 * Profiling-only switch (defaults to disabled). When non-zero, the Wi-Fi list sync path
 * emits [HeapTrace] logs describing slot accounting. The functional Wi-Fi behavior is
 * unchanged regardless of this switch.
 */
#if !defined(BROOKESIA_APP_SETTINGS_ENABLE_MEMORY_TRACE)
#   if defined(CONFIG_BROOKESIA_APP_SETTINGS_ENABLE_MEMORY_TRACE)
#       define BROOKESIA_APP_SETTINGS_ENABLE_MEMORY_TRACE CONFIG_BROOKESIA_APP_SETTINGS_ENABLE_MEMORY_TRACE
#   else
#       define BROOKESIA_APP_SETTINGS_ENABLE_MEMORY_TRACE (0)
#   endif
#endif
