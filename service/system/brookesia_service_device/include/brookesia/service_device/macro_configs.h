/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#if defined(ESP_PLATFORM)
#   include "sdkconfig.h"
#endif

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

#if !defined(BROOKESIA_SERVICE_DEVICE_ENABLE_WORKER)
#   if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_ENABLE_WORKER)
#       define BROOKESIA_SERVICE_DEVICE_ENABLE_WORKER  CONFIG_BROOKESIA_SERVICE_DEVICE_ENABLE_WORKER
#   else
#       define BROOKESIA_SERVICE_DEVICE_ENABLE_WORKER  (0)
#   endif
#endif

#if BROOKESIA_SERVICE_DEVICE_ENABLE_WORKER
#   if !defined(BROOKESIA_SERVICE_DEVICE_WORKER_NAME)
#      if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_NAME)
#          define BROOKESIA_SERVICE_DEVICE_WORKER_NAME  CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_NAME
#      else
#          define BROOKESIA_SERVICE_DEVICE_WORKER_NAME  "SvcDeviceWorker"
#      endif
#   endif

#   if !defined(BROOKESIA_SERVICE_DEVICE_WORKER_CORE_ID)
#      if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_ENABLE_WORKER)
#          define BROOKESIA_SERVICE_DEVICE_WORKER_CORE_ID  CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_CORE_ID
#      else
#          define BROOKESIA_SERVICE_DEVICE_WORKER_CORE_ID  (-1)
#      endif
#   endif

#   if !defined(BROOKESIA_SERVICE_DEVICE_WORKER_PRIORITY)
#      if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_PRIORITY)
#          define BROOKESIA_SERVICE_DEVICE_WORKER_PRIORITY  CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_PRIORITY
#      else
#          define BROOKESIA_SERVICE_DEVICE_WORKER_PRIORITY  (5)
#      endif
#   endif

#   if !defined(BROOKESIA_SERVICE_DEVICE_WORKER_STACK_SIZE)
#      if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_STACK_SIZE)
#          define BROOKESIA_SERVICE_DEVICE_WORKER_STACK_SIZE  CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_STACK_SIZE
#      else
#          define BROOKESIA_SERVICE_DEVICE_WORKER_STACK_SIZE  (6144)
#      endif
#   endif

#   if !defined(BROOKESIA_SERVICE_DEVICE_WORKER_POLL_INTERVAL_MS)
#      if defined(CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_POLL_INTERVAL_MS)
#          define BROOKESIA_SERVICE_DEVICE_WORKER_POLL_INTERVAL_MS  CONFIG_BROOKESIA_SERVICE_DEVICE_WORKER_POLL_INTERVAL_MS
#      else
#          define BROOKESIA_SERVICE_DEVICE_WORKER_POLL_INTERVAL_MS  (10)
#      endif
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
