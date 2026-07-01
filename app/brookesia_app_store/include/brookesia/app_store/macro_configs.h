/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/system_core/macro_configs.h"

#if !defined(BROOKESIA_APP_STORE_RESOURCE_DIR)
#   define BROOKESIA_APP_STORE_RESOURCE_DIR "brookesia.general.app_store"
#endif

#if !defined(BROOKESIA_APP_STORE_PACKAGE_DIR)
#   define BROOKESIA_APP_STORE_PACKAGE_DIR ""
#endif

#if !defined(BROOKESIA_APP_STORE_SERVER_ROOT_URL)
#   define BROOKESIA_APP_STORE_SERVER_ROOT_URL "https://brookesia-app-store.espressif.com/api/v1"
#endif

#if !defined(BROOKESIA_APP_STORE_INDEX_URL)
#   define BROOKESIA_APP_STORE_INDEX_URL ""
#endif

#if !defined(BROOKESIA_APP_STORE_LIST_PAGE_SIZE)
#   if defined(CONFIG_BROOKESIA_APP_STORE_LIST_PAGE_SIZE)
#       define BROOKESIA_APP_STORE_LIST_PAGE_SIZE CONFIG_BROOKESIA_APP_STORE_LIST_PAGE_SIZE
#   else
#       define BROOKESIA_APP_STORE_LIST_PAGE_SIZE (5)
#   endif
#endif

#if !defined(BROOKESIA_APP_STORE_LOG_TAG)
#   define BROOKESIA_APP_STORE_LOG_TAG "AppStore"
#endif

#if !defined(BROOKESIA_APP_STORE_VER_MAJOR)
#   define BROOKESIA_APP_STORE_VER_MAJOR (0)
#endif

#if !defined(BROOKESIA_APP_STORE_VER_MINOR)
#   define BROOKESIA_APP_STORE_VER_MINOR (8)
#endif

#if !defined(BROOKESIA_APP_STORE_VER_PATCH)
#   define BROOKESIA_APP_STORE_VER_PATCH (0)
#endif
