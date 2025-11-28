/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
#include <optional>
#include "boost/describe.hpp"
#include "boost/mp11.hpp"
#include "boost/json.hpp"

namespace esp_brookesia::lib_utils {

// ============================================================================
// Forward declarations for mutual recursion
// ============================================================================
template <typename T>
std::string describe_enum_to_string(T value);

template <typename T>
bool describe_string_to_enum(const std::string &name, T &ret_value);

template <typename T>
bool describe_number_to_enum(std::underlying_type_t<T> number, T &ret_value);

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

// Detect if type is std::map
template <typename T>
struct is_map : std::false_type {};

template <typename Key, typename Value, typename Compare, typename Alloc>
struct is_map<std::map<Key, Value, Compare, Alloc>> : std::true_type {};

template <typename T>
inline constexpr bool is_map_v = is_map<T>::value;

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
    .struct_begin = "{ ", .struct_end = " }", .field_separator = ", ",
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

// ============================================================================
// JSON Conversion Functions
// ============================================================================

// Forward declarations
template <typename T>
boost::json::value describe_member_to_json(const T &value);

template <typename T>
bool describe_json_to_member(const boost::json::value &j, T &value);

/**
 * @brief Convert boost::json::value to formatted string
 */
inline std::string describe_json_value_to_string(const boost::json::value &j, const DescribeOutputFormat *fmt = nullptr, int indent_level = 0)
{
    // Helper to create indentation
    auto make_indent = [&](int level) -> std::string {
        if (!fmt || !fmt->show_type_name)    // Use show_type_name as indicator for verbose/multiline mode
        {
            return "";
        }
        return std::string(level * 2, ' ');  // 2 spaces per indent level
    };

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
        return std::string(j.as_string());
    } else if (j.is_array()) {
        const auto &arr = j.as_array();
        if (arr.empty()) {
            return "[]";
        }

        if (fmt && fmt->show_type_name) {
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
            // Compact format
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
    } else if (j.is_object()) {
        const auto &obj = j.as_object();
        if (obj.empty()) {
            return "{}";
        }

        if (fmt && fmt->show_type_name) {
            // Multiline format for verbose mode
            std::string result = "{\n";
            bool first = true;
            for (const auto &[key, val] : obj) {
                if (!first) {
                    result += ",\n";
                }
                first = false;
                result += make_indent(indent_level + 1);
                result += fmt->field_prefix + std::string(key) + fmt->name_value_separator + describe_json_value_to_string(val, fmt, indent_level + 1);
            }
            result += "\n" + make_indent(indent_level) + "}";
            return result;
        } else {
            // Compact format
            std::string result = "{ ";
            bool first = true;
            for (const auto &[key, val] : obj) {
                if (!first) {
                    result += ", ";
                }
                first = false;
                result += std::string(key) + ": " + describe_json_value_to_string(val, fmt, indent_level);
            }
            result += " }";
            return result;
        }
    }
    return "";
}

/**
 * @brief Convert value to JSON (supports all common types)
 */
template <typename T>
boost::json::value describe_member_to_json(const T &value)
{
    // Described enum
    if constexpr (is_described_enum_v<T>) {
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
    else if constexpr (is_described_v<T>) {
        boost::json::object j;
        boost::mp11::mp_for_each<boost::describe::describe_members<T, boost::describe::mod_public>>(
        [&](auto D) {
            j[D.name] = describe_member_to_json(value.*D.pointer);
        });
        return j;
    }
    // std::optional
    else if constexpr (is_optional_v<T>) {
        return value.has_value() ? describe_member_to_json(*value) : nullptr;
    }
    // std::vector
    else if constexpr (is_vector_v<T>) {
        boost::json::array arr;
        for (const auto &item : value) {
            arr.push_back(describe_member_to_json(item));
        }
        return arr;
    }
    // std::map
    else if constexpr (is_map_v<T>) {
        boost::json::object obj;
        for (const auto &[key, val] : value) {
            std::string key_str;
            if constexpr (std::is_same_v<typename T::key_type, std::string>) {
                key_str = key;
            } else if constexpr (std::is_integral_v<typename T::key_type>) {
                key_str = std::to_string(key);
            } else if constexpr (is_described_enum_v<typename T::key_type>) {
                key_str = describe_enum_to_string(key);
            } else {
                // Fallback: convert key to JSON first, then to string
                auto key_json = describe_member_to_json(key);
                key_str = describe_json_value_to_string(key_json);
            }
            obj[key_str] = describe_member_to_json(val);
        }
        return obj;
    }
    // Basic types
    else if constexpr (std::is_same_v<T, bool>) {
        return value;
    } else if constexpr ((std::is_integral_v<T>) && (!std::is_same_v<T, bool>)) {
        // Handle integer types - ensure correct signed/unsigned conversion
        // Explicitly check for signed types first
        if constexpr (std::is_same_v<T, signed char> || std::is_same_v<T, short> ||
                      std::is_same_v<T, int> || std::is_same_v<T, long> ||
                      std::is_same_v<T, long long> || std::is_same_v<T, std::int8_t> ||
                      std::is_same_v<T, std::int16_t> || std::is_same_v<T, std::int32_t> ||
                      std::is_same_v<T, std::int64_t>) {
            return static_cast<std::int64_t>(value);
        } else {
            // unsigned types
            return static_cast<std::uint64_t>(value);
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        return static_cast<double>(value);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return boost::json::value(value);
    } else if constexpr (std::is_same_v<T, const char *>) {
        return boost::json::value(std::string(value));
    }
    // Types with ostream operator
    else if constexpr (has_ostream_operator_v<T>) {
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
bool describe_json_to_member(const boost::json::value &j, T &value)
{
    // Described enum
    if constexpr (is_described_enum_v<T>) {
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
    else if constexpr (is_described_v<T>) {
        if (!j.is_object()) {
            value = T{};
            return false;
        }
        value = T{};
        const auto &json_obj = j.as_object();
        bool success = true;
        boost::mp11::mp_for_each<boost::describe::describe_members<T, boost::describe::mod_public>>(
        [&](auto D) {
            auto it = json_obj.find(D.name);
            if (it != json_obj.end()) {
                if (!describe_json_to_member(it->value(), value.*D.pointer)) {
                    success = false;
                }
            }
        });
        return success;
    }
    // std::optional
    else if constexpr (is_optional_v<T>) {
        if (j.is_null()) {
            value = std::nullopt;
            return true;
        } else {
            typename T::value_type temp;
            if (describe_json_to_member(j, temp)) {
                value = temp;
                return true;
            }
            return false;
        }
    }
    // std::vector
    else if constexpr (is_vector_v<T>) {
        if (!j.is_array()) {
            return false;
        }
        const auto &arr = j.as_array();
        value.clear();
        value.reserve(arr.size());
        for (const auto &item : arr) {
            typename T::value_type temp;
            if (!describe_json_to_member(item, temp)) {
                return false;
            }
            value.push_back(std::move(temp));
        }
        return true;
    }
    // std::map
    else if constexpr (is_map_v<T>) {
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
                key = std::is_signed_v<typename T::key_type>
                      ? static_cast<typename T::key_type>(std::stoll(std::string(key_str)))
                      : static_cast<typename T::key_type>(std::stoull(std::string(key_str)));
            } else if constexpr (is_described_enum_v<typename T::key_type>) {
                if (!describe_string_to_enum(std::string(key_str), key)) {
                    return false;
                }
            } else {
                return false;  // Unsupported key type
            }

            if (!describe_json_to_member(val_json, val)) {
                return false;
            }
            value[std::move(key)] = std::move(val);
        }
        return true;
    }
    // Basic types
    else if constexpr (std::is_same_v<T, bool>) {
        if (j.is_bool()) {
            value = j.as_bool();
            return true;
        }
        return false;
    } else if constexpr (std::is_integral_v<T>) {
        if (j.is_number()) {
            // Handle different JSON number types
            if (j.is_int64()) {
                value = static_cast<T>(j.as_int64());
            } else if (j.is_uint64()) {
                value = static_cast<T>(j.as_uint64());
            } else {
                // Fallback for other numeric types
                value = static_cast<T>(j.as_double());
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
    }
    return false;
}

// ============================================================================
// String Format Conversion Functions
// ============================================================================

// Forward declaration
template <typename T>
std::string describe_struct_to_string_impl(const T &obj, const DescribeOutputFormat &fmt);

/**
 * @brief Output single member to string stream
 */
template <typename T>
void describe_output_member(std::ostringstream &oss, const char *name, const T &value, const DescribeOutputFormat &fmt)
{
    // Output field name
    if (fmt.quote_field_names) {
        oss << "\"" << fmt.field_prefix << name << "\"";
    } else {
        oss << fmt.field_prefix << name;
    }
    oss << fmt.name_value_separator;

    // Output value based on type
    if constexpr (is_described_enum_v<T>) {
        if (fmt.enum_as_string) {
            const char *enum_name = nullptr;
            boost::mp11::mp_for_each<boost::describe::describe_enumerators<T>>([&](auto D) {
                if (D.value == value) {
                    enum_name = D.name;
                }
            });
            if (fmt.quote_string_values) {
                oss << "\"";
            }
            oss << (enum_name ? enum_name : std::to_string(static_cast<std::underlying_type_t<T>>(value)).c_str());
            if (fmt.quote_string_values) {
                oss << "\"";
            }
        } else {
            if (fmt.quote_string_values) {
                oss << "\"";
            }
            oss << static_cast<std::underlying_type_t<T>>(value);
            if (fmt.quote_string_values) {
                oss << "\"";
            }
        }
    } else if constexpr (is_described_v<T>) {
        oss << describe_struct_to_string_impl(value, fmt);
    } else if constexpr (std::is_same_v<T, bool>) {
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
    } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, const char *>) {
        if (fmt.quote_string_values) {
            oss << "\"" << value << "\"";
        } else {
            oss << value;
        }
    } else if constexpr (is_optional_v<T> || is_vector_v<T> || is_map_v<T>) {
        // Handle containers (vector, map, optional) - convert via JSON
        auto json_value = describe_member_to_json(value);
        oss << describe_json_value_to_string(json_value, &fmt);
    } else if constexpr (has_ostream_operator_v<T>) {
        if (fmt.quote_string_values) {
            oss << "\"" << value << "\"";
        } else {
            oss << value;
        }
    } else {
        // Output address
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
std::string describe_struct_to_string_impl(const T &obj, const DescribeOutputFormat &fmt)
{
    std::ostringstream oss;
    oss << fmt.struct_begin;

    bool first = true;
    boost::mp11::mp_for_each<boost::describe::describe_members<T, boost::describe::mod_public>>(
    [&](auto D) {
        if (!first) {
            oss << fmt.field_separator;
        }
        first = false;
        describe_output_member(oss, D.name, obj.*D.pointer, fmt);
    });

    oss << fmt.struct_end;
    return oss.str();
}

} // namespace detail

// ============================================================================
// Public API - Struct Conversion Functions
// ============================================================================

/**
 * @brief Convert struct to string (using global format)
 */
template <typename T>
std::string describe_struct_to_string(const T &obj)
{
    return detail::describe_struct_to_string_impl(obj, detail::DescribeFormatManager::instance().get_format());
}

/**
 * @brief Convert struct to string (with custom format)
 */
template <typename T>
std::string describe_struct_to_string(const T &obj, const detail::DescribeOutputFormat &fmt)
{
    return detail::describe_struct_to_string_impl(obj, fmt);
}

/**
 * @brief Convert struct to JSON
 */
template <typename T>
boost::json::value describe_struct_to_json(const T &obj)
{
    return detail::describe_member_to_json(obj);
}

/**
 * @brief Convert JSON to struct
 */
template <typename T>
bool describe_json_to_struct(const boost::json::value &j, T &ret_value)
{
    return detail::describe_json_to_member(j, ret_value);
}

// ============================================================================
// Public API - Format Management Functions
// ============================================================================

inline void describe_set_global_format(const detail::DescribeOutputFormat &fmt)
{
    detail::DescribeFormatManager::instance().set_format(fmt);
}

inline const detail::DescribeOutputFormat &describe_get_global_format()
{
    return detail::DescribeFormatManager::instance().get_format();
}

inline void describe_reset_global_format()
{
    detail::DescribeFormatManager::instance().reset_to_default();
}

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
// Public API - Universal Conversion Function
// ============================================================================

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
    // Fast path for struct - use global format
    else if constexpr (detail::is_described_v<T>) {
        return describe_struct_to_string(value);
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
    } else if constexpr (std::is_same_v<T, const char *>) {
        return std::string(value);
    }
    // Complex types (vector, map, optional) - use JSON intermediate representation
    else {
        auto json_value = detail::describe_member_to_json(value);
        return detail::describe_json_value_to_string(json_value);
    }
}

/**
 * @brief Auto-detect type and convert to string (with custom format for structs)
 */
template <typename T>
std::string describe_to_string(const T &value, const detail::DescribeOutputFormat &fmt)
{
    if constexpr (detail::is_described_v<T>) {
        return describe_struct_to_string(value, fmt);
    } else {
        return describe_to_string(value);
    }
}

} // namespace esp_brookesia::lib_utils

// ============================================================================
// Convenience Macros
// ============================================================================

// Describe registration macros (wrap Boost.Describe macros)
#define BROOKESIA_DESCRIBE_STRUCT(C, Bases, Members) BOOST_DESCRIBE_STRUCT(C, Bases, Members)  ///< Register struct for reflection
#define BROOKESIA_DESCRIBE_ENUM(C, ...) BOOST_DESCRIBE_ENUM(C, __VA_ARGS__)                    ///< Register enum for reflection

// Universal string conversion macros (auto-detect type)
#define BROOKESIA_DESCRIBE_TO_STR(value) esp_brookesia::lib_utils::describe_to_string(value)                   ///< Convert any type to string
#define BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(value, fmt) esp_brookesia::lib_utils::describe_to_string(value, fmt) ///< Convert with custom format

// Enum conversion macros
#define BROOKESIA_DESCRIBE_ENUM_TO_NUM(value) \
    esp_brookesia::lib_utils::describe_enum_to_number(value)        ///< Convert enum to underlying number
#define BROOKESIA_DESCRIBE_NUM_TO_ENUM(number, ret_value) \
    esp_brookesia::lib_utils::describe_number_to_enum(number, ret_value)  ///< Convert number to enum
#define BROOKESIA_DESCRIBE_STR_TO_ENUM(str, ret_value) \
    esp_brookesia::lib_utils::describe_string_to_enum(str, ret_value)     ///< Convert string to enum

// JSON conversion macros
#define BROOKESIA_DESCRIBE_STRUCT_TO_JSON(ret_value) \
    esp_brookesia::lib_utils::describe_struct_to_json(ret_value)               ///< Convert struct to JSON
#define BROOKESIA_DESCRIBE_JSON_TO_STRUCT(json_value, ret_value) \
    esp_brookesia::lib_utils::describe_json_to_struct(json_value, ret_value)  ///< Convert JSON to struct

// Format management macros
#define BROOKESIA_DESCRIBE_SET_GLOBAL_FORMAT(fmt) \
    esp_brookesia::lib_utils::describe_set_global_format(fmt)  ///< Set global format
#define BROOKESIA_DESCRIBE_GET_GLOBAL_FORMAT() \
    esp_brookesia::lib_utils::describe_get_global_format()  ///< Get global format
#define BROOKESIA_DESCRIBE_RESET_GLOBAL_FORMAT() \
    esp_brookesia::lib_utils::describe_reset_global_format()  ///< Reset global format
