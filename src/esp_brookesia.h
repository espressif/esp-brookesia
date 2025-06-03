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

/* GUI */
/* GUI - Squareline */
#ifdef ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_HELPERS
#   include "gui/squareline/ui_helpers/ui_helpers.h"
#endif
#ifdef ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_COMP
#   include "gui/squareline/ui_comp/ui_comp.h"
#endif

/* Systems */
/* Systems - Assets */
#include "systems/assets/esp_brookesia_systems_assets.h"
#if ESP_BROOKESIA_CONF_SYSTEMS_ENABLE_PHONE
#   include "systems/phone/assets/esp_brookesia_phone_assets.h"
#endif
