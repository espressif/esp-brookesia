/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#if defined(ESP_PLATFORM)
#   include "sdkconfig.h"
#endif

#if !defined(BROOKESIA_SERVICE_PICODET_VER_MAJOR)
#   define BROOKESIA_SERVICE_PICODET_VER_MAJOR  (0)
#endif

#if !defined(BROOKESIA_SERVICE_PICODET_VER_MINOR)
#   define BROOKESIA_SERVICE_PICODET_VER_MINOR  (8)
#endif

#if !defined(BROOKESIA_SERVICE_PICODET_VER_PATCH)
#   define BROOKESIA_SERVICE_PICODET_VER_PATCH  (0)
#endif

/**
 * @brief Default log tag used by the picodet service component.
 */
#define BROOKESIA_SERVICE_PICODET_LOG_TAG "SvcPicoDet"

#if !defined(BROOKESIA_SERVICE_PICODET_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_SERVICE_PICODET_ENABLE_DEBUG_LOG)
#       define BROOKESIA_SERVICE_PICODET_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_SERVICE_PICODET_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_SERVICE_PICODET_ENABLE_DEBUG_LOG  (0)
#   endif
#endif

/**
 * @brief Whether the real esp-dl inference backend is available.
 *
 * ESP builds enable the real esp-dl detector backend. Other platforms compile the
 * detector as a stub that reports "unsupported".
 */
#if !defined(BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE)
#   if defined(ESP_PLATFORM)
#       define BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE  (1)
#   else
#       define BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE  (0)
#   endif
#endif

/**
 * @brief Stack size of the short-lived thread used for filesystem access.
 *
 * Reading the model or an image from flash disables the flash cache, which requires the running
 * task's stack to be in internal RAM. ServiceManager workers may have their stacks in PSRAM
 * (CONFIG_BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT), so file IO runs on a temporary
 * internal-stack thread when needed — same pattern as the HTTP service. Inference itself does not
 * touch flash and runs on the normal worker.
 */
#if !defined(BROOKESIA_SERVICE_PICODET_FILE_IO_THREAD_STACK_SIZE)
#   if defined(CONFIG_BROOKESIA_SERVICE_PICODET_FILE_IO_THREAD_STACK_SIZE)
#       define BROOKESIA_SERVICE_PICODET_FILE_IO_THREAD_STACK_SIZE  CONFIG_BROOKESIA_SERVICE_PICODET_FILE_IO_THREAD_STACK_SIZE
#   else
#       define BROOKESIA_SERVICE_PICODET_FILE_IO_THREAD_STACK_SIZE  (8192)
#   endif
#endif
