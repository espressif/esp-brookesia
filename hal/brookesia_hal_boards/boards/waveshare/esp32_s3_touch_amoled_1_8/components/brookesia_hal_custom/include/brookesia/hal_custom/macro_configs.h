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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////// Display //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Backlight */
#if !defined(BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT)
#   if defined(CONFIG_BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT)
#       define BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT  CONFIG_BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT
#   else
#       define BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT  (90)
#   endif
#endif

#if !defined(BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN)
#   if defined(CONFIG_BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN)
#       define BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN  CONFIG_BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN
#   else
#       define BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN  (20)
#   endif
#endif

#if !defined(BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX)
#   if defined(CONFIG_BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX)
#       define BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX  CONFIG_BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX
#   else
#       define BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX  (100)
#   endif
#endif
