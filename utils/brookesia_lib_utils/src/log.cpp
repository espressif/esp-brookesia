/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <initializer_list>
#include <string>
#include <string_view>
#include "brookesia/lib_utils/log.hpp"

namespace esp_brookesia::lib_utils {

std::string Log::format_message(const char *format, std::initializer_list<FormatArg> args)
{
    try {
        auto fmt = boost::format(format);
        for (const auto &arg : args) {
            switch (arg.type) {
            case FormatArg::Type::Int:
                fmt % arg.i;
                break;
            case FormatArg::Type::Uint:
                fmt % arg.u;
                break;
            case FormatArg::Type::Double:
                fmt % arg.d;
                break;
            case FormatArg::Type::Str:
                fmt % (arg.s ? arg.s : arg.owned_str.c_str());
                break;
            case FormatArg::Type::Ptr:
                fmt % arg.p;
                break;
            case FormatArg::Type::Char:
                fmt % static_cast<char>(arg.i);
                break;
            }
        }
        return fmt.str();
    } catch (const std::exception &e) {
        return e.what();
    }
}

void Log::write(int level, const std::source_location &loc, const char *tag, const std::string &format_str)
{
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

    switch (level) {
    case BROOKESIA_UTILS_LOG_LEVEL_TRACE:
        BROOKESIA_LOGT_IMPL_FUNC(
            tag, _BROOKESIA_LOG_FORMAT_STRING,
            _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str)
        );
        break;
    case BROOKESIA_UTILS_LOG_LEVEL_DEBUG:
        BROOKESIA_LOGD_IMPL_FUNC(
            tag, _BROOKESIA_LOG_FORMAT_STRING,
            _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str)
        );
        break;
    case BROOKESIA_UTILS_LOG_LEVEL_INFO:
        BROOKESIA_LOGI_IMPL_FUNC(
            tag, _BROOKESIA_LOG_FORMAT_STRING,
            _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str)
        );
        break;
    case BROOKESIA_UTILS_LOG_LEVEL_WARNING:
        BROOKESIA_LOGW_IMPL_FUNC(
            tag, _BROOKESIA_LOG_FORMAT_STRING,
            _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str)
        );
        break;
    case BROOKESIA_UTILS_LOG_LEVEL_ERROR:
        BROOKESIA_LOGE_IMPL_FUNC(
            tag, _BROOKESIA_LOG_FORMAT_STRING,
            _BROOKESIA_LOG_ARGS(thread_name, file_name, line, func_name, format_str)
        );
        break;
    default:
        break;
    }
}

std::string_view Log::extract_function_name(const char *func_name)
{
    if (!func_name) {
        return "???";
    }

    std::string_view sig(func_name);

    // First check if it contains lambda (e.g. "test_func_23()::<lambda()>")
    // For lambda, truncate before "::<lambda"
    size_t lambda_pos = sig.find("::<lambda");
    if (lambda_pos != std::string_view::npos) {
        sig = sig.substr(0, lambda_pos);
        // Now sig is "test_func_23()" or "void test_func_23()"
    }

    // Extract the part before parameter list
    size_t paren_pos = sig.find('(');
    std::string_view before_paren;
    if (paren_pos != std::string_view::npos) {
        before_paren = sig.substr(0, paren_pos);
    } else {
        before_paren = sig;
    }

    // Find the last "::" (class/namespace separator)
    size_t last_colon = before_paren.rfind("::");
    if (last_colon != std::string_view::npos) {
        return before_paren.substr(last_colon + 2);
    }

    // No "::", find the last space (separator between return type and function name)
    size_t last_space = before_paren.rfind(' ');
    if (last_space != std::string_view::npos) {
        return before_paren.substr(last_space + 1);
    }

    // No space and "::", return the entire part
    return before_paren;
}

std::string_view Log::extract_file_name(const char *file_path)
{
    if (!file_path) {
        return "???";
    }

    // Find the last path separator (Unix: '/', Windows: '\')
    const char *filename = strrchr(file_path, '/');
    if (!filename) {
        filename = strrchr(file_path, '\\');
    }

    return filename ? std::string_view(filename + 1) : std::string_view(file_path);
}

} // namespace esp_brookesia::lib_utils
