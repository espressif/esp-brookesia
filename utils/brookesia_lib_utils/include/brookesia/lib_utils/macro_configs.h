/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#if defined(ESP_PLATFORM)
#   include "sdkconfig.h"
#endif

/* Version */
#if !defined(BROOKESIA_UTILS_VER_MAJOR)
#   define BROOKESIA_UTILS_VER_MAJOR  (0)
#endif
#if !defined(BROOKESIA_UTILS_VER_MINOR)
#   define BROOKESIA_UTILS_VER_MINOR  (7)
#endif
#if !defined(BROOKESIA_UTILS_VER_PATCH)
#   define BROOKESIA_UTILS_VER_PATCH  (0)
#endif

/* Debug log */
#define BROOKESIA_UTILS_LOG_TAG "LibUtils"

#if !defined(BROOKESIA_UTILS_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_UTILS_ENABLE_DEBUG_LOG)
#       define BROOKESIA_UTILS_ENABLE_DEBUG_LOG CONFIG_BROOKESIA_UTILS_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_UTILS_ENABLE_DEBUG_LOG (0)
#   endif
#endif

#if BROOKESIA_UTILS_ENABLE_DEBUG_LOG
#   if !defined(BROOKESIA_UTILS_STATIC_MACHINE_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_UTILS_STATIC_MACHINE_ENABLE_DEBUG_LOG)
#       define BROOKESIA_UTILS_STATIC_MACHINE_ENABLE_DEBUG_LOG CONFIG_BROOKESIA_UTILS_STATIC_MACHINE_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_UTILS_STATIC_MACHINE_ENABLE_DEBUG_LOG (0)
#   endif
#endif

#if !defined(BROOKESIA_UTILS_TASK_SCHEDULER_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_UTILS_TASK_SCHEDULER_ENABLE_DEBUG_LOG)
#       define BROOKESIA_UTILS_TASK_SCHEDULER_ENABLE_DEBUG_LOG CONFIG_BROOKESIA_UTILS_TASK_SCHEDULER_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_UTILS_TASK_SCHEDULER_ENABLE_DEBUG_LOG (0)
#   endif
#endif

#if !defined(BROOKESIA_UTILS_THREAD_CONFIG_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_UTILS_THREAD_CONFIG_ENABLE_DEBUG_LOG)
#       define BROOKESIA_UTILS_THREAD_CONFIG_ENABLE_DEBUG_LOG CONFIG_BROOKESIA_UTILS_THREAD_CONFIG_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_UTILS_THREAD_CONFIG_ENABLE_DEBUG_LOG (0)
#   endif
#endif

#if !defined(BROOKESIA_UTILS_THREAD_PROFILER_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_UTILS_THREAD_PROFILER_ENABLE_DEBUG_LOG)
#       define BROOKESIA_UTILS_THREAD_PROFILER_ENABLE_DEBUG_LOG CONFIG_BROOKESIA_UTILS_THREAD_PROFILER_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_UTILS_THREAD_PROFILER_ENABLE_DEBUG_LOG (0)
#   endif
#endif

#if !defined(BROOKESIA_UTILS_TIME_PROFILER_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_UTILS_TIME_PROFILER_ENABLE_DEBUG_LOG)
#       define BROOKESIA_UTILS_TIME_PROFILER_ENABLE_DEBUG_LOG CONFIG_BROOKESIA_UTILS_TIME_PROFILER_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_UTILS_TIME_PROFILER_ENABLE_DEBUG_LOG (0)
#   endif
#endif

#if !defined(BROOKESIA_UTILS_MEMORY_PROFILER_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_UTILS_MEMORY_PROFILER_ENABLE_DEBUG_LOG)
#       define BROOKESIA_UTILS_MEMORY_PROFILER_ENABLE_DEBUG_LOG CONFIG_BROOKESIA_UTILS_MEMORY_PROFILER_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_UTILS_MEMORY_PROFILER_ENABLE_DEBUG_LOG (0)
#   endif
#endif
#endif // BROOKESIA_UTILS_ENABLE_DEBUG_LOG

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// Check /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Macros for check handle method
 */
#define BROOKESIA_UTILS_CHECK_HANDLE_WITH_NONE        (0) /*!< Do nothing when check failed */
#define BROOKESIA_UTILS_CHECK_HANDLE_WITH_ERROR_LOG   (1) /*!< Print error message when check failed */
#define BROOKESIA_UTILS_CHECK_HANDLE_WITH_ASSERT      (2) /*!< Assert when check failed */

#ifndef BROOKESIA_UTILS_CHECK_HANDLE_METHOD
#   ifdef CONFIG_BROOKESIA_UTILS_CHECK_HANDLE_METHOD
#       define BROOKESIA_UTILS_CHECK_HANDLE_METHOD   CONFIG_BROOKESIA_UTILS_CHECK_HANDLE_METHOD
#   else
#       define BROOKESIA_UTILS_CHECK_HANDLE_METHOD BROOKESIA_UTILS_CHECK_HANDLE_WITH_ERROR_LOG
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////// Log //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Macros for log implementation
 */
#define BROOKESIA_UTILS_LOG_IMPL_STDLIB     (0) /*!< Standard (printf) */
#define BROOKESIA_UTILS_LOG_IMPL_ESP        (1) /*!< ESP (esp_log) */

/**
 * @brief Macros for log level
 */
#define BROOKESIA_UTILS_LOG_LEVEL_TRACE     (0)     /*!< Trace information that is used for tracing the execution flow
                                                      *  of the program*/
#define BROOKESIA_UTILS_LOG_LEVEL_DEBUG     (1)     /*!< Extra information which is not necessary for normal use (values,
                                                     *   pointers, sizes, etc). */
#define BROOKESIA_UTILS_LOG_LEVEL_INFO      (2)     /*!< Information messages which describe the normal flow of events */
#define BROOKESIA_UTILS_LOG_LEVEL_WARNING   (3)     /*!< Error conditions from which recovery measures have been taken */
#define BROOKESIA_UTILS_LOG_LEVEL_ERROR     (4)     /*!< Critical errors, software module cannot recover on its own */
#define BROOKESIA_UTILS_LOG_LEVEL_NONE      (5)     /*!< No log output */

#if !defined(BROOKESIA_UTILS_LOG_IMPL_TYPE)
#   if defined(CONFIG_BROOKESIA_UTILS_LOG_IMPL_TYPE)
#       define BROOKESIA_UTILS_LOG_IMPL_TYPE CONFIG_BROOKESIA_UTILS_LOG_IMPL_TYPE
#   else
#       define BROOKESIA_UTILS_LOG_IMPL_TYPE BROOKESIA_UTILS_LOG_IMPL_STDLIB
#   endif
#endif

#if !defined(BROOKESIA_UTILS_LOG_LEVEL)
#   if defined(CONFIG_BROOKESIA_UTILS_LOG_LEVEL)
#       define BROOKESIA_UTILS_LOG_LEVEL CONFIG_BROOKESIA_UTILS_LOG_LEVEL
#   else
#       define BROOKESIA_UTILS_LOG_LEVEL BROOKESIA_UTILS_LOG_LEVEL_TRACE
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// Thread Config /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(BROOKESIA_UTILS_THREAD_CONFIG_NAME)
#   if defined(CONFIG_BROOKESIA_UTILS_THREAD_CONFIG_NAME)
#       define BROOKESIA_UTILS_THREAD_CONFIG_NAME CONFIG_BROOKESIA_UTILS_THREAD_CONFIG_NAME
#   else
#       define BROOKESIA_UTILS_THREAD_CONFIG_NAME "bthread"
#   endif
#endif

#if !defined(BROOKESIA_UTILS_THREAD_CONFIG_CORE_ID)
#   if defined(CONFIG_BROOKESIA_UTILS_THREAD_CONFIG_CORE_ID)
#       define BROOKESIA_UTILS_THREAD_CONFIG_CORE_ID CONFIG_BROOKESIA_UTILS_THREAD_CONFIG_CORE_ID
#   else
#       define BROOKESIA_UTILS_THREAD_CONFIG_CORE_ID (-1)
#   endif
#endif

#if !defined(BROOKESIA_UTILS_THREAD_CONFIG_PRIORITY)
#   if defined(CONFIG_BROOKESIA_UTILS_THREAD_CONFIG_PRIORITY)
#       define BROOKESIA_UTILS_THREAD_CONFIG_PRIORITY CONFIG_BROOKESIA_UTILS_THREAD_CONFIG_PRIORITY
#   else
#       define BROOKESIA_UTILS_THREAD_CONFIG_PRIORITY (5)
#   endif
#endif

#if !defined(BROOKESIA_UTILS_THREAD_CONFIG_STACK_SIZE)
#   if defined(CONFIG_BROOKESIA_UTILS_THREAD_CONFIG_STACK_SIZE)
#       define BROOKESIA_UTILS_THREAD_CONFIG_STACK_SIZE CONFIG_BROOKESIA_UTILS_THREAD_CONFIG_STACK_SIZE
#   else
#       define BROOKESIA_UTILS_THREAD_CONFIG_STACK_SIZE (4 * 1024)
#   endif
#endif

#if !defined(BROOKESIA_UTILS_THREAD_CONFIG_STACK_IN_EXT)
#   if defined(CONFIG_BROOKESIA_UTILS_THREAD_CONFIG_STACK_IN_EXT)
#       define BROOKESIA_UTILS_THREAD_CONFIG_STACK_IN_EXT CONFIG_BROOKESIA_UTILS_THREAD_CONFIG_STACK_IN_EXT
#   else
#       define BROOKESIA_UTILS_THREAD_CONFIG_STACK_IN_EXT (0)
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Thread Profiler ////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(ESP_PLATFORM) && defined(CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID) && \
    defined(CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS)
#   define BROOKESIA_UTILS_THREAD_PROFILER_AVAILABLE (1)
#else
#   define BROOKESIA_UTILS_THREAD_PROFILER_AVAILABLE (0)
#endif
