/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * This header file and the headers it includes use C++ syntax and can only be included in .cpp files.
 */

/* C-standard */
#include "esp_brookesia.h"

/* GUI */
/* GUI - lvgl */
#include "style/esp_brookesia_gui_style.hpp"
#include "style/esp_brookesia_gui_stylesheet_manager.hpp"
#include "gui/lvgl/esp_brookesia_lv_helper.hpp"

/* Services */
/* Services - Storage NVS */
#if ESP_BROOKESIA_SERVICES_ENABLE_STORAGE_NVS
#   include "services/storage_nvs/esp_brookesia_service_storage_nvs.hpp"
#endif

/* Systems */
/* Systems - Core */
#include "systems/base/esp_brookesia_base_context.hpp"
/* Systems - Phone */
#if ESP_BROOKESIA_SYSTEMS_ENABLE_PHONE
#   include "systems/phone/esp_brookesia_phone.hpp"
#   include "systems/phone/stylesheets/esp_brookesia_phone_stylesheets.hpp"
#endif
/* Systems - Speaker */
#if ESP_BROOKESIA_SYSTEMS_ENABLE_SPEAKER
#   include "systems/speaker/esp_brookesia_speaker.hpp"
#   include "systems/speaker/stylesheets/esp_brookesia_speaker_stylesheets.hpp"
#endif
