/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

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
#include "macro_configs.h"
#if defined(ESP_PLATFORM) && (BROOKESIA_UTILS_LOG_IMPL_TYPE == BROOKESIA_UTILS_LOG_IMPL_ESP)
#   include "log_impl_esp.h"
#else
#   include "log_impl_std.h"
#endif
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
 * Helper function to convert int8_t/uint8_t to int for proper formatting
 * This prevents boost::format from treating them as characters
 * Also converts pointers (except char*) to @0x... format
 */
template<typename T>
constexpr auto format_arg(T &&arg)
{
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, int8_t>) {
        return static_cast<int>(arg);
    } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, uint8_t>) {
        return static_cast<unsigned int>(arg);
    } else if constexpr (std::is_pointer_v<std::remove_cvref_t<T>> &&
                         !std::is_same_v<std::remove_cvref_t<T>, const char *> &&
                         !std::is_same_v<std::remove_cvref_t<T>, char *>) {
        return reinterpret_cast<const void *>(arg);
    } else {
        return std::forward<T>(arg);
    }
}

/**
 * Class to handle logging
 */
class Log {
public:
    Log(const Log &) = delete;
    Log(Log &&) = delete;
    Log &operator=(const Log &) = delete;
    Log &operator=(Log &&) = delete;

    // Modern C++ implementation using perfect forwarding + std::string
    template <int level, typename... Args>
    void print(const std::source_location &loc, const char *tag, const char *format, Args &&... args)
    {
        // Logs below the global level will not be compiled
        if constexpr (level >= BROOKESIA_UTILS_LOG_LEVEL) {
#if BROOKESIA_UTILS_LOG_ENABLE_THREAD_NAME
            auto thread_config = ThreadConfig::get_current_config();
            auto thread_name = thread_config.name.c_str();
#endif
#if BROOKESIA_UTILS_LOG_ENABLE_FILE_AND_LINE
            auto file_name = extract_file_name(loc.file_name());
            auto line = static_cast<int>(loc.line());
#endif
#if BROOKESIA_UTILS_LOG_ENABLE_FUNCTION_NAME
            auto func_name = extract_function_name(loc.function_name());
#endif
            std::string format_str;

            try {
                // 1. Create boost::format object, disable exceptions
                auto fmt = boost::format(format);
                // 2. Apply parameters one by one using fold expression, converting int8_t/uint8_t to int
                ((fmt % format_arg(std::forward<Args>(args))), ...);
                // 3. Get formatted string
                format_str = fmt.str();
            } catch (const std::exception &e) {
                format_str = std::string(e.what());
            }

            // Direct perfect forwarding to underlying implementation using macros
            if constexpr (level == BROOKESIA_UTILS_LOG_LEVEL_TRACE) {
                BROOKESIA_LOGT_IMPL_FUNC(
                    tag,
                    _BROOKESIA_LOG_FORMAT_STRING,
                    _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str)
                );
            } else if constexpr (level == BROOKESIA_UTILS_LOG_LEVEL_DEBUG) {
                BROOKESIA_LOGD_IMPL_FUNC(
                    tag,
                    _BROOKESIA_LOG_FORMAT_STRING,
                    _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str)
                );
            } else if constexpr (level == BROOKESIA_UTILS_LOG_LEVEL_INFO) {
                BROOKESIA_LOGI_IMPL_FUNC(
                    tag,
                    _BROOKESIA_LOG_FORMAT_STRING,
                    _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str)
                );
            } else if constexpr (level == BROOKESIA_UTILS_LOG_LEVEL_WARNING) {
                BROOKESIA_LOGW_IMPL_FUNC(
                    tag,
                    _BROOKESIA_LOG_FORMAT_STRING,
                    _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str)
                );
            } else if constexpr (level == BROOKESIA_UTILS_LOG_LEVEL_ERROR) {
                BROOKESIA_LOGE_IMPL_FUNC(
                    tag,
                    _BROOKESIA_LOG_FORMAT_STRING,
                    _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str)
                );
            }
        }
    }

    // Singleton pattern: Get the unique instance of the class
    static Log &getInstance()
    {
        static Log instance;
        return instance;
    }

    static std::string_view extract_function_name(const char *func_name);
    static std::string_view extract_file_name(const char *file_path);

private:
    Log() = default;
};

/**
 * Macros to simplify logging calls with a fixed tag
 */
#define BROOKESIA_LOGT_IMPL(tag, format, ...) \
    esp_brookesia::lib_utils::Log::getInstance().print<BROOKESIA_UTILS_LOG_LEVEL_TRACE>(    \
        std::source_location::current(), tag, format, ##__VA_ARGS__ \
    )
#define BROOKESIA_LOGD_IMPL(tag, format, ...) \
    esp_brookesia::lib_utils::Log::getInstance().print<BROOKESIA_UTILS_LOG_LEVEL_DEBUG>(    \
        std::source_location::current(), tag, format, ##__VA_ARGS__ \
    )
#define BROOKESIA_LOGI_IMPL(tag, format, ...) \
    esp_brookesia::lib_utils::Log::getInstance().print<BROOKESIA_UTILS_LOG_LEVEL_INFO>(     \
        std::source_location::current(), tag, format, ##__VA_ARGS__ \
    )
#define BROOKESIA_LOGW_IMPL(tag, format, ...) \
    esp_brookesia::lib_utils::Log::getInstance().print<BROOKESIA_UTILS_LOG_LEVEL_WARNING>(  \
        std::source_location::current(), tag, format, ##__VA_ARGS__ \
    )
#define BROOKESIA_LOGE_IMPL(tag, format, ...) \
    esp_brookesia::lib_utils::Log::getInstance().print<BROOKESIA_UTILS_LOG_LEVEL_ERROR>(    \
        std::source_location::current(), tag, format, ##__VA_ARGS__ \
    )

/**
 * Determine the tag to use for logging
 */
#if defined(BROOKESIA_LOG_TAG)
#   define BROOKESIA_LOG_TAG_TO_USE BROOKESIA_LOG_TAG
#else
#   define BROOKESIA_LOG_TAG_TO_USE BROOKESIA_UTILS_LOG_TAG
#endif

/**
 * Per-file control of trace level logging and trace guard
 * Users can define this macro before including this header to override the default behavior
 * Example usage in source file:
 *   #define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1  // Disable all debug & trace features for this file
 *   #include "brookesia/lib_utils/log.hpp"
 */
#ifndef BROOKESIA_LOG_DISABLE_DEBUG_TRACE
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 0
#endif

/**
 * Compile-time log level filtering macros
 * These macros will be completely optimized out if the log level is below the global level
 */
#if (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_TRACE) && \
    (!BROOKESIA_LOG_DISABLE_DEBUG_TRACE)
#   define BROOKESIA_LOGT(format, ...) BROOKESIA_LOGT_IMPL(BROOKESIA_LOG_TAG_TO_USE, format, ##__VA_ARGS__)
#else
#   define BROOKESIA_LOGT(format, ...) ((void)0)
#endif

#if (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG) && \
    (!BROOKESIA_LOG_DISABLE_DEBUG_TRACE)
#   define BROOKESIA_LOGD(format, ...) BROOKESIA_LOGD_IMPL(BROOKESIA_LOG_TAG_TO_USE, format, ##__VA_ARGS__)
#else
#   define BROOKESIA_LOGD(format, ...) ((void)0)
#endif

#if (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_INFO)
#   define BROOKESIA_LOGI(format, ...) BROOKESIA_LOGI_IMPL(BROOKESIA_LOG_TAG_TO_USE, format, ##__VA_ARGS__)
#else
#   define BROOKESIA_LOGI(format, ...) ((void)0)
#endif

#if (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_WARNING)
#   define BROOKESIA_LOGW(format, ...) BROOKESIA_LOGW_IMPL(BROOKESIA_LOG_TAG_TO_USE, format, ##__VA_ARGS__)
#else
#   define BROOKESIA_LOGW(format, ...) ((void)0)
#endif

#if (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_ERROR)
#   define BROOKESIA_LOGE(format, ...) BROOKESIA_LOGE_IMPL(BROOKESIA_LOG_TAG_TO_USE, format, ##__VA_ARGS__)
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

// Log trace RAII class
template<bool Enabled>
class LogTraceGuard {
public:
    LogTraceGuard(
        const void *this_ptr = nullptr, const std::source_location &loc = std::source_location::current(),
        const char *tag = BROOKESIA_LOG_TAG_TO_USE
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

// Log trace guard macros - controlled by BROOKESIA_LOG_DISABLE_DEBUG_TRACE
#define _BROOKESIA_LOG_CONCAT(a, b) a##b
#define BROOKESIA_LOG_CONCAT(a, b) _BROOKESIA_LOG_CONCAT(a, b)
#if (!BROOKESIA_LOG_DISABLE_DEBUG_TRACE) && (BROOKESIA_UTILS_LOG_LEVEL == BROOKESIA_UTILS_LOG_LEVEL_TRACE)
#   define BROOKESIA_LOG_TRACE_GUARD() \
        esp_brookesia::lib_utils::LogTraceGuard<true> BROOKESIA_LOG_CONCAT(_log_trace_guard_, __LINE__){}
#   define BROOKESIA_LOG_TRACE_GUARD_WITH_THIS() \
        esp_brookesia::lib_utils::LogTraceGuard<true> BROOKESIA_LOG_CONCAT(_log_trace_guard_, __LINE__){this}
#else
#   define BROOKESIA_LOG_TRACE_GUARD() \
        esp_brookesia::lib_utils::LogTraceGuard<false> BROOKESIA_LOG_CONCAT(_log_trace_guard_, __LINE__){}
#   define BROOKESIA_LOG_TRACE_GUARD_WITH_THIS() \
        esp_brookesia::lib_utils::LogTraceGuard<false> BROOKESIA_LOG_CONCAT(_log_trace_guard_, __LINE__){this}
#endif
