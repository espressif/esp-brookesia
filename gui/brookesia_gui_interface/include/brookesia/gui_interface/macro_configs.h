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
#if !defined(BROOKESIA_GUI_INTERFACE_ENABLE_ASSET_BUNDLE)
#   if defined(CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_ASSET_BUNDLE)
#       define BROOKESIA_GUI_INTERFACE_ENABLE_ASSET_BUNDLE  CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_ASSET_BUNDLE
#   else
#       define BROOKESIA_GUI_INTERFACE_ENABLE_ASSET_BUNDLE  (0)
#   endif
#endif

#if !defined(BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG)
#   if defined(CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG)
#       define BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG  CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG
#   else
#       define BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG  (0)
#   endif
#endif

/**
 * @brief Keep unmounted dynamic screens resident (B4 keep-alive).
 *
 * When enabled, unmounting a dynamic screen only unmounts the backend screen and
 * keeps the runtime subtree, so a later mount reuses the built nodes instead of
 * rebuilding them. Off by default: it trades resident memory and per-flush apply
 * cost for page-switch latency.
 */
#if !defined(BROOKESIA_GUI_INTERFACE_KEEP_DYNAMIC_SCREENS)
#   if defined(CONFIG_BROOKESIA_GUI_INTERFACE_KEEP_DYNAMIC_SCREENS)
#       define BROOKESIA_GUI_INTERFACE_KEEP_DYNAMIC_SCREENS  CONFIG_BROOKESIA_GUI_INTERFACE_KEEP_DYNAMIC_SCREENS
#   else
#       define BROOKESIA_GUI_INTERFACE_KEEP_DYNAMIC_SCREENS  (0)
#   endif
#endif

/**
 * @brief Prewarm Dynamic screens during the idle pass instead of on first mount.
 */
#if !defined(BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS)
#   if defined(CONFIG_BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS)
#       define BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS  CONFIG_BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS
#   else
#       define BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS  (0)
#   endif
#endif

/**
 * @brief Delay before the prewarm pass starts after a document load, in milliseconds.
 */
#if !defined(BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS_DELAY_MS)
#   if defined(CONFIG_BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS_DELAY_MS)
#       define BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS_DELAY_MS  CONFIG_BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS_DELAY_MS
#   else
#       define BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS_DELAY_MS  (0)
#   endif
#endif

/**
 * @brief Upper bound on the prewarmed Dynamic screens (0 = all of them).
 */
#if !defined(BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS_MAX)
#   if defined(CONFIG_BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS_MAX)
#       define BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS_MAX  CONFIG_BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS_MAX
#   else
#       define BROOKESIA_GUI_INTERFACE_PREWARM_DYNAMIC_SCREENS_MAX  (0)
#   endif
#endif

/**
 * @brief Number of interned ResolvedStyle objects served from a contiguous pool.
 *
 * 0 keeps every interned style on the heap. A non-zero value reserves that many
 * slots up front and falls back to the heap when they run out.
 */
#if !defined(BROOKESIA_GUI_INTERFACE_STYLE_POOL_SLOTS)
#   if defined(CONFIG_BROOKESIA_GUI_INTERFACE_STYLE_POOL_SLOTS)
#       define BROOKESIA_GUI_INTERFACE_STYLE_POOL_SLOTS  CONFIG_BROOKESIA_GUI_INTERFACE_STYLE_POOL_SLOTS
#   else
#       define BROOKESIA_GUI_INTERFACE_STYLE_POOL_SLOTS  (0)
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
