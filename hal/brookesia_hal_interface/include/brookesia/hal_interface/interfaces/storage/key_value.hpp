/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file key_value.hpp
 * @brief Declares the generic key-value storage HAL interface.
 */

namespace esp_brookesia::hal::storage {

/**
 * @brief Generic namespace-based key-value storage interface.
 */
class KeyValueIface: public Interface {
public:
    static constexpr const char *NAME = "StorageKeyValue";  ///< Interface registry name.

    /**
     * @brief Supported stored value type.
     */
    enum class ValueType {
        Bool,   ///< Boolean value.
        Int,    ///< 32-bit signed integer value.
        String, ///< String value.
        Max,    ///< Invalid value type.
    };

    /**
     * @brief Value stored in a key-value namespace.
     */
    using Value = std::variant<bool, int32_t, std::string>;

    /**
     * @brief Key-value mapping payload for batch operations.
     */
    using KeyValueMap = std::map<std::string, Value>;

    /**
     * @brief Metadata for one stored entry.
     */
    struct EntryInfo {
        std::string nspace; ///< Namespace name.
        std::string key;    ///< Entry key.
        ValueType type;     ///< Entry value type.
    };

    /**
     * @brief Capability limits exposed by the key-value backend.
     *
     * A value of 0 means the backend does not declare a limit for that field.
     */
    struct Info {
        uint32_t max_namespace_length = 0; ///< Maximum namespace length in characters.
        uint32_t max_key_length = 0;       ///< Maximum key length in characters.
    };

    /**
     * @brief Construct a key-value storage interface.
     */
    KeyValueIface()
        : Interface(NAME)
    {
    }

    /**
     * @brief Virtual destructor for polymorphic storage interfaces.
     */
    virtual ~KeyValueIface() = default;

    /**
     * @brief Initialize the key-value storage backend.
     *
     * @return true on success, otherwise false.
     */
    virtual bool init() = 0;

    /**
     * @brief Deinitialize the key-value storage backend.
     */
    virtual void deinit() = 0;

    /**
     * @brief List entries under a namespace.
     *
     * @param[in] nspace Namespace to list.
     * @param[out] entries Listed entries.
     * @return true on success, otherwise false.
     */
    virtual bool list(const std::string &nspace, std::vector<EntryInfo> &entries) = 0;

    /**
     * @brief Set key-value pairs under a namespace.
     *
     * @param[in] nspace Namespace to write.
     * @param[in] key_value_map Values to write.
     * @return true on success, otherwise false.
     */
    virtual bool set(const std::string &nspace, const KeyValueMap &key_value_map) = 0;

    /**
     * @brief Get key-value pairs under a namespace.
     *
     * Passing an empty key list returns all entries in the namespace.
     *
     * @param[in] nspace Namespace to read.
     * @param[in] keys Keys to read. Empty means all keys.
     * @param[out] key_value_map Read values.
     * @return true on success, otherwise false.
     */
    virtual bool get(
        const std::string &nspace, const std::vector<std::string> &keys, KeyValueMap &key_value_map
    ) = 0;

    /**
     * @brief Erase keys under a namespace.
     *
     * Passing an empty key list erases the whole namespace.
     *
     * @param[in] nspace Namespace to erase.
     * @param[in] keys Keys to erase. Empty means all keys.
     * @return true on success, otherwise false.
     */
    virtual bool erase(const std::string &nspace, const std::vector<std::string> &keys) = 0;

    /**
     * @brief Get the most recent backend error.
     *
     * @return Human-readable error string.
     */
    const std::string &get_last_error() const
    {
        return last_error_;
    }

    /**
     * @brief Get key-value backend capability limits.
     *
     * @return Key-value backend information.
     */
    const Info &get_info() const
    {
        return info_;
    }

protected:
    Info info_;
    std::string last_error_;
};

BROOKESIA_DESCRIBE_ENUM(KeyValueIface::ValueType, Bool, Int, String, Max);
BROOKESIA_DESCRIBE_STRUCT(KeyValueIface::EntryInfo, (), (nspace, key, type));
BROOKESIA_DESCRIBE_STRUCT(KeyValueIface::Info, (), (max_namespace_length, max_key_length));

} // namespace esp_brookesia::hal::storage
