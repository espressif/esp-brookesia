/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "private/sparse_document.hpp"

namespace esp_brookesia::gui {

std::expected<SparseDocument, std::string> parse_sparse_document(
    std::string_view json, std::string_view base_dir, const Environment &environment);
std::expected<SparseParsedDocument, std::string> parse_sparse_document_file_with_metadata(
    std::string_view path, const Environment &environment);
std::expected<SparseThemeAsset, std::string> parse_sparse_theme_asset_json(
    std::string_view json, std::string_view base_dir, const Environment &environment = {});
std::expected<SparseThemeAsset, std::string> parse_sparse_theme_asset_file(
    std::string_view path, const Environment &environment = {});

} // namespace esp_brookesia::gui
