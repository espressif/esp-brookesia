/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * This header file and the headers it includes do not contain any C++ syntax and can be directly included in .c files.
 */

/* Configuration */
#include "esp_brookesia_conf_internal.h"
#include "esp_brookesia_versions.h"

/* Core */
#include "core/esp_brookesia_core_utils.h"
#include "core/esp_brookesia_style_type.h"
#include "core/esp_brookesia_core_type.h"
#include "core/esp_brookesia_lv_type.h"

/* assets */
#include "assets/esp_brookesia_assets.h"

/* Systems */
// Phone
#include "systems/phone/esp_brookesia_phone_type.h"

/* Squareline */
#ifdef ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_HELPERS
#include "squareline/ui_helpers/ui_helpers.h"
#endif
#ifdef ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_COMP
#include "squareline/ui_comp/ui_comp.h"
#endif

/* Widgets */
// App Launcher
#include "widgets/app_launcher/esp_brookesia_app_launcher_type.h"
// Recents Screen
#include "widgets/recents_screen/esp_brookesia_recents_screen_type.h"
// Gesture
#include "widgets/gesture/esp_brookesia_gesture_type.h"
// Navigation Bar
#include "widgets/navigation_bar/esp_brookesia_navigation_bar_type.h"
// Status Bar
#include "widgets/status_bar/esp_brookesia_status_bar_type.h"
