/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/sparse_backend.hpp"

namespace esp_brookesia::gui {

// Keep public snapshots outside the recursive subtree builder's stack frame.
BackendHandle create_backend_node(
    IBackend &backend, const SparseNode &node, BackendHandle parent,
    std::string_view parent_path, std::string_view scope_root_absolute_path)
{
    return backend.create_node(to_public_node_snapshot(node), parent, parent_path, scope_root_absolute_path);
}

void apply_backend_props(IBackend &backend, BackendHandle handle, const SparseNode &node, PropsApplyMask mask)
{
    backend.apply_props(handle, to_public_node_snapshot(node), mask);
}

void apply_backend_style(
    IBackend &backend, BackendHandle handle, const SparseResolvedStyle &style, StyleApplyMask mask)
{
    backend.apply_style(handle, to_public(style), mask);
}

} // namespace esp_brookesia::gui
