/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#if defined(ESP_PLATFORM)
#   include "sdkconfig.h"
#endif

namespace esp_brookesia::lib_utils {

/**
 * Whether the calling context may read files from the filesystem directly.
 *
 * On ESP targets without PSRAM XIP, flash metadata I/O issued from a PSRAM
 * stack can trip esp_task_stack_is_sane_cache_disabled(), so those builds must
 * route reads through the storage worker instead. PC builds and PSRAM-XIP
 * targets read directly, which avoids a scheduler round-trip per file.
 *
 * Keep this as the single definition: the parser and the image loader must agree
 * or one of them reads through a path the platform does not support.
 */
inline bool can_read_local_files()
{
#if !defined(ESP_PLATFORM)
    return true;
#elif defined(CONFIG_SPIRAM_XIP_FROM_PSRAM) && CONFIG_SPIRAM_XIP_FROM_PSRAM
    return true;
#else
    return false;
#endif
}

} // namespace esp_brookesia::lib_utils
