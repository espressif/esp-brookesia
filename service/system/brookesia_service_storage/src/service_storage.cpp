/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <cstdint>
#include <expected>
#include <string_view>
#include <utility>
#include <vector>

#include "boost/format.hpp"
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

constexpr uint64_t FNV1A64_OFFSET_BASIS = 14695981039346656037ULL;
constexpr uint64_t FNV1A64_PRIME = 1099511628211ULL;
constexpr char BASE36_DIGITS[] = "0123456789abcdefghijklmnopqrstuvwxyz";

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

} // namespace

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
        fs_iface_ = std::move(fs_iface);
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
