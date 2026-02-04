/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <vector>
#include <string>
#include <memory>
#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"
#include "boost/format.hpp"
#include "brookesia/service_nvs/macro_configs.h"
#if !BROOKESIA_SERVICE_NVS_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_nvs/service_nvs.hpp"

namespace esp_brookesia::service {

bool NVS::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_NVS_VER_MAJOR, BROOKESIA_SERVICE_NVS_VER_MINOR,
        BROOKESIA_SERVICE_NVS_VER_PATCH
    );

    /* Initialize NVS flash */
    esp_err_t ret = nvs_flash_init();
    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        BROOKESIA_LOGI("NVS partition was truncated and needs to be erased");
        BROOKESIA_CHECK_ESP_ERR_RETURN(nvs_flash_erase(), false, "Erase NVS flash failed");
        BROOKESIA_CHECK_ESP_ERR_RETURN(nvs_flash_init(), false, "Init NVS flash failed");
    } else {
        BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Initialize NVS flash failed");
    }

    return true;
}

void NVS::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    /* Deinitialize NVS flash */
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(nvs_flash_deinit(), {}, {
        BROOKESIA_LOGE("Deinitialize NVS flash failed");
    });
}

std::expected<boost::json::array, std::string> NVS::function_list(const std::string &nspace)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: nspace(%1%)", nspace);

    esp_err_t ret = ESP_OK;
    std::vector<Helper::EntryInfo> entries;
    nvs_iterator_t it = NULL;
    lib_utils::FunctionGuard release_iterator_guard([&]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (it != NULL) {
            nvs_release_iterator(it);
        }
    });

    ret = nvs_entry_find(NVS_DEFAULT_PART_NAME, nspace.c_str(), NVS_TYPE_ANY, &it);
    if (ret != ESP_OK) {
        return std::unexpected((
                                   boost::format("Failed to find entry in namespace '%1%': %2%") %
                                   nspace % esp_err_to_name(ret)
                               ).str());
    }

    do {
        nvs_entry_info_t info;
        ret = nvs_entry_info(it, &info);
        if (ret != ESP_OK) {
            return std::unexpected((
                                       boost::format("Failed to get entry info in namespace '%1%': %2%") %
                                       nspace % esp_err_to_name(ret)
                                   ).str());
        }

        auto nvs_value_type_to_string = [](nvs_type_t type) {
            switch (type) {
            case NVS_TYPE_U8:
                return Helper::ValueType::Bool;
            case NVS_TYPE_I32:
                return Helper::ValueType::Int;
            case NVS_TYPE_STR:
                return Helper::ValueType::String;
            default:
                return Helper::ValueType::Max;
            }
        };
        entries.emplace_back(info.namespace_name, info.key, nvs_value_type_to_string(info.type));

        ret = nvs_entry_next(&it);
    } while (ret == ESP_OK);

    if (ret != ESP_ERR_NVS_NOT_FOUND) {
        return std::unexpected((
                                   boost::format("Error occurred when iterating entries in namespace '%1%': %2%") %
                                   nspace % esp_err_to_name(ret)
                               ).str());
    }

    return BROOKESIA_DESCRIBE_TO_JSON(entries).as_array();
}

std::expected<void, std::string> NVS::function_set(const std::string &nspace, boost::json::object &&key_value_map)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: nspace(%1%), key_value_map(%2%)", nspace, BROOKESIA_DESCRIBE_TO_STR(key_value_map));

    KeyValueMap parsed_key_value_map;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(key_value_map, parsed_key_value_map)) {
        return std::unexpected((
                                   boost::format("Failed to parse key-value map in namespace '%1%': %2%") %
                                   nspace % BROOKESIA_DESCRIBE_TO_STR(key_value_map)
                               ).str());
    }

    // Open NVS namespace using C++ interface
    esp_err_t ret = ESP_OK;
    auto handle = nvs::open_nvs_handle(nspace.c_str(), NVS_READWRITE, &ret);
    if (ret != ESP_OK) {
        return std::unexpected((
                                   boost::format("Failed to open NVS namespace '%1%': %2%") % nspace %
                                   esp_err_to_name(ret)
                               ).str());
    }

    auto store_key_value_pair = [&handle, &nspace](const std::string & key, const Helper::Value & value) ->
    std::expected<void, std::string> {
        const char *key_str = key.c_str();
        esp_err_t ret = ESP_FAIL;

        std::visit([&](auto &&value)
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, bool>) {
                // Store boolean as uint8_t (0 or 1)
                uint8_t bool_value = value ? 1 : 0;
                ret = handle->set_item(key_str, bool_value);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                // Store int as int32_t
                ret = handle->set_item(key_str, value);
            } else if constexpr (std::is_same_v<T, std::string>) {
                // Store string
                ret = handle->set_string(key_str, value.c_str());
            }
        }, value);

        if (ret != ESP_OK)
        {
            return std::unexpected((
                                       boost::format("Failed to set key '%1%' in namespace '%2%': %3%") %
                                       key % nspace % esp_err_to_name(ret)
                                   ).str());
        }
        return {};
    };

    // Store key-value pairs
    for (const auto &[key, value] : parsed_key_value_map) {
        auto result = store_key_value_pair(key, value);
        if (!result) {
            return std::unexpected(result.error());
        }
    }

    // Commit changes
    ret = handle->commit();
    if (ret != ESP_OK) {
        return std::unexpected((
                                   boost::format("Failed to commit NVS changes in namespace '%1%': %2%") % nspace %
                                   esp_err_to_name(ret)
                               ).str());
    }
    return {};
}

std::expected<boost::json::object, std::string> NVS::function_get(const std::string &nspace, boost::json::array &&keys)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: nspace(%1%), keys(%2%)", nspace, keys);

    // Open NVS namespace using C++ interface
    esp_err_t err;
    auto handle = nvs::open_nvs_handle(nspace.c_str(), NVS_READONLY, &err);
    if (err != ESP_OK) {
        return std::unexpected((
                                   boost::format("Failed to open NVS namespace '%1%': %2%") % nspace %
                                   esp_err_to_name(err)
                               ).str());
    }

    // Helper lambda to get value from NVS
    auto get_key_value =
    [&handle, &nspace](const std::string & key) -> std::expected<Helper::Value, std::string> {
        const char *key_str = key.c_str();

        // Try uint8_t first (for boolean)
        uint8_t bool_value;
        esp_err_t err = handle->get_item(key_str, bool_value);
        if (err == ESP_OK)
        {
            BROOKESIA_LOGD("Get key '%1%' = %2% (bool)", key_str, bool_value != 0);
            return bool_value != 0;
        }

        // Try int32_t
        int32_t i32_value;
        err = handle->get_item(key_str, i32_value);
        if (err == ESP_OK)
        {
            BROOKESIA_LOGD("Get key '%1%' = %2% (int32)", key_str, i32_value);
            return i32_value;
        }

        // Try string
        size_t required_size = 0;
        err = handle->get_item_size(nvs::ItemType::SZ, key_str, required_size);
        if (err == ESP_OK && required_size > 0)
        {
            std::vector<char> buffer(required_size);
            err = handle->get_string(key_str, buffer.data(), required_size);
            if (err == ESP_OK) {
                std::string str_value(buffer.data());
                BROOKESIA_LOGD("Get key '%1%' = '%2%' (string)", key_str, str_value);
                return str_value;
            }
        } else if (err == ESP_OK && required_size == 0)
        {
            BROOKESIA_LOGD("Get key '%1%' = '' (empty string)", key_str);
            return std::string("");
        }

        // Key not found
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            return std::unexpected((
                                       boost::format("Key '%1%' not found in namespace '%2%'") % key % nspace
                                   ).str());
        }

        // Other error
        return std::unexpected((
                                   boost::format("Failed to get key '%1%' in namespace '%2%': %3%") % key % nspace % esp_err_to_name(err)
                               ).str());
    };

    KeyValueMap key_value_map;

    if (keys.empty()) {
        BROOKESIA_LOGD("No keys provided, get all keys in namespace '%1%'", nspace);

        auto items_array = function_list(nspace);
        if (!items_array) {
            return std::unexpected(items_array.error());
        }

        // Parse entry infos
        std::vector<Helper::EntryInfo> entries;
        if (!BROOKESIA_DESCRIBE_FROM_JSON(items_array.value(), entries)) {
            return std::unexpected((
                                       boost::format("Failed to parse entry list in namespace '%1%'") % nspace
                                   ).str());
        }

        // Get all keys
        for (const auto &entry : entries) {
            auto kv_result = get_key_value(entry.key);
            if (kv_result) {
                key_value_map[entry.key] = kv_result.value();
            } else {
                BROOKESIA_LOGW("Failed to get key '%1%': %2%", entry.key, kv_result.error());
            }
        }

        BROOKESIA_LOGD("Got %1% keys in namespace '%2%'", key_value_map.size(), nspace);
    } else {
        // Get each key
        for (const auto &key_json : keys) {
            if (!key_json.is_string()) {
                BROOKESIA_LOGW("Key must be a string, skipping");
                continue;
            }

            std::string key = std::string(key_json.as_string());
            auto kv_result = get_key_value(key);
            if (kv_result) {
                key_value_map[key] = kv_result.value();
            } else {
                BROOKESIA_LOGW("Skipping key '%1%': %2%", key, kv_result.error());
            }
        }

        BROOKESIA_LOGD("Retrieved %1% key-value pairs from namespace '%2%'", key_value_map.size(), nspace);
    }

    return BROOKESIA_DESCRIBE_TO_JSON(key_value_map).as_object();
}

std::expected<void, std::string> NVS::function_erase(const std::string &nspace, boost::json::array &&keys)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: nspace(%1%), keys(%2%)", nspace, keys);

    // Open NVS namespace using C++ interface
    esp_err_t err;
    auto handle = nvs::open_nvs_handle(nspace.c_str(), NVS_READWRITE, &err);
    if (err != ESP_OK) {
        return std::unexpected((
                                   boost::format("Failed to open NVS namespace '%1%': %2%") % nspace %
                                   esp_err_to_name(err)
                               ).str());
    }

    // If keys array is empty, erase all keys in the namespace
    if (keys.empty()) {
        err = handle->erase_all();
        if (err != ESP_OK) {
            return std::unexpected((
                                       boost::format("Failed to erase all keys in namespace '%1%': %2%") % nspace %
                                       esp_err_to_name(err)
                                   ).str());
        }
        BROOKESIA_LOGD("Erased all keys in namespace '%1%'", nspace);
    } else {
        // Erase specific keys
        size_t erased_count = 0;
        for (const auto &key_json : keys) {
            if (!key_json.is_string()) {
                BROOKESIA_LOGW("Key must be a string, skipping");
                continue;
            }

            std::string key = std::string(key_json.as_string());
            const char *key_str = key.c_str();

            err = handle->erase_item(key_str);
            if (err == ESP_OK) {
                erased_count++;
                BROOKESIA_LOGD("Erased key '%1%' from namespace '%2%'", key_str, nspace);
            } else if (err == ESP_ERR_NVS_NOT_FOUND) {
                BROOKESIA_LOGW("Key '%1%' not found in namespace '%2%'", key_str, nspace);
                // Continue even if key not found
            } else {
                return std::unexpected((
                                           boost::format("Failed to erase key '%1%' in namespace '%2%': %3%") % key %
                                           nspace % esp_err_to_name(err)
                                       ).str());
            }
        }
        BROOKESIA_LOGD("Erased %zu key(s) from namespace '%1%'", erased_count, nspace);
    }

    // Commit changes
    err = handle->commit();
    if (err != ESP_OK) {
        return std::unexpected((
                                   boost::format("Failed to commit NVS changes in namespace '%1%': %2%") % nspace %
                                   esp_err_to_name(err)
                               ).str());
    }

    return {};
}

#if BROOKESIA_SERVICE_NVS_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, NVS, NVS::get_instance().get_attributes().name, NVS::get_instance(),
    BROOKESIA_SERVICE_NVS_PLUGIN_SYMBOL
);
#endif // BROOKESIA_SERVICE_NVS_ENABLE_AUTO_REGISTER

} // namespace esp_brookesia::service
