/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"
#include "brookesia/service_manager/macro_configs.h"

/**
 * @brief Default log tag used by `mcp_utils` components.
 */
#define BROOKESIA_MCP_UTILS_LOG_TAG "McpUtils"

#if !defined(BROOKESIA_MCP_UTILS_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_MCP_UTILS_ENABLE_DEBUG_LOG)
#       define BROOKESIA_MCP_UTILS_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_MCP_UTILS_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_MCP_UTILS_ENABLE_DEBUG_LOG  (0)
#   endif
#endif
