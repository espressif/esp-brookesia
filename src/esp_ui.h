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
#include "esp_ui_conf_internal.h"
#include "esp_ui_versions.h"

/* Core */
#include "core/esp_ui_core_utils.h"
#include "core/esp_ui_style_type.h"
#include "core/esp_ui_core_type.h"

/* fonts */
#include "fonts/esp_ui_fonts.h"

/* Systems */
// Phone
#include "systems/phone/esp_ui_phone_type.h"

/* Squareline */
#ifdef ESP_UI_SQUARELINE_USE_INTERNAL_UI_HELPERS
#include "squareline/esp_ui_squareline_ui_helpers.h"
#endif

/* Widgets */
// App Launcher
#include "widgets/app_launcher/esp_ui_app_launcher_type.h"
// Recents Screen
#include "widgets/recents_screen/esp_ui_recents_screen_type.h"
// Gesture
#include "widgets/gesture/esp_ui_gesture_type.h"
// Navigation Bar
#include "widgets/navigation_bar/esp_ui_navigation_bar_type.h"
// Status Bar
#include "widgets/status_bar/esp_ui_status_bar_type.h"
