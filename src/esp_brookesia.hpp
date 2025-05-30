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
#include "esp_brookesia.h"

/* GUI */
/* GUI - lvgl */
#include "gui/esp_brookesia_gui_type.hpp"
#include "gui/lvgl/esp_brookesia_lv_helper.hpp"

/* Systems */
/* Systems - Core */
#include "systems/core/esp_brookesia_core.hpp"
/* Systems - Phone */
#if ESP_BROOKESIA_CONF_SYSTEMS_ENABLE_PHONE
#   include "systems/phone/esp_brookesia_phone.hpp"
#   include "systems/phone/stylesheets/esp_brookesia_phone_stylesheets.hpp"
#   if ESP_BROOKESIA_CONF_PHONE_ENABLE_APP_EXAMPLES
#       if ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_SIMPLE_CONF
#           include "systems/phone/app_examples/simple_conf/src/phone_app_simple_conf.hpp"
#       endif
#       if ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_COMPLEX_CONF
#           include "systems/phone/app_examples/complex_conf/src/phone_app_complex_conf.hpp"
#       endif
#       if ESP_BROOKESIA_CONF_PHONE_APP_EXAMPLES_ENABLE_SQUARELINE
#           include "systems/phone/app_examples/squareline/src/phone_app_squareline.hpp"
#       endif
#   endif
#endif
