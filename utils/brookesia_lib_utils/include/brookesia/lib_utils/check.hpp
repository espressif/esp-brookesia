/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#if defined(ESP_PLATFORM)
#   include "esp_err.h"
#endif
#include "brookesia/lib_utils/macro_configs.h"
#include "brookesia/lib_utils/log.hpp"

#ifndef unlikely
#   define unlikely(x)  (x)
#endif

#if BROOKESIA_UTILS_CHECK_HANDLE_METHOD == BROOKESIA_UTILS_CHECK_HANDLE_WITH_NONE

/**
 * @brief Check if the pointer is NULL; if NULL, log an error and execute the specified code block.
 *
 * @param ptr Pointer to check
 * @param process_code Code block to execute if the pointer is NULL
 * @param ... Additional arguments which used to keep compatible with the previous macros, they will be ignored
 */
#define BROOKESIA_CHECK_NULL_EXECUTE(ptr, process_code, ...) do { \
        if (unlikely(ptr == nullptr)) { \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Check if the value is false; if false, execute the specified code block.
 *
 * @param value Value to check
 * @param process_code Code block to execute if the expression is false
 * @param ... Additional arguments which used to keep compatible with the previous macros, they will be ignored
 */
#define BROOKESIA_CHECK_FALSE_EXECUTE(value, process_code, ...) do { \
        if (unlikely((value) == false)) { \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Check if the expression throws an exception; if it does, execute the specified code block.
 *
 * @param expression Expression to check
 * @param process_code Code block to execute if the expression throws an exception
 * @param ... Additional arguments which used to keep compatible with the previous macros, they will be ignored
 */
#define BROOKESIA_CHECK_EXCEPTION_EXECUTE(expression, process_code, ...) do { \
        try { \
            expression; \
        } catch (const std::exception &e) { \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Check if the ESP result is not `ESP_OK`; if not, execute the specified code block.
 *
 * @param esp_result ESP result to check
 * @param process_code Code block to execute if the ESP result is not `ESP_OK`
 * @param ... Additional arguments which used to keep compatible with the previous macros, they will be ignored
 */
#define BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_result, process_code, ...) do { \
        if (unlikely((esp_result) != ESP_OK)) { \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Check if the value is within the range [min, max]; if not, execute the specified code block.
 *
 * @param value Value to check
 * @param min Minimum value
 * @param max Maximum value
 * @param process_code Code block to execute if the value is out of range
 * @param ... Additional arguments which used to keep compatible with the previous macros, they will be ignored
 */
#define BROOKESIA_CHECK_OUT_RANGE_EXECUTE(value, min, max, process_code, ...) do { \
        auto _value_ = (value); \
        if (unlikely((_value_) < (min) || (_value_) > (max))) { \
            { process_code } \
        } \
    } while(0)

#elif BROOKESIA_UTILS_CHECK_HANDLE_METHOD == BROOKESIA_UTILS_CHECK_HANDLE_WITH_ERROR_LOG

/**
 * @brief Check if the pointer is NULL; if NULL, log an error and execute the specified code block.
 *
 * @param ptr Pointer to check
 * @param process_code Code block to execute if the pointer is NULL
 * @param ... Additional code to execute before the process_code
 */
#define BROOKESIA_CHECK_NULL_EXECUTE(ptr, process_code, ...) do { \
        if (unlikely(ptr == nullptr)) { \
            BROOKESIA_LOGE("Checked null: {%1%}", #ptr); \
            { __VA_ARGS__ } \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Check if the value is false; if false, log an error and execute the specified code block.
 *
 * @param value Value to check
 * @param process_code Code block to execute if the value is false
 * @param ... Additional code to execute before the process_code
 */
#define BROOKESIA_CHECK_FALSE_EXECUTE(value, process_code, ...) do { \
        if (unlikely((value) == false)) { \
            BROOKESIA_LOGE("Checked false: {%1%}", #value); \
            { __VA_ARGS__ } \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Check if the expression throws an exception; if it does, log an error and execute the specified code block.
 *
 * @param expression Expression to check
 * @param process_code Code block to execute if the expression throws an exception
 * @param ... Additional code to execute before the process_code
 */
#define BROOKESIA_CHECK_EXCEPTION_EXECUTE(expression, process_code, ...) do { \
        try { \
            expression; \
        } catch (const std::exception &e) { \
            BROOKESIA_LOGE("Checked exception: {(%1%) throws: (%2%)}", #expression, e.what()); \
            { __VA_ARGS__ } \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Check if the ESP result is not `ESP_OK`; if not, log an error and execute the specified code block.
 *
 * @param esp_result ESP result to check
 * @param process_code Code block to execute if the ESP result is not `ESP_OK`
 * @param ... Additional code to execute before the process_code
 */
#define BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_result, process_code, ...) do { \
        auto _esp_result_ = (esp_result); \
        if (unlikely(_esp_result_ != ESP_OK)) { \
            BROOKESIA_LOGE( \
                "Checked ESP error: {(%1%) == (%2%)(%3%)}", #esp_result, esp_err_to_name(_esp_result_), _esp_result_ \
            ); \
            { __VA_ARGS__ } \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Check if the value is within the range [min, max]; if not, execute the specified code block.
 *
 * @param value Value to check
 * @param min Minimum value
 * @param max Maximum value
 * @param process_code Code block to execute if the value is out of range
 * @param ... Additional code to execute before the process_code
 */
#define BROOKESIA_CHECK_OUT_RANGE_EXECUTE(value, min, max, process_code, ...) do { \
        auto _value_ = (value); \
        auto _min_ = (min); \
        auto _max_ = (max); \
        if (unlikely(_value_ < _min_ || _value_ > _max_)) { \
            BROOKESIA_LOGE("Checked out of range: {(%1%)(%2%) ∉ [%3%, %4%]}", #value, _value_, _min_, _max_); \
            { __VA_ARGS__ } \
            { process_code } \
        } \
    } while(0)

#elif BROOKESIA_UTILS_CHECK_HANDLE_METHOD == BROOKESIA_UTILS_CHECK_HANDLE_WITH_ASSERT

/**
 * @brief Check if the pointer is NULL; if NULL, assert.
 *
 * @param ptr Pointer to check
 * @param ... Additional arguments which used to keep compatible with the previous macros, they will be ignored
 */
#define BROOKESIA_CHECK_NULL_EXECUTE(ptr, ...) do { \
        if (unlikely(ptr == nullptr)) { \
            assert(false); \
        } \
    } while(0)

/**
 * @brief Check if the value is false; if false, assert.
 *
 * @param value Value to check
 * @param ... Additional arguments which used to keep compatible with the previous macros, they will be ignored
 */
#define BROOKESIA_CHECK_FALSE_EXECUTE(value, ...) do { \
        if (unlikely((value) == false)) { \
            assert(false); \
        } \
    } while(0)

/**
 * @brief Check if the expression throws an exception; if it does, assert.
 *
 * @param expression Expression to check
 * @param ... Additional arguments which used to keep compatible with the previous macros, they will be ignored
 */
#define BROOKESIA_CHECK_EXCEPTION_EXECUTE(expression, ...) do { \
        try {  \
            expression;  \
        } catch (const std::exception &e) {  \
            assert(false);  \
        }  \
    } while(0)  \

/**
 * @brief Check if the ESP result is not `ESP_OK`; if not, assert.
 *
 * @param esp_result ESP result to check
 * @param ... Additional arguments which used to keep compatible with the previous macros, they will be ignored
 */
#define BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_result, ...) do { \
        if (unlikely((esp_result) != ESP_OK)) { \
            assert(false); \
        } \
    } while(0)

/**
 * @brief Check if the value is within the range [min, max]; if not, assert.
 *
 * @param value Value to check
 * @param min Minimum value
 * @param max Maximum value
 * @param ... Additional arguments which used to keep compatible with the previous macros, they will be ignored
 */
#define BROOKESIA_CHECK_OUT_RANGE_EXECUTE(value, min, max, ...) do { \
        auto _value_ = (value); \
        if (unlikely(_value_ < (min) || _value_ > (max))) { \
            assert(false); \
        } \
    } while(0)

#endif // BROOKESIA_UTILS_CHECK_HANDLE_METHOD

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Check NULL /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Check if the pointer is NULL; if NULL, log an error and return the specified value.
 *
 * @param x Pointer to check
 * @param ret Value to return if the pointer is NULL
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_NULL_RETURN(value, ret, fmt, ...) \
    BROOKESIA_CHECK_NULL_EXECUTE(value, { return ret;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Check if the pointer is NULL; if NULL, return without a value.
 *
 * @param value Value to check
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_NULL_EXIT(value, fmt, ...) \
    BROOKESIA_CHECK_NULL_EXECUTE(value, {return;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Check if the pointer is NULL; if NULL, goto the specified label.
 *
 * @param value Value to check
 * @param goto_tag Label to jump to if the pointer is NULL
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_NULL_GOTO(value, goto_tag, fmt, ...) \
    BROOKESIA_CHECK_NULL_EXECUTE(value, {goto goto_tag;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Check False /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Check if the value is false; if false, return the specified value.
 *
 * @param value Value to check
 * @param ret Value to return if the value is false
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_FALSE_RETURN(value, ret, fmt, ...) \
    BROOKESIA_CHECK_FALSE_EXECUTE(value, {return ret;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Check if the value is false; if false, return without a value.
 *
 * @param value Value to check
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_FALSE_EXIT(value, fmt, ...) \
    BROOKESIA_CHECK_FALSE_EXECUTE(value, {return;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Check if the value is false; if false, goto the specified label.
 *
 * @param value Value to check
 * @param goto_tag Label to jump to if the value is false
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_FALSE_GOTO(value, goto_tag, fmt, ...) \
    BROOKESIA_CHECK_FALSE_EXECUTE(value, {goto goto_tag;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Check Error /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Check if the value is not `ESP_OK`; if not, log an error and return the specified value.
 *
 * @param x Value to check
 * @param ret Value to return if the value is not `ESP_OK`
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_ESP_ERR_RETURN(value, ret, fmt, ...) \
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(value, {return ret;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Check if the value is not `ESP_OK`; if not, log an error and return without a value.
 *
 * @param value Value to check
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_ESP_ERR_EXIT(value, fmt, ...) \
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(value, {return;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Check if the value is not `ESP_OK`; if not, log an error and goto the specified label.
 *
 * @param value Value to check
 * @param goto_tag Label to jump to if the value is not `ESP_OK`
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_ESP_ERR_GOTO(value, goto_tag, fmt, ...) \
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(value, {goto goto_tag;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Check Exception /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Check if the expression throws an exception; if it does, log an error and execute the specified code block.
 *
 * @param expression Expression to check
 * @param ret Value to return if the expression throws an exception
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_EXCEPTION_RETURN(expression, ret, fmt, ...) \
    BROOKESIA_CHECK_EXCEPTION_EXECUTE(expression, {return ret;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Check if the expression throws an exception; if it does, log an error and return without a value.
 *
 * @param expression Expression to check
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_EXCEPTION_EXIT(expression, fmt, ...) \
    BROOKESIA_CHECK_EXCEPTION_EXECUTE(expression, {return;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Check if the expression throws an exception; if it does, log an error and execute the specified code block.
 *
 * @param expression Expression to check
 * @param goto_tag Label to jump to if the expression throws an exception
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_EXCEPTION_GOTO(expression, goto_tag, fmt, ...) \
    BROOKESIA_CHECK_EXCEPTION_EXECUTE(expression, {goto goto_tag;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Check Range /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Check if the value is within the range [min, max]; if within, return the specified value.
 *
 * @param value Value to check
 * @param min Minimum value
 * @param max Maximum value
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_OUT_RANGE(value, min, max, fmt, ...) \
    BROOKESIA_CHECK_OUT_RANGE_EXECUTE(value, min, max, {}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Check if the value is within the range [min, max]; if within, return the specified value.
 *
 * @param value Value to check
 * @param min Minimum value
 * @param max Maximum value
 * @param ret Value to return if the value is within the range
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_OUT_RANGE_RETURN(value, min, max, ret, fmt, ...) \
    BROOKESIA_CHECK_OUT_RANGE_EXECUTE(value, min, max, {return ret;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Check if the value is within the range [min, max]; if within, return without a value.
 *
 * @param value Value to check
 * @param min Minimum value
 * @param max Maximum value
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_OUT_RANGE_EXIT(value, min, max, fmt, ...) \
    BROOKESIA_CHECK_OUT_RANGE_EXECUTE(value, min, max, {return;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Check if the value is within the range [min, max]; if within, goto the specified label.
 *
 * @param value Value to check
 * @param min Minimum value
 * @param max Maximum value
 * @param goto_tag Label to jump to if the value is within the range
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define BROOKESIA_CHECK_OUT_RANGE_GOTO(value, min, max, goto_tag, fmt, ...) \
    BROOKESIA_CHECK_OUT_RANGE_EXECUTE(value, min, max, {goto goto_tag;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})
