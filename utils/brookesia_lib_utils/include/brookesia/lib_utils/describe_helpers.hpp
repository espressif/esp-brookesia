/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cctype>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>
#include <optional>
#include "boost/describe.hpp"
#include "boost/mp11.hpp"
#include "boost/json.hpp"

namespace esp_brookesia::lib_utils {

namespace detail {

// ============================================================================
// Type Traits - Type detection utilities
// ============================================================================

// Detect if type can be described by boost::describe (struct)
template <typename T, typename = void>
struct is_described : std::false_type {};

template <typename T>
struct is_described<T, std::void_t<decltype(boost::describe::describe_members<T, boost::describe::mod_public>())>>
: std::true_type {};

template <typename T>
inline constexpr bool is_described_v = is_described<T>::value;

// Detect if type is a described enum by boost::describe
template <typename T, typename = void>
struct is_described_enum : std::false_type {};

template <typename T>
struct is_described_enum<T, std::void_t<decltype(boost::describe::describe_enumerators<T>())>>
: std::true_type {};

template <typename T>
inline constexpr bool is_described_enum_v = is_described_enum<T>::value;

// Detect if type has operator<< support
template <typename T, typename = void>
struct has_ostream_operator : std::false_type {};

template <typename T>
struct has_ostream_operator < T, std::void_t < decltype(std::declval<std::ostream &>() << std::declval<const T &>()) >>
: std::true_type {};

template <typename T>
inline constexpr bool has_ostream_operator_v = has_ostream_operator<T>::value;

// Detect if type is std::optional
template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

// Detect if type is std::vector
template <typename T>
struct is_vector : std::false_type {};

template <typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type {};

template <typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

// Detect if type is std::span
template <typename T>
struct is_span : std::false_type {};

template <typename T, std::size_t Extent>
struct is_span<std::span<T, Extent>> : std::true_type {};

template <typename T>
inline constexpr bool is_span_v = is_span<T>::value;

// Detect if type is std::map
template <typename T>
struct is_map : std::false_type {};

template <typename Key, typename Value, typename Compare, typename Alloc>
struct is_map<std::map<Key, Value, Compare, Alloc>> : std::true_type {};

template <typename T>
inline constexpr bool is_map_v = is_map<T>::value;

// Detect if type is std::variant or inherits from std::variant
template <typename T>
struct is_variant : std::false_type {};

template <typename... Types>
struct is_variant<std::variant<Types...>> : std::true_type {};

// Helper to detect if T is derived from std::variant<Args...>
template <typename T, typename = void>
struct is_variant_derived : std::false_type {};

template <typename T>
struct is_variant_derived<T, std::void_t<typename T::Base>>
            : is_variant<typename T::Base> {};

template <typename T>
inline constexpr bool is_variant_v = is_variant<std::decay_t<T>>::value || is_variant_derived<std::decay_t<T>>::value;

// Helper to get the actual variant type (either T itself or T::Base)
template <typename T, typename = void>
struct get_variant_type {
    using type = T;  // For direct std::variant
};

template <typename T>
struct get_variant_type<T, std::enable_if_t<is_variant_derived<T>::value>> {
    using type = typename T::Base;  // For types derived from std::variant
};

template <typename T>
using get_variant_type_t = typename get_variant_type<std::decay_t<T>>::type;

// Detect if type is std::function
template <typename T>
struct is_function : std::false_type {};

template <typename Ret, typename... Args>
struct is_function<std::function<Ret(Args...)>> : std::true_type {};

template <typename T>
inline constexpr bool is_function_v = is_function<T>::value;

/**
 * @brief Convert PascalCase/camelCase to snake_case
 * Examples: "IntervalMs" -> "interval_ms", "Count" -> "count"
 */
inline std::string to_snake_case(const std::string &input)
{
    std::string result;
    result.reserve(input.size() + 4); // Reserve extra space for underscores

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (std::isupper(c)) {
            // Add underscore before uppercase letters (except at the start)
            if (i > 0) {
                result += '_';
            }
            result += static_cast<char>(std::tolower(c));
        } else {
            result += c;
        }
    }
    return result;
}

/**
 * @brief Find JSON object key with flexible matching (supports PascalCase/camelCase to snake_case conversion)
 * @param obj The JSON object to search in
 * @param target_key The target key in snake_case format
 * @return Iterator to the found key-value pair, or obj.end() if not found
 */
inline boost::json::object::const_iterator find_json_key_flexible(
    const boost::json::object &obj, const std::string &target_key)
{
    // First, try exact match
    auto it = obj.find(target_key);
    if (it != obj.end()) {
        return it;
    }

    // Try to find by converting each key to snake_case
    for (auto iter = obj.begin(); iter != obj.end(); ++iter) {
        std::string key_snake = to_snake_case(std::string(iter->key()));
        if (key_snake == target_key) {
            return iter;
        }
    }

    return obj.end();
}

} // namespace detail

// ============================================================================
// Public API - Enum Conversion Functions
// ============================================================================

/**
 * @brief Convert enum to string
 */
template <typename T>
std::string describe_enum_to_string(T value)
{
    static_assert(std::is_enum_v<T>, "T must be an enum type");
    static_assert(detail::is_described_enum_v<T>, "Enum must be described with BOOST_DESCRIBE_ENUM");

    const char *enum_name = nullptr;
    boost::mp11::mp_for_each<boost::describe::describe_enumerators<T>>([&](auto D) {
        if (D.value == value) {
            enum_name = D.name;
        }
    });

    return enum_name ? std::string(enum_name)
           : std::to_string(static_cast<std::underlying_type_t<T>>(value));
}

/**
 * @brief Convert enum to number
 */
template <typename T>
constexpr std::underlying_type_t<T> describe_enum_to_number(T value) noexcept
{
    static_assert(std::is_enum_v<T>, "T must be an enum type");
    return static_cast<std::underlying_type_t<T>>(value);
}

/**
 * @brief Convert string to enum
 */
template <typename T>
bool describe_string_to_enum(const std::string &name, T &ret_value)
{
    static_assert(std::is_enum_v<T>, "T must be an enum type");
    static_assert(detail::is_described_enum_v<T>, "Enum must be described with BOOST_DESCRIBE_ENUM");

    bool found = false;
    boost::mp11::mp_for_each<boost::describe::describe_enumerators<T>>([&](auto D) {
        if (name == D.name) {
            ret_value = D.value;
            found = true;
        }
    });
    return found;
}

/**
 * @brief Convert number to enum
 */
template <typename T>
bool describe_number_to_enum(std::underlying_type_t<T> number, T &ret_value)
{
    static_assert(std::is_enum_v<T>, "T must be an enum type");
    static_assert(detail::is_described_enum_v<T>, "Enum must be described with BOOST_DESCRIBE_ENUM");

    bool found = false;
    boost::mp11::mp_for_each<boost::describe::describe_enumerators<T>>([&](auto D) {
        if (static_cast<std::underlying_type_t<T>>(D.value) == number) {
            ret_value = D.value;
            found = true;
        }
    });
    return found;
}

// ============================================================================
// Public API - JSON Conversion Functions
// ============================================================================

/**
 * @brief Convert value to JSON (supports all common types)
 */
template <typename T>
boost::json::value describe_to_json(const T &value)
{
    // Basic types
    if constexpr (std::is_same_v<T, bool>) {
        return value;
    } else if constexpr (std::is_integral_v<T>) {
        // Handle integer types - use std::is_signed_v for cleaner code
        if constexpr (std::is_signed_v<T>) {
            return static_cast<std::int64_t>(value);
        } else {
            return static_cast<std::uint64_t>(value);
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        return static_cast<double>(value);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return boost::json::value(value);
    } else if constexpr (std::is_same_v<T, const char *> || std::is_same_v<T, char *>) {
        // Handle null pointers safely
        return value ? boost::json::value(std::string(value)) : boost::json::value("");
    }
    // char arrays (string literals like "hello")
    else if constexpr (std::is_array_v<T> &&(std::is_same_v<std::remove_extent_t<T>, char> ||
                       std::is_same_v<std::remove_extent_t<T>, const char>)) {
        return boost::json::value(std::string(value));
    }
    // Non-char* pointers - format as @0x...
    else if constexpr (std::is_pointer_v<T>) {
        std::ostringstream oss;
        oss << "@0x" << std::hex << reinterpret_cast<uintptr_t>(value);
        return boost::json::value(oss.str());
    }
    // std::monostate - serialize as null
    else if constexpr (std::is_same_v<T, std::monostate>) {
        return nullptr;
    }
    // std::optional
    else if constexpr (detail::is_optional_v<T>) {
        return value.has_value() ? describe_to_json(*value) : nullptr;
    }
    // std::vector, std::span (sequence containers)
    else if constexpr (detail::is_vector_v<T> || detail::is_span_v<T>) {
        boost::json::array arr;
        arr.reserve(value.size());  // Performance optimization
        for (const auto &item : value) {
            arr.push_back(describe_to_json(item));
        }
        return arr;
    }
    // std::map
    else if constexpr (detail::is_map_v<T>) {
        boost::json::object obj;
        for (const auto &[key, val] : value) {
            std::string key_str;
            if constexpr (std::is_same_v<typename T::key_type, std::string>) {
                key_str = key;
            } else if constexpr (std::is_integral_v<typename T::key_type>) {
                key_str = std::to_string(key);
            } else if constexpr (detail::is_described_enum_v<typename T::key_type>) {
                key_str = describe_enum_to_string(key);
            } else {
                // Fallback: convert key to JSON first, then to string
                key_str = boost::json::value_to<std::string>(describe_to_json(key));
            }
            obj[key_str] = describe_to_json(val);
        }
        return obj;
    }
    // std::variant - serialize the currently held value
    else if constexpr (detail::is_variant_v<T>) {
        // Handle both std::variant and types derived from std::variant
        if constexpr (detail::is_variant<std::decay_t<T>>::value) {
            // Direct std::variant
            return std::visit([](const auto & v) -> boost::json::value {
                return describe_to_json(v);
            }, value);
        } else {
            // Type derived from std::variant - cast to base
            const detail::get_variant_type_t<T> &base = value;
            return std::visit([](const auto & v) -> boost::json::value {
                return describe_to_json(v);
            }, base);
        }
    }
    // std::function - output status and address
    else if constexpr (detail::is_function_v<T>) {
        if (value) {
            // Function is not empty, return a descriptive string with address
            std::ostringstream oss;
            oss << "<function@0x" << std::hex << reinterpret_cast<uintptr_t>(&value) << ">";
            return boost::json::value(oss.str());
        } else {
            // Function is empty/null
            return boost::json::value("<function:empty>");
        }
    }
    // boost::json::value - return directly without conversion
    else if constexpr (std::is_same_v<T, boost::json::value>) {
        return value;
    }
    // boost::json::object - convert to boost::json::value
    else if constexpr (std::is_same_v<T, boost::json::object>) {
        return boost::json::value(value);
    }
    // boost::json::array - convert to boost::json::value
    else if constexpr (std::is_same_v<T, boost::json::array>) {
        return boost::json::value(value);
    }
    // Described enum
    else if constexpr (detail::is_described_enum_v<T>) {
        const char *enum_name = nullptr;
        boost::mp11::mp_for_each<boost::describe::describe_enumerators<T>>([&](auto D) {
            if (D.value == value) {
                enum_name = D.name;
            }
        });
        return enum_name ? boost::json::value(boost::json::string(enum_name))
               : boost::json::value(static_cast<std::underlying_type_t<T>>(value));
    }
    // Described struct
    else if constexpr (detail::is_described_v<T>) {
        boost::json::object j;
        boost::mp11::mp_for_each<boost::describe::describe_members<T, boost::describe::mod_public>>(
        [&](auto D) {
            // Serialize all fields, including null optionals (for symmetry)
            j[D.name] = describe_to_json(value.*D.pointer);
        });
        return j;
    }
    // Types with ostream operator
    else if constexpr (detail::has_ostream_operator_v<T>) {
        std::ostringstream oss;
        oss << value;
        return boost::json::value(oss.str());
    }
    // Fallback
    else {
        return boost::json::value("");
    }
}

/**
 * @brief Convert JSON to value (supports all common types)
 */
template <typename T>
bool describe_from_json(const boost::json::value &j, T &value)
{
    // Basic types
    if constexpr (std::is_same_v<T, bool>) {
        if (j.is_bool()) {
            value = j.as_bool();
            return true;
        }
        return false;
    } else if constexpr (std::is_integral_v<T>) {
        if (j.is_number()) {
            // Handle different JSON number types with range checking
            if (j.is_int64()) {
                std::int64_t temp = j.as_int64();
                // Check for overflow
                if constexpr (sizeof(T) < sizeof(std::int64_t)) {
                    if (temp < std::numeric_limits<T>::min() || temp > std::numeric_limits<T>::max()) {
                        return false;  // Overflow detected
                    }
                }
                value = static_cast<T>(temp);
            } else if (j.is_uint64()) {
                std::uint64_t temp = j.as_uint64();
                // Check for overflow
                if constexpr (sizeof(T) < sizeof(std::uint64_t)) {
                    if (temp > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
                        return false;  // Overflow detected
                    }
                }
                value = static_cast<T>(temp);
            } else {
                // Fallback for other numeric types (e.g., double)
                double temp = j.as_double();
                if (temp < std::numeric_limits<T>::min() || temp > std::numeric_limits<T>::max()) {
                    return false;  // Overflow detected
                }
                value = static_cast<T>(temp);
            }
            return true;
        }
        return false;
    } else if constexpr (std::is_floating_point_v<T>) {
        if (j.is_number()) {
            value = static_cast<T>(boost::json::value_to<double>(j));
            return true;
        }
        return false;
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (j.is_string()) {
            value = boost::json::value_to<std::string>(j);
            return true;
        }
        return false;
    } else if constexpr (std::is_same_v<T, const char *> || std::is_same_v<T, char *>) {
        // WARNING: Cannot safely deserialize to const char* due to lifetime issues
        // Use std::string instead for safe deserialization
        // This branch is disabled to prevent undefined behavior
        static_assert(
            (!std::is_same_v<T, const char *>) && (!std::is_same_v<T, char *>),
            "Cannot deserialize JSON to const char* or char* (use std::string instead)"
        );
        return false;
    }
    // Non-char* pointers - deserialize from @0x... format
    else if constexpr (std::is_pointer_v<T> &&
                       !std::is_same_v<T, const char *> &&
                       !std::is_same_v<T, char *>) {
        if (!j.is_string()) {
            return false;
        }
        std::string str = boost::json::value_to<std::string>(j);
        // Check if string starts with @0x
        if (str.length() < 3 || str.substr(0, 3) != "@0x") {
            return false;
        }
        // Extract hex part after @0x
        std::string hex_str = str.substr(3);
        if (hex_str.empty()) {
            return false;
        }
        try {
            // Parse hex string to uintptr_t
            uintptr_t addr = std::stoull(hex_str, nullptr, 16);
            // Convert to pointer: first to void*, then to target pointer type
            void *void_ptr = reinterpret_cast<void *>(addr);
            value = reinterpret_cast<T>(void_ptr);
            return true;
        } catch (const std::exception &) {
            return false; // Invalid hex format
        }
    }
    // std::monostate - deserialize from null
    else if constexpr (std::is_same_v<T, std::monostate>) {
        if (j.is_null()) {
            value = std::monostate{};
            return true;
        }
        return false;
    }
    // std::optional
    else if constexpr (detail::is_optional_v<T>) {
        if (j.is_null()) {
            value = std::nullopt;
            return true;
        } else {
            typename T::value_type temp;
            if (describe_from_json(j, temp)) {
                value = temp;
                return true;
            }
            return false;
        }
    }
    // std::vector
    else if constexpr (detail::is_vector_v<T>) {
        if (!j.is_array()) {
            return false;
        }
        const auto &arr = j.as_array();
        value.clear();
        value.reserve(arr.size());
        for (const auto &item : arr) {
            typename T::value_type temp;
            if (!describe_from_json(item, temp)) {
                return false;
            }
            value.push_back(std::move(temp));
        }
        return true;
    }
    // std::span - skip deserialization (non-owning view type)
    else if constexpr (detail::is_span_v<T>) {
        // std::span is a non-owning view, cannot be deserialized from JSON
        // Skip this field silently - the span will remain unchanged
        (void)j;
        (void)value;
        return true;  // Return true to not fail the overall deserialization
    }
    // std::map
    else if constexpr (detail::is_map_v<T>) {
        if (!j.is_object()) {
            return false;
        }
        const auto &obj = j.as_object();
        value.clear();
        for (const auto &[key_str, val_json] : obj) {
            typename T::key_type key;
            typename T::mapped_type val;

            if constexpr (std::is_same_v<typename T::key_type, std::string>) {
                key = std::string(key_str);
            } else if constexpr (std::is_integral_v<typename T::key_type>) {
                // Optimize: avoid temporary string construction
                std::string key_string(key_str);
                try {
                    if constexpr (std::is_signed_v<typename T::key_type>) {
                        key = static_cast<typename T::key_type>(std::stoll(key_string));
                    } else {
                        key = static_cast<typename T::key_type>(std::stoull(key_string));
                    }
                } catch (const std::exception &) {
                    return false;  // Invalid key format
                }
            } else if constexpr (detail::is_described_enum_v<typename T::key_type>) {
                if (!describe_string_to_enum(std::string(key_str), key)) {
                    return false;
                }
            } else {
                return false;  // Unsupported key type
            }

            if (!describe_from_json(val_json, val)) {
                return false;
            }
            value[std::move(key)] = std::move(val);
        }
        return true;
    }
    // std::variant - try to convert to each alternative type
    else if constexpr (detail::is_variant_v<T>) {
        bool success = false;
        // Get the actual variant type (either T or T::Base)
        using VariantType = detail::get_variant_type_t<T>;
        // Try each alternative type in order
        boost::mp11::mp_for_each<boost::mp11::mp_iota_c<std::variant_size_v<VariantType>>>([&](auto I) {
            if (!success) {
                using AlternativeType = std::variant_alternative_t<I, VariantType>;
                AlternativeType temp;
                if (describe_from_json(j, temp)) {
                    value = std::move(temp);
                    success = true;
                }
            }
        });
        return success;
    }
    // std::function - cannot convert from JSON (functions are not serializable)
    // But we accept the serialized string format and set function to empty
    else if constexpr (detail::is_function_v<T>) {
        if (j.is_string()) {
            std::string str = boost::json::value_to<std::string>(j);
            // Accept serialized function format (e.g., "<function@0x...>" or "<function:empty>")
            // but set to empty since we cannot restore the actual function
            value = T{}; // Set to empty function
            return true;
        }
        return false;
    }
    // boost::json::value - return directly
    else if constexpr (std::is_same_v<T, boost::json::value>) {
        value = j;
        return true;
    }
    // boost::json::object - convert from JSON object
    else if constexpr (std::is_same_v<T, boost::json::object>) {
        if (j.is_object()) {
            value = j.as_object();
            return true;
        }
        return false;
    }
    // boost::json::array - convert from JSON array
    else if constexpr (std::is_same_v<T, boost::json::array>) {
        if (j.is_array()) {
            value = j.as_array();
            return true;
        }
        return false;
    }
    // Described enum
    else if constexpr (detail::is_described_enum_v<T>) {
        if (j.is_string()) {
            return describe_string_to_enum(boost::json::value_to<std::string>(j), value);
        } else if (j.is_number()) {
            auto num = std::is_signed_v<std::underlying_type_t<T>>
                       ? static_cast<std::underlying_type_t<T>>(boost::json::value_to<std::int64_t>(j))
                       : static_cast<std::underlying_type_t<T>>(boost::json::value_to<std::uint64_t>(j));
            return describe_number_to_enum(num, value);
        }
        return false;
    }
    // Described struct
    else if constexpr (detail::is_described_v<T>) {
        if (!j.is_object()) {
            value = T{};
            return false;
        }
        value = T{};
        const auto &json_obj = j.as_object();
        bool success = true;
        boost::mp11::mp_for_each<boost::describe::describe_members<T, boost::describe::mod_public>>(
        [&](auto D) {
            // Use flexible key matching (supports PascalCase/camelCase to snake_case conversion)
            auto it = detail::find_json_key_flexible(json_obj, D.name);
            if (it != json_obj.end()) {
                if (!describe_from_json(it->value(), value.*D.pointer)) {
                    success = false;
                }
            }
        });
        return success;
    }
    return false;
}

// ============================================================================
// JSON Serialization/Deserialization Functions
// ============================================================================

template <typename T>
std::string describe_json_serialize(const T &value)
{
    boost::json::value json_value = describe_to_json(value);
    return boost::json::serialize(json_value);
}

template <typename T>
bool describe_json_deserialize(const std::string &str, T &value)
{
    boost::system::error_code error_code;
    boost::json::value json_value = boost::json::parse(str, error_code);
    if (error_code) {
        return false;
    }
    return describe_from_json(json_value, value);
}

// ============================================================================
// Format Configuration
// ============================================================================

/**
 * @brief Output format configuration
 */
struct DescribeOutputFormat {
    const char *struct_begin = "{ ";      // Struct begin symbol
    const char *struct_end = " }";        // Struct end symbol
    const char *field_separator = ", ";   // Field separator
    const char *field_prefix = "";        // Field name prefix (e.g., "." for C++ style)
    const char *name_value_separator = ": "; // Name-value separator
    const char *address_prefix = "@";     // Address prefix
    bool hex_address = false;             // Whether address uses hexadecimal
    bool quote_field_names = false;       // Whether to add quotes to field names
    bool quote_string_values = false;     // Whether to add quotes to string values
    bool enum_as_string = true;           // Whether enum outputs as string (false outputs numeric value)
    bool show_type_name = false;          // Whether to show type name
};

// Predefined formats
inline constexpr DescribeOutputFormat DESCRIBE_FORMAT_DEFAULT = {
    .struct_begin = "{ ", .struct_end = " }", .field_separator = ", ",
    .name_value_separator = ": ", .address_prefix = "@"
};

inline constexpr DescribeOutputFormat DESCRIBE_FORMAT_COMPACT = {
    .struct_begin = "{", .struct_end = "}", .field_separator = ",",
    .name_value_separator = "=", .address_prefix = "@0x", .hex_address = true
};

inline constexpr DescribeOutputFormat DESCRIBE_FORMAT_VERBOSE = {
    .struct_begin = "{\n  ", .struct_end = "\n}", .field_separator = ",\n  ",
    .field_prefix = ".", .name_value_separator = " = ", .address_prefix = "0x", .hex_address = true,
    .show_type_name = true  // Enable multiline format for arrays/objects
};

inline constexpr DescribeOutputFormat DESCRIBE_FORMAT_JSON = {
    .struct_begin = "{", .struct_end = "}", .field_separator = ",",
    .name_value_separator = ": ", .address_prefix = "\"@", .hex_address = true,
    .quote_field_names = true, .quote_string_values = true
};

inline constexpr DescribeOutputFormat DESCRIBE_FORMAT_PYTHON = {
    .struct_begin = "{'", .struct_end = "'}", .field_separator = "', '",
    .name_value_separator = "': ", .address_prefix = "<@0x", .hex_address = true
};

inline constexpr DescribeOutputFormat DESCRIBE_FORMAT_CPP = {
    .struct_begin = "{", .struct_end = "}", .field_separator = ", ",
    .field_prefix = ".", .name_value_separator = " = ", .address_prefix = "@0x", .hex_address = true
};

// ============================================================================
// Public API - Format Management Functions
// ============================================================================

/**
 * @brief Global format manager (Singleton)
 */
class DescribeFormatManager {
public:
    static DescribeFormatManager &instance()
    {
        static DescribeFormatManager mgr;
        return mgr;
    }

    void set_format(const DescribeOutputFormat &fmt)
    {
        format_ = fmt;
    }
    const DescribeOutputFormat &get_format() const
    {
        return format_;
    }
    void reset_to_default()
    {
        format_ = DESCRIBE_FORMAT_DEFAULT;
    }

private:
    DescribeFormatManager() : format_(DESCRIBE_FORMAT_DEFAULT) {}
    DescribeOutputFormat format_;
};

inline void describe_set_global_format(const DescribeOutputFormat &fmt)
{
    DescribeFormatManager::instance().set_format(fmt);
}

inline const DescribeOutputFormat &describe_get_global_format()
{
    return DescribeFormatManager::instance().get_format();
}

inline void describe_reset_global_format()
{
    DescribeFormatManager::instance().reset_to_default();
}

// ============================================================================
// String Format Conversion Functions
// ============================================================================

/**
 * @brief Convert boost::json::value to formatted string
 *
 * @note When fmt is nullptr or using default format, uses boost::json::serialize for better performance.
 *       Custom formatting is only used when fmt.show_type_name is true (multiline mode) or
 *       custom field_prefix/name_value_separator are specified.
 */
inline std::string describe_json_value_to_string(const boost::json::value &j, const DescribeOutputFormat &fmt, int indent_level = 0)
{
    // Custom formatting path
    // Helper to create indentation
    auto make_indent = [&](int level) -> std::string {
        if (!fmt.show_type_name)
        {
            return "";
        }
        return std::string(level * 2, ' ');  // 2 spaces per indent level
    };

    // Basic types
    if (j.is_null()) {
        return "null";
    } else if (j.is_bool()) {
        return j.as_bool() ? "true" : "false";
    } else if (j.is_int64()) {
        return std::to_string(j.as_int64());
    } else if (j.is_uint64()) {
        return std::to_string(j.as_uint64());
    } else if (j.is_double()) {
        return std::to_string(j.as_double());
    } else if (j.is_string()) {
        std::string str = std::string(j.as_string());
        if (fmt.quote_string_values) {
            return "\"" + str + "\"";
        }
        return str;
    }
    // boost::json::array
    else if (j.is_array()) {
        const auto &arr = j.as_array();
        if (arr.empty()) {
            return "[]";
        }

        if (fmt.show_type_name) {
            // Multiline format for verbose mode
            std::string result = "[\n";
            bool first = true;
            for (const auto &item : arr) {
                if (!first) {
                    result += ",\n";
                }
                first = false;
                result += make_indent(indent_level + 1);
                result += describe_json_value_to_string(item, fmt, indent_level + 1);
            }
            result += "\n" + make_indent(indent_level) + "]";
            return result;
        } else {
            // Compact format with custom separators
            std::string result = "[";
            bool first = true;
            for (const auto &item : arr) {
                if (!first) {
                    result += ", ";
                }
                first = false;
                result += describe_json_value_to_string(item, fmt, indent_level);
            }
            result += "]";
            return result;
        }
    }
    // boost::json::object
    else if (j.is_object()) {
        const auto &obj = j.as_object();
        if (obj.empty()) {
            return "{}";
        }

        if (fmt.show_type_name) {
            // Multiline format for verbose mode
            std::string result = "{\n";
            bool first = true;
            for (const auto &[key, val] : obj) {
                if (!first) {
                    result += ",\n";
                }
                first = false;
                result += make_indent(indent_level + 1);
                std::string key_str = fmt.field_prefix;
                if (fmt.quote_field_names) {
                    key_str += "\"" + std::string(key) + "\"";
                } else {
                    key_str += std::string(key);
                }
                result += key_str + fmt.name_value_separator + describe_json_value_to_string(val, fmt, indent_level + 1);
            }
            result += "\n" + make_indent(indent_level) + "}";
            return result;
        } else {
            // Compact format with custom separators
            std::string result = "{ ";
            bool first = true;
            for (const auto &[key, val] : obj) {
                if (!first) {
                    result += ", ";
                }
                first = false;
                std::string key_str;
                if (fmt.quote_field_names) {
                    key_str = "\"" + std::string(key) + "\"";
                } else {
                    key_str = std::string(key);
                }
                result += key_str + fmt.name_value_separator + describe_json_value_to_string(val, fmt, indent_level);
            }
            result += " }";
            return result;
        }
    }
    return "";
}

// Forward declaration
template <typename T>
std::string describe_to_string_with_fmt(const T &obj, const DescribeOutputFormat &fmt);

/**
 * @brief Output single member to string stream
 */
template <typename T>
void describe_output_member(std::ostringstream &oss, const char *name, const T &value, const DescribeOutputFormat &fmt)
{
    // std::optional - always output (consistent with describe_to_json)
    if constexpr (detail::is_optional_v<T>) {
        // Handle optional - convert via JSON
        auto json_value = describe_to_json(value);
        // Output field name and value (including null)
        if (fmt.quote_field_names) {
            oss << "\"" << fmt.field_prefix << name << "\"";
        } else {
            oss << fmt.field_prefix << name;
        }
        oss << fmt.name_value_separator;
        oss << describe_json_value_to_string(json_value, fmt);
        return;
    }

    // Output field name
    if (fmt.quote_field_names) {
        oss << "\"" << fmt.field_prefix << name << "\"";
    } else {
        oss << fmt.field_prefix << name;
    }
    oss << fmt.name_value_separator;

    // Output value based on type
    // Basic types
    if constexpr (std::is_same_v<T, bool>) {
        // Handle bool explicitly
        if (fmt.quote_string_values) {
            oss << "\"";
        }
        oss << (value ? "true" : "false");
        if (fmt.quote_string_values) {
            oss << "\"";
        }
    } else if constexpr (std::is_integral_v<T>) {
        // Handle all integral types explicitly to ensure correct signed/unsigned handling
        if (fmt.quote_string_values) {
            oss << "\"";
        }
        // Special handling for char types to print as numbers
        if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, signed char>) {
            oss << static_cast<int>(value);
        } else if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, unsigned char>) {
            oss << static_cast<unsigned int>(value);
        } else {
            // For all other integer types, use std::to_string which handles signedness correctly
            oss << std::to_string(value);
        }
        if (fmt.quote_string_values) {
            oss << "\"";
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        // Handle floating point types
        if (fmt.quote_string_values) {
            oss << "\"";
        }
        oss << value;
        if (fmt.quote_string_values) {
            oss << "\"";
        }
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (fmt.quote_string_values) {
            oss << "\"" << value << "\"";
        } else {
            oss << value;
        }
    } else if constexpr (std::is_same_v<T, const char *> || std::is_same_v<T, char *>) {
        const char *str_value = value ? value : "";
        if (fmt.quote_string_values) {
            oss << "\"" << str_value << "\"";
        } else {
            oss << str_value;
        }
    }
    // char arrays (string literals)
    else if constexpr (std::is_array_v<T> &&(std::is_same_v<std::remove_extent_t<T>, char> ||
                       std::is_same_v<std::remove_extent_t<T>, const char>)) {
        if (fmt.quote_string_values) {
            oss << "\"" << value << "\"";
        } else {
            oss << value;
        }
    }
    // Non-char* pointers - format as @0x...
    else if constexpr (std::is_pointer_v<T> &&
                       !std::is_same_v<T, const char *> &&
                       !std::is_same_v<T, char *>) {
        if (fmt.quote_string_values) {
            oss << "\"";
        }
        if (value) {
            auto flags = oss.flags();
            oss << "@0x" << std::hex << reinterpret_cast<uintptr_t>(value);
            oss.flags(flags);
        } else {
            oss << "@0x0";
        }
        if (fmt.quote_string_values) {
            oss << "\"";
        }
    }
    // std::vector, std::span, std::map, std::variant
    else if constexpr (detail::is_vector_v<T> || detail::is_span_v<T> || detail::is_map_v<T> || detail::is_variant_v<T>) {
        // Handle containers (vector, span, map, variant) - convert via JSON
        auto json_value = describe_to_json(value);
        oss << describe_json_value_to_string(json_value, fmt);
    }
    // std::function - output status and address
    else if constexpr (detail::is_function_v<T>) {
        if (fmt.quote_string_values) {
            oss << "\"";
        }
        if (value) {
            oss << "<function@0x" << std::hex << reinterpret_cast<uintptr_t>(&value) << ">";
        } else {
            oss << "<function:empty>";
        }
        if (fmt.quote_string_values) {
            oss << "\"";
        }
    }
    // boost::json::value - output as JSON without quotes
    else if constexpr (std::is_same_v<T, boost::json::value>) {
        oss << describe_json_value_to_string(value, fmt);
    }
    // boost::json::object - output as JSON without quotes
    else if constexpr (std::is_same_v<T, boost::json::object>) {
        oss << describe_json_value_to_string(boost::json::value(value), fmt);
    }
    // boost::json::array - output as JSON without quotes
    else if constexpr (std::is_same_v<T, boost::json::array>) {
        oss << describe_json_value_to_string(boost::json::value(value), fmt);
    }
    // Described enum
    else if constexpr (detail::is_described_enum_v<T>) {
        if (fmt.quote_string_values) {
            oss << "\"";
        }
        if (fmt.enum_as_string) {
            const char *enum_name = nullptr;
            boost::mp11::mp_for_each<boost::describe::describe_enumerators<T>>([&](auto D) {
                if (D.value == value) {
                    enum_name = D.name;
                }
            });
            if (enum_name) {
                oss << enum_name;
            } else {
                oss << static_cast<std::underlying_type_t<T>>(value);
            }
        } else {
            oss << static_cast<std::underlying_type_t<T>>(value);
        }
        if (fmt.quote_string_values) {
            oss << "\"";
        }
    }
    // Described struct
    else if constexpr (detail::is_described_v<T>) {
        oss << describe_to_string_with_fmt(value, fmt);
    }
    // Types with ostream operator
    else if constexpr (detail::has_ostream_operator_v<T>) {
        if (fmt.quote_string_values) {
            oss << "\"" << value << "\"";
        } else {
            oss << value;
        }
    }
    // Fallback - output address
    else {
        if (fmt.quote_string_values) {
            oss << "\"";
        }
        oss << fmt.address_prefix;
        if (fmt.hex_address) {
            auto flags = oss.flags();
            oss << "0x" << std::hex << reinterpret_cast<uintptr_t>(&value);
            oss.flags(flags);
        } else {
            oss << static_cast<const void *>(&value);
        }
        if (fmt.quote_string_values) {
            oss << "\"";
        }
    }
}

/**
 * @brief Convert struct to string with custom format
 */
template <typename T>
std::string describe_to_string_with_fmt(const T &obj, const DescribeOutputFormat &fmt)
{
    // Only use describe_members for described structs
    if constexpr (detail::is_described_v<T>) {
        std::ostringstream oss;
        oss << fmt.struct_begin;

        bool first = true;
        boost::mp11::mp_for_each<boost::describe::describe_members<T, boost::describe::mod_public>>(
        [&](auto D) {
            // Add separator before outputting field
            if (!first) {
                oss << fmt.field_separator;
            }
            // Output all fields uniformly (including optional fields)
            describe_output_member(oss, D.name, obj.*D.pointer, fmt);
            first = false;
        });

        oss << fmt.struct_end;
        return oss.str();
    } else {
        // For non-described types (vector, map, variant, etc.), convert via JSON
        auto json_value = describe_to_json(obj);
        return describe_json_value_to_string(json_value, fmt);
    }
}

/**
 * @brief Auto-detect type and convert to string
 *
 * Supports: struct, enum, vector, map, optional, and basic types (bool, int, float, string, const char *)
 */
template <typename T>
std::string describe_to_string(const T &value)
{
    // Fast path for enum - direct conversion
    if constexpr (detail::is_described_enum_v<T>) {
        return describe_enum_to_string(value);
    }
    // Fast path for bool
    else if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    }
    // Fast path for integral types
    else if constexpr (std::is_integral_v<T>) {
        return std::to_string(value);
    }
    // Fast path for floating point types
    else if constexpr (std::is_floating_point_v<T>) {
        return std::to_string(value);
    }
    // Fast path for string types
    else if constexpr (std::is_same_v<T, std::string>) {
        return value;
    } else if constexpr (std::is_same_v<T, const char *> || std::is_same_v<T, char *>) {
        return value ? std::string(value) : std::string("");
    }
    // Fast path for char arrays (string literals like "hello")
    else if constexpr (std::is_array_v<T> &&(std::is_same_v<std::remove_extent_t<T>, char> ||
                       std::is_same_v<std::remove_extent_t<T>, const char>)) {
        return std::string(value);
    }
    // Complex types (struct, vector, map, optional) - use JSON intermediate representation
    else {
        return describe_to_string_with_fmt(value, describe_get_global_format());
    }
}

} // namespace esp_brookesia::lib_utils

// ============================================================================
// Convenience Macros
// ============================================================================

// Describe registration macros (wrap Boost.Describe macros)
#define BROOKESIA_DESCRIBE_STRUCT(C, Bases, Members) BOOST_DESCRIBE_STRUCT(C, Bases, Members)  ///< Register struct for reflection
#define BROOKESIA_DESCRIBE_ENUM(C, ...) BOOST_DESCRIBE_ENUM(C, __VA_ARGS__)                    ///< Register enum for reflection

// Enum conversion macros
#define BROOKESIA_DESCRIBE_ENUM_TO_STR(value) \
    esp_brookesia::lib_utils::describe_enum_to_string(value)     ///< Convert enum to string
#define BROOKESIA_DESCRIBE_ENUM_TO_NUM(value) \
    esp_brookesia::lib_utils::describe_enum_to_number(value)        ///< Convert enum to underlying number
#define BROOKESIA_DESCRIBE_NUM_TO_ENUM(number, ret_value) \
    esp_brookesia::lib_utils::describe_number_to_enum(number, ret_value)  ///< Convert number to enum
#define BROOKESIA_DESCRIBE_STR_TO_ENUM(str, ret_value) \
    esp_brookesia::lib_utils::describe_string_to_enum(str, ret_value)     ///< Convert string to enum

// JSON conversion macros
#define BROOKESIA_DESCRIBE_TO_JSON(value) \
    esp_brookesia::lib_utils::describe_to_json(value)               ///< Convert any type to JSON
#define BROOKESIA_DESCRIBE_FROM_JSON(json_value, ret_value) \
    esp_brookesia::lib_utils::describe_from_json(json_value, ret_value)  ///< Convert JSON to any type

// String serialization/deserialization macros
#define BROOKESIA_DESCRIBE_JSON_SERIALIZE(value) \
    esp_brookesia::lib_utils::describe_json_serialize(value)  ///< Convert any type to JSON string
#define BROOKESIA_DESCRIBE_JSON_DESERIALIZE(str, ret_value) \
    esp_brookesia::lib_utils::describe_json_deserialize(str, ret_value)  ///< Convert JSON string to any type

// Format management macros
#define BROOKESIA_DESCRIBE_FORMAT_VERBOSE esp_brookesia::lib_utils::DESCRIBE_FORMAT_VERBOSE  ///< Verbose format
#define BROOKESIA_DESCRIBE_FORMAT_JSON esp_brookesia::lib_utils::DESCRIBE_FORMAT_JSON  ///< JSON format
#define BROOKESIA_DESCRIBE_FORMAT_COMPACT esp_brookesia::lib_utils::DESCRIBE_FORMAT_COMPACT  ///< Compact format
#define BROOKESIA_DESCRIBE_FORMAT_DEFAULT esp_brookesia::lib_utils::DESCRIBE_FORMAT_DEFAULT  ///< Default format
#define BROOKESIA_DESCRIBE_FORMAT_PYTHON esp_brookesia::lib_utils::DESCRIBE_FORMAT_PYTHON  ///< Python format
#define BROOKESIA_DESCRIBE_FORMAT_CPP esp_brookesia::lib_utils::DESCRIBE_FORMAT_CPP  ///< C++ format
#define BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(fmt) \
    esp_brookesia::lib_utils::describe_set_global_format(fmt)  ///< Set global format
#define BROOKESIA_DESCRIBE_GET_GLOBAL_FORMAT() \
    esp_brookesia::lib_utils::describe_get_global_format()  ///< Get global format
#define BROOKESIA_DESCRIBE_RESET_GLOBAL_FORMAT() \
    esp_brookesia::lib_utils::describe_reset_global_format()  ///< Reset global format

// Universal string conversion macros (auto-detect type)
#define BROOKESIA_DESCRIBE_TO_STR(value) \
    esp_brookesia::lib_utils::describe_to_string(value) ///< Convert with default format
#define BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(value, fmt) \
    esp_brookesia::lib_utils::describe_to_string_with_fmt(value, fmt) ///< Convert with custom format
