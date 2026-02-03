/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"
#include "brookesia/service_manager/macro_configs.h"

/* Debug log */
#define BROOKESIA_SERVICE_NVS_LOG_TAG "SvcNVS"

/* Auto register */
#if !defined(BROOKESIA_SERVICE_NVS_ENABLE_AUTO_REGISTER)
#   if defined(CONFIG_BROOKESIA_SERVICE_NVS_ENABLE_AUTO_REGISTER)
#       define BROOKESIA_SERVICE_NVS_ENABLE_AUTO_REGISTER  CONFIG_BROOKESIA_SERVICE_NVS_ENABLE_AUTO_REGISTER
#   else
#       define BROOKESIA_SERVICE_NVS_ENABLE_AUTO_REGISTER  (0)
#   endif
#endif

/* Plugin symbol */
#if BROOKESIA_SERVICE_NVS_ENABLE_AUTO_REGISTER
#   if !defined(BROOKESIA_SERVICE_NVS_PLUGIN_SYMBOL)
#       define BROOKESIA_SERVICE_NVS_PLUGIN_SYMBOL  service_nvs_symbol
#   endif
#endif

#if !defined(BROOKESIA_SERVICE_NVS_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_SERVICE_NVS_ENABLE_DEBUG_LOG)
#       define BROOKESIA_SERVICE_NVS_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_SERVICE_NVS_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_SERVICE_NVS_ENABLE_DEBUG_LOG  (0)
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Worker /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT
#   if !defined(BROOKESIA_SERVICE_NVS_WORKER_NAME)
#      if defined(CONFIG_BROOKESIA_SERVICE_NVS_WORKER_NAME)
#          define BROOKESIA_SERVICE_NVS_WORKER_NAME  CONFIG_BROOKESIA_SERVICE_NVS_WORKER_NAME
#      else
#          define BROOKESIA_SERVICE_NVS_WORKER_NAME  "SvcNvsWorker"
#      endif
#   endif
#   if !defined(BROOKESIA_SERVICE_NVS_WORKER_CORE_ID)
#      if defined(CONFIG_BROOKESIA_SERVICE_NVS_WORKER_CORE_ID)
#          define BROOKESIA_SERVICE_NVS_WORKER_CORE_ID  CONFIG_BROOKESIA_SERVICE_NVS_WORKER_CORE_ID
#      else
#          define BROOKESIA_SERVICE_NVS_WORKER_CORE_ID  (-1)
#      endif
#   endif
#   if !defined(BROOKESIA_SERVICE_NVS_WORKER_PRIORITY)
#      if defined(CONFIG_BROOKESIA_SERVICE_NVS_WORKER_PRIORITY)
#          define BROOKESIA_SERVICE_NVS_WORKER_PRIORITY  CONFIG_BROOKESIA_SERVICE_NVS_WORKER_PRIORITY
#      else
#          define BROOKESIA_SERVICE_NVS_WORKER_PRIORITY  (5)
#      endif
#   endif
#   if !defined(BROOKESIA_SERVICE_NVS_WORKER_STACK_SIZE)
#      if defined(CONFIG_BROOKESIA_SERVICE_NVS_WORKER_STACK_SIZE)
#          define BROOKESIA_SERVICE_NVS_WORKER_STACK_SIZE  CONFIG_BROOKESIA_SERVICE_NVS_WORKER_STACK_SIZE
#      else
#          define BROOKESIA_SERVICE_NVS_WORKER_STACK_SIZE  (5120)
#      endif
#   endif
#   if !defined(BROOKESIA_SERVICE_NVS_WORKER_POLL_INTERVAL_MS)
#      if defined(CONFIG_BROOKESIA_SERVICE_NVS_WORKER_POLL_INTERVAL_MS)
#          define BROOKESIA_SERVICE_NVS_WORKER_POLL_INTERVAL_MS  CONFIG_BROOKESIA_SERVICE_NVS_WORKER_POLL_INTERVAL_MS
#      else
#          define BROOKESIA_SERVICE_NVS_WORKER_POLL_INTERVAL_MS  (5)
#      endif
#   endif
#endif // BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT
