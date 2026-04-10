/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief LVGL 8 / LVGL 9 heap compatibility for brookesia_app_rainmaker UI.
 *
 * LVGL 9 uses lv_malloc/lv_free; LVGL 8 uses lv_mem_alloc/lv_mem_free.
 */

#ifndef UI_LV_COMPAT_H
#define UI_LV_COMPAT_H

#include "lvgl.h"

#if LVGL_VERSION_MAJOR >= 9
#define UI_LV_MALLOC(size)       lv_malloc(size)
#define UI_LV_FREE(p)            lv_free(p)
#else
#define UI_LV_MALLOC(size)       lv_mem_alloc(size)
#define UI_LV_FREE(p)            lv_mem_free(p)
#endif

#endif /* UI_LV_COMPAT_H */
