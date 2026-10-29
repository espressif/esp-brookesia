/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>

#include "brookesia/gui_interface/validator.hpp"
#include "private/sparse_document.hpp"

namespace esp_brookesia::gui {

ValidationResult validate_sparse_document(const SparseDocument &document);
ValidationResult validate_sparse_screen_node(
    const SparseNode &screen,
    const std::vector<const SparseNode *> &siblings,
    const std::vector<const SparseNode *> &templates);

} // namespace esp_brookesia::gui
