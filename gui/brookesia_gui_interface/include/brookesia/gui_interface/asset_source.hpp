/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace esp_brookesia::gui {

/**
 * Bytes of `path` when a registered asset bundle serves it.
 *
 * Documents register their <resource root>/assets.bundle on load, and any component
 * that reads resources (the LVGL image loader, for example) can consult the same
 * bundle instead of opening the file again. Returns nullopt when no bundle covers
 * the path, so callers keep their normal filesystem path as the fallback.
 */
std::optional<std::string> read_bundled_asset(const std::filesystem::path &path);

} // namespace esp_brookesia::gui
