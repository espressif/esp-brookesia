/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <type_traits>
#include <utility>
#include <vector>
#include "nvs.h"
#include "nvs_handle.hpp"
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_STORAGE_KEY_VALUE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/nvs_flash_manager.hpp"
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "key_value_nvs_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_KEY_VALUE_IMPL

namespace esp_brookesia::hal {

namespace {

storage::KeyValueIface::ValueType to_value_type(nvs_type_t type)
{
    switch (type) {
    case NVS_TYPE_U8:
        return storage::KeyValueIface::ValueType::Bool;
    case NVS_TYPE_I32:
        return storage::KeyValueIface::ValueType::Int;
    case NVS_TYPE_STR:
        return storage::KeyValueIface::ValueType::String;
    default:
        return storage::KeyValueIface::ValueType::Max;
    }
}

} // namespace

StorageKeyValueNvsImpl::StorageKeyValueNvsImpl()
{
    info_ = storage::KeyValueIface::Info{
        .max_namespace_length = NVS_NS_NAME_MAX_SIZE - 1,
        .max_key_length = NVS_KEY_NAME_MAX_SIZE - 1,
    };
}

StorageKeyValueNvsImpl::~StorageKeyValueNvsImpl()
{
    deinit();
}

bool StorageKeyValueNvsImpl::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_initialized_) {
        return true;
    }

    if (!nvs_flash_manager::acquire()) {
        set_last_error(nvs_flash_manager::get_last_error());
        return false;
    }

    is_initialized_ = true;
    last_error_.clear();
    return true;
}

void StorageKeyValueNvsImpl::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_initialized_) {
        return;
    }

    nvs_flash_manager::release();
    is_initialized_ = false;
}

bool StorageKeyValueNvsImpl::list(const std::string &nspace, std::vector<EntryInfo> &entries)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: nspace(%1%)", nspace);

    entries.clear();
    if (!is_initialized_) {
        set_last_error("Storage KV NVS backend is not initialized");
        return false;
    }

    nvs_iterator_t it = nullptr;
    lib_utils::FunctionGuard release_iterator_guard([&]() {
        if (it != nullptr) {
            nvs_release_iterator(it);
        }
    });

    auto result = nvs_entry_find(NVS_DEFAULT_PART_NAME, nspace.c_str(), NVS_TYPE_ANY, &it);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        last_error_.clear();
        return true;
    }
    if (result != ESP_OK) {
        set_last_error(boost::format("Failed to find entries in namespace '%1%': %2%") % nspace %
                       esp_err_to_name(result));
        return false;
    }

    do {
        nvs_entry_info_t info = {};
        result = nvs_entry_info(it, &info);
        if (result != ESP_OK) {
            set_last_error(boost::format("Failed to get entry info in namespace '%1%': %2%") % nspace %
                           esp_err_to_name(result));
            return false;
        }

        entries.push_back(EntryInfo{
            .nspace = info.namespace_name,
            .key = info.key,
            .type = to_value_type(info.type),
        });

        result = nvs_entry_next(&it);
    } while (result == ESP_OK);

    if (result != ESP_ERR_NVS_NOT_FOUND) {
        set_last_error(boost::format("Failed to iterate entries in namespace '%1%': %2%") % nspace %
                       esp_err_to_name(result));
        return false;
    }

    last_error_.clear();
    return true;
}

bool StorageKeyValueNvsImpl::set(const std::string &nspace, const KeyValueMap &key_value_map)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: nspace(%1%), key_value_count(%2%)", nspace, key_value_map.size());

    if (!is_initialized_) {
        set_last_error("Storage KV NVS backend is not initialized");
        return false;
    }

    esp_err_t result = ESP_OK;
    auto handle = nvs::open_nvs_handle(nspace.c_str(), NVS_READWRITE, &result);
    if (result != ESP_OK) {
        set_last_error(boost::format("Failed to open namespace '%1%': %2%") % nspace % esp_err_to_name(result));
        return false;
    }

    for (const auto &[key, value] : key_value_map) {
        std::visit([&](auto &&stored_value) {
            using T = std::decay_t<decltype(stored_value)>;
            if constexpr (std::is_same_v<T, bool>) {
                result = handle->set_item(key.c_str(), static_cast<uint8_t>(stored_value ? 1 : 0));
            } else if constexpr (std::is_same_v<T, int32_t>) {
                result = handle->set_item(key.c_str(), stored_value);
            } else if constexpr (std::is_same_v<T, std::string>) {
                result = handle->set_string(key.c_str(), stored_value.c_str());
            }
        }, value);

        if (result != ESP_OK) {
            set_last_error(boost::format("Failed to set key '%1%' in namespace '%2%': %3%") % key % nspace %
                           esp_err_to_name(result));
            return false;
        }
    }

    result = handle->commit();
    if (result != ESP_OK) {
        set_last_error(boost::format("Failed to commit namespace '%1%': %2%") % nspace % esp_err_to_name(result));
        return false;
    }

    last_error_.clear();
    return true;
}

bool StorageKeyValueNvsImpl::get(
    const std::string &nspace, const std::vector<std::string> &keys, KeyValueMap &key_value_map
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: nspace(%1%), keys(%2%)", nspace, BROOKESIA_DESCRIBE_TO_STR(keys));

    key_value_map.clear();
    if (!is_initialized_) {
        set_last_error("Storage KV NVS backend is not initialized");
        return false;
    }

    std::vector<std::string> keys_to_read = keys;
    if (keys_to_read.empty()) {
        std::vector<EntryInfo> entries;
        if (!list(nspace, entries)) {
            return false;
        }
        for (const auto &entry : entries) {
            keys_to_read.push_back(entry.key);
        }
    }

    for (const auto &key : keys_to_read) {
        Value value;
        if (get_one(nspace, key, value)) {
            key_value_map.emplace(key, std::move(value));
        } else {
            BROOKESIA_LOGW("Skipping key '%1%': %2%", key, last_error_);
        }
    }

    last_error_.clear();
    return true;
}

bool StorageKeyValueNvsImpl::erase(const std::string &nspace, const std::vector<std::string> &keys)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: nspace(%1%), keys(%2%)", nspace, BROOKESIA_DESCRIBE_TO_STR(keys));

    if (!is_initialized_) {
        set_last_error("Storage KV NVS backend is not initialized");
        return false;
    }

    esp_err_t result = ESP_OK;
    auto handle = nvs::open_nvs_handle(nspace.c_str(), NVS_READWRITE, &result);
    if (result != ESP_OK) {
        set_last_error(boost::format("Failed to open namespace '%1%': %2%") % nspace % esp_err_to_name(result));
        return false;
    }

    if (keys.empty()) {
        result = handle->erase_all();
        if (result != ESP_OK) {
            set_last_error(boost::format("Failed to erase namespace '%1%': %2%") % nspace % esp_err_to_name(result));
            return false;
        }
    } else {
        for (const auto &key : keys) {
            result = handle->erase_item(key.c_str());
            if (result == ESP_ERR_NVS_NOT_FOUND) {
                BROOKESIA_LOGW("Key '%1%' not found in namespace '%2%'", key, nspace);
                continue;
            }
            if (result != ESP_OK) {
                set_last_error(boost::format("Failed to erase key '%1%' in namespace '%2%': %3%") % key % nspace %
                               esp_err_to_name(result));
                return false;
            }
        }
    }

    result = handle->commit();
    if (result != ESP_OK) {
        set_last_error(boost::format("Failed to commit namespace '%1%': %2%") % nspace % esp_err_to_name(result));
        return false;
    }

    last_error_.clear();
    return true;
}

void StorageKeyValueNvsImpl::set_last_error(const boost::format &format)
{
    set_last_error(format.str());
}

void StorageKeyValueNvsImpl::set_last_error(std::string error)
{
    last_error_ = std::move(error);
    BROOKESIA_LOGW("%1%", last_error_);
}

bool StorageKeyValueNvsImpl::get_one(const std::string &nspace, const std::string &key, Value &value)
{
    esp_err_t result = ESP_OK;
    auto handle = nvs::open_nvs_handle(nspace.c_str(), NVS_READONLY, &result);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        set_last_error(boost::format("Namespace '%1%' not found") % nspace);
        return false;
    }
    if (result != ESP_OK) {
        set_last_error(boost::format("Failed to open namespace '%1%': %2%") % nspace % esp_err_to_name(result));
        return false;
    }

    uint8_t bool_value = 0;
    result = handle->get_item(key.c_str(), bool_value);
    if (result == ESP_OK) {
        value = (bool_value != 0);
        return true;
    }

    int32_t int_value = 0;
    result = handle->get_item(key.c_str(), int_value);
    if (result == ESP_OK) {
        value = int_value;
        return true;
    }

    size_t required_size = 0;
    result = handle->get_item_size(nvs::ItemType::SZ, key.c_str(), required_size);
    if (result == ESP_OK) {
        if (required_size == 0) {
            value = std::string();
            return true;
        }

        std::vector<char> buffer(required_size);
        result = handle->get_string(key.c_str(), buffer.data(), required_size);
        if (result == ESP_OK) {
            value = std::string(buffer.data());
            return true;
        }
    }

    if (result == ESP_ERR_NVS_NOT_FOUND) {
        set_last_error(boost::format("Key '%1%' not found in namespace '%2%'") % key % nspace);
    } else {
        set_last_error(boost::format("Failed to get key '%1%' in namespace '%2%': %3%") % key % nspace %
                       esp_err_to_name(result));
    }

    return false;
}

} // namespace esp_brookesia::hal

#endif // BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_KEY_VALUE_IMPL
