/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
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
 *
 * A thread-local AssetBundleReadScope can restrict the lookup to one document's hold,
 * or skip the bundle for a root whose document is reading source files.
 */
std::optional<std::string> read_bundled_asset(const std::filesystem::path &path);

// Active while one document's images are loaded or released. The image cache appends
// bundled_asset_read_token() so two documents can keep different bytes for one path.
// `cache_identity` distinguishes one document's accepted source revision from another
// document, or from an older revision of the same document, that bypasses the same root.
class AssetBundleReadScope {
public:
    AssetBundleReadScope() = default;
    static AssetBundleReadScope bypass(std::string root, std::string cache_identity);
    static AssetBundleReadScope hold(std::string root, std::uint64_t hold_id);

    AssetBundleReadScope(const AssetBundleReadScope &) = delete;
    AssetBundleReadScope &operator=(const AssetBundleReadScope &) = delete;
    AssetBundleReadScope(AssetBundleReadScope &&other) noexcept;
    AssetBundleReadScope &operator=(AssetBundleReadScope &&) = delete;
    ~AssetBundleReadScope();

private:
    explicit AssetBundleReadScope(bool active)
        : active_(active)
    {
    }

    bool active_ = false;
};

// Empty when no scope is active. Otherwise a stable suffix for the current document's bytes.
std::string bundled_asset_read_token();

} // namespace esp_brookesia::gui
