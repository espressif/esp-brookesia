/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"

#if !defined(BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER)
#       define BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER  CONFIG_BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER
#   else
#       define BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER  (0)
#   endif
#endif

#if BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER
#   if !defined(BROOKESIA_SERVICE_DEVICE_PLUGIN_SYMBOL)
#       define BROOKESIA_SERVICE_DEVICE_PLUGIN_SYMBOL  service_device_symbol
#   endif
#endif

#define BROOKESIA_SERVICE_DEVICE_LOG_TAG "SvcDevice"

#if !defined(BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG)
#       define BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG  (0)
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// Worker ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(BROOKESIA_SERVICE_DEVICE_WORKER_NAME)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_NAME)
#       define BROOKESIA_SERVICE_DEVICE_WORKER_NAME  CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_NAME
#   else
#       define BROOKESIA_SERVICE_DEVICE_WORKER_NAME  "SvcDeviceWorker"
#   endif
#endif

#if !defined(BROOKESIA_SERVICE_DEVICE_WORKER_PRIORITY)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_PRIORITY)
#       define BROOKESIA_SERVICE_DEVICE_WORKER_PRIORITY  CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_PRIORITY
#   else
#       define BROOKESIA_SERVICE_DEVICE_WORKER_PRIORITY  (5)
#   endif
#endif

#if !defined(BROOKESIA_SERVICE_DEVICE_WORKER_STACK_SIZE)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_STACK_SIZE)
#       define BROOKESIA_SERVICE_DEVICE_WORKER_STACK_SIZE  CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_STACK_SIZE
#   else
#       define BROOKESIA_SERVICE_DEVICE_WORKER_STACK_SIZE  (6144)
#   endif
#endif

#if !defined(BROOKESIA_SERVICE_DEVICE_WORKER_POLL_INTERVAL_MS)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_POLL_INTERVAL_MS)
#       define BROOKESIA_SERVICE_DEVICE_WORKER_POLL_INTERVAL_MS  CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_POLL_INTERVAL_MS
#   else
#       define BROOKESIA_SERVICE_DEVICE_WORKER_POLL_INTERVAL_MS  (10)
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////// Persistent Parameters ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(BROOKESIA_SERVICE_DEVICE_NVS_SAVE_DATA_TIMEOUT_MS)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_NVS_SAVE_DATA_TIMEOUT_MS)
#       define BROOKESIA_SERVICE_DEVICE_NVS_SAVE_DATA_TIMEOUT_MS  CONFIG_BROOKESIA_SERVICE_DEVICE_NVS_SAVE_DATA_TIMEOUT_MS
#   else
#       define BROOKESIA_SERVICE_DEVICE_NVS_SAVE_DATA_TIMEOUT_MS  (20)
#   endif
#endif

#if !defined(BROOKESIA_SERVICE_DEVICE_NVS_ERASE_DATA_TIMEOUT_MS)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_NVS_ERASE_DATA_TIMEOUT_MS)
#       define BROOKESIA_SERVICE_DEVICE_NVS_ERASE_DATA_TIMEOUT_MS  CONFIG_BROOKESIA_SERVICE_DEVICE_NVS_ERASE_DATA_TIMEOUT_MS
#   else
#       define BROOKESIA_SERVICE_DEVICE_NVS_ERASE_DATA_TIMEOUT_MS  (20)
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////// Power Battery ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(BROOKESIA_SERVICE_DEVICE_POWER_BATTERY_POLL_INTERVAL_MS)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_POWER_BATTERY_POLL_INTERVAL_MS)
#       define BROOKESIA_SERVICE_DEVICE_POWER_BATTERY_POLL_INTERVAL_MS  CONFIG_BROOKESIA_SERVICE_DEVICE_POWER_BATTERY_POLL_INTERVAL_MS
#   else
#       define BROOKESIA_SERVICE_DEVICE_POWER_BATTERY_POLL_INTERVAL_MS  (1000)
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////// Audio ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_DEFAULT)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_DEFAULT)
#       define BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_DEFAULT  CONFIG_BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_DEFAULT
#   else
#       define BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_DEFAULT  (75)
#   endif
#endif
#if !defined(BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MIN)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MIN)
#       define BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MIN  CONFIG_BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MIN
#   else
#       define BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MIN  (0)
#   endif
#endif
#if !defined(BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MAX)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MAX)
#       define BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MAX  CONFIG_BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MAX
#   else
#       define BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MAX  (100)
#   endif
#endif
#if !defined(BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_DEFAULT_MUTE)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_DEFAULT_MUTE)
#       define BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_DEFAULT_MUTE  CONFIG_BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_DEFAULT_MUTE
#   else
#       define BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_DEFAULT_MUTE  (0)
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////// Display //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT)
#       define BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT  CONFIG_BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT
#   else
#       define BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT  (90)
#   endif
#endif
#if !defined(BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN)
#       define BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN  CONFIG_BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN
#   else
#       define BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN  (20)
#   endif
#endif
#if !defined(BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX)
#       define BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX  CONFIG_BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX
#   else
#       define BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX  (100)
#   endif
#endif
#if !defined(BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_DEFAULT_ON)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_DEFAULT_ON)
#       define BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_DEFAULT_ON  CONFIG_BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_DEFAULT_ON
#   else
#       define BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_DEFAULT_ON  (0)
#   endif
#endif
