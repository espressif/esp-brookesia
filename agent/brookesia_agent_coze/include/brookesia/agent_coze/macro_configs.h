/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"

/* Debug log */
#define BROOKESIA_AGENT_COZE_LOG_TAG "Coze"

#if !defined(BROOKESIA_AGENT_COZE_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_AGENT_COZE_ENABLE_DEBUG_LOG)
#       define BROOKESIA_AGENT_COZE_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_AGENT_COZE_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_AGENT_COZE_ENABLE_DEBUG_LOG  (0)
#   endif
#endif
