/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_core/app/package.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "boost/json.hpp"
#include "zlib.h"

#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

namespace esp_brookesia::system::core {
namespace {

namespace fs = std::filesystem;

constexpr const char *APP_MANIFEST_FILE = "manifest.json";
constexpr const char *APP_RUNTIME_PROFILE_FILE = "profile.json";
constexpr const char *UNSUPPORTED_SYSTEM_ERROR_PREFIX = "Package does not support system type";
constexpr uint32_t ZIP_EOCD_SIGNATURE = 0x06054b50U;
constexpr uint32_t ZIP_CENTRAL_FILE_SIGNATURE = 0x02014b50U;
constexpr uint32_t ZIP_LOCAL_FILE_SIGNATURE = 0x04034b50U;
constexpr uint16_t ZIP_METHOD_STORE = 0;
constexpr uint16_t ZIP_METHOD_DEFLATE = 8;

struct ZipEntry {
    std::string name;
    uint16_t compression_method = 0;
    uint16_t flags = 0;
    uint32_t compressed_size = 0;
    uint32_t uncompressed_size = 0;
    uint32_t local_header_offset = 0;
    bool is_directory = false;
};

bool is_embedded_vfs_path(const fs::path &path)
{
    const std::string path_str = path.generic_string();
    return path_str == "/littlefs" || path_str.rfind("/littlefs/", 0) == 0 ||
           path_str == "/spiffs" || path_str.rfind("/spiffs/", 0) == 0;
}

std::expected<void, std::string> ensure_directory_tree(const fs::path &dir, std::string_view context)
{
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (!ec) {
        return {};
    }

    if (is_embedded_vfs_path(dir)) {
        return {};
    }

    return std::unexpected(
               "Failed to create directory(" + std::string(context) + "): " + dir.string() + ", error: " + ec.message()
           );
}

std::expected<std::string, std::string> read_text_file(const fs::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::unexpected("Failed to open file: " + path.string());
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::expected<std::vector<uint8_t>, std::string> read_binary_file(const fs::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::unexpected("Failed to open package file: " + path.string());
    }

    file.seekg(0, std::ios::end);
    const std::streamoff end_pos = file.tellg();
    if (end_pos < 0) {
        return std::unexpected("Failed to get package file size: " + path.string());
    }
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(end_pos));
    if (!data.empty()) {
        file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!file) {
            return std::unexpected("Failed to read package file: " + path.string());
        }
    }
    return data;
}

bool check_range(size_t offset, size_t size, size_t total_size)
{
    return (offset <= total_size) && (size <= (total_size - offset));
}

uint16_t read_u16_le(const std::vector<uint8_t> &data, size_t offset)
{
    return static_cast<uint16_t>(data[offset]) |
           static_cast<uint16_t>(static_cast<uint16_t>(data[offset + 1]) << 8U);
}

uint32_t read_u32_le(const std::vector<uint8_t> &data, size_t offset)
{
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8U) |
           (static_cast<uint32_t>(data[offset + 2]) << 16U) |
           (static_cast<uint32_t>(data[offset + 3]) << 24U);
}

std::optional<size_t> find_eocd_offset(const std::vector<uint8_t> &zip_data)
{
    if (zip_data.size() < 22) {
        return std::nullopt;
    }

    const size_t min_offset = zip_data.size() > (22 + 0xFFFF) ? (zip_data.size() - (22 + 0xFFFF)) : 0;
    for (size_t offset = zip_data.size() - 22; offset >= min_offset; --offset) {
        if (!check_range(offset, 4, zip_data.size())) {
            return std::nullopt;
        }
        if (read_u32_le(zip_data, offset) == ZIP_EOCD_SIGNATURE) {
            return offset;
        }
        if (offset == 0) {
            break;
        }
    }
    return std::nullopt;
}

std::expected<std::vector<ZipEntry>, std::string> parse_central_directory(const std::vector<uint8_t> &zip_data)
{
    auto eocd_offset = find_eocd_offset(zip_data);
    if (!eocd_offset) {
        return std::unexpected("ZIP end of central directory not found");
    }
    const size_t eocd = eocd_offset.value();
    if (!check_range(eocd, 22, zip_data.size())) {
        return std::unexpected("Invalid ZIP EOCD size");
    }

    const uint16_t total_entries = read_u16_le(zip_data, eocd + 10);
    const uint32_t central_dir_size = read_u32_le(zip_data, eocd + 12);
    const uint32_t central_dir_offset = read_u32_le(zip_data, eocd + 16);
    if (!check_range(static_cast<size_t>(central_dir_offset), static_cast<size_t>(central_dir_size), zip_data.size())) {
        return std::unexpected("Invalid ZIP central directory range");
    }

    size_t cursor = static_cast<size_t>(central_dir_offset);
    std::vector<ZipEntry> entries;
    entries.reserve(total_entries);

    for (uint16_t index = 0; index < total_entries; ++index) {
        if (!check_range(cursor, 46, zip_data.size())) {
            return std::unexpected("Invalid ZIP central directory header");
        }
        if (read_u32_le(zip_data, cursor) != ZIP_CENTRAL_FILE_SIGNATURE) {
            return std::unexpected("Invalid ZIP central directory signature");
        }

        ZipEntry entry;
        entry.flags = read_u16_le(zip_data, cursor + 8);
        entry.compression_method = read_u16_le(zip_data, cursor + 10);
        entry.compressed_size = read_u32_le(zip_data, cursor + 20);
        entry.uncompressed_size = read_u32_le(zip_data, cursor + 24);
        const uint16_t file_name_len = read_u16_le(zip_data, cursor + 28);
        const uint16_t extra_len = read_u16_le(zip_data, cursor + 30);
        const uint16_t comment_len = read_u16_le(zip_data, cursor + 32);
        entry.local_header_offset = read_u32_le(zip_data, cursor + 42);

        const size_t file_name_offset = cursor + 46;
        if (!check_range(file_name_offset, static_cast<size_t>(file_name_len), zip_data.size())) {
            return std::unexpected("Invalid ZIP file name range");
        }
        entry.name.assign(
            reinterpret_cast<const char *>(zip_data.data() + file_name_offset),
            static_cast<size_t>(file_name_len)
        );
        entry.is_directory = !entry.name.empty() && entry.name.back() == '/';

        const size_t advance = 46 + static_cast<size_t>(file_name_len) + static_cast<size_t>(extra_len) +
                               static_cast<size_t>(comment_len);
        if (!check_range(cursor, advance, zip_data.size())) {
            return std::unexpected("Invalid ZIP central directory entry size");
        }
        cursor += advance;
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::expected<std::vector<uint8_t>, std::string> inflate_raw_deflate(
    const uint8_t *compressed_data, size_t compressed_size, size_t uncompressed_size
)
{
    std::vector<uint8_t> output(uncompressed_size);
    z_stream stream{};
    stream.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(compressed_data));
    stream.avail_in = static_cast<uInt>(compressed_size);
    stream.next_out = reinterpret_cast<Bytef *>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());

    int result = inflateInit2(&stream, -MAX_WBITS);
    if (result != Z_OK) {
        return std::unexpected("zlib inflateInit2 failed");
    }

    result = inflate(&stream, Z_FINISH);
    const bool ok = (result == Z_STREAM_END) && (stream.total_out == output.size());
    inflateEnd(&stream);
    if (!ok) {
        return std::unexpected("zlib inflate failed");
    }
    return output;
}

std::expected<std::vector<uint8_t>, std::string> extract_zip_entry(
    const std::vector<uint8_t> &zip_data, const ZipEntry &entry
)
{
    const size_t local_header = static_cast<size_t>(entry.local_header_offset);
    if (!check_range(local_header, 30, zip_data.size())) {
        return std::unexpected("Invalid ZIP local file header range");
    }
    if (read_u32_le(zip_data, local_header) != ZIP_LOCAL_FILE_SIGNATURE) {
        return std::unexpected("Invalid ZIP local file signature");
    }

    const uint16_t local_file_name_len = read_u16_le(zip_data, local_header + 26);
    const uint16_t local_extra_len = read_u16_le(zip_data, local_header + 28);
    const size_t data_offset = local_header + 30 + static_cast<size_t>(local_file_name_len) +
                               static_cast<size_t>(local_extra_len);
    if (!check_range(data_offset, static_cast<size_t>(entry.compressed_size), zip_data.size())) {
        return std::unexpected("Invalid ZIP compressed data range");
    }

    const uint8_t *compressed_data = zip_data.data() + data_offset;
    switch (entry.compression_method) {
    case ZIP_METHOD_STORE: {
        std::vector<uint8_t> output(static_cast<size_t>(entry.compressed_size));
        if (!output.empty()) {
            std::memcpy(output.data(), compressed_data, output.size());
        }
        return output;
    }
    case ZIP_METHOD_DEFLATE:
        return inflate_raw_deflate(
                   compressed_data,
                   static_cast<size_t>(entry.compressed_size),
                   static_cast<size_t>(entry.uncompressed_size)
               );
    default:
        return std::unexpected("Unsupported ZIP compression method: " + std::to_string(entry.compression_method));
    }
}

bool is_safe_relative_path(std::string_view path)
{
    if (path.empty() || path.front() == '/' || path.front() == '\\') {
        return false;
    }

    const auto normalized = fs::path(std::string(path)).lexically_normal();
    if (normalized.empty() || normalized.is_absolute()) {
        return false;
    }
    for (const auto &part : normalized) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

std::expected<const boost::json::object *, std::string> get_object(
    const boost::json::object &object,
    std::string_view key
)
{
    auto it = object.find(key);
    if ((it == object.end()) || !it->value().is_object()) {
        return std::unexpected("manifest." + std::string(key) + " must be an object");
    }
    return &it->value().as_object();
}

std::expected<std::string, std::string> get_required_string(
    const boost::json::object &object,
    std::string_view path,
    std::string_view key
)
{
    auto it = object.find(key);
    if ((it == object.end()) || !it->value().is_string()) {
        return std::unexpected("manifest." + std::string(path) + "." + std::string(key) + " must be a string");
    }
    std::string value(it->value().as_string());
    if (value.empty()) {
        return std::unexpected("manifest." + std::string(path) + "." + std::string(key) + " must not be empty");
    }
    return value;
}

std::expected<std::string, std::string> get_optional_string(
    const boost::json::object &object,
    std::string_view path,
    std::string_view key,
    std::string default_value = {}
)
{
    auto it = object.find(key);
    if ((it == object.end()) || it->value().is_null()) {
        return default_value;
    }
    if (!it->value().is_string()) {
        return std::unexpected("manifest." + std::string(path) + "." + std::string(key) + " must be a string");
    }
    return std::string(it->value().as_string());
}

std::expected<bool, std::string> get_optional_bool(
    const boost::json::object &object,
    std::string_view path,
    std::string_view key,
    bool default_value
)
{
    auto it = object.find(key);
    if ((it == object.end()) || it->value().is_null()) {
        return default_value;
    }
    if (!it->value().is_bool()) {
        return std::unexpected("manifest." + std::string(path) + "." + std::string(key) + " must be a bool");
    }
    return it->value().as_bool();
}

std::expected<int32_t, std::string> get_optional_int32(
    const boost::json::object &object,
    std::string_view path,
    std::string_view key,
    int32_t default_value
)
{
    auto it = object.find(key);
    if ((it == object.end()) || it->value().is_null()) {
        return default_value;
    }

    int64_t value = 0;
    if (it->value().is_int64()) {
        value = it->value().as_int64();
    } else if (it->value().is_uint64()) {
        const auto unsigned_value = it->value().as_uint64();
        if (unsigned_value > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
            return std::unexpected(
                       "manifest." + std::string(path) + "." + std::string(key) + " is out of int32 range"
                   );
        }
        value = static_cast<int64_t>(unsigned_value);
    } else {
        return std::unexpected("manifest." + std::string(path) + "." + std::string(key) + " must be an integer");
    }

    if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
        return std::unexpected("manifest." + std::string(path) + "." + std::string(key) + " is out of int32 range");
    }
    return static_cast<int32_t>(value);
}

std::expected<std::vector<std::string>, std::string> get_optional_string_array(
    const boost::json::object &object,
    std::string_view path,
    std::string_view key
)
{
    std::vector<std::string> values;
    auto it = object.find(key);
    if ((it == object.end()) || it->value().is_null()) {
        return values;
    }
    if (!it->value().is_array()) {
        return std::unexpected("manifest." + std::string(path) + "." + std::string(key) + " must be an array");
    }
    for (const auto &item : it->value().as_array()) {
        if (!item.is_string()) {
            return std::unexpected("manifest." + std::string(path) + "." + std::string(key) + " item must be a string");
        }
        std::string value(item.as_string());
        if (value.empty()) {
            return std::unexpected(
                       "manifest." + std::string(path) + "." + std::string(key) + " item must not be empty"
                   );
        }
        values.emplace_back(std::move(value));
    }
    return values;
}

std::expected<std::map<std::string, std::string>, std::string> get_optional_localized_names(
    const boost::json::object &object,
    std::string_view path,
    std::string_view key
)
{
    std::map<std::string, std::string> names;
    auto it = object.find(key);
    if ((it == object.end()) || it->value().is_null()) {
        return names;
    }
    if (it->value().is_string()) {
        return std::unexpected(
                   "manifest." + std::string(path) + "." + std::string(key) +
                   " string form is no longer supported; use an object such as {\"en\":\"App\"}"
               );
    }
    if (!it->value().is_object()) {
        return std::unexpected("manifest." + std::string(path) + "." + std::string(key) + " must be an object");
    }

    for (const auto &[language, value] : it->value().as_object()) {
        if (language.empty()) {
            return std::unexpected(
                       "manifest." + std::string(path) + "." + std::string(key) + " language key must not be empty"
                   );
        }
        if (!value.is_string()) {
            return std::unexpected(
                       "manifest." + std::string(path) + "." + std::string(key) +
                       "." + std::string(language) + " must be a string"
                   );
        }
        std::string name(value.as_string());
        if (name.empty()) {
            return std::unexpected(
                       "manifest." + std::string(path) + "." + std::string(key) +
                       "." + std::string(language) + " must not be empty"
                   );
        }
        names.emplace(std::string(language), std::move(name));
    }
    return names;
}

std::expected<runtime::BackendType, std::string> parse_backend_type(std::string_view type)
{
    runtime::BackendType parsed = runtime::BackendType::Unknown;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(type), parsed) ||
            (parsed == runtime::BackendType::Unknown)) {
        return std::unexpected("Invalid runtime type in manifest: " + std::string(type));
    }
    return parsed;
}

template <size_t N>
std::expected<void, std::string> reject_unknown_manifest_fields(
    const boost::json::object &object,
    std::string_view owner,
    const std::array<std::string_view, N> &allowed_fields
)
{
    for (const auto &[key, value] : object) {
        (void)value;
        const auto key_view = std::string_view(key.data(), key.size());
        if (std::find(allowed_fields.begin(), allowed_fields.end(), key_view) == allowed_fields.end()) {
            return std::unexpected(
                       "manifest." + std::string(owner) + " contains unsupported field: " + std::string(key_view)
                   );
        }
    }
    return {};
}

std::expected<GuiAppLayer, std::string> parse_gui_app_layer(std::string_view layer)
{
    GuiAppLayer parsed = GuiAppLayer::AppDefault;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(layer), parsed)) {
        return std::unexpected("Invalid gui layer in manifest: " + std::string(layer));
    }
    return parsed;
}

std::expected<gui::MountStackMode, std::string> parse_gui_mount_mode(std::string_view mount_mode)
{
    gui::MountStackMode parsed = gui::MountStackMode::Replace;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(mount_mode), parsed) ||
            parsed == gui::MountStackMode::Max) {
        return std::unexpected("Invalid gui screen flow mount_mode in manifest: " + std::string(mount_mode));
    }
    return parsed;
}

std::expected<std::vector<GuiScreenFlowEntry>, std::string> get_gui_screen_flows(
    const boost::json::object &object,
    std::string_view owner,
    bool required,
    bool require_layer
)
{
    std::vector<GuiScreenFlowEntry> flows;
    auto it = object.find("screen_flows");
    if ((it == object.end()) || it->value().is_null()) {
        if (required) {
            return std::unexpected(std::string(owner) + ".screen_flows must be an array");
        }
        return flows;
    }
    if (!it->value().is_array()) {
        return std::unexpected(std::string(owner) + ".screen_flows must be an array");
    }
    for (const auto &item : it->value().as_array()) {
        if (!item.is_object()) {
            return std::unexpected(std::string(owner) + ".screen_flows item must be an object");
        }
        const auto &entry = item.as_object();
        const std::string entry_owner = std::string(owner) + ".screen_flows[]";
        auto screen_flow = get_required_string(entry, entry_owner, "screen_flow");
        if (!screen_flow) {
            return std::unexpected(screen_flow.error());
        }
        std::expected<std::string, std::string> layer =
            require_layer ? get_required_string(entry, entry_owner, "layer") :
            get_optional_string(entry, entry_owner, "layer", "AppDefault");
        if (!layer) {
            return std::unexpected(layer.error());
        }
        auto parsed_layer = parse_gui_app_layer(*layer);
        if (!parsed_layer) {
            return std::unexpected(parsed_layer.error());
        }
        auto mount_mode = get_optional_string(entry, entry_owner, "mount_mode", "Replace");
        if (!mount_mode) {
            return std::unexpected(mount_mode.error());
        }
        auto parsed_mount_mode = parse_gui_mount_mode(*mount_mode);
        if (!parsed_mount_mode) {
            return std::unexpected(parsed_mount_mode.error());
        }
        auto z_order = get_optional_int32(entry, entry_owner, "z_order", 0);
        if (!z_order) {
            return std::unexpected(z_order.error());
        }
        flows.push_back(GuiScreenFlowEntry{
            .screen_flow = *screen_flow,
            .layer = *parsed_layer,
            .mount_mode = *parsed_mount_mode,
            .z_order = *z_order,
        });
    }
    if (required && flows.empty()) {
        return std::unexpected(std::string(owner) + ".screen_flows must not be empty");
    }
    return flows;
}

bool supports_system(const std::vector<std::string> &systems, std::string_view system_type)
{
    if (system_type.empty() || systems.empty()) {
        return true;
    }
    return std::find(systems.begin(), systems.end(), std::string(system_type)) != systems.end();
}

std::expected<AppManifest, std::string> parse_manifest_to_app_manifest(
    const std::string &manifest_json,
    const fs::path &package_dir,
    std::string_view system_type
)
{
    boost::system::error_code error_code;
    auto parsed = boost::json::parse(manifest_json, error_code);
    if (error_code || !parsed.is_object()) {
        return std::unexpected("manifest.json must be a JSON object");
    }

    const auto &root = parsed.as_object();
    auto package = get_object(root, "package");
    if (!package) {
        return std::unexpected(package.error());
    }
    auto runtime = get_object(root, "runtime");
    if (!runtime) {
        return std::unexpected(runtime.error());
    }
    auto root_fields = reject_unknown_manifest_fields(root, "json", std::array<std::string_view, 2> {
        "package",
        "runtime",
    });
    auto package_fields = reject_unknown_manifest_fields(**package, "package", std::array<std::string_view, 5> {
        "id",
        "name",
        "version",
        "visible",
        "systems",
    });
    auto runtime_fields = reject_unknown_manifest_fields(**runtime, "runtime", std::array<std::string_view, 4> {
        "type",
        "entry",
        "resource_dir",
        "arguments",
    });
    if (!root_fields || !package_fields || !runtime_fields) {
        return std::unexpected(
                   !root_fields ? root_fields.error() :
                   !package_fields ? package_fields.error() :
                   runtime_fields.error()
               );
    }

    auto id = get_required_string(**package, "package", "id");
    auto version = get_required_string(**package, "package", "version");
    auto localized_names = get_optional_localized_names(**package, "package", "name");
    auto visible = get_optional_bool(**package, "package", "visible", true);
    auto systems = get_optional_string_array(**package, "package", "systems");
    auto runtime_type_string = get_required_string(**runtime, "runtime", "type");
    auto entry = get_required_string(**runtime, "runtime", "entry");
    auto resource_dir = get_optional_string(**runtime, "runtime", "resource_dir");
    auto arguments = get_optional_string_array(**runtime, "runtime", "arguments");
    if (!id || !version || !localized_names || !visible || !systems ||
            !runtime_type_string || !entry || !resource_dir || !arguments) {
        return std::unexpected(
                   !id ? id.error() :
                   !version ? version.error() :
                   !localized_names ? localized_names.error() :
                   !visible ? visible.error() :
                   !systems ? systems.error() :
                   !runtime_type_string ? runtime_type_string.error() :
                   !entry ? entry.error() :
                   !resource_dir ? resource_dir.error() :
                   arguments.error()
               );
    }
    if (!supports_system(*systems, system_type)) {
        return std::unexpected(std::string(UNSUPPORTED_SYSTEM_ERROR_PREFIX) + ": " + std::string(system_type));
    }
    if (!is_safe_relative_path(*entry)) {
        return std::unexpected("manifest.runtime.entry must be a safe relative path");
    }
    if (!resource_dir->empty() && !is_safe_relative_path(*resource_dir)) {
        return std::unexpected("manifest.runtime.resource_dir must be a safe relative path");
    }
    auto runtime_type = parse_backend_type(*runtime_type_string);
    if (!runtime_type) {
        return std::unexpected(runtime_type.error());
    }

    AppManifest manifest;
    manifest.id = *id;
    manifest.localized_names = std::move(*localized_names);
    manifest.name = resolve_app_display_name(manifest, "en");
    manifest.version = *version;
    manifest.kind = AppKind::Runtime;
    manifest.visible = *visible;
    manifest.supported_systems = *systems;
    manifest.runtime_type = *runtime_type;
    manifest.app_path = package_dir.generic_string();
    manifest.entry = *entry;
    manifest.resource_dir = *resource_dir;
    manifest.arguments = *arguments;

    return manifest;
}

std::expected<RuntimeAppResourceDescriptor, std::string> parse_runtime_app_resource_descriptor(
    const std::string &descriptor_json,
    const fs::path &descriptor_path
)
{
    boost::system::error_code error_code;
    auto parsed = boost::json::parse(descriptor_json, error_code);
    if (error_code || !parsed.is_object()) {
        return std::unexpected("Runtime profile.json must be a JSON object: " + descriptor_path.string());
    }

    const auto &root = parsed.as_object();
    if (root.contains("version") || root.contains("assets") || root.contains("variants")) {
        return std::unexpected("Runtime profile.json must not be a JSON UI document");
    }

    std::string icon_id;
    auto icon_it = root.find("icon_id");
    if ((icon_it != root.end()) && !icon_it->value().is_null()) {
        if (!icon_it->value().is_string()) {
            return std::unexpected("Runtime profile.json icon_id must be a string");
        }
        icon_id = std::string(icon_it->value().as_string());
    }

    auto root_it = root.find("root");
    if ((root_it == root.end()) || root_it->value().is_null()) {
        if (auto flows_it = root.find("screen_flows"); (flows_it != root.end()) && !flows_it->value().is_null()) {
            return std::unexpected("Runtime profile.json screen_flows requires root");
        }
        return RuntimeAppResourceDescriptor{
            .icon_id = std::move(icon_id),
            .gui = {},
        };
    }
    if (!root_it->value().is_string()) {
        return std::unexpected("Runtime profile.json root must be a string");
    }
    std::string document_root(root_it->value().as_string());
    if (document_root.empty()) {
        return std::unexpected("Runtime profile.json root must not be empty");
    }
    if (!is_safe_relative_path(document_root)) {
        return std::unexpected("Runtime profile.json root must be a safe relative path");
    }
    auto screen_flows = get_gui_screen_flows(root, "root", true, true);
    if (!screen_flows) {
        return std::unexpected(screen_flows.error());
    }
    return RuntimeAppResourceDescriptor{
        .icon_id = std::move(icon_id),
        .gui = AppGuiDescriptor{
            .root_kind = GuiRootKind::File,
            .root = std::move(document_root),
            .resources = {},
            .screen_flows = std::move(*screen_flows),
        },
    };
}

std::expected<void, std::string> validate_runtime_gui_document(const fs::path &path)
{
    auto document_json = read_text_file(path);
    if (!document_json) {
        return std::unexpected("Runtime GUI document not found: " + path.string());
    }

    boost::system::error_code error_code;
    auto parsed = boost::json::parse(*document_json, error_code);
    if (error_code || !parsed.is_object()) {
        return std::unexpected("Runtime GUI document must be a JSON object: " + path.string());
    }
    const auto &root = parsed.as_object();
    if (auto app_it = root.find("app"); (app_it != root.end()) && !app_it->value().is_null()) {
        return std::unexpected("Runtime GUI document app must be null or omitted");
    }
    return {};
}

} // namespace

std::expected<AppManifest, std::string> unpack_app_package_to(
    std::string_view package_path,
    std::string_view install_dir,
    std::string_view system_type
)
{
    BROOKESIA_LOG_TRACE_GUARD();
    BROOKESIA_LOGD("Params: package(%1%), install_dir(%2%), system_type(%3%)", package_path, install_dir, system_type);

    const fs::path package_file{std::string{package_path}};
    const fs::path output_root{std::string{install_dir}};
    auto zip_data = read_binary_file(package_file);
    if (!zip_data) {
        return std::unexpected(zip_data.error());
    }

    auto entries = parse_central_directory(*zip_data);
    if (!entries) {
        return std::unexpected(entries.error());
    }

    std::optional<std::string> manifest_json;
    for (const auto &entry : *entries) {
        if (entry.is_directory) {
            continue;
        }
        if (entry.name == APP_MANIFEST_FILE) {
            auto file_data = extract_zip_entry(*zip_data, entry);
            if (!file_data) {
                return std::unexpected("Failed to extract manifest.json: " + file_data.error());
            }
            manifest_json = std::string(file_data->begin(), file_data->end());
            break;
        }
    }
    if (!manifest_json) {
        return std::unexpected("Package missing manifest.json");
    }

    auto manifest = parse_manifest_to_app_manifest(*manifest_json, output_root, system_type);
    if (!manifest) {
        return std::unexpected(manifest.error());
    }

    // Toolkit-generated bpk files follow the naming convention:
    //   <package.id>.<debug|release>.<version>.bpk
    // Unpack to <install_dir>/<package.id>/ for a deterministic directory name.
    const fs::path output_dir = output_root / manifest->id;

    auto ensure_output_dir = ensure_directory_tree(output_dir, "install_dir");
    if (!ensure_output_dir) {
        return std::unexpected(ensure_output_dir.error());
    }

    for (const auto &entry : *entries) {
        if (entry.is_directory) {
            continue;
        }
        if (entry.name.starts_with("META-INF/")) {
            continue;
        }
        if (!is_safe_relative_path(entry.name)) {
            return std::unexpected("Unsafe path in package: " + entry.name);
        }

        auto file_data = extract_zip_entry(*zip_data, entry);
        if (!file_data) {
            return std::unexpected("Failed to extract '" + entry.name + "': " + file_data.error());
        }

        fs::path destination = output_dir / fs::path(entry.name);
        auto ensure_parent_dir = ensure_directory_tree(destination.parent_path(), entry.name);
        if (!ensure_parent_dir) {
            return std::unexpected(ensure_parent_dir.error());
        }

        std::ofstream output(destination, std::ios::binary);
        if (!output) {
            return std::unexpected("Failed to open destination file: " + destination.string());
        }
        if (!file_data->empty()) {
            output.write(reinterpret_cast<const char *>(file_data->data()), static_cast<std::streamsize>(file_data->size()));
            if (!output) {
                return std::unexpected("Failed to write destination file: " + destination.string());
            }
        }
    }

    manifest->app_path = output_dir.generic_string();
    return *manifest;
}

std::expected<AppManifest, std::string> read_app_package_manifest(
    std::string_view package_path,
    std::string_view system_type
)
{
    BROOKESIA_LOG_TRACE_GUARD();
    BROOKESIA_LOGD("Params: package(%1%), system_type(%2%)", package_path, system_type);

    const fs::path package_file{std::string{package_path}};
    auto zip_data = read_binary_file(package_file);
    if (!zip_data) {
        return std::unexpected(zip_data.error());
    }

    auto entries = parse_central_directory(*zip_data);
    if (!entries) {
        return std::unexpected(entries.error());
    }

    for (const auto &entry : *entries) {
        if (entry.is_directory || entry.name != APP_MANIFEST_FILE) {
            continue;
        }
        auto file_data = extract_zip_entry(*zip_data, entry);
        if (!file_data) {
            return std::unexpected("Failed to extract manifest.json: " + file_data.error());
        }
        const std::string manifest_json(file_data->begin(), file_data->end());
        return parse_manifest_to_app_manifest(manifest_json, package_file.parent_path(), system_type);
    }

    return std::unexpected("Package missing manifest.json");
}

std::expected<AppManifest, std::string> read_unpacked_app_manifest(
    std::string_view package_dir,
    std::string_view system_type
)
{
    const fs::path app_dir{std::string(package_dir)};
    auto manifest_json = read_text_file(app_dir / APP_MANIFEST_FILE);
    if (!manifest_json) {
        return std::unexpected(manifest_json.error());
    }
    return parse_manifest_to_app_manifest(*manifest_json, app_dir, system_type);
}

std::expected<RuntimeAppResourceDescriptor, std::string> read_runtime_app_resource_descriptor(
    const AppManifest &manifest
)
{
    if (manifest.kind != AppKind::Runtime) {
        return std::unexpected("App manifest is not a runtime app");
    }
    if (manifest.app_path.empty()) {
        return std::unexpected("Runtime app manifest app_path is empty");
    }

    fs::path root_path = fs::path(manifest.app_path);
    if (!manifest.resource_dir.empty()) {
        root_path /= manifest.resource_dir;
    }
    root_path /= APP_RUNTIME_PROFILE_FILE;
    auto root_json = read_text_file(root_path);
    if (!root_json) {
        return std::unexpected(root_json.error());
    }
    auto descriptor = parse_runtime_app_resource_descriptor(*root_json, root_path);
    if (!descriptor) {
        return std::unexpected(descriptor.error());
    }
    if (descriptor->gui.root_kind == GuiRootKind::File) {
        auto document_result = validate_runtime_gui_document(root_path.parent_path() / descriptor->gui.root);
        if (!document_result) {
            return std::unexpected(document_result.error());
        }
    }
    return descriptor;
}

std::expected<AppGuiDescriptor, std::string> read_runtime_app_gui_descriptor(const AppManifest &manifest)
{
    auto descriptor = read_runtime_app_resource_descriptor(manifest);
    if (!descriptor) {
        return std::unexpected(descriptor.error());
    }
    return std::move(descriptor->gui);
}

std::expected<runtime::AppConfig, std::string> make_runtime_app_config(const AppManifest &manifest)
{
    if (manifest.kind != AppKind::Runtime) {
        return std::unexpected("App manifest is not a runtime app");
    }
    if (manifest.runtime_type == runtime::BackendType::Unknown) {
        return std::unexpected("Runtime app manifest has unknown backend type");
    }
    if (manifest.entry.empty()) {
        return std::unexpected("Runtime app manifest entry is empty");
    }

    runtime::AppConfig config;
    config.type = manifest.runtime_type;
    config.app_path = manifest.app_path;
    config.entry = manifest.entry;
    config.resource_dir = manifest.resource_dir;
    config.arguments = manifest.arguments;
    return config;
}

} // namespace esp_brookesia::system::core
