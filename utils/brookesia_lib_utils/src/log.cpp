/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string>
#include <string_view>
#include "brookesia/lib_utils/log.hpp"

namespace esp_brookesia::lib_utils {

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
