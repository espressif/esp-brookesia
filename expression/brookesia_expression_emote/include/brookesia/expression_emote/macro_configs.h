/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"

/* Debug log */
#define BROOKESIA_EXPRESSION_EMOTE_LOG_TAG "ExprEmote"

#if !defined(BROOKESIA_EXPRESSION_EMOTE_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_EXPRESSION_EMOTE_ENABLE_DEBUG_LOG)
#       define BROOKESIA_EXPRESSION_EMOTE_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_EXPRESSION_EMOTE_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_EXPRESSION_EMOTE_ENABLE_DEBUG_LOG  (0)
#   endif
#endif
