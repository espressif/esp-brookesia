/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <sys/statvfs.h>
#include <type_traits>
#include <utility>

#include "brookesia/hal_linux/macro_configs.h"
#if !BROOKESIA_HAL_LINUX_STORAGE_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interfaces/storage/file_system.hpp"
#include "brookesia/hal_interface/interfaces/storage/key_value.hpp"
#include "brookesia/hal_linux/storage/device.hpp"

namespace esp_brookesia::hal {

namespace {

struct FileSystemEntry {
    const char *subdirectory;
    storage::FileSystemIface::Info info;
};

constexpr std::array<FileSystemEntry, 4> FILE_SYSTEM_ENTRIES = {{
        {
            .subdirectory = "spiffs",
            .info = {
                .fs_type = storage::FileSystemIface::FileSystemType::SPIFFS,
                .medium_type = storage::FileSystemIface::MediumType::Flash,
                .mount_point = "/spiffs",
                .root_path = "",
                .supports_directories = false,
            },
        },
        {
            .subdirectory = "littlefs",
            .info = {
                .fs_type = storage::FileSystemIface::FileSystemType::LittleFS,
                .medium_type = storage::FileSystemIface::MediumType::Flash,
                .mount_point = "/littlefs",
                .root_path = "",
                .supports_directories = true,
            },
        },
        {
            .subdirectory = "fatfs",
            .info = {
                .fs_type = storage::FileSystemIface::FileSystemType::FATFS,
                .medium_type = storage::FileSystemIface::MediumType::Flash,
                .mount_point = "/fatfs",
                .root_path = "",
                .supports_directories = true,
            },
        },
        {
            .subdirectory = "sdcard",
            .info = {
                .fs_type = storage::FileSystemIface::FileSystemType::FATFS,
                .medium_type = storage::FileSystemIface::MediumType::SDCard,
                .mount_point = "/sdcard",
                .root_path = "",
                .supports_directories = true,
            },
        },
    }
};

std::filesystem::path get_executable_directory()
{
    std::array<char, 4096> buffer = {};
    const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length > 0) {
        buffer[static_cast<size_t>(length)] = '\0';
        return std::filesystem::path(buffer.data()).parent_path();
    }

    std::error_code error_code;
    const auto current_path = std::filesystem::current_path(error_code);
    if (!error_code) {
        return current_path;
    }

    return {};
}

std::filesystem::path get_linux_storage_system_root()
{
    const auto executable_directory = get_executable_directory();
    if (!executable_directory.empty()) {
        return executable_directory / ".brookesia";
    }

    if (const char *xdg_state_home = std::getenv("XDG_STATE_HOME")) {
        return std::filesystem::path(xdg_state_home) / ".brookesia";
    }
    if (const char *home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".brookesia";
    }
    return std::filesystem::temp_directory_path() / ".brookesia";
}

std::filesystem::path get_linux_storage_kv_root()
{
    if (const char *kv_dir = std::getenv("BROOKESIA_HAL_LINUX_KV_DIR")) {
        if (kv_dir[0] != '\0') {
            return std::filesystem::path(kv_dir);
        }
    }

    return get_linux_storage_system_root() / "kv";
}

} // namespace

class StorageKeyValueLinuxImpl: public storage::KeyValueIface {
public:
    bool init() override
    {
        std::lock_guard lock(mutex_);
        root_dir_ = get_linux_storage_kv_root();
        std::error_code error_code;
        std::filesystem::create_directories(root_dir_, error_code);
        if (error_code) {
            last_error_ = "Failed to create key-value storage root: " + error_code.message();
            BROOKESIA_LOGE("%1%", last_error_);
            return false;
        }
        load_all_locked();
        is_initialized_ = true;
        last_error_.clear();
        return true;
    }

    void deinit() override
    {
        std::lock_guard lock(mutex_);
        namespaces_.clear();
        is_initialized_ = false;
        last_error_.clear();
    }

    bool list(const std::string &nspace, std::vector<EntryInfo> &entries) override
    {
        std::lock_guard lock(mutex_);
        entries.clear();
        if (!check_initialized()) {
            return false;
        }

        auto namespace_it = namespaces_.find(nspace);
        if (namespace_it == namespaces_.end()) {
            return true;
        }

        entries.reserve(namespace_it->second.size());
        for (const auto &[key, value] : namespace_it->second) {
            entries.push_back(EntryInfo{
                .nspace = nspace,
                .key = key,
                .type = get_value_type(value),
            });
        }

        last_error_.clear();
        return true;
    }

    bool set(const std::string &nspace, const KeyValueMap &key_value_map) override
    {
        std::lock_guard lock(mutex_);
        if (!check_initialized()) {
            return false;
        }

        auto &storage = namespaces_[nspace];
        for (const auto &[key, value] : key_value_map) {
            storage[key] = value;
        }

        if (!save_namespace_locked(nspace)) {
            return false;
        }
        last_error_.clear();
        return true;
    }

    bool get(const std::string &nspace, const std::vector<std::string> &keys, KeyValueMap &key_value_map) override
    {
        std::lock_guard lock(mutex_);
        key_value_map.clear();
        if (!check_initialized()) {
            return false;
        }

        auto namespace_it = namespaces_.find(nspace);
        if (namespace_it == namespaces_.end()) {
            return true;
        }

        if (keys.empty()) {
            key_value_map = namespace_it->second;
            last_error_.clear();
            return true;
        }

        for (const auto &key : keys) {
            auto value_it = namespace_it->second.find(key);
            if (value_it != namespace_it->second.end()) {
                key_value_map.emplace(key, value_it->second);
            }
        }

        last_error_.clear();
        return true;
    }

    bool erase(const std::string &nspace, const std::vector<std::string> &keys) override
    {
        std::lock_guard lock(mutex_);
        if (!check_initialized()) {
            return false;
        }

        auto namespace_it = namespaces_.find(nspace);
        if (namespace_it == namespaces_.end()) {
            return true;
        }

        if (keys.empty()) {
            namespaces_.erase(namespace_it);
            remove_namespace_file_locked(nspace);
            last_error_.clear();
            return true;
        }

        for (const auto &key : keys) {
            namespace_it->second.erase(key);
        }
        if (namespace_it->second.empty()) {
            namespaces_.erase(namespace_it);
            remove_namespace_file_locked(nspace);
        } else if (!save_namespace_locked(nspace)) {
            return false;
        }

        last_error_.clear();
        return true;
    }

private:
    static ValueType get_value_type(const Value &value)
    {
        return std::visit([](auto &&stored_value) {
            using T = std::decay_t<decltype(stored_value)>;
            if constexpr (std::is_same_v<T, bool>) {
                return ValueType::Bool;
            } else if constexpr (std::is_same_v<T, int32_t>) {
                return ValueType::Int;
            } else {
                return ValueType::String;
            }
        }, value);
    }

    static std::string hex_encode(std::string_view input)
    {
        std::ostringstream stream;
        stream << std::hex << std::setfill('0');
        for (unsigned char ch : input) {
            stream << std::setw(2) << static_cast<int>(ch);
        }
        return stream.str();
    }

    static bool hex_decode(const std::string &input, std::string &output)
    {
        if ((input.size() % 2) != 0) {
            return false;
        }
        output.clear();
        output.reserve(input.size() / 2);
        for (size_t i = 0; i < input.size(); i += 2) {
            char *end = nullptr;
            const std::string byte = input.substr(i, 2);
            const long value = std::strtol(byte.c_str(), &end, 16);
            if ((end == nullptr) || (*end != '\0') || (value < 0) || (value > 0xff)) {
                return false;
            }
            output.push_back(static_cast<char>(value));
        }
        return true;
    }

    std::filesystem::path get_namespace_path_locked(const std::string &nspace) const
    {
        return root_dir_ / (hex_encode(nspace) + ".kv");
    }

    bool save_namespace_locked(const std::string &nspace)
    {
        std::error_code error_code;
        std::filesystem::create_directories(root_dir_, error_code);
        if (error_code) {
            last_error_ = "Failed to create key-value storage root: " + error_code.message();
            BROOKESIA_LOGE("%1%", last_error_);
            return false;
        }

        std::ofstream file(get_namespace_path_locked(nspace), std::ios::trunc);
        if (!file) {
            last_error_ = "Failed to open key-value namespace file";
            BROOKESIA_LOGE("%1%", last_error_);
            return false;
        }

        const auto namespace_it = namespaces_.find(nspace);
        if (namespace_it == namespaces_.end()) {
            return true;
        }
        for (const auto &[key, value] : namespace_it->second) {
            file << hex_encode(key) << '\t';
            std::visit([&file](auto &&stored_value) {
                using T = std::decay_t<decltype(stored_value)>;
                if constexpr (std::is_same_v<T, bool>) {
                    file << "b\t" << (stored_value ? "1" : "0");
                } else if constexpr (std::is_same_v<T, int32_t>) {
                    file << "i\t" << stored_value;
                } else {
                    file << "s\t" << hex_encode(stored_value);
                }
            }, value);
            file << '\n';
        }
        return true;
    }

    void remove_namespace_file_locked(const std::string &nspace)
    {
        std::error_code error_code;
        std::filesystem::remove(get_namespace_path_locked(nspace), error_code);
    }

    void load_all_locked()
    {
        namespaces_.clear();
        std::error_code error_code;
        if (!std::filesystem::exists(root_dir_, error_code)) {
            return;
        }
        for (const auto &entry : std::filesystem::directory_iterator(root_dir_, error_code)) {
            if (error_code || !entry.is_regular_file() || entry.path().extension() != ".kv") {
                continue;
            }
            std::string nspace;
            if (!hex_decode(entry.path().stem().string(), nspace)) {
                continue;
            }
            std::ifstream file(entry.path());
            std::string line;
            while (std::getline(file, line)) {
                std::istringstream stream(line);
                std::string encoded_key;
                std::string type;
                std::string encoded_value;
                if (!std::getline(stream, encoded_key, '\t') || !std::getline(stream, type, '\t') ||
                        !std::getline(stream, encoded_value)) {
                    continue;
                }
                std::string key;
                if (!hex_decode(encoded_key, key)) {
                    continue;
                }
                if (type == "b") {
                    namespaces_[nspace][key] = (encoded_value == "1");
                } else if (type == "i") {
                    namespaces_[nspace][key] = static_cast<int32_t>(std::strtol(encoded_value.c_str(), nullptr, 10));
                } else if (type == "s") {
                    std::string value;
                    if (hex_decode(encoded_value, value)) {
                        namespaces_[nspace][key] = std::move(value);
                    }
                }
            }
        }
    }

    bool check_initialized()
    {
        if (is_initialized_) {
            return true;
        }

        last_error_ = "Storage key-value linux is not initialized";
        BROOKESIA_LOGE("%1%", last_error_);
        return false;
    }

    bool is_initialized_ = false;
    std::mutex mutex_;
    std::map<std::string, KeyValueMap> namespaces_;
    std::filesystem::path root_dir_;
};

class StorageFileSystemLinuxImpl: public storage::FileSystemIface {
public:
    StorageFileSystemLinuxImpl()
        : storage::FileSystemIface()
    {
        root_dir_ = get_file_system_root();
        std::error_code error_code;
        for (const auto &entry : FILE_SYSTEM_ENTRIES) {
            std::filesystem::create_directories(root_dir_ / entry.subdirectory, error_code);
            auto info = entry.info;
            info.root_path = (root_dir_ / entry.subdirectory).lexically_normal().generic_string();
            info_list_.push_back(std::move(info));
        }
    }

    bool get_capacity(const char *mount_point, Capacity &capacity) override
    {
        const std::string mount_point_string(mount_point == nullptr ? "" : mount_point);
        std::filesystem::path path = {};
        for (const auto &entry : FILE_SYSTEM_ENTRIES) {
            if (mount_point_string == entry.info.mount_point) {
                path = root_dir_ / entry.subdirectory;
                break;
            }
        }
        if (path.empty()) {
            BROOKESIA_LOGW("File system not found for mount point: %1%", mount_point_string);
            return false;
        }

        struct statvfs stat = {};
        if (statvfs(path.c_str(), &stat) != 0) {
            return false;
        }

        capacity.total_bytes = static_cast<uint64_t>(stat.f_blocks) * stat.f_frsize;
        capacity.free_bytes = static_cast<uint64_t>(stat.f_bavail) * stat.f_frsize;
        capacity.used_bytes = capacity.total_bytes - capacity.free_bytes;
        return true;
    }

private:
    static std::filesystem::path get_file_system_root()
    {
        if (const char *fs_root = std::getenv("BROOKESIA_HAL_LINUX_FS_ROOT")) {
            if (fs_root[0] != '\0') {
                return std::filesystem::path(fs_root);
            }
        }
        return get_linux_storage_system_root() / "fs";
    }

    std::filesystem::path root_dir_;
};

bool StorageLinuxDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> StorageLinuxDevice::get_interface_specs() const
{
    return {
        {storage::KeyValueIface::NAME, KEY_VALUE_IFACE_NAME},
        {storage::FileSystemIface::NAME, FILE_SYSTEM_IFACE_NAME},
    };
}

bool StorageLinuxDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        key_value_iface_ = std::make_shared<StorageKeyValueLinuxImpl>(), false,
        "Failed to create storage key-value linux"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        key_value_iface_->init(), false, "Failed to initialize storage key-value linux"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        file_system_iface_ = std::make_shared<StorageFileSystemLinuxImpl>(), false,
        "Failed to create storage file-system linux"
    );

    interfaces_.emplace(KEY_VALUE_IFACE_NAME, key_value_iface_);
    interfaces_.emplace(FILE_SYSTEM_IFACE_NAME, file_system_iface_);
    return true;
}

void StorageLinuxDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (key_value_iface_) {
        key_value_iface_->deinit();
    }
    interfaces_.erase(FILE_SYSTEM_IFACE_NAME);
    interfaces_.erase(KEY_VALUE_IFACE_NAME);
    file_system_iface_.reset();
    key_value_iface_.reset();
}

#if BROOKESIA_HAL_LINUX_ENABLE_STORAGE_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, StorageLinuxDevice, StorageLinuxDevice::DEVICE_NAME, StorageLinuxDevice::get_instance(),
    BROOKESIA_HAL_LINUX_STORAGE_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
