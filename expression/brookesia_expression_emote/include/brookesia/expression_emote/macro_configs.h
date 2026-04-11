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
