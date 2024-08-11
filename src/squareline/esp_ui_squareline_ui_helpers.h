/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_ui_conf_internal.h"

#if !ESP_UI_SQUARELINE_USE_INTERNAL_UI_HELPERS
#error "`ESP_UI_SQUARELINE_ENABLE` must be enabled in `esp_ui_conf.h`"
#endif

#if defined(ESP_UI_SQ1_3_4_LV8_3_3)
#include "SQ1_3_4_LV8_3_3/ui_helpers.h"
#elif defined(ESP_UI_SQ1_3_4_LV8_3_4)
#include "SQ1_3_4_LV8_3_4/ui_helpers.h"
#elif defined(ESP_UI_SQ1_3_4_LV8_3_6)
#include "SQ1_3_4_LV8_3_6/ui_helpers.h"
#elif defined(ESP_UI_SQ1_4_0_LV8_3_6)
#include "SQ1_4_0_LV8_3_6/ui_helpers.h"
#elif defined(ESP_UI_SQ1_4_0_LV8_3_11)
#include "SQ1_4_0_LV8_3_11/ui_helpers.h"
#elif defined(ESP_UI_SQ1_4_1_LV8_3_6)
#include "SQ1_4_1_LV8_3_6/ui_helpers.h"
#elif defined(ESP_UI_SQ1_4_1_LV8_3_11)
#include "SQ1_4_1_LV8_3_11/ui_helpers.h"
#else
#error "Unsupported SquareLine Studio version and LVGL version"
#endif
