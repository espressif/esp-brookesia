/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <cstdlib>
#include "lvgl.h"
#include "gui/esp_brookesia_gui_type.hpp"
#include "esp_brookesia_lv_object.hpp"

namespace esp_brookesia::gui {

class LvScreen: public LvObject {
public:
    using LvObject::LvObject;

    LvScreen();

    bool load();
};

using LvScreenUniquePtr = std::unique_ptr<LvScreen>;

} // namespace esp_brookesia::gui
