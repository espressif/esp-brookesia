/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * @brief Logging helpers, formatting adapters, and convenience macros for lib_utils.
 */

#include <algorithm>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <string>
#include <source_location>
#include "boost/format.hpp"
#include "brookesia/lib_utils/macro_configs.h"
#if defined(ESP_PLATFORM) && (BROOKESIA_UTILS_LOG_IMPL_TYPE == BROOKESIA_UTILS_LOG_IMPL_ESP)
#   include "log_impl_esp.h"
#else
#   include "log_impl_std.h"
#endif
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/thread_config.hpp"

namespace esp_brookesia::lib_utils {

/**
 * Helper macros to assemble format string and arguments based on enabled macros
 * These macros use string literal concatenation (adjacent string literals are automatically concatenated)
 */

// Format string components
#define _BROOKESIA_LOG_FORMAT_THREAD_NAME "<%s>"
#define _BROOKESIA_LOG_FORMAT_FILE_LINE "[%.*s:%04d]"
#define _BROOKESIA_LOG_FORMAT_FUNCTION "(%.*s)"
#define _BROOKESIA_LOG_FORMAT_MESSAGE ": %s"

// Assemble format string using string literal concatenation
#if BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_FORMAT_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_FORMAT_MESSAGE
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_FORMAT_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_FORMAT_MESSAGE
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_FORMAT_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_FORMAT_MESSAGE
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME
#   define _BROOKESIA_LOG_FORMAT_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_MESSAGE
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_FORMAT_STRING \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_FORMAT_MESSAGE
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_FORMAT_STRING \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_FORMAT_MESSAGE
#elif BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_FORMAT_STRING \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_FORMAT_MESSAGE
#else
#   define _BROOKESIA_LOG_FORMAT_STRING \
        _BROOKESIA_LOG_FORMAT_MESSAGE
#endif

// Helper macro to expand arguments based on enabled macros
#define _BROOKESIA_LOG_ARGS_THREAD_NAME(thread_name) thread_name
#define _BROOKESIA_LOG_ARGS_FILE_LINE(file_name, line) (int)(file_name).size(), (file_name).data(), line
#define _BROOKESIA_LOG_ARGS_FUNCTION(func_name) (int)(func_name).size(), (func_name).data()
#define _BROOKESIA_LOG_ARGS_MESSAGE(format_str) (format_str).data()

// Assemble arguments
#if BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str) \
        _BROOKESIA_LOG_ARGS_THREAD_NAME(thread_name) , \
        _BROOKESIA_LOG_ARGS_FILE_LINE(file_name, line) , \
        _BROOKESIA_LOG_ARGS_FUNCTION(func_name) , \
        _BROOKESIA_LOG_ARGS_MESSAGE(format_str)
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str) \
        _BROOKESIA_LOG_ARGS_THREAD_NAME(thread_name) , \
        _BROOKESIA_LOG_ARGS_FILE_LINE(file_name, line) , \
        _BROOKESIA_LOG_ARGS_MESSAGE(format_str)
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str) \
        _BROOKESIA_LOG_ARGS_THREAD_NAME(thread_name) , \
        _BROOKESIA_LOG_ARGS_FUNCTION(func_name) , \
        _BROOKESIA_LOG_ARGS_MESSAGE(format_str)
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME
#   define _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str) \
        _BROOKESIA_LOG_ARGS_THREAD_NAME(thread_name) , \
        _BROOKESIA_LOG_ARGS_MESSAGE(format_str)
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str) \
        _BROOKESIA_LOG_ARGS_FILE_LINE(file_name, line) , \
        _BROOKESIA_LOG_ARGS_FUNCTION(func_name) , \
        _BROOKESIA_LOG_ARGS_MESSAGE(format_str)
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str) \
        _BROOKESIA_LOG_ARGS_FILE_LINE(file_name, line) , \
        _BROOKESIA_LOG_ARGS_MESSAGE(format_str)
#elif BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str) \
        _BROOKESIA_LOG_ARGS_FUNCTION(func_name) , \
        _BROOKESIA_LOG_ARGS_MESSAGE(format_str)
#else
#   define _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str) \
        _BROOKESIA_LOG_ARGS_MESSAGE(format_str)
#endif

/**
 * Type-erased format argument that preserves type category for boost::format.
 * Only 6 type slots are used, so boost::format::operator% is instantiated exactly 6 times
 * (in the .cpp file), regardless of how many distinct argument types callers use.
 *
 * For types requiring string conversion (described structs/enums), the result is stored in
 * `owned_str` and `s` is left null. format_message checks `s` first; if null, uses `owned_str`.
 */
struct FormatArg {
    enum class Type : uint8_t { Int, Uint, Double, Str, Ptr, Char };
    Type type;
    union {
        int64_t i;
        uint64_t u;
        double d;
        const char *s;
        const void *p;
    };
    std::string owned_str;
};

/**
 * @brief Convert a log argument to FormatArg while preserving its type category.
 */
template<typename T>
FormatArg make_format_arg(T &&arg)
{
    using D = std::remove_cvref_t<T>;
    FormatArg a{};
    if constexpr (std::is_same_v<D, bool>) {
        a.type = FormatArg::Type::Str;
        a.s = arg ? "true" : "false";
    } else if constexpr (std::is_same_v<D, std::nullptr_t>) {
        a.type = FormatArg::Type::Str;
        a.s = "(null)";
    } else if constexpr (std::is_same_v<D, const char *> || std::is_same_v<D, char *> ||
                         std::is_same_v<D, const unsigned char *> || std::is_same_v<D, unsigned char *>) {
        a.type = FormatArg::Type::Str;
        a.s = arg ? reinterpret_cast<const char *>(arg) : "(null)";
    } else if constexpr (std::is_array_v<D> &&
                         (std::is_same_v<std::remove_extent_t<D>, char> ||
                          std::is_same_v<std::remove_extent_t<D>, const char> ||
                          std::is_same_v<std::remove_extent_t<D>, unsigned char> ||
                          std::is_same_v<std::remove_extent_t<D>, const unsigned char>)) {
        a.type = FormatArg::Type::Str;
        a.s = reinterpret_cast<const char *>(arg);
    } else if constexpr (std::is_same_v<D, std::string>) {
        a.type = FormatArg::Type::Str;
        a.s = arg.c_str();
    } else if constexpr (std::is_same_v<D, std::string_view>) {
        a.type = FormatArg::Type::Str;
        a.s = arg.data();
    } else if constexpr (std::is_same_v<D, char>) {
        a.type = FormatArg::Type::Char;
        a.i = static_cast<int64_t>(arg);
    } else if constexpr (std::is_floating_point_v<D>) {
        a.type = FormatArg::Type::Double;
        a.d = static_cast<double>(arg);
    } else if constexpr (std::is_signed_v<D> &&std::is_integral_v<D>) {
        a.type = FormatArg::Type::Int;
        a.i = static_cast<int64_t>(arg);
    } else if constexpr (std::is_unsigned_v<D> &&std::is_integral_v<D>) {
        a.type = FormatArg::Type::Uint;
        a.u = static_cast<uint64_t>(arg);
    } else if constexpr (std::is_pointer_v<D>) {
        a.type = FormatArg::Type::Ptr;
        a.p = reinterpret_cast<const void *>(arg);
    } else if constexpr (std::is_enum_v<D> &&
                         !detail::is_described_enum_v<D>) {
        using U = std::underlying_type_t<D>;
        if constexpr (std::is_signed_v<U>) {
            a.type = FormatArg::Type::Int;
            a.i = static_cast<int64_t>(static_cast<U>(arg));
        } else {
            a.type = FormatArg::Type::Uint;
            a.u = static_cast<uint64_t>(static_cast<U>(arg));
        }
    } else if constexpr (std::is_same_v<D, boost::json::string>) {
        a.type = FormatArg::Type::Str;
        a.s = arg.c_str();
    } else {
        a.type = FormatArg::Type::Str;
        a.owned_str = describe_to_string(arg);
    }
    return a;
}

/**
 * @brief Logging backend facade used by the public logging macros.
 */
class Log {
public:
    Log(const Log &) = delete;
    Log(Log &&) = delete;
    Log &operator=(const Log &) = delete;
    Log &operator=(Log &&) = delete;

    /**
     * @brief Print a preformatted message.
     *
     * @param level Log level defined by `BROOKESIA_UTILS_LOG_LEVEL_*`.
     * @param loc Source location used for metadata extraction.
     * @param tag Log tag string.
     * @param format Message string passed through without argument formatting.
     */
    void print(int level, const std::source_location &loc, const char *tag, const char *format)
    {
        write(level, loc, tag, std::string(format));
    }

    /**
     * @brief Format a message and print it.
     *
     * @tparam Args Format argument types.
     * @param level Log level defined by `BROOKESIA_UTILS_LOG_LEVEL_*`.
     * @param loc Source location used for metadata extraction.
     * @param tag Log tag string.
     * @param format Boost-format-style message format.
     * @param args Format arguments forwarded through the type-erased formatter.
     */
    template <typename... Args>
    void print(int level, const std::source_location &loc, const char *tag, const char *format, Args &&... args)
    {
        write(level, loc, tag, format_message(format, {make_format_arg(std::forward<Args>(args))...}));
    }

    /**
     * @brief Get the singleton logging facade.
     *
     * @return Reference to the shared `Log` instance.
     */
    static Log &getInstance()
    {
        static Log instance;
        return instance;
    }

    /**
     * @brief Emit a fully formatted message through the selected backend.
     *
     * @param level Log level defined by `BROOKESIA_UTILS_LOG_LEVEL_*`.
     * @param loc Source location used for metadata extraction.
     * @param tag Log tag string.
     * @param message Final message body.
     */
    static void write(int level, const std::source_location &loc, const char *tag, const std::string &message);
    /**
     * @brief Format a message using `boost::format`-style placeholders.
     *
     * @param format Format string.
     * @param args Type-erased format arguments.
     * @return Formatted message string.
     */
    static std::string format_message(const char *format, std::initializer_list<FormatArg> args);
    /**
     * @brief Extract the display-friendly function name from a source location string.
     *
     * @param func_name Raw function signature string.
     * @return Trimmed function name view.
     */
    static std::string_view extract_function_name(const char *func_name);
    /**
     * @brief Extract the leaf file name from a source path.
     *
     * @param file_path Raw source file path.
     * @return File name view without directory components.
     */
    static std::string_view extract_file_name(const char *file_path);

private:
    Log() = default;
};

/**
 * Macros to simplify logging calls with a fixed tag
 */
#define BROOKESIA_LOGT_IMPL(tag, format, ...) \
    esp_brookesia::lib_utils::Log::getInstance().print(BROOKESIA_UTILS_LOG_LEVEL_TRACE,     \
        std::source_location::current(), tag, format, ##__VA_ARGS__ \
    )
#define BROOKESIA_LOGD_IMPL(tag, format, ...) \
    esp_brookesia::lib_utils::Log::getInstance().print(BROOKESIA_UTILS_LOG_LEVEL_DEBUG,     \
        std::source_location::current(), tag, format, ##__VA_ARGS__ \
    )
#define BROOKESIA_LOGI_IMPL(tag, format, ...) \
    esp_brookesia::lib_utils::Log::getInstance().print(BROOKESIA_UTILS_LOG_LEVEL_INFO,      \
        std::source_location::current(), tag, format, ##__VA_ARGS__ \
    )
#define BROOKESIA_LOGW_IMPL(tag, format, ...) \
    esp_brookesia::lib_utils::Log::getInstance().print(BROOKESIA_UTILS_LOG_LEVEL_WARNING,   \
        std::source_location::current(), tag, format, ##__VA_ARGS__ \
    )
#define BROOKESIA_LOGE_IMPL(tag, format, ...) \
    esp_brookesia::lib_utils::Log::getInstance().print(BROOKESIA_UTILS_LOG_LEVEL_ERROR,     \
        std::source_location::current(), tag, format, ##__VA_ARGS__ \
    )

/**
 * @brief Default tag used by `BROOKESIA_LOG*` macros in the current translation unit.
 */
#if defined(BROOKESIA_LOG_TAG)
constexpr auto TAG = BROOKESIA_LOG_TAG;
#else
constexpr auto TAG = BROOKESIA_UTILS_LOG_TAG;
#endif

/**
 * @brief Per-file switch that disables `BROOKESIA_LOGT`, `BROOKESIA_LOGD`, and trace-guard helpers.
 *
 * Users can define this macro before including this header to override the default behavior.
 *
 * @code
 *   #define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1  // Disable all debug & trace features for this file
 *   #include "brookesia/lib_utils/log.hpp"
 * @endcode
 */
#ifndef BROOKESIA_LOG_DISABLE_DEBUG_TRACE
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 0
#endif

/**
 * @brief Compile-time log level filtering macros.
 *
 * Calls below the configured log level expand to no-ops.
 */
/** @def BROOKESIA_LOGT
 *  @brief Emit a trace-level log message for the current translation unit tag.
 */
/** @def BROOKESIA_LOGD
 *  @brief Emit a debug-level log message for the current translation unit tag.
 */
/** @def BROOKESIA_LOGI
 *  @brief Emit an info-level log message for the current translation unit tag.
 */
/** @def BROOKESIA_LOGW
 *  @brief Emit a warning-level log message for the current translation unit tag.
 */
/** @def BROOKESIA_LOGE
 *  @brief Emit an error-level log message for the current translation unit tag.
 */
#if (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_TRACE) && \
    (!BROOKESIA_LOG_DISABLE_DEBUG_TRACE)
#   define BROOKESIA_LOGT(format, ...) BROOKESIA_LOGT_IMPL(esp_brookesia::lib_utils::TAG, format, ##__VA_ARGS__)
#else
#   define BROOKESIA_LOGT(format, ...) ((void)0)
#endif

#if (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG) && \
    (!BROOKESIA_LOG_DISABLE_DEBUG_TRACE)
#   define BROOKESIA_LOGD(format, ...) BROOKESIA_LOGD_IMPL(esp_brookesia::lib_utils::TAG, format, ##__VA_ARGS__)
#else
#   define BROOKESIA_LOGD(format, ...) ((void)0)
#endif

#if (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_INFO)
#   define BROOKESIA_LOGI(format, ...) BROOKESIA_LOGI_IMPL(esp_brookesia::lib_utils::TAG, format, ##__VA_ARGS__)
#else
#   define BROOKESIA_LOGI(format, ...) ((void)0)
#endif

#if (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_WARNING)
#   define BROOKESIA_LOGW(format, ...) BROOKESIA_LOGW_IMPL(esp_brookesia::lib_utils::TAG, format, ##__VA_ARGS__)
#else
#   define BROOKESIA_LOGW(format, ...) ((void)0)
#endif

#if (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_ERROR)
#   define BROOKESIA_LOGE(format, ...) BROOKESIA_LOGE_IMPL(esp_brookesia::lib_utils::TAG, format, ##__VA_ARGS__)
#else
#   define BROOKESIA_LOGE(format, ...) ((void)0)
#endif

// Macros for LogTraceGuard (with Enter/Exit messages and optional this pointer)
#define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR ": (@%p) Enter"
#define _BROOKESIA_LOG_TRACE_FORMAT_ENTER ": Enter"
#define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR ": (@%p) Exit"
#define _BROOKESIA_LOG_TRACE_FORMAT_EXIT ": Exit"

// Assemble format string for trace guard Enter (with this pointer)
#if BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR
#elif BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR
#else
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR_STRING \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR
#endif

// Assemble format string for trace guard Enter (without this pointer)
#if BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_STRING \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_STRING \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER
#elif BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_STRING \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER
#else
#   define _BROOKESIA_LOG_TRACE_FORMAT_ENTER_STRING \
        _BROOKESIA_LOG_TRACE_FORMAT_ENTER
#endif

// Assemble format string for trace guard Exit (with this pointer)
#if BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR
#elif BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR_STRING \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR
#else
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR_STRING \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR
#endif

// Assemble format string for trace guard Exit (without this pointer)
#if BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_STRING \
        _BROOKESIA_LOG_FORMAT_THREAD_NAME \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_STRING \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_STRING \
        _BROOKESIA_LOG_FORMAT_FILE_LINE \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT
#elif BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_STRING \
        _BROOKESIA_LOG_FORMAT_FUNCTION \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT
#else
#   define _BROOKESIA_LOG_TRACE_FORMAT_EXIT_STRING \
        _BROOKESIA_LOG_TRACE_FORMAT_EXIT
#endif

// Helper macro for trace guard arguments (with this pointer)
#if BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_ARGS_WITH_PTR(thread_name, file_name, line, func_name, this_ptr) \
        _BROOKESIA_LOG_ARGS_THREAD_NAME(thread_name) , \
        _BROOKESIA_LOG_ARGS_FILE_LINE(file_name, line) , \
        _BROOKESIA_LOG_ARGS_FUNCTION(func_name) , \
        this_ptr
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_TRACE_ARGS_WITH_PTR(thread_name, file_name, line, func_name, this_ptr) \
        _BROOKESIA_LOG_ARGS_THREAD_NAME(thread_name) , \
        _BROOKESIA_LOG_ARGS_FILE_LINE(file_name, line) , \
        this_ptr
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_ARGS_WITH_PTR(thread_name, file_name, line, func_name, this_ptr) \
        _BROOKESIA_LOG_ARGS_THREAD_NAME(thread_name) , \
        _BROOKESIA_LOG_ARGS_FUNCTION(func_name) , \
        this_ptr
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME
#   define _BROOKESIA_LOG_TRACE_ARGS_WITH_PTR(thread_name, file_name, line, func_name, this_ptr) \
        _BROOKESIA_LOG_ARGS_THREAD_NAME(thread_name) , \
        this_ptr
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_ARGS_WITH_PTR(thread_name, file_name, line, func_name, this_ptr) \
        _BROOKESIA_LOG_ARGS_FILE_LINE(file_name, line) , \
        _BROOKESIA_LOG_ARGS_FUNCTION(func_name) , \
        this_ptr
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_TRACE_ARGS_WITH_PTR(thread_name, file_name, line, func_name, this_ptr) \
        _BROOKESIA_LOG_ARGS_FILE_LINE(file_name, line) , \
        this_ptr
#elif BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_ARGS_WITH_PTR(thread_name, file_name, line, func_name, this_ptr) \
        _BROOKESIA_LOG_ARGS_FUNCTION(func_name) , \
        this_ptr
#else
#   define _BROOKESIA_LOG_TRACE_ARGS_WITH_PTR(thread_name, file_name, line, func_name, this_ptr) \
        this_ptr
#endif

// Helper macro for trace guard arguments (without this pointer)
#if BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_ARGS(thread_name, file_name, line, func_name) \
        _BROOKESIA_LOG_ARGS_THREAD_NAME(thread_name) , \
        _BROOKESIA_LOG_ARGS_FILE_LINE(file_name, line) , \
        _BROOKESIA_LOG_ARGS_FUNCTION(func_name)
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_TRACE_ARGS(thread_name, file_name, line, func_name) \
        _BROOKESIA_LOG_ARGS_THREAD_NAME(thread_name) , \
        _BROOKESIA_LOG_ARGS_FILE_LINE(file_name, line)
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_ARGS(thread_name, file_name, line, func_name) \
        _BROOKESIA_LOG_ARGS_THREAD_NAME(thread_name) , \
        _BROOKESIA_LOG_ARGS_FUNCTION(func_name)
#elif BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME
#   define _BROOKESIA_LOG_TRACE_ARGS(thread_name, file_name, line, func_name) \
        _BROOKESIA_LOG_ARGS_THREAD_NAME(thread_name)
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE && BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_ARGS(thread_name, file_name, line, func_name) \
        _BROOKESIA_LOG_ARGS_FILE_LINE(file_name, line) , \
        _BROOKESIA_LOG_ARGS_FUNCTION(func_name)
#elif BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
#   define _BROOKESIA_LOG_TRACE_ARGS(thread_name, file_name, line, func_name) \
        _BROOKESIA_LOG_ARGS_FILE_LINE(file_name, line),
#elif BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
#   define _BROOKESIA_LOG_TRACE_ARGS(thread_name, file_name, line, func_name) \
        _BROOKESIA_LOG_ARGS_FUNCTION(func_name)
#endif

/**
 * @brief RAII helper that logs function entry and exit at trace level.
 *
 * @tparam Enabled Compile-time flag that removes all work when `false`.
 */
template<bool Enabled>
class LogTraceGuard {
public:
    /**
     * @brief Construct a trace guard for the current scope.
     *
     * @param this_ptr Optional object pointer logged for member functions.
     * @param loc Source location captured for the scope.
     * @param tag Log tag string.
     */
    LogTraceGuard(
        const void *this_ptr = nullptr, const std::source_location &loc = std::source_location::current(),
        const char *tag = esp_brookesia::lib_utils::TAG
    )
    {
        if constexpr (Enabled) {
            _tag = tag;
#if BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME
            auto thread_config = ThreadConfig::get_current_config();
            auto thread_name = thread_config.name.c_str();
#endif
#if BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
            _file_name = Log::extract_file_name(loc.file_name());
            _line = static_cast<size_t>(loc.line());
#endif
#if BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
            _func_name = Log::extract_function_name(loc.function_name());
#endif
            _this_ptr = this_ptr;

            if (_this_ptr) {
                BROOKESIA_LOGT_IMPL_FUNC(
                    _tag,
                    _BROOKESIA_LOG_TRACE_FORMAT_ENTER_WITH_PTR_STRING,
                    _BROOKESIA_LOG_TRACE_ARGS_WITH_PTR(thread_name, _file_name, _line, _func_name, _this_ptr)
                );
            } else {
#if defined(_BROOKESIA_LOG_TRACE_ARGS)
                BROOKESIA_LOGT_IMPL_FUNC(
                    _tag,
                    _BROOKESIA_LOG_TRACE_FORMAT_ENTER_STRING,
                    _BROOKESIA_LOG_TRACE_ARGS(thread_name, _file_name, _line, _func_name)
                );
#else
                BROOKESIA_LOGT_IMPL_FUNC(
                    _tag,
                    _BROOKESIA_LOG_TRACE_FORMAT_ENTER_STRING
                );
#endif
            }
        } else {
            (void)this_ptr;
            (void)loc;
            (void)tag;
        }
    }

    /**
     * @brief Log scope exit when trace logging is enabled.
     */
    ~LogTraceGuard()
    {
#if BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME
        auto thread_config = ThreadConfig::get_current_config();
        auto thread_name = thread_config.name.c_str();
#endif
        if constexpr (Enabled) {
            if (_this_ptr) {
                BROOKESIA_LOGT_IMPL_FUNC(
                    _tag,
                    _BROOKESIA_LOG_TRACE_FORMAT_EXIT_WITH_PTR_STRING,
                    _BROOKESIA_LOG_TRACE_ARGS_WITH_PTR(thread_name, _file_name, _line, _func_name, _this_ptr)
                );
            } else {
#if defined(_BROOKESIA_LOG_TRACE_ARGS)
                BROOKESIA_LOGT_IMPL_FUNC(
                    _tag,
                    _BROOKESIA_LOG_TRACE_FORMAT_EXIT_STRING,
                    _BROOKESIA_LOG_TRACE_ARGS(thread_name, _file_name, _line, _func_name)
                );
#else
                BROOKESIA_LOGT_IMPL_FUNC(
                    _tag,
                    _BROOKESIA_LOG_TRACE_FORMAT_EXIT_STRING
                );
#endif
            }
        }
    }

    LogTraceGuard(const LogTraceGuard &) = delete;
    LogTraceGuard(LogTraceGuard &&) = delete;
    LogTraceGuard &operator=(const LogTraceGuard &) = delete;
    LogTraceGuard &operator=(LogTraceGuard &&) = delete;

private:
    [[no_unique_address]] const char *_tag;
    [[no_unique_address]] size_t _line;
    [[no_unique_address]] std::string_view _func_name;
    [[no_unique_address]] std::string_view _file_name;
    [[no_unique_address]] const void *_this_ptr;
};

} // namespace esp_brookesia::lib_utils

/**
 * @brief Create a scope guard that logs entry and exit for the current function.
 */
#define _BROOKESIA_LOG_CONCAT(a, b) a##b
#define BROOKESIA_LOG_CONCAT(a, b) _BROOKESIA_LOG_CONCAT(a, b)
#if (!BROOKESIA_LOG_DISABLE_DEBUG_TRACE) && (BROOKESIA_UTILS_LOG_LEVEL == BROOKESIA_UTILS_LOG_LEVEL_TRACE)
#   define BROOKESIA_LOG_TRACE_GUARD() \
        esp_brookesia::lib_utils::LogTraceGuard<true> BROOKESIA_LOG_CONCAT(_log_trace_guard_, __LINE__){}
/**
 * @brief Create a scope guard that logs entry and exit and includes `this`.
 */
#   define BROOKESIA_LOG_TRACE_GUARD_WITH_THIS() \
        esp_brookesia::lib_utils::LogTraceGuard<true> BROOKESIA_LOG_CONCAT(_log_trace_guard_, __LINE__){this}
#else
#   define BROOKESIA_LOG_TRACE_GUARD() \
        esp_brookesia::lib_utils::LogTraceGuard<false> BROOKESIA_LOG_CONCAT(_log_trace_guard_, __LINE__){}
#   define BROOKESIA_LOG_TRACE_GUARD_WITH_THIS() \
        esp_brookesia::lib_utils::LogTraceGuard<false> BROOKESIA_LOG_CONCAT(_log_trace_guard_, __LINE__){this}
#endif
