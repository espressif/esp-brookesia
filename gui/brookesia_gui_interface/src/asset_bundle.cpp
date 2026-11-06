/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/asset_bundle.hpp"

#include <algorithm>
#include <cstring>
#include <system_error>

namespace esp_brookesia::gui {

namespace parser_detail {

namespace {

constexpr char kMagic[4] = {'B', 'G', 'B', '1'};
constexpr uint32_t kVersion = 1;
constexpr size_t kHeaderSize = 24;
// The index is a small fraction of the payload (about 1.3%); this cap only
// exists so a corrupt header cannot ask for an unbounded allocation.
constexpr uint32_t kMaxIndexBytes = 1024 * 1024;

uint32_t read_u32_le(const uint8_t *bytes)
{
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

} // namespace

std::expected<std::shared_ptr<AssetBundle>, std::string> AssetBundle::open(
    const std::filesystem::path &bundle_path)
{
    std::ifstream stream(bundle_path, std::ios::binary);
    if (!stream) {
        return std::unexpected("Asset bundle is missing: " + bundle_path.generic_string());
    }

    stream.seekg(0, std::ios::end);
    const auto file_size = stream.tellg();
    if (file_size < static_cast<std::streamoff>(kHeaderSize)) {
        return std::unexpected("Asset bundle is shorter than its header");
    }
    stream.seekg(0, std::ios::beg);

    uint8_t header[kHeaderSize];
    stream.read(reinterpret_cast<char *>(header), sizeof(header));
    if (stream.gcount() != static_cast<std::streamsize>(sizeof(header))) {
        return std::unexpected("Asset bundle header could not be read");
    }
    if (std::memcmp(header, kMagic, sizeof(kMagic)) != 0) {
        return std::unexpected("Asset bundle has an unknown magic");
    }
    const uint32_t version = read_u32_le(header + 4);
    if (version != kVersion) {
        return std::unexpected("Asset bundle version " + std::to_string(version) + " is not supported");
    }
    const uint32_t count = read_u32_le(header + 12);
    const uint32_t index_size = read_u32_le(header + 16);
    const uint32_t payload_offset = read_u32_le(header + 20);
    if (index_size > kMaxIndexBytes) {
        return std::unexpected("Asset bundle index is unreasonably large");
    }
    if (payload_offset < kHeaderSize + index_size || payload_offset > static_cast<uint64_t>(file_size)) {
        return std::unexpected("Asset bundle index and payload overlap");
    }

    std::string index(index_size, '\0');
    if (index_size > 0) {
        stream.read(index.data(), static_cast<std::streamsize>(index_size));
        if (stream.gcount() != static_cast<std::streamsize>(index_size)) {
            return std::unexpected("Asset bundle index could not be read");
        }
    }

    auto bundle = std::shared_ptr<AssetBundle>(new AssetBundle());
    bundle->root_dir_ = bundle_path.parent_path().lexically_normal();
    bundle->payload_offset_ = payload_offset;
    bundle->file_size_ = static_cast<uint64_t>(file_size);
    bundle->entries_.reserve(count);

    const auto *cursor = reinterpret_cast<const uint8_t *>(index.data());
    const auto *end = cursor + index.size();
    for (uint32_t i = 0; i < count; ++i) {
        if (end - cursor < 4) {
            return std::unexpected("Asset bundle index is truncated");
        }
        const uint32_t path_len = read_u32_le(cursor);
        cursor += 4;
        if (static_cast<size_t>(end - cursor) < path_len + 12) {
            return std::unexpected("Asset bundle entry is truncated");
        }
        Entry entry;
        entry.key.assign(reinterpret_cast<const char *>(cursor), path_len);
        cursor += path_len;
        entry.offset = read_u32_le(cursor);
        entry.size = read_u32_le(cursor + 4);
        entry.flags = read_u32_le(cursor + 8);
        cursor += 12;
        if (bundle->payload_offset_ + entry.offset + entry.size > bundle->file_size_) {
            return std::unexpected("Asset bundle entry points past the end of the file: " + entry.key);
        }
        bundle->entries_.push_back(std::move(entry));
    }

    bundle->stream_ = std::move(stream);
    return bundle;
}

std::optional<std::string> AssetBundle::make_key(const std::filesystem::path &path) const
{
    const std::string full = path.lexically_normal().generic_string();
    const std::string root = root_dir_.generic_string();
    if (full.size() <= root.size() || full.compare(0, root.size(), root) != 0) {
        return std::nullopt;
    }
    if (full[root.size()] != '/') {
        return std::nullopt;
    }
    return full.substr(root.size() + 1);
}

std::optional<std::string> AssetBundle::read(const std::filesystem::path &path) const
{
    const auto key = make_key(path);
    if (!key.has_value()) {
        return std::nullopt;
    }

    // The packer writes entries in sorted order, so a binary search keeps the
    // lookup cost independent of the tree size.
    const auto it = std::lower_bound(
                        entries_.begin(), entries_.end(), *key,
    [](const Entry & entry, const std::string & value) {
        return entry.key < value;
    });
    if (it == entries_.end() || it->key != *key) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    stream_.clear();
    stream_.seekg(static_cast<std::streamoff>(payload_offset_ + it->offset), std::ios::beg);
    if (!stream_) {
        return std::nullopt;
    }
    std::string text(it->size, '\0');
    if (it->size > 0) {
        stream_.read(text.data(), static_cast<std::streamsize>(it->size));
        if (stream_.gcount() != static_cast<std::streamsize>(it->size)) {
            return std::nullopt;
        }
    }
    return text;
}

} // namespace parser_detail

} // namespace esp_brookesia::gui
