/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <span>
#include <type_traits>
#include "boost/json.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/base.hpp"

namespace esp_brookesia::service::helper {

class NVS: public Base<NVS> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * They are used as parameter and return types for functions and events.
     * Users can access or modify these types via serialization and deserialization.
     */
    enum class ValueType {
        Bool,
        Int,
        String,
        Max,
    };

    using Value = std::variant<bool, int32_t, std::string>;
    using KeyValueMap = std::map<std::string, Value>;

    struct EntryInfo {
        std::string nspace;
        std::string key;
        ValueType type;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId {
        List,
        Set,
        Get,
        Erase,
        Max,
    };

    enum class EventId {
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionListParam {
        Nspace,
    };

    enum class FunctionSetParam {
        Nspace,
        KeyValuePairs,
    };

    enum class FunctionGetParam {
        Nspace,
        Keys,
    };

    enum class FunctionEraseParam {
        Nspace,
        Keys,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // NVS has no events, so no event parameter types are defined

private:
    static constexpr std::string_view DEFAULT_NAMESPACE = "default";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static FunctionSchema function_schema_list()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::List),
            .description = (boost::format("List information of key-value pairs in the NVS namespace. "
                                          "Return a JSON array of objects. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<EntryInfo>({
                {"storage", "key1", ValueType::String},
                {"storage", "key2", ValueType::Int}
            }))).str(),
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionListParam::Nspace),
                    .description = "The namespace of the NVS namespace to list, optional. "
                    "If not provided, the default namespace will be used.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string(DEFAULT_NAMESPACE))
                }
            }
        };
    }

    static FunctionSchema function_schema_set()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Set),
            .description = "Set key-value pairs in the NVS namespace",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetParam::Nspace),
                    .description = "The namespace of the key-value pairs to set. "
                    "Optional. If not provided, the default namespace will be used. "
                    "If provided empty, the default namespace will be used",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string(DEFAULT_NAMESPACE))
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetParam::KeyValuePairs),
                    .description = (boost::format("The JSON object of key-value pairs to set, "
                                                  "should be one of the following: %1%. Example: %2%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<ValueType>({
                        ValueType::Bool, ValueType::Int, ValueType::String
                    })) % BROOKESIA_DESCRIBE_JSON_SERIALIZE(KeyValueMap({
                        {"key1", std::string("value1")}, {"key2", 2}, {"key3", true}
                    }))).str(),
                    .type = FunctionValueType::Object
                }
            }
        };
    }

    static FunctionSchema function_schema_get()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Get),
            .description = (boost::format("Get key-value pairs from the NVS namespace by keys. "
                                          "Return a JSON object of key-value pairs. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(KeyValueMap({
                {"key1", "value1"}, {"key2", 2}, {"key3", true}
            }))).str(),
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionGetParam::Nspace),
                    .description = "The namespace of the key-value pairs to get, optional. "
                    "If not provided, the default namespace will be used.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string(DEFAULT_NAMESPACE))
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionGetParam::Keys),
                    .description = (boost::format("The JSON array of keys to get, optional. "
                                                  "If not provided, all key-value pairs in the namespace will be "
                                                  "returned. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({"key1", "key2", "key3"}))).str(),
                    .type = FunctionValueType::Array,
                    .default_value = FunctionValue(boost::json::array())
                }
            }
        };
    }

    static FunctionSchema function_schema_erase()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Erase),
            .description = "Erase key-value pairs from the NVS namespace",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionEraseParam::Nspace),
                    .description = "The namespace of the key-value pairs to erase, optional. "
                    "If not provided, the default namespace will be used.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string(DEFAULT_NAMESPACE))
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionEraseParam::Keys),
                    .description = (boost::format("The keys of the key-value pairs to erase, optional. "
                                                  "If not provided or empty, all key-value pairs in the namespace will "
                                                  "be erased. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({"key1", "key2", "key3"}))).str(),
                    .type = FunctionValueType::Array,
                    .default_value = FunctionValue(boost::json::array())
                }
            }
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // NVS has no events, so no event schemas are defined

public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the functions required by the Base class /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static constexpr std::string_view get_name()
    {
        return "NVS";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_list(),
                function_schema_set(),
                function_schema_get(),
                function_schema_erase(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        return std::span<const EventSchema>();
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function helper methods //////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static constexpr uint32_t DEFAULT_TIMEOUT_MS = 100;

    /**
     * @brief Save key-value pairs to the NVS namespace
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

        boost::json::value json_value;
        if constexpr (std::is_same_v<T, bool>) {
            json_value = value;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            json_value = value;
        } else if constexpr (std::is_integral_v<T> &&sizeof(T) * 8 <= 32) {
            json_value = static_cast<int32_t>(value);
        } else {
            json_value = BROOKESIA_DESCRIBE_JSON_SERIALIZE(value);
        }

        boost::json::object data_object{{key, std::move(json_value)}};
        auto result = call_function_sync<void>(FunctionId::Set,
        FunctionParameterMap{
            {BROOKESIA_DESCRIBE_TO_STR(FunctionSetParam::Nspace), nspace},
            {BROOKESIA_DESCRIBE_TO_STR(FunctionSetParam::KeyValuePairs), std::move(data_object)}
        }, timeout_ms);
        if (!result) {
            return std::unexpected(
                       (boost::format("Failed to save %1% to NVS %2%: %3%") % key % nspace % result.error()).str()
                   );
        }

        return {};
    }

    /**
     * @brief Get key-value pair from the NVS namespace
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

        auto result = call_function_sync<boost::json::object>(FunctionId::Get,
        FunctionParameterMap{
            {BROOKESIA_DESCRIBE_TO_STR(FunctionGetParam::Nspace), nspace},
            {BROOKESIA_DESCRIBE_TO_STR(FunctionGetParam::Keys), boost::json::array{key}}
        }, timeout_ms);
        if (!result) {
            return std::unexpected(
                       (boost::format("Failed to get %1% from NVS %2%: %3%") % key % nspace % result.error()).str()
                   );
        }

        auto &data_obj = *result;
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
        } else if constexpr (std::is_integral_v<T> &&sizeof(T) * 8 <= 32) {
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
     * @brief Erase key-value pairs from the NVS namespace
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
        auto result = call_function_sync<void>(FunctionId::Erase,
        FunctionParameterMap{
            {BROOKESIA_DESCRIBE_TO_STR(FunctionEraseParam::Nspace), nspace},
            {BROOKESIA_DESCRIBE_TO_STR(FunctionEraseParam::Keys), std::move(keys_array)}
        }, timeout_ms);
        if (!result) {
            return std::unexpected(
                       (boost::format("Failed to erase keys from NVS %1%: %2%") % nspace % result.error()).str()
                   );
        }

        return {};
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROOKESIA_DESCRIBE_ENUM(NVS::FunctionId, List, Set, Get, Erase, Max);
BROOKESIA_DESCRIBE_ENUM(NVS::EventId, Max);
BROOKESIA_DESCRIBE_ENUM(NVS::FunctionListParam, Nspace);
BROOKESIA_DESCRIBE_ENUM(NVS::FunctionSetParam, Nspace, KeyValuePairs);
BROOKESIA_DESCRIBE_ENUM(NVS::FunctionGetParam, Nspace, Keys);
BROOKESIA_DESCRIBE_ENUM(NVS::FunctionEraseParam, Nspace, Keys);
BROOKESIA_DESCRIBE_ENUM(NVS::ValueType, Bool, Int, String, Max);
BROOKESIA_DESCRIBE_STRUCT(NVS::EntryInfo, (), (nspace, key, type));

} // namespace esp_brookesia::service::helper
