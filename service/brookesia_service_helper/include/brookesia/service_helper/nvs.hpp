/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "boost/format.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/function/definition.hpp"
#include "brookesia/service_manager/event/definition.hpp"

namespace esp_brookesia::service::helper {

class NVS {
public:
    enum class ValueType {
        Bool,
        Int,
        String,
        Max,
    };

    using Value = std::variant<bool, int, std::string>;

    struct KeyValuePair {
        std::string key;
        Value value;
    };

    struct EntryInfo {
        std::string nspace;
        std::string key;
        ValueType type;
    };

    enum FunctionIndex {
        FunctionIndexList,
        FunctionIndexSet,
        FunctionIndexGet,
        FunctionIndexErase,
        FunctionIndexMax,
    };

    static constexpr const char *SERVICE_NAME = "nvs";
    static constexpr const char *DEFAULT_NAMESPACE = "storage";

    static const FunctionSchema *get_function_definitions()
    {
        static const FunctionSchema FUNCTION_DEFINITIONS[FunctionIndexMax] = {
            [FunctionIndexList] = {
                // name
                "list",
                // description
                (boost::format("List information of key-value pairs in the NVS namespace. "
                               "Return a JSON array of objects. Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<EntryInfo>({
                    {"storage", "key1", ValueType::String},
                    {"storage", "key2", ValueType::Int}
                }))).str(),
                // parameters
                {
                    // parameters[0]
                    {
                        // name
                        "nspace",
                        // description
                        "The namespace of the NVS namespace to list, optional. "
                        "If not provided, the default namespace will be used.",
                        // type
                        FunctionValueType::String,
                        // default value
                        DEFAULT_NAMESPACE
                    }
                },
            },
            [FunctionIndexSet] = {
                // name
                "set",
                // description
                "Set key-value pairs in the NVS namespace",
                // parameters
                {
                    // parameters[0]
                    {
                        // name
                        "nspace",
                        // description
                        "The namespace of the key-value pairs to set. "
                        "Optional. If not provided, the default namespace will be used. "
                        "If provided empty, the default namespace will be used",
                        // type
                        FunctionValueType::String,
                        // default value
                        DEFAULT_NAMESPACE
                    },
                    // parameters[1]
                    {
                        // name
                        "key_value_pairs",
                        // description
                        (boost::format("The JSON array of key-value pairs to set. "
                                       "The value type can be one of the following: %1%. Example: %2%")
                        % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<ValueType>({
                            ValueType::Bool, ValueType::Int, ValueType::String
                        }))
                        % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<KeyValuePair>({
                            {"key1", std::string("value1")}, {"key2", 2}, {"key3", true}
                        }))).str(),
                        // type
                        FunctionValueType::Array
                    }
                }
            },
            [FunctionIndexGet] = {
                // name
                "get",
                // description
                (boost::format("Get key-value pairs from the NVS namespace by keys. "
                               "Return a JSON object of key-value pairs. Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(boost::json::object({
                    {"key1", "value1"}, {"key2", 2}, {"key3", true}
                }))).str(),
                // parameters
                {
                    // parameters[0]
                    {
                        // name
                        "nspace",
                        // description
                        "The namespace of the key-value pairs to get, optional. "
                        "If not provided, the default namespace will be used.",
                        // type
                        FunctionValueType::String,
                        // default value
                        DEFAULT_NAMESPACE
                    },
                    // parameters[1]
                    {
                        // name
                        "keys",
                        // description
                        (boost::format("The JSON array of keys to get, optional. "
                                       "If not provided, all key-value pairs in the namespace will be returned. "
                                       "Example: %1%")
                        % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({"key1", "key2", "key3"}))).str(),
                        // type
                        FunctionValueType::Array,
                        // default value
                        boost::json::array()
                    }
                }
            },
            [FunctionIndexErase] = {
                // name
                "erase",
                // description
                "Erase key-value pairs from the NVS namespace",
                // parameters
                {
                    // parameters[0]
                    {
                        // name
                        "nspace",
                        // description
                        "The namespace of the key-value pairs to erase, optional. "
                        "If not provided, the default namespace will be used.",
                        // type
                        FunctionValueType::String,
                        // default value
                        DEFAULT_NAMESPACE
                    },
                    // parameters[1]
                    {
                        // name
                        "keys",
                        // description
                        (boost::format("The keys of the key-value pairs to erase, optional. "
                                       "If not provided or empty, all key-value pairs in the namespace will be erased. "
                                       "Example: %1%")
                        % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({"key1", "key2", "key3"}))).str(),
                        // type
                        FunctionValueType::Array,
                        // default value
                        boost::json::array()
                    }
                }
            },
        };
        return FUNCTION_DEFINITIONS;
    }
};

BROOKESIA_DESCRIBE_ENUM(NVS::ValueType, Bool, Int, String, Max);
BROOKESIA_DESCRIBE_STRUCT(NVS::EntryInfo, (), (nspace, key, type));
BROOKESIA_DESCRIBE_STRUCT(NVS::KeyValuePair, (), (key, value));

} // namespace esp_brookesia::service::helper
