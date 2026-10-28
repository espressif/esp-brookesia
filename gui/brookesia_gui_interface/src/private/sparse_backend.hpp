/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/gui_interface/backend.hpp"
#include "private/sparse_document.hpp"

namespace esp_brookesia::gui {

BackendHandle create_backend_node(
    IBackend &backend, const SparseNode &node, BackendHandle parent,
    std::string_view parent_path, std::string_view scope_root_absolute_path);
void apply_backend_props(IBackend &backend, BackendHandle handle, const SparseNode &node, PropsApplyMask mask);
void apply_backend_style(
    IBackend &backend, BackendHandle handle, const SparseResolvedStyle &style,
    StyleApplyMask mask = StyleApplyMask::All);

} // namespace esp_brookesia::gui
