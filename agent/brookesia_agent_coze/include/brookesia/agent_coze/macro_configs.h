/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"

/* Auto register */
#if !defined(BROOKESIA_AGENT_COZE_ENABLE_AUTO_REGISTER)
#   if defined(CONFIG_BROOKESIA_AGENT_COZE_ENABLE_AUTO_REGISTER)
#       define BROOKESIA_AGENT_COZE_ENABLE_AUTO_REGISTER  CONFIG_BROOKESIA_AGENT_COZE_ENABLE_AUTO_REGISTER
#   else
#       define BROOKESIA_AGENT_COZE_ENABLE_AUTO_REGISTER  (0)
#   endif
#endif

/* Plugin symbol */
#if BROOKESIA_AGENT_COZE_ENABLE_AUTO_REGISTER
#   if !defined(BROOKESIA_AGENT_COZE_PLUGIN_SYMBOL)
#       define BROOKESIA_AGENT_COZE_PLUGIN_SYMBOL  agent_coze_symbol
#   endif
#endif

/* Debug log */
#define BROOKESIA_AGENT_COZE_LOG_TAG "AgentCoze"

#if !defined(BROOKESIA_AGENT_COZE_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_AGENT_COZE_ENABLE_DEBUG_LOG)
#       define BROOKESIA_AGENT_COZE_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_AGENT_COZE_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_AGENT_COZE_ENABLE_DEBUG_LOG  (0)
#   endif
#endif
