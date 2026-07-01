/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/gui_interface/document.hpp"
#include "brookesia/gui_interface/macro_configs.h"

namespace esp_brookesia::gui {

ValidationResult validate_document(const Document &document);

} // namespace esp_brookesia::gui
