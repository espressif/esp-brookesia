/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>

#include "brookesia/gui_interface/document.hpp"
#include "brookesia/gui_interface/macro_configs.h"

namespace esp_brookesia::gui {

ValidationResult validate_document(const Document &document);

/**
 * Validate one fully parsed screen against sibling screens that are already complete.
 * Template nodes contribute action ownership only. Sibling stubs must not be passed.
 * Failure leaves the caller's screen definition unchanged.
 */
ValidationResult validate_screen_node(
    const Node &screen,
    const std::vector<const Node *> &siblings,
    const std::vector<const Node *> &templates);

} // namespace esp_brookesia::gui
