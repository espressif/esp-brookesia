/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_brookesia_internal.h"

#if !ESP_BROOKESIA_ENABLE_SERVICES
#   error "Services is not enabled, please enable it in the menuconfig"
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// Storage NVS ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(ESP_BROOKESIA_SERVICES_ENABLE_STORAGE_NVS)
#   if defined(CONFIG_ESP_BROOKESIA_SERVICES_ENABLE_STORAGE_NVS)
#       define ESP_BROOKESIA_SERVICES_ENABLE_STORAGE_NVS  CONFIG_ESP_BROOKESIA_SERVICES_ENABLE_STORAGE_NVS
#   else
#       define ESP_BROOKESIA_SERVICES_ENABLE_STORAGE_NVS  (0)
#   endif
#endif

#if ESP_BROOKESIA_SERVICES_ENABLE_STORAGE_NVS
#   if !defined(ESP_BROOKESIA_STORAGE_NVS_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_ESP_BROOKESIA_STORAGE_NVS_ENABLE_DEBUG_LOG)
#           define ESP_BROOKESIA_STORAGE_NVS_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_STORAGE_NVS_ENABLE_DEBUG_LOG
#       else
#           define ESP_BROOKESIA_STORAGE_NVS_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#endif
