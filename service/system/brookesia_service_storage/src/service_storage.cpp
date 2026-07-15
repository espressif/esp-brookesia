/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <dirent.h>
#include <unistd.h>

#include "boost/format.hpp"
#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#endif
#include "brookesia/service_storage/macro_configs.h"
#if !BROOKESIA_SERVICE_STORAGE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_storage/service_storage.hpp"

namespace esp_brookesia::service {

namespace {

using StorageKvIface = hal::storage::KeyValueIface;
using StorageFsIface = hal::storage::FileSystemIface;
using StorageFileInfo = helper::Storage::FileInfo;
using StorageFileEntry = helper::Storage::FileEntry;
using StorageFileType = helper::Storage::FileType;
using ValidatedPathSet = std::unordered_set<std::filesystem::path>;

constexpr uint64_t FNV1A64_OFFSET_BASIS = 14695981039346656037ULL;
constexpr uint64_t FNV1A64_PRIME = 1099511628211ULL;
constexpr char BASE36_DIGITS[] = "0123456789abcdefghijklmnopqrstuvwxyz";
constexpr size_t FILE_IO_CHUNK_SIZE = 4096;

class InternalBuffer {
public:
    explicit InternalBuffer(size_t size)
        : size_(size)
    {
#if defined(ESP_PLATFORM)
        data_ = static_cast<uint8_t *>(heap_caps_malloc(size_, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#else
        buffer_.resize(size_);
        data_ = buffer_.data();
#endif
    }

    ~InternalBuffer()
    {
#if defined(ESP_PLATFORM)
        heap_caps_free(data_);
#endif
    }

    InternalBuffer(const InternalBuffer &) = delete;
    InternalBuffer &operator=(const InternalBuffer &) = delete;

    uint8_t *data() const
    {
        return data_;
    }

    size_t size() const
    {
        return size_;
    }

    bool is_valid() const
    {
        return data_ != nullptr;
    }

private:
    size_t size_ = 0;
    uint8_t *data_ = nullptr;
#if !defined(ESP_PLATFORM)
    std::vector<uint8_t> buffer_;
#endif
};

std::vector<std::string> parse_keys(const boost::json::array &keys)
{
    std::vector<std::string> result;
    result.reserve(keys.size());
    for (const auto &key_json : keys) {
        if (!key_json.is_string()) {
            BROOKESIA_LOGW("Key must be a string, skipping");
            continue;
        }
        result.emplace_back(std::string(key_json.as_string()));
    }
    return result;
}

std::expected<std::vector<std::string>, std::string> parse_name_parts(const boost::json::array &parts)
{
    if (parts.empty()) {
        return std::unexpected("KV name parts are empty");
    }

    std::vector<std::string> result;
    result.reserve(parts.size());
    for (const auto &part_json : parts) {
        if (!part_json.is_string()) {
            return std::unexpected("KV name part must be a string");
        }
        std::string part(part_json.as_string());
        if (part.empty()) {
            return std::unexpected("KV name part must not be empty");
        }
        result.emplace_back(std::move(part));
    }
    return result;
}

std::string join_name_parts(const std::vector<std::string> &parts, std::string_view separator)
{
    std::string result;
    size_t total_size = 0;
    for (const auto &part : parts) {
        total_size += part.size();
    }
    if (parts.size() > 1) {
        total_size += separator.size() * (parts.size() - 1);
    }
    result.reserve(total_size);

    bool first = true;
    for (const auto &part : parts) {
        if (!first) {
            result.append(separator);
        }
        result.append(part);
        first = false;
    }
    return result;
}

uint64_t fnv1a64(std::string_view input)
{
    uint64_t hash = FNV1A64_OFFSET_BASIS;
    for (const auto ch : input) {
        hash ^= static_cast<uint8_t>(ch);
        hash *= FNV1A64_PRIME;
    }
    return hash;
}

std::string to_base36(uint64_t value)
{
    if (value == 0) {
        return "0";
    }

    std::string result;
    while (value > 0) {
        result.push_back(BASE36_DIGITS[value % 36]);
        value /= 36;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

std::expected<helper::Storage::KvNameResult, std::string> make_kv_name(
    const boost::json::array &parts, const std::string &separator, uint32_t max_length, std::string_view type_name
)
{
    auto parsed_parts = parse_name_parts(parts);
    if (!parsed_parts) {
        return std::unexpected(parsed_parts.error());
    }

    auto original_name = join_name_parts(parsed_parts.value(), separator);
    if ((max_length == 0) || (original_name.size() <= max_length)) {
        return helper::Storage::KvNameResult{
            .name = original_name,
            .original_name = original_name,
            .hashed = false,
            .warning = "",
        };
    }

    std::string hashed_name = "h" + to_base36(fnv1a64(original_name));
    if (hashed_name.size() > max_length) {
        return std::unexpected(
                   (boost::format(
                        "KV %1% '%2%' length %3% exceeds backend limit %4%, and hash '%5%' is still too long"
                    ) % type_name % original_name % original_name.size() % max_length % hashed_name).str()
               );
    }

    return helper::Storage::KvNameResult{
        .name = hashed_name,
        .original_name = original_name,
        .hashed = true,
        .warning = (boost::format(
                        "KV %1% '%2%' length %3% exceeds backend limit %4%; using stable hash '%5%'"
                    ) % type_name % original_name % original_name.size() % max_length % hashed_name).str(),
    };
}

std::string get_last_error_or_default(const StorageKvIface &iface, const std::string &default_error)
{
    const auto &last_error = iface.get_last_error();
    if (!last_error.empty()) {
        return last_error;
    }
    return default_error;
}

bool path_has_parent_reference(const std::filesystem::path &path)
{
    for (const auto &part : path) {
        if (part == "..") {
            return true;
        }
    }
    return false;
}

bool path_is_same_or_child(const std::filesystem::path &path, const std::filesystem::path &root)
{
    const auto normalized_path = path.lexically_normal();
    const auto normalized_root = root.lexically_normal();
    auto path_it = normalized_path.begin();
    for (auto root_it = normalized_root.begin(); root_it != normalized_root.end(); ++root_it, ++path_it) {
        if (path_it == normalized_path.end() || *path_it != *root_it) {
            return false;
        }
    }
    return true;
}

bool path_is_same(const std::filesystem::path &a, const std::filesystem::path &b)
{
    return a.lexically_normal() == b.lexically_normal();
}

std::expected<void, std::string> validate_no_symlink_components(
    const std::filesystem::path &path, ValidatedPathSet *validated_directories = nullptr
)
{
#if defined(ESP_PLATFORM)
    // ESP storage backends currently exposed by FileSystemIface are SPIFFS,
    // LittleFS and FATFS. None supports symbolic links through ESP-IDF VFS, so
    // querying every path component only adds flash metadata I/O. Absolute-path,
    // parent-reference and mount-root containment checks remain active.
    (void)path;
    (void)validated_directories;
    return {};
#else
    std::filesystem::path current = path.root_path();
    for (const auto &part : path.relative_path()) {
        current /= part;
        if ((validated_directories != nullptr) && validated_directories->contains(current)) {
            continue;
        }
        std::error_code error_code;
        const auto status = std::filesystem::symlink_status(current, error_code);
        if (error_code == std::errc::no_such_file_or_directory) {
            return {};
        }
        if (error_code) {
            return std::unexpected(
                       "Failed to inspect Storage FS path '" + current.generic_string() + "': " +
                       error_code.message()
                   );
        }
        if (std::filesystem::is_symlink(status)) {
            return std::unexpected("Storage FS path must not contain symbolic links: " + current.generic_string());
        }
        if (!std::filesystem::exists(status)) {
            return {};
        }
        if ((validated_directories != nullptr) && std::filesystem::is_directory(status)) {
            validated_directories->insert(current);
        }
    }
    return {};
#endif
}

std::expected<std::filesystem::path, std::string> make_weakly_canonical(const std::filesystem::path &path)
{
    std::error_code error_code;
    auto canonical_path = std::filesystem::weakly_canonical(path, error_code);
    if (error_code) {
        return std::unexpected(
                   "Failed to canonicalize Storage FS path '" + path.generic_string() + "': " +
                   error_code.message()
               );
    }
    return canonical_path;
}

std::vector<std::filesystem::path> get_file_system_roots(const StorageFsIface::Info &info)
{
    std::vector<std::filesystem::path> roots;
    auto append_root = [&roots](std::string root) {
        if (root.empty()) {
            return;
        }
        auto normalized = std::filesystem::path(std::move(root)).lexically_normal();
        auto duplicated = std::any_of(roots.begin(), roots.end(), [&normalized](const auto & existing) {
            return path_is_same(existing, normalized);
        });
        if (!duplicated) {
            roots.push_back(std::move(normalized));
        }
    };

    append_root(info.root_path);
    if (info.mount_point != nullptr) {
        append_root(info.mount_point);
    }
    return roots;
}

std::expected<std::vector<std::filesystem::path>, std::string> make_file_system_roots(const StorageFsIface &iface)
{
    std::vector<std::filesystem::path> roots;
    for (const auto &info : iface.get_all_info()) {
        for (const auto &root : get_file_system_roots(info)) {
            if (!root.is_absolute()) {
                return std::unexpected("Storage FS root must be absolute: " + root.generic_string());
            }
            auto validation = validate_no_symlink_components(root);
            if (!validation) {
                return std::unexpected(validation.error());
            }
            auto canonical_root = make_weakly_canonical(root);
            if (!canonical_root) {
                return std::unexpected(canonical_root.error());
            }
            auto duplicated = std::any_of(roots.begin(), roots.end(), [&canonical_root](const auto & existing) {
                return path_is_same(existing, canonical_root.value());
            });
            if (!duplicated) {
                roots.push_back(std::move(canonical_root.value()));
            }
        }
    }
    return roots;
}

std::expected<std::filesystem::path, std::string> resolve_file_system_path_internal(
    const std::vector<std::filesystem::path> &roots, const std::string &raw_path,
    ValidatedPathSet *validated_directories
)
{
    if (raw_path.empty()) {
        return std::unexpected("Storage FS path is empty");
    }

    std::filesystem::path path(raw_path);
    if (!path.is_absolute()) {
        return std::unexpected("Storage FS path must be absolute: " + raw_path);
    }
    if (path_has_parent_reference(path)) {
        return std::unexpected("Storage FS path must not contain '..': " + raw_path);
    }

    path = path.lexically_normal();
    for (const auto &root : roots) {
        if (!path_is_same_or_child(path, root)) {
            continue;
        }

        // The canonical root is cached at initialization. Checking every component once
        // still detects a root replaced by a symbolic link while avoiding a second path
        // walk through weakly_canonical() for every file operation.
        auto validation = validate_no_symlink_components(path, validated_directories);
        if (!validation) {
            return std::unexpected(validation.error());
        }
        return path;
    }

    return std::unexpected("Storage FS path is outside mounted file systems: " + raw_path);
}

bool is_file_system_root(const std::vector<std::filesystem::path> &roots, const std::filesystem::path &path)
{
    for (const auto &root : roots) {
        if (path_is_same(path, root)) {
            return true;
        }
    }
    return false;
}

std::expected<void, std::string> ensure_parent_directory(const std::filesystem::path &path)
{
    auto parent_path = path.parent_path();
    if (parent_path.empty()) {
        return {};
    }

    auto validation = validate_no_symlink_components(parent_path);
    if (!validation) {
        return std::unexpected(validation.error());
    }

    std::error_code error_code;
    std::filesystem::create_directories(parent_path, error_code);
    if (error_code) {
        return std::unexpected(
                   "Failed to create parent directory '" + parent_path.generic_string() + "': " +
                   error_code.message()
               );
    }
    return validate_no_symlink_components(parent_path);
}

std::expected<void, std::string> create_directory_tree(const std::filesystem::path &path)
{
    std::error_code error_code;
    std::filesystem::create_directories(path, error_code);
    if (error_code) {
        return std::unexpected(
                   "Failed to create directory '" + path.generic_string() + "': " + error_code.message()
               );
    }
    return {};
}

uint64_t make_mtime_ms(const std::filesystem::path &path)
{
    std::error_code error_code;
    auto time = std::filesystem::last_write_time(path, error_code);
    if (error_code) {
        return 0;
    }

    auto count = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
    if (count < 0) {
        return 0;
    }
    return static_cast<uint64_t>(count);
}

StorageFileInfo make_file_info(const std::filesystem::path &path)
{
    std::error_code error_code;
    const auto status = std::filesystem::symlink_status(path, error_code);
    if (error_code || !std::filesystem::exists(status)) {
        return {};
    }

    StorageFileInfo info;
    info.exists = true;
    if (std::filesystem::is_symlink(status)) {
        info.type = StorageFileType::Other;
        return info;
    }
    info.mtime_ms = make_mtime_ms(path);
    if (std::filesystem::is_regular_file(status)) {
        info.type = StorageFileType::File;
        auto size = std::filesystem::file_size(path, error_code);
        if (!error_code) {
            info.size = size;
        }
    } else if (std::filesystem::is_directory(status)) {
        info.type = StorageFileType::Directory;
    } else {
        info.type = StorageFileType::Other;
    }
    return info;
}

std::expected<size_t, std::string> read_file_to_buffer(const std::filesystem::path &path, const RawBuffer &buffer)
{
    if (buffer.data_size == 0) {
        return 0;
    }

    auto *output = buffer.to_ptr<uint8_t>();
    if (output == nullptr) {
        return std::unexpected("Storage FS read requires a mutable output buffer");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::unexpected("Failed to open file for read: " + path.generic_string());
    }

    InternalBuffer chunk(std::min(FILE_IO_CHUNK_SIZE, buffer.data_size));
    if (!chunk.is_valid()) {
        return std::unexpected("Failed to allocate internal Storage FS read buffer");
    }

    size_t total_read = 0;
    while (total_read < buffer.data_size && input) {
        const auto read_size = std::min(chunk.size(), buffer.data_size - total_read);
        input.read(reinterpret_cast<char *>(chunk.data()), static_cast<std::streamsize>(read_size));
        const auto bytes_read = input.gcount();
        if (bytes_read > 0) {
            std::memcpy(output + total_read, chunk.data(), static_cast<size_t>(bytes_read));
            total_read += static_cast<size_t>(bytes_read);
        }
        if (static_cast<size_t>(bytes_read) < read_size) {
            break;
        }
    }
    if (input.bad()) {
        return std::unexpected("Failed to read file: " + path.generic_string());
    }
    return total_read;
}

std::expected<std::string, std::string> read_file_to_string(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::unexpected("Failed to open file for read: " + path.generic_string());
    }

    InternalBuffer chunk(FILE_IO_CHUNK_SIZE);
    if (!chunk.is_valid()) {
        return std::unexpected("Failed to allocate internal Storage FS read buffer");
    }

    std::string result;
    while (input) {
        input.read(reinterpret_cast<char *>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
        const auto bytes_read = input.gcount();
        if (bytes_read > 0) {
            result.append(reinterpret_cast<char *>(chunk.data()), static_cast<size_t>(bytes_read));
        }
    }
    if (input.bad()) {
        return std::unexpected("Failed to read file: " + path.generic_string());
    }
    return result;
}

std::expected<void, std::string> write_bytes_to_file(
    const std::filesystem::path &path, const uint8_t *data, size_t data_size
)
{
    if (data == nullptr && data_size > 0) {
        return std::unexpected("Storage FS write source data is null");
    }

    auto parent_result = ensure_parent_directory(path);
    if (!parent_result) {
        return std::unexpected(parent_result.error());
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return std::unexpected("Failed to open file for write: " + path.generic_string());
    }

    InternalBuffer chunk(std::min(FILE_IO_CHUNK_SIZE, std::max<size_t>(data_size, 1)));
    if (!chunk.is_valid()) {
        return std::unexpected("Failed to allocate internal Storage FS write buffer");
    }

    size_t total_written = 0;
    while (total_written < data_size) {
        const auto write_size = std::min(chunk.size(), data_size - total_written);
        std::memcpy(chunk.data(), data + total_written, write_size);
        output.write(reinterpret_cast<const char *>(chunk.data()), static_cast<std::streamsize>(write_size));
        if (!output) {
            return std::unexpected("Failed to write file: " + path.generic_string());
        }
        total_written += write_size;
    }
    return {};
}

std::expected<void, std::string> copy_file_chunked(
    const std::filesystem::path &source_path, const std::filesystem::path &destination_path, bool overwrite
)
{
    auto destination_info = make_file_info(destination_path);
    if (destination_info.exists && !overwrite) {
        return {};
    }

    auto parent_result = ensure_parent_directory(destination_path);
    if (!parent_result) {
        return std::unexpected(parent_result.error());
    }

    std::ifstream input(source_path, std::ios::binary);
    if (!input) {
        return std::unexpected("Failed to open source file for copy: " + source_path.generic_string());
    }

    std::ofstream output(destination_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return std::unexpected("Failed to open destination file for copy: " + destination_path.generic_string());
    }

    InternalBuffer chunk(FILE_IO_CHUNK_SIZE);
    if (!chunk.is_valid()) {
        return std::unexpected("Failed to allocate internal Storage FS copy buffer");
    }

    while (input) {
        input.read(reinterpret_cast<char *>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
        const auto bytes_read = input.gcount();
        if (bytes_read > 0) {
            output.write(reinterpret_cast<const char *>(chunk.data()), bytes_read);
            if (!output) {
                return std::unexpected("Failed to write destination file for copy: " + destination_path.generic_string());
            }
        }
    }
    if (input.bad()) {
        return std::unexpected("Failed to read source file for copy: " + source_path.generic_string());
    }
    return {};
}

std::expected<void, std::string> copy_path_tree(
    const std::filesystem::path &source_path, const std::filesystem::path &destination_path, bool overwrite
)
{
    auto source_validation = validate_no_symlink_components(source_path);
    if (!source_validation) {
        return std::unexpected(source_validation.error());
    }
    auto destination_validation = validate_no_symlink_components(destination_path);
    if (!destination_validation) {
        return std::unexpected(destination_validation.error());
    }

    auto source_info = make_file_info(source_path);
    if (!source_info.exists) {
        return std::unexpected("Storage FS copy source does not exist: " + source_path.generic_string());
    }

    if (source_info.type == StorageFileType::File) {
        return copy_file_chunked(source_path, destination_path, overwrite);
    }
    if (source_info.type != StorageFileType::Directory) {
        return std::unexpected("Storage FS copy source is not a file or directory: " + source_path.generic_string());
    }
    if (path_is_same_or_child(destination_path, source_path)) {
        return std::unexpected("Storage FS copy destination must not be inside source directory");
    }

    auto create_result = create_directory_tree(destination_path);
    if (!create_result) {
        return std::unexpected(create_result.error());
    }

    std::error_code error_code;
    std::filesystem::directory_iterator iterator(source_path, error_code);
    if (error_code) {
        return std::unexpected(
                   "Failed to list source directory for copy '" + source_path.generic_string() + "': " +
                   error_code.message()
               );
    }

    for (const auto &entry : iterator) {
        auto child_result = copy_path_tree(entry.path(), destination_path / entry.path().filename(), overwrite);
        if (!child_result) {
            return std::unexpected(child_result.error());
        }
    }
    return {};
}

std::string make_posix_error_message(std::string_view action, const std::filesystem::path &path)
{
    return std::string(action) + " '" + path.generic_string() + "': " + std::strerror(errno);
}

std::expected<std::vector<std::filesystem::path>, std::string> list_directory_children_for_remove(
    const std::filesystem::path &path
)
{
    const auto path_text = path.generic_string();
    DIR *dir = ::opendir(path_text.c_str());
    if (dir == nullptr) {
        return std::unexpected(make_posix_error_message("Failed to open Storage FS directory", path));
    }

    std::vector<std::filesystem::path> children;
    errno = 0;
    while (auto *entry = ::readdir(dir)) {
        const std::string_view name(entry->d_name);
        if (name == "." || name == "..") {
            continue;
        }
        children.push_back(path / name);
    }
    if (errno != 0) {
        auto error = make_posix_error_message("Failed to read Storage FS directory", path);
        (void)::closedir(dir);
        return std::unexpected(std::move(error));
    }
    if (::closedir(dir) != 0) {
        return std::unexpected(make_posix_error_message("Failed to close Storage FS directory", path));
    }
    return children;
}

std::expected<void, std::string> remove_path_tree(const std::filesystem::path &path)
{
    auto validation = validate_no_symlink_components(path);
    if (!validation) {
        return std::unexpected(validation.error());
    }

    const auto info = make_file_info(path);
    if (!info.exists) {
        return {};
    }

    const auto path_text = path.generic_string();
    if (info.type == StorageFileType::Directory) {
        auto children = list_directory_children_for_remove(path);
        if (!children) {
            return std::unexpected(children.error());
        }
        for (const auto &child : *children) {
            auto child_result = remove_path_tree(child);
            if (!child_result) {
                return std::unexpected(child_result.error());
            }
        }
        if (::rmdir(path_text.c_str()) != 0) {
            return std::unexpected(make_posix_error_message("Failed to remove Storage FS directory", path));
        }
        return {};
    }

    if (::unlink(path_text.c_str()) != 0) {
        return std::unexpected(make_posix_error_message("Failed to remove Storage FS file", path));
    }
    return {};
}

} // namespace

std::string Storage::get_component_version()
{
    return make_version(
               BROOKESIA_SERVICE_STORAGE_VER_MAJOR, BROOKESIA_SERVICE_STORAGE_VER_MINOR,
               BROOKESIA_SERVICE_STORAGE_VER_PATCH
           );
}

bool Storage::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_STORAGE_VER_MAJOR, BROOKESIA_SERVICE_STORAGE_VER_MINOR,
        BROOKESIA_SERVICE_STORAGE_VER_PATCH
    );

    if (auto kv_iface = hal::acquire_first_interface<StorageKvIface>(); kv_iface) {
        BROOKESIA_LOGI("Using storage KV interface: %1%", kv_iface.instance_name());
        kv_iface_ = std::move(kv_iface);
        if (!kv_iface_->init()) {
            BROOKESIA_LOGE("Failed to initialize storage KV interface: %1%", kv_iface_->get_last_error());
            kv_iface_.reset();
        }
    } else {
        BROOKESIA_LOGW("Storage KV HAL interface is not available");
    }

    if (auto fs_iface = hal::acquire_first_interface<StorageFsIface>(); fs_iface) {
        BROOKESIA_LOGI("Using storage file-system interface: %1%", fs_iface.instance_name());
        auto roots = make_file_system_roots(*fs_iface);
        if (!roots) {
            BROOKESIA_LOGE("Failed to initialize storage file-system roots: %1%", roots.error());
        } else {
            file_system_roots_ = std::move(roots.value());
            fs_iface_ = std::move(fs_iface);
        }
    } else {
        BROOKESIA_LOGW("Storage file-system HAL interface is not available");
    }

    return kv_iface_ || fs_iface_;
}

void Storage::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (kv_iface_) {
        kv_iface_->deinit();
        kv_iface_.reset();
    }
    fs_iface_.reset();
    file_system_roots_.clear();
    validated_batch_directories_.clear();
    batch_path_validation_cache_enabled_ = false;
}

void Storage::on_function_batch_start(const std::vector<FunctionCall> &calls)
{
    const auto read_text_name = BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::FunctionId::FSReadText);
    const auto stat_name = BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::FunctionId::FSStat);
    // Only read-only batches may reuse their directory validation snapshot. File paths
    // themselves are always inspected, and the snapshot is discarded when the batch ends.
    auto is_cacheable_call = [&read_text_name, &stat_name](const auto & call) {
        return (call.name == read_text_name) || (call.name == stat_name);
    };
    batch_path_validation_cache_enabled_ =
        !calls.empty() && std::all_of(calls.begin(), calls.end(), is_cacheable_call);
    validated_batch_directories_.clear();
}

void Storage::on_function_batch_end()
{
    batch_path_validation_cache_enabled_ = false;
    validated_batch_directories_.clear();
}

std::expected<std::filesystem::path, std::string> Storage::resolve_file_system_path(const std::string &path)
{
    auto *validated_directories = batch_path_validation_cache_enabled_ ?
                                  &validated_batch_directories_ : nullptr;
    return resolve_file_system_path_internal(file_system_roots_, path, validated_directories);
}

std::expected<boost::json::array, std::string> Storage::function_list(const std::string &nspace)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: nspace(%1%)", nspace);

    if (!kv_iface_) {
        return std::unexpected("Storage KV HAL interface is not available");
    }

    std::vector<StorageKvIface::EntryInfo> entries;
    if (!kv_iface_->list(nspace, entries)) {
        return std::unexpected(get_last_error_or_default(*kv_iface_, (boost::format(
                                   "Failed to list entries in namespace '%1%'"
                               ) % nspace).str()));
    }

    return BROOKESIA_DESCRIBE_TO_JSON(entries).as_array();
}

std::expected<void, std::string> Storage::function_set(const std::string &nspace, boost::json::object &&key_value_map)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: nspace(%1%), key_value_map(%2%)", nspace, BROOKESIA_DESCRIBE_TO_STR(key_value_map));

    if (!kv_iface_) {
        return std::unexpected("Storage KV HAL interface is not available");
    }

    KeyValueMap parsed_key_value_map;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(key_value_map, parsed_key_value_map)) {
        return std::unexpected((
                                   boost::format("Failed to parse key-value map in namespace '%1%': %2%") %
                                   nspace % BROOKESIA_DESCRIBE_TO_STR(key_value_map)
                               ).str());
    }

    if (!kv_iface_->set(nspace, parsed_key_value_map)) {
        return std::unexpected(get_last_error_or_default(*kv_iface_, (boost::format(
                                   "Failed to set key-value map in namespace '%1%'"
                               ) % nspace).str()));
    }

    return {};
}

std::expected<boost::json::array, std::string> Storage::function_get_file_systems()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!fs_iface_) {
        return std::unexpected("Storage file-system HAL interface is not available");
    }

    auto infos = fs_iface_->get_all_info();

    return BROOKESIA_DESCRIBE_TO_JSON(infos).as_array();
}

std::expected<boost::json::object, std::string> Storage::function_get_file_system_capacity(
    const std::string &mount_point
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: mount_point(%1%)", mount_point);

    if (!fs_iface_) {
        return std::unexpected("Storage file-system HAL interface is not available");
    }
    if (mount_point.empty()) {
        return std::unexpected("Mount point is empty");
    }

    FileSystemCapacity capacity = {};
    if (!fs_iface_->get_capacity(mount_point.c_str(), capacity)) {
        return std::unexpected("Failed to get storage file-system capacity");
    }

    return BROOKESIA_DESCRIBE_TO_JSON(capacity).as_object();
}

std::expected<boost::json::object, std::string> Storage::function_fs_stat(const std::string &raw_path)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: path(%1%)", raw_path);

    if (!fs_iface_) {
        return std::unexpected("Storage file-system HAL interface is not available");
    }

    auto path = resolve_file_system_path(raw_path);
    if (!path) {
        return std::unexpected(path.error());
    }

    return BROOKESIA_DESCRIBE_TO_JSON(make_file_info(path.value())).as_object();
}

std::expected<boost::json::array, std::string> Storage::function_fs_list(const std::string &raw_path)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: path(%1%)", raw_path);

    if (!fs_iface_) {
        return std::unexpected("Storage file-system HAL interface is not available");
    }

    auto path = resolve_file_system_path(raw_path);
    if (!path) {
        return std::unexpected(path.error());
    }

    auto directory_info = make_file_info(path.value());
    if (!directory_info.exists) {
        return boost::json::array();
    }
    if (directory_info.type != StorageFileType::Directory) {
        return std::unexpected("Storage FS list path is not a directory: " + path->generic_string());
    }

    std::vector<StorageFileEntry> entries;
    std::error_code error_code;
    std::filesystem::directory_iterator iterator(path.value(), error_code);
    if (error_code) {
        return std::unexpected(
                   "Failed to list Storage FS directory '" + path->generic_string() + "': " + error_code.message()
               );
    }

    for (const auto &entry : iterator) {
        auto entry_validation = validate_no_symlink_components(entry.path());
        if (!entry_validation) {
            return std::unexpected(entry_validation.error());
        }
        entries.emplace_back(StorageFileEntry{
            .name = entry.path().filename().generic_string(),
            .info = make_file_info(entry.path()),
        });
    }

    return BROOKESIA_DESCRIBE_TO_JSON(entries).as_array();
}

std::expected<void, std::string> Storage::function_fs_mkdir(const std::string &raw_path)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: path(%1%)", raw_path);

    if (!fs_iface_) {
        return std::unexpected("Storage file-system HAL interface is not available");
    }

    auto path = resolve_file_system_path(raw_path);
    if (!path) {
        return std::unexpected(path.error());
    }

    auto result = create_directory_tree(path.value());
    return result;
}

std::expected<std::string, std::string> Storage::function_fs_read_text(const std::string &raw_path)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: path(%1%)", raw_path);

    if (!fs_iface_) {
        return std::unexpected("Storage file-system HAL interface is not available");
    }

    auto path = resolve_file_system_path(raw_path);
    if (!path) {
        return std::unexpected(path.error());
    }

    auto result = read_file_to_string(path.value());
    return result;
}

std::expected<double, std::string> Storage::function_fs_read(const std::string &raw_path, const RawBuffer &buffer)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: path(%1%), buffer(%2%)", raw_path, BROOKESIA_DESCRIBE_TO_STR(buffer));

    if (!fs_iface_) {
        return std::unexpected("Storage file-system HAL interface is not available");
    }

    auto path = resolve_file_system_path(raw_path);
    if (!path) {
        return std::unexpected(path.error());
    }

    auto result = read_file_to_buffer(path.value(), buffer);
    if (!result) {
        return std::unexpected(result.error());
    }
    return static_cast<double>(result.value());
}

std::expected<void, std::string> Storage::function_fs_write_text(
    const std::string &raw_path, const std::string &data
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: path(%1%), data_size(%2%)", raw_path, data.size());

    if (!fs_iface_) {
        return std::unexpected("Storage file-system HAL interface is not available");
    }

    auto path = resolve_file_system_path(raw_path);
    if (!path) {
        return std::unexpected(path.error());
    }

    return write_bytes_to_file(path.value(), reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

std::expected<void, std::string> Storage::function_fs_write(const std::string &raw_path, const RawBuffer &data)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: path(%1%), data(%2%)", raw_path, BROOKESIA_DESCRIBE_TO_STR(data));

    if (!fs_iface_) {
        return std::unexpected("Storage file-system HAL interface is not available");
    }

    auto path = resolve_file_system_path(raw_path);
    if (!path) {
        return std::unexpected(path.error());
    }

    return write_bytes_to_file(path.value(), data.to_const_ptr<uint8_t>(), data.data_size);
}

std::expected<void, std::string> Storage::function_fs_remove(const std::string &raw_path)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: path(%1%)", raw_path);

    if (!fs_iface_) {
        return std::unexpected("Storage file-system HAL interface is not available");
    }

    auto path = resolve_file_system_path(raw_path);
    if (!path) {
        return std::unexpected(path.error());
    }
    if (is_file_system_root(file_system_roots_, path.value())) {
        return std::unexpected("Storage FS remove path must not be a mounted file-system root");
    }

    return remove_path_tree(path.value());
}

std::expected<void, std::string> Storage::function_fs_rename(
    const std::string &raw_from, const std::string &raw_to
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: from(%1%), to(%2%)", raw_from, raw_to);

    if (!fs_iface_) {
        return std::unexpected("Storage file-system HAL interface is not available");
    }

    auto from = resolve_file_system_path(raw_from);
    if (!from) {
        return std::unexpected(from.error());
    }
    auto to = resolve_file_system_path(raw_to);
    if (!to) {
        return std::unexpected(to.error());
    }
    if (is_file_system_root(file_system_roots_, from.value()) ||
            is_file_system_root(file_system_roots_, to.value())) {
        return std::unexpected("Storage FS rename source/destination must not be a mounted file-system root");
    }

    auto parent_result = ensure_parent_directory(to.value());
    if (!parent_result) {
        return std::unexpected(parent_result.error());
    }

    std::error_code error_code;
    std::filesystem::rename(from.value(), to.value(), error_code);
    if (error_code) {
        return std::unexpected(
                   "Failed to rename Storage FS path '" + from->generic_string() + "' to '" +
                   to->generic_string() + "': " + error_code.message()
               );
    }
    return {};
}

std::expected<void, std::string> Storage::function_fs_copy_tree(
    const std::string &raw_from, const std::string &raw_to, bool overwrite
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: from(%1%), to(%2%), overwrite(%3%)", raw_from, raw_to, overwrite);

    if (!fs_iface_) {
        return std::unexpected("Storage file-system HAL interface is not available");
    }

    auto from = resolve_file_system_path(raw_from);
    if (!from) {
        return std::unexpected(from.error());
    }
    auto to = resolve_file_system_path(raw_to);
    if (!to) {
        return std::unexpected(to.error());
    }
    if (is_file_system_root(file_system_roots_, to.value())) {
        return std::unexpected("Storage FS copy destination must not be a mounted file-system root");
    }

    return copy_path_tree(from.value(), to.value(), overwrite);
}

std::expected<boost::json::object, std::string> Storage::function_get(const std::string &nspace, boost::json::array &&keys)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: nspace(%1%), keys(%2%)", nspace, keys);

    if (!kv_iface_) {
        return std::unexpected("Storage KV HAL interface is not available");
    }

    auto parsed_keys = parse_keys(keys);
    if (!keys.empty() && parsed_keys.empty()) {
        return boost::json::object();
    }

    KeyValueMap key_value_map;
    if (!kv_iface_->get(nspace, parsed_keys, key_value_map)) {
        return std::unexpected(get_last_error_or_default(*kv_iface_, (boost::format(
                                   "Failed to get key-value map in namespace '%1%'"
                               ) % nspace).str()));
    }

    return BROOKESIA_DESCRIBE_TO_JSON(key_value_map).as_object();
}

std::expected<void, std::string> Storage::function_erase(const std::string &nspace, boost::json::array &&keys)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: nspace(%1%), keys(%2%)", nspace, keys);

    if (!kv_iface_) {
        return std::unexpected("Storage KV HAL interface is not available");
    }

    auto parsed_keys = parse_keys(keys);
    if (!keys.empty() && parsed_keys.empty()) {
        return {};
    }

    if (!kv_iface_->erase(nspace, parsed_keys)) {
        return std::unexpected(get_last_error_or_default(*kv_iface_, (boost::format(
                                   "Failed to erase key-value map in namespace '%1%'"
                               ) % nspace).str()));
    }

    return {};
}

std::expected<boost::json::object, std::string> Storage::function_make_kv_key(
    boost::json::array &&parts, const std::string &separator
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!kv_iface_) {
        return std::unexpected("Storage KV HAL interface is not available");
    }

    auto result = make_kv_name(parts, separator, kv_iface_->get_info().max_key_length, "key");
    if (!result) {
        return std::unexpected(result.error());
    }
    return BROOKESIA_DESCRIBE_TO_JSON(result.value()).as_object();
}

std::expected<boost::json::object, std::string> Storage::function_make_kv_namespace(
    boost::json::array &&parts, const std::string &separator
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!kv_iface_) {
        return std::unexpected("Storage KV HAL interface is not available");
    }

    auto result = make_kv_name(parts, separator, kv_iface_->get_info().max_namespace_length, "namespace");
    if (!result) {
        return std::unexpected(result.error());
    }
    return BROOKESIA_DESCRIBE_TO_JSON(result.value()).as_object();
}

#if BROOKESIA_SERVICE_STORAGE_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, Storage, Storage::get_instance().get_attributes().name, Storage::get_instance(),
    BROOKESIA_SERVICE_STORAGE_PLUGIN_SYMBOL
);
#endif // BROOKESIA_SERVICE_STORAGE_ENABLE_AUTO_REGISTER

} // namespace esp_brookesia::service
