/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_brookesia_internal.h"

#if !ESP_BROOKESIA_ENABLE_AI_FRAMEWORK
#   error "AI framework is not enabled, please enable it in the menuconfig"
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// Agent /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_AGENT)
#   if defined(CONFIG_ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_AGENT)
#       define ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_AGENT  CONFIG_ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_AGENT
#   else
#       define ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_AGENT  (0)
#   endif
#endif

#if ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_AGENT
#   if !defined(ESP_BROOKESIA_AGENT_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_ESP_BROOKESIA_AGENT_ENABLE_DEBUG_LOG)
#           define ESP_BROOKESIA_AGENT_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_AGENT_ENABLE_DEBUG_LOG
#       else
#           define ESP_BROOKESIA_AGENT_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// Expression ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_EXPRESSION)
#   if defined(CONFIG_ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_EXPRESSION)
#       define ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_EXPRESSION  CONFIG_ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_EXPRESSION
#   else
#       define ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_EXPRESSION  (0)
#   endif
#endif

#if ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_EXPRESSION
#   if !defined(ESP_BROOKESIA_EXPRESSION_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_ESP_BROOKESIA_EXPRESSION_ENABLE_DEBUG_LOG)
#           define ESP_BROOKESIA_EXPRESSION_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_EXPRESSION_ENABLE_DEBUG_LOG
#       else
#           define ESP_BROOKESIA_EXPRESSION_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#endif
