/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"

/**
 * @brief Enable automatic plugin registration for the emote expression component.
 */
#if !defined(BROOKESIA_EXPRESSION_EMOTE_ENABLE_AUTO_REGISTER)
#   if defined(CONFIG_BROOKESIA_EXPRESSION_EMOTE_ENABLE_AUTO_REGISTER)
#       define BROOKESIA_EXPRESSION_EMOTE_ENABLE_AUTO_REGISTER  CONFIG_BROOKESIA_EXPRESSION_EMOTE_ENABLE_AUTO_REGISTER
#   else
#       define BROOKESIA_EXPRESSION_EMOTE_ENABLE_AUTO_REGISTER  (0)
#   endif
#endif

/**
 * @brief Linker symbol exported when automatic plugin registration is enabled.
 */
#if BROOKESIA_EXPRESSION_EMOTE_ENABLE_AUTO_REGISTER
#   if !defined(BROOKESIA_EXPRESSION_EMOTE_PLUGIN_SYMBOL)
#       define BROOKESIA_EXPRESSION_EMOTE_PLUGIN_SYMBOL  expression_emote_symbol
#   endif
#endif

/**
 * @brief Default log tag used by the emote expression component.
 */
#define BROOKESIA_EXPRESSION_EMOTE_LOG_TAG "ExprEmote"

#if !defined(BROOKESIA_EXPRESSION_EMOTE_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_EXPRESSION_EMOTE_ENABLE_DEBUG_LOG)
#       define BROOKESIA_EXPRESSION_EMOTE_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_EXPRESSION_EMOTE_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_EXPRESSION_EMOTE_ENABLE_DEBUG_LOG  (0)
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Worker /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(BROOKESIA_EXPRESSION_EMOTE_ENABLE_WORKER)
#   if defined(CONFIG_BROOKESIA_EXPRESSION_EMOTE_ENABLE_WORKER)
#       define BROOKESIA_EXPRESSION_EMOTE_ENABLE_WORKER  CONFIG_BROOKESIA_EXPRESSION_EMOTE_ENABLE_WORKER
#   else
#       define BROOKESIA_EXPRESSION_EMOTE_ENABLE_WORKER  (0)
#   endif
#endif
#if BROOKESIA_EXPRESSION_EMOTE_ENABLE_WORKER
#   if !defined(BROOKESIA_EXPRESSION_EMOTE_WORKER_NAME)
#      if defined(CONFIG_BROOKESIA_EXPRESSION_EMOTE_WORKER_NAME)
#          define BROOKESIA_EXPRESSION_EMOTE_WORKER_NAME  CONFIG_BROOKESIA_EXPRESSION_EMOTE_WORKER_NAME
#      else
#          define BROOKESIA_EXPRESSION_EMOTE_WORKER_NAME  "EmoteWorker"
#      endif
#   endif
#   if !defined(BROOKESIA_EXPRESSION_EMOTE_WORKER_CORE_ID)
#      if defined(CONFIG_BROOKESIA_EXPRESSION_EMOTE_WORKER_CORE_ID)
#          define BROOKESIA_EXPRESSION_EMOTE_WORKER_CORE_ID  CONFIG_BROOKESIA_EXPRESSION_EMOTE_WORKER_CORE_ID
#      else
#          define BROOKESIA_EXPRESSION_EMOTE_WORKER_CORE_ID  (-1)
#      endif
#   endif
#   if !defined(BROOKESIA_EXPRESSION_EMOTE_WORKER_PRIORITY)
#      if defined(CONFIG_BROOKESIA_EXPRESSION_EMOTE_WORKER_PRIORITY)
#          define BROOKESIA_EXPRESSION_EMOTE_WORKER_PRIORITY  CONFIG_BROOKESIA_EXPRESSION_EMOTE_WORKER_PRIORITY
#      else
#          define BROOKESIA_EXPRESSION_EMOTE_WORKER_PRIORITY  (5)
#      endif
#   endif
#   if !defined(BROOKESIA_EXPRESSION_EMOTE_WORKER_STACK_SIZE)
#      if defined(CONFIG_BROOKESIA_EXPRESSION_EMOTE_WORKER_STACK_SIZE)
#          define BROOKESIA_EXPRESSION_EMOTE_WORKER_STACK_SIZE  CONFIG_BROOKESIA_EXPRESSION_EMOTE_WORKER_STACK_SIZE
#      else
#          define BROOKESIA_EXPRESSION_EMOTE_WORKER_STACK_SIZE  (5120)
#      endif
#   endif
#   if !defined(BROOKESIA_EXPRESSION_EMOTE_WORKER_POLL_INTERVAL_MS)
#      if defined(CONFIG_BROOKESIA_EXPRESSION_EMOTE_WORKER_POLL_INTERVAL_MS)
#          define BROOKESIA_EXPRESSION_EMOTE_WORKER_POLL_INTERVAL_MS  CONFIG_BROOKESIA_EXPRESSION_EMOTE_WORKER_POLL_INTERVAL_MS
#      else
#          define BROOKESIA_EXPRESSION_EMOTE_WORKER_POLL_INTERVAL_MS  (5)
#      endif
#   endif
#endif // BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT
