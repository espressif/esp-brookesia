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
 * @brief Default log tag used by the component.
 */
#define BROOKESIA_GUI_INTERFACE_LOG_TAG "GuiIface"

#if !defined(BROOKESIA_GUI_INTERFACE_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_DEBUG_LOG)
#       define BROOKESIA_GUI_INTERFACE_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_GUI_INTERFACE_ENABLE_DEBUG_LOG  (0)
#   endif
#endif

/**
 * @brief Enable the GUI runtime memory/heap trace instrumentation.
 *
 * Profiling-only switch (defaults to disabled). When non-zero, the runtime emits
 * [HeapTrace] logs and per-substep PSRAM accounting during document load and view
 * creation. The actual memory optimizations (node_map storage, move-into-record,
 * descendant-definition drop) are always active and are NOT gated by this switch.
 */
#if !defined(BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE)
#   if defined(CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE)
#       define BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE  CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
#   else
#       define BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE  (0)
#   endif
#endif

/**
 * @brief Enable parser/runtime stage timing logs.
 *
 * This switch is separate from memory tracing so startup timing can be
 * diagnosed without heap snapshots. Memory tracing implies profile logging.
 */
#if !defined(BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG)
#   if defined(CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG)
#       define BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG  CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG
#   else
#       define BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG  (0)
#   endif
#endif

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE && !BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG
#   undef BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG
#   define BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG  (1)
#endif

#if BROOKESIA_GUI_INTERFACE_ENABLE_DEBUG_LOG
#   if !defined(BROOKESIA_GUI_INTERFACE_DATA_STORE_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_BROOKESIA_GUI_INTERFACE_DATA_STORE_ENABLE_DEBUG_LOG)
#           define BROOKESIA_GUI_INTERFACE_DATA_STORE_ENABLE_DEBUG_LOG \
                CONFIG_BROOKESIA_GUI_INTERFACE_DATA_STORE_ENABLE_DEBUG_LOG
#       else
#           define BROOKESIA_GUI_INTERFACE_DATA_STORE_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#   if !defined(BROOKESIA_GUI_INTERFACE_PARSER_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_BROOKESIA_GUI_INTERFACE_PARSER_ENABLE_DEBUG_LOG)
#           define BROOKESIA_GUI_INTERFACE_PARSER_ENABLE_DEBUG_LOG \
                CONFIG_BROOKESIA_GUI_INTERFACE_PARSER_ENABLE_DEBUG_LOG
#       else
#           define BROOKESIA_GUI_INTERFACE_PARSER_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#   if !defined(BROOKESIA_GUI_INTERFACE_RUNTIME_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_BROOKESIA_GUI_INTERFACE_RUNTIME_ENABLE_DEBUG_LOG)
#           define BROOKESIA_GUI_INTERFACE_RUNTIME_ENABLE_DEBUG_LOG \
                CONFIG_BROOKESIA_GUI_INTERFACE_RUNTIME_ENABLE_DEBUG_LOG
#       else
#           define BROOKESIA_GUI_INTERFACE_RUNTIME_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#   if !defined(BROOKESIA_GUI_INTERFACE_VALIDATOR_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_BROOKESIA_GUI_INTERFACE_VALIDATOR_ENABLE_DEBUG_LOG)
#           define BROOKESIA_GUI_INTERFACE_VALIDATOR_ENABLE_DEBUG_LOG \
                CONFIG_BROOKESIA_GUI_INTERFACE_VALIDATOR_ENABLE_DEBUG_LOG
#       else
#           define BROOKESIA_GUI_INTERFACE_VALIDATOR_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#   if !defined(BROOKESIA_GUI_INTERFACE_WIDGET_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_BROOKESIA_GUI_INTERFACE_WIDGET_ENABLE_DEBUG_LOG)
#           define BROOKESIA_GUI_INTERFACE_WIDGET_ENABLE_DEBUG_LOG \
                CONFIG_BROOKESIA_GUI_INTERFACE_WIDGET_ENABLE_DEBUG_LOG
#       else
#           define BROOKESIA_GUI_INTERFACE_WIDGET_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#endif // BROOKESIA_GUI_INTERFACE_ENABLE_DEBUG_LOG
