/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"

/**
 * @file macro_configs.h
 * @brief Build-time macro configuration for the HAL interface component.
 */

/**
 * @brief Default log tag for HAL interface logs.
 */
#define BROOKESIA_HAL_INTERFACE_LOG_TAG "HalIface"

#if !defined(BROOKESIA_HAL_INTERFACE_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_HAL_INTERFACE_ENABLE_DEBUG_LOG)
#       define BROOKESIA_HAL_INTERFACE_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_HAL_INTERFACE_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_HAL_INTERFACE_ENABLE_DEBUG_LOG  (0)
#   endif
#endif
