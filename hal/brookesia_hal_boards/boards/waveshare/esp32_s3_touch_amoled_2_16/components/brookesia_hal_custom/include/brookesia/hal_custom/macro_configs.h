/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"

/**
 * @brief Default log tag used by the component.
 */
#define BROOKESIA_HAL_CUSTOM_LOG_TAG "HalCustom"

#if !defined(BROOKESIA_HAL_CUSTOM_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_HAL_CUSTOM_ENABLE_DEBUG_LOG)
#       define BROOKESIA_HAL_CUSTOM_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_HAL_CUSTOM_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_HAL_CUSTOM_ENABLE_DEBUG_LOG  (0)
#   endif
#endif
