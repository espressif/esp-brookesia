/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace esp_brookesia::gui {

namespace parser_detail {

// Reader for the packed <resource root>/assets.bundle produced by
// tools/gui_asset_bundle. On littlefs every file open costs ~5.25 ms regardless
// of size, so a document with 30 assets spends about 157 ms of its 230 ms of I/O
// paying for opens. The bundle replaces those with one open plus seek+read.
//
// The index is kept in RAM (about 1.3% of the payload size) and the payload is
// read on demand, so the peak memory stays at one asset instead of the whole
// tree.
class AssetBundle {
public:
    static constexpr const char *kFileName = "assets.bundle";

    // Opens and validates the bundle; returns an error string when it is missing
    // or malformed so the caller can fall back to per-file reads.
    static std::expected<std::shared_ptr<AssetBundle>, std::string> open(const std::filesystem::path &bundle_path);

    AssetBundle(const AssetBundle &) = delete;
    AssetBundle &operator=(const AssetBundle &) = delete;

    [[nodiscard]] const std::filesystem::path &root_dir() const
    {
        return root_dir_;
    }

    [[nodiscard]] size_t entry_count() const
    {
        return entries_.size();
    }

    // Reads the bytes stored for `path` when it falls under the bundle root and
    // the bundle holds an entry for it. Returns nullopt otherwise, which means
    // "not packed" rather than "failed": the caller falls back to the filesystem.
    std::optional<std::string> read(const std::filesystem::path &path) const;

private:
    struct Entry {
        std::string key;
        uint32_t offset = 0;
        uint32_t size = 0;
        uint32_t flags = 0;
    };

    AssetBundle() = default;

    [[nodiscard]] std::optional<std::string> make_key(const std::filesystem::path &path) const;

    std::filesystem::path root_dir_;
    std::vector<Entry> entries_;
    uint64_t payload_offset_ = 0;
    uint64_t file_size_ = 0;
    mutable std::ifstream stream_;
    mutable std::mutex mutex_;
};

} // namespace parser_detail

} // namespace esp_brookesia::gui
