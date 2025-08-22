/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * This header file and the headers it includes do not contain any C++ syntax and can be directly included in .c files.
 */

#include "esp_brookesia_internal.h"

/* AI Framework */
#if ESP_BROOKESIA_ENABLE_AI_FRAMEWORK
#   include "ai_framework/esp_brookesia_ai_framework_internal.h"
#endif

/* GUI */
#if ESP_BROOKESIA_ENABLE_GUI
#   include "gui/esp_brookesia_gui_internal.h"
#endif
/* GUI - Squareline */
#ifdef ESP_BROOKESIA_SQUARELINE_ENABLE_UI_HELPERS
#   include "gui/squareline/ui_helpers/ui_helpers.h"
#endif
#ifdef ESP_BROOKESIA_SQUARELINE_ENABLE_UI_COMP
#   include "gui/squareline/ui_comp/ui_comp.h"
#endif

/* Services */
#if ESP_BROOKESIA_ENABLE_SERVICES
#   include "services/esp_brookesia_services_internal.h"
#endif

/* Systems */
#if ESP_BROOKESIA_ENABLE_SYSTEMS
#   include "systems/esp_brookesia_systems_internal.h"
#endif
/* Systems - Core */
#include "systems/base/assets/esp_brookesia_base_assets.h"
/* Systems - Phone */
#if ESP_BROOKESIA_SYSTEMS_ENABLE_PHONE
#   include "systems/phone/assets/esp_brookesia_phone_assets.h"
#endif
/* Systems - Speaker */
#if ESP_BROOKESIA_SYSTEMS_ENABLE_SPEAKER
#   include "systems/speaker/assets/esp_brookesia_speaker_assets.h"
#endif
