/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
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

namespace esp_brookesia::lib_utils {

/**
 * Helper function to convert int8_t/uint8_t to int for proper formatting
 * This prevents boost::format from treating them as characters
 */
template<typename T>
constexpr auto format_arg(T &&arg)
{
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, int8_t>) {
        return static_cast<int>(arg);
    } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, uint8_t>) {
        return static_cast<unsigned int>(arg);
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
            auto line = static_cast<int>(loc.line());
            auto func_name = extract_function_name(loc.function_name());
            auto file_name = extract_file_name(loc.file_name());
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

            // Direct perfect forwarding to underlying implementation
            if constexpr (level == BROOKESIA_UTILS_LOG_LEVEL_TRACE) {
                BROOKESIA_LOGT_IMPL_FUNC(
                    tag, "[%.*s:%04d](%.*s): %s",
                    (int)file_name.size(), file_name.data(),
                    line,
                    (int)func_name.size(), func_name.data(),
                    format_str.data()
                );
            } else if constexpr (level == BROOKESIA_UTILS_LOG_LEVEL_DEBUG) {
                BROOKESIA_LOGD_IMPL_FUNC(
                    tag, "[%.*s:%04d](%.*s): %s",
                    (int)file_name.size(), file_name.data(),
                    line,
                    (int)func_name.size(), func_name.data(),
                    format_str.data()
                );
            } else if constexpr (level == BROOKESIA_UTILS_LOG_LEVEL_INFO) {
                BROOKESIA_LOGI_IMPL_FUNC(
                    tag, "[%.*s:%04d](%.*s): %s",
                    (int)file_name.size(), file_name.data(),
                    line,
                    (int)func_name.size(), func_name.data(),
                    format_str.data()
                );
            } else if constexpr (level == BROOKESIA_UTILS_LOG_LEVEL_WARNING) {
                BROOKESIA_LOGW_IMPL_FUNC(
                    tag, "[%.*s:%04d](%.*s): %s",
                    (int)file_name.size(), file_name.data(),
                    line,
                    (int)func_name.size(), func_name.data(),
                    format_str.data()
                );
            } else if constexpr (level == BROOKESIA_UTILS_LOG_LEVEL_ERROR) {
                BROOKESIA_LOGE_IMPL_FUNC(
                    tag, "[%.*s:%04d](%.*s): %s",
                    (int)file_name.size(), file_name.data(),
                    line,
                    (int)func_name.size(), func_name.data(),
                    format_str.data()
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
            _line = static_cast<size_t>(loc.line());
            _func_name = Log::extract_function_name(loc.function_name());
            _file_name = Log::extract_file_name(loc.file_name());
            _this_ptr = this_ptr;

            if (_this_ptr) {
                BROOKESIA_LOGT_IMPL_FUNC(
                    _tag, "[%.*s:%04d](%.*s): (@%p) Enter", (int)_file_name.size(), _file_name.data(),
                    _line, (int)_func_name.size(), _func_name.data(), _this_ptr
                );
            } else {
                BROOKESIA_LOGT_IMPL_FUNC(
                    _tag, "[%.*s:%04d](%.*s): Enter", (int)_file_name.size(), _file_name.data(),
                    _line, (int)_func_name.size(), _func_name.data()
                );
            }
        } else {
            (void)this_ptr;
            (void)loc;
            (void)tag;
        }
    }

    ~LogTraceGuard()
    {
        if constexpr (Enabled) {
            if (_this_ptr) {
                BROOKESIA_LOGT_IMPL_FUNC(
                    _tag, "[%.*s:%04d](%.*s): (@%p) Exit", (int)_file_name.size(), _file_name.data(),
                    _line, (int)_func_name.size(), _func_name.data(), _this_ptr
                );
            } else {
                BROOKESIA_LOGT_IMPL_FUNC(
                    _tag, "[%.*s:%04d](%.*s): Exit", (int)_file_name.size(), _file_name.data(), _line,
                    (int)_func_name.size(), _func_name.data()
                );
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
#if (!BROOKESIA_LOG_DISABLE_DEBUG_TRACE)
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
