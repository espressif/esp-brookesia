/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/props_impl.hpp"

#include <fstream>

#include "brookesia/lib_utils/platform.hpp"
#include "brookesia/gui_interface/asset_source.hpp"

namespace esp_brookesia::gui::lvgl::props_detail {



uint32_t read_be32(const uint8_t *data)
{
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

bool is_lvgl_bin_path(std::string_view path)
{
    return path.ends_with(".bin") || path.ends_with(".BIN");
}

bool is_png_path(std::string_view path)
{
    return path.ends_with(".png") || path.ends_with(".PNG");
}

bool is_jpeg_path(std::string_view path)
{
    return path.ends_with(".jpg") || path.ends_with(".JPG") ||
           path.ends_with(".jpeg") || path.ends_with(".JPEG");
}

bool is_jpeg_decoder_available()
{
#if defined(ESP_PLATFORM)
#   if defined(CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER) && CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
    return true;
#   else
    return false;
#   endif
#else
#   if defined(LV_USE_TJPGD) && LV_USE_TJPGD && defined(LV_USE_FS_MEMFS) && LV_USE_FS_MEMFS
    return true;
#   else
    return false;
#   endif
#endif
}

std::expected<lv_image_header_t, std::string> make_png_image_header(
    const uint8_t *data, size_t data_size, const std::string &path
)
{
    static constexpr size_t PNG_HEADER_SIZE = 24;
    static constexpr uint8_t PNG_MAGIC[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};

    if (data_size < PNG_HEADER_SIZE) {
        return std::unexpected("PNG image is too small: " + path);
    }
    if (std::memcmp(data, PNG_MAGIC, sizeof(PNG_MAGIC)) != 0) {
        return std::unexpected("Invalid PNG image magic: " + path);
    }

    const auto width = read_be32(data + 16);
    const auto height = read_be32(data + 20);
    if (width == 0 || height == 0 ||
            width > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
            height > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
        return std::unexpected("Invalid PNG image dimensions: " + path);
    }

    lv_image_header_t header {};
    header.magic = LV_IMAGE_HEADER_MAGIC;
    header.cf = LV_COLOR_FORMAT_RAW_ALPHA;
    header.flags = 0;
    header.w = static_cast<int32_t>(width);
    header.h = static_cast<int32_t>(height);
    header.stride = 0;
    header.reserved_2 = 0;
    return header;
}

std::expected<lv_image_header_t, std::string> read_png_image_header(const std::string &path)
{
    static constexpr size_t PNG_HEADER_SIZE = 24;

    uint8_t header_data[PNG_HEADER_SIZE] {};
    auto read_result = StorageHelper::fs_read(path, service::RawBuffer(header_data, sizeof(header_data)));
    if (!read_result) {
        return std::unexpected("Failed to read PNG image header: " + path + ", error: " + read_result.error());
    }
    if (read_result.value() != sizeof(header_data)) {
        return std::unexpected("PNG image header read size mismatch: " + path);
    }
    return make_png_image_header(header_data, sizeof(header_data), path);
}

bool is_jpeg_start_of_frame_marker(uint8_t marker)
{
    switch (marker) {
    case 0xc0:
    case 0xc1:
    case 0xc2:
    case 0xc3:
    case 0xc5:
    case 0xc6:
    case 0xc7:
    case 0xc9:
    case 0xca:
    case 0xcb:
    case 0xcd:
    case 0xce:
    case 0xcf:
        return true;
    default:
        return false;
    }
}

std::expected<lv_image_header_t, std::string> make_jpeg_image_header(
    const uint8_t *data, size_t data_size, const std::string &path
)
{
    static constexpr size_t JPEG_MINIMUM_SIZE = 4;
    static constexpr uint8_t JPEG_MARKER_PREFIX = 0xff;
    static constexpr uint8_t JPEG_START_OF_IMAGE = 0xd8;
    static constexpr uint8_t JPEG_START_OF_SCAN = 0xda;
    static constexpr uint8_t JPEG_END_OF_IMAGE = 0xd9;

    if (data_size < JPEG_MINIMUM_SIZE || data[0] != JPEG_MARKER_PREFIX || data[1] != JPEG_START_OF_IMAGE) {
        return std::unexpected("Invalid JPEG image magic: " + path);
    }

    size_t offset = 2;
    while (offset < data_size) {
        while (offset < data_size && data[offset] != JPEG_MARKER_PREFIX) {
            ++offset;
        }
        while (offset < data_size && data[offset] == JPEG_MARKER_PREFIX) {
            ++offset;
        }
        if (offset >= data_size) {
            break;
        }

        const uint8_t marker = data[offset++];
        if (marker == JPEG_END_OF_IMAGE || marker == JPEG_START_OF_SCAN) {
            break;
        }
        if (marker == 0x00 || marker == JPEG_START_OF_IMAGE || marker == 0x01 ||
                (marker >= 0xd0 && marker <= 0xd7)) {
            continue;
        }
        if (offset + 2 > data_size) {
            return std::unexpected("Truncated JPEG image segment: " + path);
        }

        const size_t segment_size = (static_cast<size_t>(data[offset]) << 8) | data[offset + 1];
        if (segment_size < 2 || segment_size > data_size - offset) {
            return std::unexpected("Invalid JPEG image segment size: " + path);
        }
        if (is_jpeg_start_of_frame_marker(marker)) {
            if (segment_size < 7) {
                return std::unexpected("Invalid JPEG image frame header: " + path);
            }
            const uint32_t height = (static_cast<uint32_t>(data[offset + 3]) << 8) | data[offset + 4];
            const uint32_t width = (static_cast<uint32_t>(data[offset + 5]) << 8) | data[offset + 6];
            if (width == 0 || height == 0) {
                return std::unexpected("Invalid JPEG image dimensions: " + path);
            }

            lv_image_header_t header {};
            header.magic = LV_IMAGE_HEADER_MAGIC;
            header.cf = LV_COLOR_FORMAT_RAW;
            header.flags = 0;
            header.w = static_cast<int32_t>(width);
            header.h = static_cast<int32_t>(height);
            header.stride = 0;
            header.reserved_2 = 0;
            return header;
        }
        offset += segment_size;
    }
    return std::unexpected("JPEG image frame header is not found: " + path);
}

namespace {

// The storage service round-trip costs several milliseconds per file, which is what made
// image-heavy screens show up as 120-136 ms in the subtree build profile. Read the bytes
// directly when this context can touch the filesystem (PC builds and PSRAM-XIP targets,
// same gate the parser uses) and keep the service as the fallback.
bool can_read_image_file_locally()
{
    return esp_brookesia::lib_utils::can_read_local_files();
}

std::expected<std::string, std::string> read_image_file_locally(const std::string &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::unexpected("Failed to open image file: " + path);
    }
    std::string bytes;
    bytes.reserve(16384);
    char chunk[16384];
    while (input) {
        input.read(chunk, static_cast<std::streamsize>(sizeof(chunk)));
        const auto count = input.gcount();
        if (count > 0) {
            bytes.append(chunk, static_cast<size_t>(count));
        }
    }
    if (input.bad()) {
        return std::unexpected("Failed to read image file: " + path);
    }
    return bytes;
}

} // namespace

std::expected<std::vector<uint8_t>, std::string> read_storage_file_bytes(
    const std::string &path, std::string_view format_name
)
{
    std::expected<std::string, std::string> content = std::unexpected("Image read not attempted");
    if (auto bundled = esp_brookesia::gui::read_bundled_asset(std::filesystem::path(path));
            bundled.has_value()) {
        content = std::move(*bundled);
    } else if (can_read_image_file_locally()) {
        content = read_image_file_locally(path);
        if (!content) {
            content = StorageHelper::fs_read_text(path);
        }
    } else {
        content = StorageHelper::fs_read_text(path);
    }
    if (!content) {
        return std::unexpected(
                   "Failed to read " + std::string(format_name) + " image file: " + path +
                   ", error: " + content.error()
               );
    }
    if (content->size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
        return std::unexpected(std::string(format_name) + " image is too large: " + path);
    }

    std::vector<uint8_t> data(content->size());
    if (!content->empty()) {
        std::memcpy(data.data(), content->data(), content->size());
    }
    return data;
}

std::expected<std::shared_ptr<BinaryImageSource>, std::string> load_binary_image_source(const std::string &path)
{
    auto data = read_storage_file_bytes(path, "LVGL bin");
    if (!data) {
        return std::unexpected(data.error());
    }
    if (data->size() < 12) {
        return std::unexpected("Invalid LVGL image bin size: " + path);
    }

    auto source = std::make_shared<BinaryImageSource>();
    source->data = std::move(data.value());
    if (source->data[0] != LV_IMAGE_HEADER_MAGIC) {
        return std::unexpected("Invalid LVGL image bin magic: " + path);
    }

    source->descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    source->descriptor.header.cf = source->data[1];
    source->descriptor.header.flags = read_le16(source->data, 2);
    source->descriptor.header.w = read_le16(source->data, 4);
    source->descriptor.header.h = read_le16(source->data, 6);
    source->descriptor.header.stride = read_le16(source->data, 8);
    source->descriptor.header.reserved_2 = 0;
    source->descriptor.data_size = static_cast<uint32_t>(source->data.size() - 12);
    source->descriptor.data = source->data.data() + 12;
    source->descriptor.reserved = nullptr;
    source->descriptor.reserved_2 = nullptr;
    return source;
}

std::expected<std::shared_ptr<BinaryImageSource>, std::string> load_encoded_png_image_source(
    const std::string &path)
{
    auto data = read_storage_file_bytes(path, "PNG");
    if (!data) {
        return std::unexpected(data.error());
    }

    auto source = std::make_shared<BinaryImageSource>();
    source->data = std::move(data.value());

    auto header = make_png_image_header(source->data.data(), source->data.size(), path);
    if (!header) {
        return std::unexpected(header.error());
    }

    source->descriptor.header = *header;
    source->descriptor.data_size = static_cast<uint32_t>(source->data.size());
    source->descriptor.data = source->data.data();
    source->descriptor.reserved = nullptr;
    source->descriptor.reserved_2 = nullptr;
    return source;
}

std::expected<std::shared_ptr<BinaryImageSource>, std::string> load_encoded_jpeg_image_source(
    const std::string &path)
{
    if (!is_jpeg_decoder_available()) {
        return std::unexpected("JPEG image decoder is not available: " + path);
    }

    auto data = read_storage_file_bytes(path, "JPEG");
    if (!data) {
        return std::unexpected(data.error());
    }

    auto source = std::make_shared<BinaryImageSource>();
    source->data = std::move(data.value());

    auto header = make_jpeg_image_header(source->data.data(), source->data.size(), path);
    if (!header) {
        return std::unexpected(header.error());
    }

    source->descriptor.header = *header;
    source->descriptor.data_size = static_cast<uint32_t>(source->data.size());
    source->descriptor.data = source->data.data();
    source->descriptor.reserved = nullptr;
    source->descriptor.reserved_2 = nullptr;
    return source;
}
}
