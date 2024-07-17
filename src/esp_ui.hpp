/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * This header file and the headers it includes use C++ syntax and can only be included in .cpp files.
 */

/* C-standard */
#include "esp_ui.h"

/* Core */
#include "core/esp_ui_core_app.hpp"
#include "core/esp_ui_core_home.hpp"
#include "core/esp_ui_core_manager.hpp"
#include "core/esp_ui_template.hpp"
#include "core/esp_ui_core.hpp"

/* Widgets */
// App Launcher
#include "widgets/app_launcher/esp_ui_app_launcher.hpp"
#include "widgets/app_launcher/esp_ui_app_launcher_icon.hpp"
// Recents Screen
#include "widgets/recents_screen/esp_ui_recents_screen.hpp"
// Gesture
#include "widgets/gesture/esp_ui_gesture.hpp"
// Navigation Bar
#include "widgets/navigation_bar/esp_ui_navigation_bar.hpp"
// Status Bar
#include "widgets/status_bar/esp_ui_status_bar.hpp"
#include "widgets/status_bar/esp_ui_status_bar_icon.hpp"

/* Systems */
// Phone
#include "systems/phone/esp_ui_phone_app.hpp"
#include "systems/phone/esp_ui_phone_home.hpp"
#include "systems/phone/esp_ui_phone_manager.hpp"
#include "systems/phone/esp_ui_phone.hpp"
