/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "brookesia/runtime_manager/types.hpp"
#include "brookesia/service_helper.hpp"

namespace esp_brookesia::runtime::wasm {

class ResultStore {
public:
    int32_t store(std::string result)
    {
        if (results_.size() >= max_result_count) {
            results_.erase(results_.begin());
        }
        const int32_t result_id = next_result_id_++;
        results_.emplace(result_id, std::move(result));
        return result_id;
    }

    const std::string *find(int32_t result_id) const
    {
        auto it = results_.find(result_id);
        return it != results_.end() ? &it->second : nullptr;
    }

    void erase(int32_t result_id)
    {
        results_.erase(result_id);
    }

private:
    static constexpr size_t max_result_count = 64;

    std::map<int32_t, std::string> results_;
    int32_t next_result_id_ = 1;
};

struct HostContext {
    const std::vector<NativeModule> *modules = nullptr;
    ResultStore results;
};

inline std::expected<std::vector<uint8_t>, std::string> read_wasm_file(const std::string &path)
{
    constexpr uint32_t storage_fs_timeout_ms = 5000;
    auto info = service::helper::Storage::fs_stat(path, storage_fs_timeout_ms);
    if (!info) {
        return std::unexpected("Failed to stat WASM file: " + path + ", error: " + info.error());
    }
    if (!info->exists || info->type != service::helper::Storage::FileType::File || info->size == 0) {
        return std::unexpected("Invalid WASM file size: " + path);
    }

    std::vector<uint8_t> bytes(static_cast<size_t>(info->size));
    auto read_result = service::helper::Storage::fs_read(
                           path, service::RawBuffer(bytes.data(), bytes.size()), storage_fs_timeout_ms
                       );
    if (!read_result) {
        return std::unexpected("Failed to read WASM file: " + path + ", error: " + read_result.error());
    }
    if (read_result.value() != bytes.size()) {
        return std::unexpected("WASM file read size mismatch: " + path);
    }
    return bytes;
}

inline int32_t store_result(HostContext &host_context, std::string result)
{
    return host_context.results.store(std::move(result));
}

inline int64_t native_value_to_i64(const NativeValue &value)
{
    return std::visit(
    [](const auto & item) -> int64_t {
        using ItemType = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<ItemType, int64_t>)
        {
            return item;
        } else if constexpr (std::is_same_v<ItemType, double>)
        {
            return static_cast<int64_t>(item);
        } else if constexpr (std::is_same_v<ItemType, bool>)
        {
            return item ? 1 : 0;
        } else
        {
            return 0;
        }
    },
    value
           );
}

inline bool native_value_to_bool(const NativeValue &value)
{
    return std::visit(
    [](const auto & item) -> bool {
        using ItemType = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<ItemType, bool>)
        {
            return item;
        } else if constexpr (std::is_same_v<ItemType, int64_t>)
        {
            return item != 0;
        } else if constexpr (std::is_same_v<ItemType, double>)
        {
            return item != 0;
        } else
        {
            return false;
        }
    },
    value
           );
}

inline std::string native_value_to_string(const NativeValue &value)
{
    return std::holds_alternative<std::string>(value) ? std::get<std::string>(value) : "";
}

} // namespace esp_brookesia::runtime::wasm
