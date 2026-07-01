/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <expected>
#include <initializer_list>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include "boost/format.hpp"
#include "boost/json.hpp"
#include "brookesia/hal_interface/interfaces/storage/file_system.hpp"
#include "brookesia/hal_interface/interfaces/storage/key_value.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/helper/base.hpp"

namespace esp_brookesia::service::helper {

struct StorageFileSystemInfo {
    hal::storage::FileSystemIface::FileSystemType fs_type = hal::storage::FileSystemIface::FileSystemType::LittleFS;
    hal::storage::FileSystemIface::MediumType medium_type = hal::storage::FileSystemIface::MediumType::Flash;
    std::string mount_point;
    bool supports_directories = false;
};

/**
 * @brief Helper schema definitions for the Storage service.
 */
class Storage : public Base<Storage> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * They are used as parameter and return types for functions and events.
     * Users can access or modify these types via serialization and deserialization.
     */
    using ValueType = hal::storage::KeyValueIface::ValueType;
    using Value = hal::storage::KeyValueIface::Value;
    using KeyValueMap = hal::storage::KeyValueIface::KeyValueMap;
    using EntryInfo = hal::storage::KeyValueIface::EntryInfo;
    using FileSystemInfo = StorageFileSystemInfo;
    using FileSystemCapacity = hal::storage::FileSystemIface::Capacity;

    struct KvNameResult {
        std::string name;
        std::string original_name;
        bool hashed = false;
        std::string warning;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Storage service function identifiers.
     */
    enum class FunctionId {
        KVList,
        KVSet,
        KVGet,
        KVErase,
        GetFileSystems,
        GetFileSystemCapacity,
        MakeKVKey,
        MakeKVNamespace,
        Max,
    };

    /**
     * @brief Storage service event identifiers.
     */
    enum class EventId {
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Parameter keys for `FunctionId::KVList`.
     */
    enum class FunctionKVListParam {
        Nspace,
    };

    /**
     * @brief Parameter keys for `FunctionId::KVSet`.
     */
    enum class FunctionKVSetParam {
        Nspace,
        KeyValuePairs,
    };

    /**
     * @brief Parameter keys for `FunctionId::KVGet`.
     */
    enum class FunctionKVGetParam {
        Nspace,
        Keys,
    };

    /**
     * @brief Parameter keys for `FunctionId::KVErase`.
     */
    enum class FunctionKVEraseParam {
        Nspace,
        Keys,
    };

    enum class FunctionGetFileSystemCapacityParam {
        MountPoint,
    };

    enum class FunctionMakeKVNameParam {
        Parts,
        Separator,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Storage has no events, so no event parameter types are defined

private:
    static constexpr std::string_view DEFAULT_NAMESPACE = "storage";

    template <typename T>
    static boost::json::object make_key_value_object(const std::string &key, const T &value)
    {
        boost::json::value json_value;
        if constexpr (std::is_same_v<T, bool>) {
            json_value = value;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            json_value = value;
        } else if constexpr (std::is_integral_v<T> &&(sizeof(T) * 8 <= 32)) {
            json_value = static_cast<int32_t>(value);
        } else {
            json_value = BROOKESIA_DESCRIBE_JSON_SERIALIZE(value);
        }

        return boost::json::object{{key, std::move(json_value)}};
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static FunctionSchema function_schema_list()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::KVList),
            .description = "List key-value entries in a Storage namespace.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionKVListParam::Nspace),
                    .description = "Namespace to list (optional). Uses the default namespace when omitted.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string(DEFAULT_NAMESPACE))
                }
            },
            .default_timeout_ms = 1000,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<EntryInfo>({
                    {"storage", "key1", ValueType::String},
                    {"storage", "key2", ValueType::Int}
                }))).str(),
            }
        };
    }

    static FunctionSchema function_schema_set()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::KVSet),
            .description = "Set key-value pairs in a Storage namespace.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionKVSetParam::Nspace),
                    .description = "Namespace to write (optional). Uses the default namespace when omitted or empty.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string(DEFAULT_NAMESPACE))
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionKVSetParam::KeyValuePairs),
                    .description = (boost::format("Key-value pairs as a JSON object. Allowed value types: %1%. "
                                                  "Example: %2%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<ValueType>({
                        ValueType::Bool, ValueType::Int, ValueType::String
                    })) % BROOKESIA_DESCRIBE_JSON_SERIALIZE(KeyValueMap({
                        {"key1", std::string("value1")}, {"key2", 2}, {"key3", true}
                    }))).str(),
                    .type = FunctionValueType::Object
                }
            },
            .default_timeout_ms = 1000,
        };
    }

    static FunctionSchema function_schema_get()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::KVGet),
            .description = "Get key-value pairs by keys from a Storage namespace.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionKVGetParam::Nspace),
                    .description = "Namespace to read (optional). Uses the default namespace when omitted.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string(DEFAULT_NAMESPACE))
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionKVGetParam::Keys),
                    .description = (boost::format("Keys to read as JSON array<string> (optional). "
                                                  "Returns all pairs when omitted. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({"key1", "key2", "key3"}))).str(),
                    .type = FunctionValueType::Array,
                    .default_value = FunctionValue(boost::json::array())
                }
            },
            .default_timeout_ms = 1000,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(KeyValueMap({
                    {"key1", "value1"}, {"key2", 2}, {"key3", true}
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_erase()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::KVErase),
            .description = "Erase key-value pairs from a Storage namespace.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionKVEraseParam::Nspace),
                    .description = "Namespace to erase (optional). Uses the default namespace when omitted.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string(DEFAULT_NAMESPACE))
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionKVEraseParam::Keys),
                    .description = (boost::format("Keys to erase as JSON array<string> (optional). "
                                                  "Erases all pairs when omitted or empty. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({"key1", "key2", "key3"}))).str(),
                    .type = FunctionValueType::Array,
                    .default_value = FunctionValue(boost::json::array())
                }
            },
            .default_timeout_ms = 1000,
        };
    }

    static FunctionSchema function_schema_get_file_systems()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetFileSystems),
            .description = "Get mounted storage file systems.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<FileSystemInfo>({
                    {
                        .fs_type = hal::storage::FileSystemIface::FileSystemType::LittleFS,
                        .medium_type = hal::storage::FileSystemIface::MediumType::Flash,
                        .mount_point = "/littlefs",
                        .supports_directories = true,
                    },
                    {
                        .fs_type = hal::storage::FileSystemIface::FileSystemType::FATFS,
                        .medium_type = hal::storage::FileSystemIface::MediumType::SDCard,
                        .mount_point = "/sdcard",
                        .supports_directories = true,
                    },
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_get_file_system_capacity()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetFileSystemCapacity),
            .description = "Get one mounted storage file system capacity.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionGetFileSystemCapacityParam::MountPoint),
                    .description = "Mount point returned by GetFileSystems.",
                    .type = FunctionValueType::String,
                }
            },
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE((FileSystemCapacity{
                    .total_bytes = 1024 * 1024,
                    .used_bytes = 256 * 1024,
                    .free_bytes = 768 * 1024,
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_make_kv_key()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::MakeKVKey),
            .description = "Generate a key that satisfies the active Storage KV backend limits.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionMakeKVNameParam::Parts),
                    .description = "Name parts as JSON array<string>. Empty parts are rejected.",
                    .type = FunctionValueType::Array,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionMakeKVNameParam::Separator),
                    .description = "Separator inserted between name parts.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string("."))
                }
            },
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE((KvNameResult{
                    .name = "h1abc",
                    .original_name = "bkl.display_lcd.On",
                    .hashed = true,
                    .warning = "KV key exceeded backend limit and was replaced by a stable hash",
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_make_kv_namespace()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::MakeKVNamespace),
            .description = "Generate a namespace that satisfies the active Storage KV backend limits.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionMakeKVNameParam::Parts),
                    .description = "Namespace parts as JSON array<string>. Empty parts are rejected.",
                    .type = FunctionValueType::Array,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionMakeKVNameParam::Separator),
                    .description = "Separator inserted between namespace parts.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string("."))
                }
            },
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE((KvNameResult{
                    .name = "Display",
                    .original_name = "Display",
                    .hashed = false,
                    .warning = "",
                }))).str(),
            },
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Storage has no events, so no event schemas are defined

public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the functions required by the Base class /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Service name used by `ServiceManager`.
     *
     * @return std::string_view Stable service name.
     */
    static constexpr std::string_view get_name()
    {
        return "Storage";
    }

    /**
     * @brief Get function schemas exported by Storage service.
     *
     * @return std::span<const FunctionSchema> Static function schema span.
     */
    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_list(),
                function_schema_set(),
                function_schema_get(),
                function_schema_erase(),
                function_schema_get_file_systems(),
                function_schema_get_file_system_capacity(),
                function_schema_make_kv_key(),
                function_schema_make_kv_namespace(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    /**
     * @brief Get event schemas exported by Storage service.
     *
     * @return std::span<const EventSchema> Empty span because Storage has no events.
     */
    static std::span<const EventSchema> get_event_schemas()
    {
        return std::span<const EventSchema>();
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function helper methods //////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Default timeout for synchronous Storage helper calls.
     */
    static constexpr uint32_t DEFAULT_TIMEOUT_MS = 1000;

    static std::expected<KvNameResult, std::string> make_kv_key(
        std::initializer_list<std::string_view> parts, std::string_view separator = ".",
        uint32_t timeout_ms = DEFAULT_TIMEOUT_MS
    )
    {
        return make_kv_name(FunctionId::MakeKVKey, parts, separator, timeout_ms);
    }

    static std::expected<KvNameResult, std::string> make_kv_namespace(
        std::initializer_list<std::string_view> parts, std::string_view separator = ".",
        uint32_t timeout_ms = DEFAULT_TIMEOUT_MS
    )
    {
        return make_kv_name(FunctionId::MakeKVNamespace, parts, separator, timeout_ms);
    }

    /**
     * @brief Save key-value pairs to the Storage namespace
     *
     * @note The storage method depends on the type T:
     *
     * **Direct Storage (No Serialization):**
     * - `bool`: Stored directly as JSON boolean value (true/false)
     * - `int32_t`: Stored directly as JSON number (int64_t in JSON)
     * - Integer types with size <= 32 bits (int8_t, uint8_t, int16_t, uint16_t, char, short, etc.):
     *   Converted to int32_t and stored as JSON number for optimal performance
     *
     * **Serialized Storage:**
     * - Integer types with size > 32 bits (int64_t, uint64_t, long long, etc.):
     *   Serialized to JSON string using BROOKESIA_DESCRIBE_JSON_SERIALIZE
     * - Floating point types (float, double):
     *   Serialized to JSON string using BROOKESIA_DESCRIBE_JSON_SERIALIZE
     * - String types (std::string, const char*):
     *   Serialized to JSON string using BROOKESIA_DESCRIBE_JSON_SERIALIZE
     * - Complex types (std::vector, std::map, custom structs, etc.):
     *   Serialized to JSON string using BROOKESIA_DESCRIBE_JSON_SERIALIZE
     *
     * @tparam T The type of the value to save
     * @param nspace The namespace of the key-value pairs to save
     * @param key The key of the key-value pair to save
     * @param value The value of the key-value pair to save
     * @param timeout_ms The timeout in milliseconds
     * @return std::expected<void, std::string> The result of the operation
     */
    template <typename T>
    static std::expected<void, std::string> save_key_value(
        const std::string &nspace, const std::string &key, const T &value, uint32_t timeout_ms = DEFAULT_TIMEOUT_MS
    )
    {
        auto binding = ServiceManager::get_instance().bind(get_name().data());
        if (!binding.is_valid()) {
            return std::unexpected("Failed to bind service");
        }

        auto data_object = make_key_value_object(key, value);
        auto result = call_function_sync(FunctionId::KVSet, nspace, std::move(data_object), Timeout(timeout_ms));
        if (!result) {
            return std::unexpected(
                       (boost::format("Failed to save %1% to Storage %2%: %3%") % key % nspace % result.error()).str()
                   );
        }

        return {};
    }

    /**
     * @brief Save key-value pairs to the Storage namespace asynchronously.
     *
     * The value conversion rules are identical to `save_key_value()`. The helper keeps a temporary
     * Storage service binding alive until the asynchronous function result is delivered.
     *
     * @tparam T The type of the value to save
     * @param nspace The namespace of the key-value pairs to save
     * @param key The key of the key-value pair to save
     * @param value The value of the key-value pair to save
     * @param handler Optional result handler
     * @return true if the async call was submitted, false otherwise
     */
    template <typename T>
    static bool save_key_value_async(
        const std::string &nspace, const std::string &key, const T &value,
        ServiceBase::FunctionResultHandler handler = nullptr
    )
    {
        auto binding = std::make_shared<ServiceBinding>(ServiceManager::get_instance().bind(get_name().data()));
        if (!binding->is_valid()) {
            return false;
        }

        auto wrapped_handler = [
                                   binding,
                                   handler = std::move(handler)
        ](FunctionResult && result) mutable {
            if (handler != nullptr)
            {
                handler(std::move(result));
            }
        };

        auto data_object = make_key_value_object(key, value);
        return call_function_async(
                   FunctionId::KVSet,
                   nspace,
                   std::move(data_object),
                   ServiceBase::FunctionResultHandler(std::move(wrapped_handler))
               );
    }

    /**
     * @brief Get key-value pair from the Storage namespace
     *
     * @note The retrieval method depends on the type T and matches the storage method used in save_key_value():
     *
     * **Direct Retrieval (No Deserialization):**
     * - `bool`: Retrieved directly from JSON boolean value
     * - `int32_t`: Retrieved directly from JSON number
     * - Integer types with size <= 32 bits (int8_t, uint8_t, int16_t, uint16_t, char, short, etc.):
     *   Retrieved directly from JSON number and converted to the target integer type
     *
     * **Deserialized Retrieval:**
     * - Integer types with size > 32 bits (int64_t, uint64_t, long long, etc.):
     *   Retrieved directly from JSON string and deserialized to the target integer type
     * - Floating point types (float, double):
     *   Retrieved directly from JSON string and deserialized to the target floating point type
     * - String types (std::string):
     *   Retrieved directly from JSON string and deserialized to the target string type
     * - Complex types (std::vector, std::map, custom structs, etc.):
     *   Retrieved directly from JSON string and deserialized to the target complex type
     *
     * @tparam T The type of the value to retrieve
     * @param nspace The namespace of the key-value pair to retrieve
     * @param key The key of the key-value pair to retrieve
     * @param timeout_ms The timeout in milliseconds
     * @return std::expected<T, std::string> The retrieved value or error message
     */
    template <typename T>
    static std::expected<T, std::string> get_key_value(
        const std::string &nspace, const std::string &key, uint32_t timeout_ms = DEFAULT_TIMEOUT_MS
    )
    {
        auto binding = ServiceManager::get_instance().bind(get_name().data());
        if (!binding.is_valid()) {
            return std::unexpected("Failed to bind service");
        }

        auto result = call_function_sync<boost::json::object>(
                          FunctionId::KVGet, nspace, boost::json::array{key}, Timeout(timeout_ms)
                      );
        if (!result) {
            return std::unexpected(
                       (boost::format("Failed to get %1% from Storage %2%: %3%") % key % nspace % result.error()).str()
                   );
        }

        auto &data_obj = result.value();
        if (!data_obj.contains(key)) {
            return std::unexpected((boost::format("Key %1% not found in namespace %2%") % key % nspace).str());
        }

        auto value_json = data_obj.at(key);
        T value;

        if constexpr (std::is_same_v<T, bool>) {
            if (!value_json.is_bool()) {
                return std::unexpected(
                           (boost::format("Value for key %1% in namespace %2% is not a boolean") % key % nspace).str()
                       );
            }
            value = value_json.as_bool();
        } else if constexpr (std::is_integral_v<T> &&(sizeof(T) * 8 <= 32)) {
            if (!value_json.is_number()) {
                return std::unexpected(
                           (boost::format("Value for key %1% in namespace %2% is not a number") % key % nspace).str()
                       );
            }
            if (value_json.is_int64()) {
                value = static_cast<T>(value_json.as_int64());
            } else if (value_json.is_uint64()) {
                value = static_cast<T>(value_json.as_uint64());
            } else {
                return std::unexpected(
                           (boost::format("Value for key %1% in namespace %2% is not a integer") % key % nspace).str()
                       );
            }
        } else {
            if (!value_json.is_string()) {
                return std::unexpected(
                           (boost::format("Value for key %1% in namespace %2% is not a string") % key % nspace).str()
                       );
            }
            auto value_str = std::string(value_json.as_string());
            if (!BROOKESIA_DESCRIBE_JSON_DESERIALIZE(value_str, value)) {
                return std::unexpected((boost::format("Failed to parse value from: %1%") % value_str).str());
            }
        }

        return value;
    }

    /**
     * @brief Erase key-value pairs from the Storage namespace
     *
     * @param nspace The namespace of the key-value pairs to erase
     * @param keys The keys of the key-value pairs to erase, optional. If not provided or empty, all key-value pairs in the namespace will be erased
     * @param timeout_ms The timeout in milliseconds
     * @return std::expected<void, std::string> The result of the operation
     */
    static std::expected<void, std::string> erase_keys(
        const std::string &nspace, const std::vector<std::string> &keys = {}, uint32_t timeout_ms = DEFAULT_TIMEOUT_MS
    )
    {
        auto binding = ServiceManager::get_instance().bind(get_name().data());
        if (!binding.is_valid()) {
            return std::unexpected("Failed to bind service");
        }

        boost::json::array keys_array;
        keys_array.reserve(keys.size());
        for (const auto &key : keys) {
            keys_array.push_back(boost::json::value(key));
        }
        auto result = call_function_sync(FunctionId::KVErase, nspace, std::move(keys_array), Timeout(timeout_ms));
        if (!result) {
            return std::unexpected(
                       (boost::format("Failed to erase keys from Storage %1%: %2%") % nspace % result.error()).str()
                   );
        }

        return {};
    }

private:
    static std::expected<KvNameResult, std::string> make_kv_name(
        FunctionId function_id, std::initializer_list<std::string_view> parts, std::string_view separator,
        uint32_t timeout_ms
    )
    {
        boost::json::array parts_json;
        parts_json.reserve(parts.size());
        for (const auto part : parts) {
            parts_json.emplace_back(std::string(part));
        }

        auto result = call_function_sync<boost::json::object>(
                          function_id, std::move(parts_json), std::string(separator), Timeout(timeout_ms)
                      );
        if (!result) {
            return std::unexpected(result.error());
        }

        KvNameResult name_result;
        if (!BROOKESIA_DESCRIBE_FROM_JSON(result.value(), name_result)) {
            return std::unexpected(
                       (boost::format("Failed to parse Storage KV name result: %1%") %
                        BROOKESIA_DESCRIBE_TO_STR(result.value())).str()
                   );
        }
        return name_result;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROOKESIA_DESCRIBE_STRUCT(StorageFileSystemInfo, (), (fs_type, medium_type, mount_point, supports_directories));
BROOKESIA_DESCRIBE_STRUCT(Storage::KvNameResult, (), (name, original_name, hashed, warning));
BROOKESIA_DESCRIBE_ENUM(
    Storage::FunctionId, KVList, KVSet, KVGet, KVErase, GetFileSystems, GetFileSystemCapacity, MakeKVKey,
    MakeKVNamespace, Max
);
BROOKESIA_DESCRIBE_ENUM(Storage::EventId, Max);
BROOKESIA_DESCRIBE_ENUM(Storage::FunctionKVListParam, Nspace);
BROOKESIA_DESCRIBE_ENUM(Storage::FunctionKVSetParam, Nspace, KeyValuePairs);
BROOKESIA_DESCRIBE_ENUM(Storage::FunctionKVGetParam, Nspace, Keys);
BROOKESIA_DESCRIBE_ENUM(Storage::FunctionKVEraseParam, Nspace, Keys);
BROOKESIA_DESCRIBE_ENUM(Storage::FunctionGetFileSystemCapacityParam, MountPoint);
BROOKESIA_DESCRIBE_ENUM(Storage::FunctionMakeKVNameParam, Parts, Separator);

} // namespace esp_brookesia::service::helper
