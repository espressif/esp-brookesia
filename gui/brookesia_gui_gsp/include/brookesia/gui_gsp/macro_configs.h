/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#if defined(ESP_PLATFORM)
#   include "sdkconfig.h"
#endif

/**
 * @brief Default log tag used by the GSP GUI adapter.
 */
#define BROOKESIA_GUI_GSP_LOG_TAG "GuiGsp"

#if !defined(BROOKESIA_GUI_GSP_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_GUI_GSP_ENABLE_DEBUG_LOG)
#       define BROOKESIA_GUI_GSP_ENABLE_DEBUG_LOG CONFIG_BROOKESIA_GUI_GSP_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_GUI_GSP_ENABLE_DEBUG_LOG 0
#   endif
#endif
