/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/gui_interface/document.hpp"
#include "brookesia/gui_interface/macro_configs.h"

namespace esp_brookesia::gui {

struct ParsedDocument {
    Document document;
    std::vector<std::string> dependency_files;
};

std::expected<Document, std::string> parse_document(
    std::string_view json, std::string_view base_dir, const Environment &environment
);
std::expected<std::vector<FontAsset>, std::string> parse_font_asset_set_json(
    std::string_view json,
    std::string_view base_dir
);
std::expected<std::vector<FontAsset>, std::string> parse_font_asset_set_file(std::string_view path);
std::expected<std::vector<ImageAsset>, std::string> parse_image_asset_set_json(
    std::string_view json,
    std::string_view base_dir
);
std::expected<std::vector<ImageAsset>, std::string> parse_image_asset_set_file(std::string_view path);
std::expected<ThemeAsset, std::string> parse_theme_asset_json(
    std::string_view json,
    std::string_view base_dir,
    const Environment &environment = {}
);
std::expected<ThemeAsset, std::string> parse_theme_asset_file(
    std::string_view path,
    const Environment &environment = {}
);
std::expected<ParsedDocument, std::string> parse_document_file_with_metadata(
    std::string_view path,
    const Environment &environment
);
std::expected<Document, std::string> parse_document_file(std::string_view path, const Environment &environment);

} // namespace esp_brookesia::gui
