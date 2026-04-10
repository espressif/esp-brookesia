/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"

/**
 * @brief Default log tag used by the custom service component.
 */
#define BROOKESIA_SERVICE_CUSTOM_LOG_TAG "SvcCustom"

/**
 * @brief Enable automatic plugin registration for the custom service component.
 */
#if !defined(BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER)
#   if defined(CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER)
#       define BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER  CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER
#   else
#       define BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER  (0)
#   endif
#endif

/**
 * @brief Linker symbol exported when automatic plugin registration is enabled.
 */
#if BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER
#   if !defined(BROOKESIA_SERVICE_CUSTOM_PLUGIN_SYMBOL)
#       define BROOKESIA_SERVICE_CUSTOM_PLUGIN_SYMBOL  service_custom_symbol
#   endif
#endif

#if !defined(BROOKESIA_SERVICE_CUSTOM_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_DEBUG_LOG)
#       define BROOKESIA_SERVICE_CUSTOM_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_SERVICE_CUSTOM_ENABLE_DEBUG_LOG  (0)
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Worker /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(BROOKESIA_SERVICE_CUSTOM_ENABLE_WORKER)
#   if defined(CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_WORKER)
#       define BROOKESIA_SERVICE_CUSTOM_ENABLE_WORKER  CONFIG_BROOKESIA_SERVICE_CUSTOM_ENABLE_WORKER
#   else
#       define BROOKESIA_SERVICE_CUSTOM_ENABLE_WORKER  (0)
#   endif
#endif // BROOKESIA_SERVICE_CUSTOM_ENABLE_WORKER

#if BROOKESIA_SERVICE_CUSTOM_ENABLE_WORKER
#   if !defined(BROOKESIA_SERVICE_CUSTOM_WORKER_NAME)
#      if defined(CONFIG_BROOKESIA_SERVICE_CUSTOM_WORKER_NAME)
#          define BROOKESIA_SERVICE_CUSTOM_WORKER_NAME  CONFIG_BROOKESIA_SERVICE_CUSTOM_WORKER_NAME
#      else
#          define BROOKESIA_SERVICE_CUSTOM_WORKER_NAME  "SvcCustomWorker"
#      endif
#   endif
#   if !defined(BROOKESIA_SERVICE_CUSTOM_WORKER_CORE_ID)
#      if defined(CONFIG_BROOKESIA_SERVICE_CUSTOM_WORKER_CORE_ID)
#          define BROOKESIA_SERVICE_CUSTOM_WORKER_CORE_ID  CONFIG_BROOKESIA_SERVICE_CUSTOM_WORKER_CORE_ID
#      else
#          define BROOKESIA_SERVICE_CUSTOM_WORKER_CORE_ID  (-1)
#      endif
#   endif
#   if !defined(BROOKESIA_SERVICE_CUSTOM_WORKER_PRIORITY)
#      if defined(CONFIG_BROOKESIA_SERVICE_CUSTOM_WORKER_PRIORITY)
#          define BROOKESIA_SERVICE_CUSTOM_WORKER_PRIORITY  CONFIG_BROOKESIA_SERVICE_CUSTOM_WORKER_PRIORITY
#      else
#          define BROOKESIA_SERVICE_CUSTOM_WORKER_PRIORITY  (5)
#      endif
#   endif
#   if !defined(BROOKESIA_SERVICE_CUSTOM_WORKER_STACK_SIZE)
#      if defined(CONFIG_BROOKESIA_SERVICE_CUSTOM_WORKER_STACK_SIZE)
#          define BROOKESIA_SERVICE_CUSTOM_WORKER_STACK_SIZE  CONFIG_BROOKESIA_SERVICE_CUSTOM_WORKER_STACK_SIZE
#      else
#          define BROOKESIA_SERVICE_CUSTOM_WORKER_STACK_SIZE  (8192)
#      endif
#   endif
#   if !defined(BROOKESIA_SERVICE_CUSTOM_WORKER_POLL_INTERVAL_MS)
#      if defined(CONFIG_BROOKESIA_SERVICE_CUSTOM_WORKER_POLL_INTERVAL_MS)
#          define BROOKESIA_SERVICE_CUSTOM_WORKER_POLL_INTERVAL_MS  CONFIG_BROOKESIA_SERVICE_CUSTOM_WORKER_POLL_INTERVAL_MS
#      else
#          define BROOKESIA_SERVICE_CUSTOM_WORKER_POLL_INTERVAL_MS  (5)
#      endif
#   endif
#   if !defined(BROOKESIA_SERVICE_CUSTOM_WORKER_STACK_IN_EXT)
#      if defined(CONFIG_BROOKESIA_SERVICE_CUSTOM_WORKER_STACK_IN_EXT)
#          define BROOKESIA_SERVICE_CUSTOM_WORKER_STACK_IN_EXT  CONFIG_BROOKESIA_SERVICE_CUSTOM_WORKER_STACK_IN_EXT
#      else
#          define BROOKESIA_SERVICE_CUSTOM_WORKER_STACK_IN_EXT  (0)
#      endif
#   endif
#endif // BROOKESIA_SERVICE_CUSTOM_ENABLE_WORKER
