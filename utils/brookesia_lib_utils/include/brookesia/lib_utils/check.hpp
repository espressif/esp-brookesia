/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
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
 * @brief Execute a fallback code block when a pointer is null.
 *
 * @param ptr Pointer expression to test.
 * @param process_code Code block executed when @p ptr is `nullptr`.
 * @param ... Compatibility arguments kept for legacy call sites and ignored in this mode.
 */
#define BROOKESIA_CHECK_NULL_EXECUTE(ptr, process_code, ...) do { \
        if (unlikely(ptr == nullptr)) { \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Execute a fallback code block when an expression evaluates to `false`.
 *
 * @param value Expression to test.
 * @param process_code Code block executed when @p value converts to `false`.
 * @param ... Compatibility arguments kept for legacy call sites and ignored in this mode.
 */
#define BROOKESIA_CHECK_FALSE_EXECUTE(value, process_code, ...) do { \
        if (unlikely(static_cast<bool>(value) == false)) { \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Execute a fallback code block when an expression throws `std::exception`.
 *
 * @param expression Expression to evaluate inside a `try` block.
 * @param process_code Code block executed when @p expression throws `std::exception`.
 * @param ... Compatibility arguments kept for legacy call sites and ignored in this mode.
 */
#define BROOKESIA_CHECK_EXCEPTION_EXECUTE(expression, process_code, ...) do { \
        try { \
            expression; \
        } catch (const std::exception &e) { \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Execute a fallback code block when an ESP-IDF error code is not `ESP_OK`.
 *
 * @param esp_result Expression that yields an ESP-IDF error code.
 * @param process_code Code block executed when @p esp_result is not `ESP_OK`.
 * @param ... Compatibility arguments kept for legacy call sites and ignored in this mode.
 */
#define BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_result, process_code, ...) do { \
        if (unlikely((esp_result) != ESP_OK)) { \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Execute a fallback code block when a value is outside the inclusive range [`min`, `max`].
 *
 * @param value Expression to test.
 * @param min Inclusive lower bound.
 * @param max Inclusive upper bound.
 * @param process_code Code block executed when @p value is outside the range.
 * @param ... Compatibility arguments kept for legacy call sites and ignored in this mode.
 */
#define BROOKESIA_CHECK_OUT_RANGE_EXECUTE(value, min, max, process_code, ...) do { \
        auto _value_ = (value); \
        if (unlikely((_value_) < (min) || (_value_) > (max))) { \
            { process_code } \
        } \
    } while(0)

#elif BROOKESIA_UTILS_CHECK_HANDLE_METHOD == BROOKESIA_UTILS_CHECK_HANDLE_WITH_ERROR_LOG

/**
 * @brief Log a null-pointer check failure and execute a fallback code block.
 *
 * @param ptr Pointer expression to test.
 * @param process_code Code block executed when @p ptr is `nullptr`.
 * @param ... Optional code block executed after the built-in log and before @p process_code.
 */
#define BROOKESIA_CHECK_NULL_EXECUTE(ptr, process_code, ...) do { \
        if (unlikely(ptr == nullptr)) { \
            { __VA_ARGS__ } \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Log a failed boolean check and execute a fallback code block.
 *
 * @param value Expression to test.
 * @param process_code Code block executed when @p value converts to `false`.
 * @param ... Optional code block executed after the built-in log and before @p process_code.
 */
#define BROOKESIA_CHECK_FALSE_EXECUTE(value, process_code, ...) do { \
        if (unlikely(static_cast<bool>(value) == false)) { \
            { __VA_ARGS__ } \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Log an exception check failure and execute a fallback code block.
 *
 * @param expression Expression to evaluate inside a `try` block.
 * @param process_code Code block executed when @p expression throws `std::exception`.
 * @param ... Optional code block executed after the built-in log and before @p process_code.
 */
#define BROOKESIA_CHECK_EXCEPTION_EXECUTE(expression, process_code, ...) do { \
        try { \
            expression; \
        } catch (const std::exception &e) { \
            BROOKESIA_LOGE("Checked exception: %1%", e.what()); \
            { __VA_ARGS__ } \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Log an ESP-IDF error check failure and execute a fallback code block.
 *
 * @param esp_result Expression that yields an ESP-IDF error code.
 * @param process_code Code block executed when @p esp_result is not `ESP_OK`.
 * @param ... Optional code block executed after the built-in log and before @p process_code.
 */
#define BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_result, process_code, ...) do { \
        auto _esp_result_ = (esp_result); \
        if (unlikely(_esp_result_ != ESP_OK)) { \
            BROOKESIA_LOGE( \
                "Checked ESP error: %1%(%2%)", esp_err_to_name(_esp_result_), _esp_result_ \
            ); \
            { __VA_ARGS__ } \
            { process_code } \
        } \
    } while(0)

/**
 * @brief Log an out-of-range check failure and execute a fallback code block.
 *
 * @param value Expression to test.
 * @param min Inclusive lower bound.
 * @param max Inclusive upper bound.
 * @param process_code Code block executed when @p value is outside the range.
 * @param ... Optional code block executed after the built-in log and before @p process_code.
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
 * @brief Assert when a pointer is null.
 *
 * @param ptr Pointer expression to test.
 * @param ... Compatibility arguments kept for legacy call sites and ignored in this mode.
 */
#define BROOKESIA_CHECK_NULL_EXECUTE(ptr, ...) do { \
        if (unlikely(ptr == nullptr)) { \
            assert(false); \
        } \
    } while(0)

/**
 * @brief Assert when an expression evaluates to `false`.
 *
 * @param value Expression to test.
 * @param ... Compatibility arguments kept for legacy call sites and ignored in this mode.
 */
#define BROOKESIA_CHECK_FALSE_EXECUTE(value, ...) do { \
        if (unlikely(static_cast<bool>(value) == false)) { \
            assert(false); \
        } \
    } while(0)

/**
 * @brief Assert when an expression throws `std::exception`.
 *
 * @param expression Expression to evaluate inside a `try` block.
 * @param ... Compatibility arguments kept for legacy call sites and ignored in this mode.
 */
#define BROOKESIA_CHECK_EXCEPTION_EXECUTE(expression, ...) do { \
        try {  \
            expression;  \
        } catch (const std::exception &e) {  \
            assert(false);  \
        }  \
    } while(0)  \

/**
 * @brief Assert when an ESP-IDF error code is not `ESP_OK`.
 *
 * @param esp_result Expression that yields an ESP-IDF error code.
 * @param ... Compatibility arguments kept for legacy call sites and ignored in this mode.
 */
#define BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_result, ...) do { \
        if (unlikely((esp_result) != ESP_OK)) { \
            assert(false); \
        } \
    } while(0)

/**
 * @brief Assert when a value is outside the inclusive range [`min`, `max`].
 *
 * @param value Expression to test.
 * @param min Inclusive lower bound.
 * @param max Inclusive upper bound.
 * @param ... Compatibility arguments kept for legacy call sites and ignored in this mode.
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
 * @brief Return a value when a pointer is null.
 *
 * @param value Pointer expression to test.
 * @param ret Value returned when @p value is `nullptr`.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_NULL_RETURN(value, ret, fmt, ...) \
    BROOKESIA_CHECK_NULL_EXECUTE(value, { return ret;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Return from the current `void` function when a pointer is null.
 *
 * @param value Pointer expression to test.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_NULL_EXIT(value, fmt, ...) \
    BROOKESIA_CHECK_NULL_EXECUTE(value, {return;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Jump to a label when a pointer is null.
 *
 * @param value Pointer expression to test.
 * @param goto_tag Label jumped to when @p value is `nullptr`.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_NULL_GOTO(value, goto_tag, fmt, ...) \
    BROOKESIA_CHECK_NULL_EXECUTE(value, {goto goto_tag;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Check False /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Return a value when an expression evaluates to `false`.
 *
 * @param value Expression to test.
 * @param ret Value returned when @p value converts to `false`.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_FALSE_RETURN(value, ret, fmt, ...) \
    BROOKESIA_CHECK_FALSE_EXECUTE(value, {return ret;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Return from the current `void` function when an expression evaluates to `false`.
 *
 * @param value Expression to test.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_FALSE_EXIT(value, fmt, ...) \
    BROOKESIA_CHECK_FALSE_EXECUTE(value, {return;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Jump to a label when an expression evaluates to `false`.
 *
 * @param value Expression to test.
 * @param goto_tag Label jumped to when @p value converts to `false`.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_FALSE_GOTO(value, goto_tag, fmt, ...) \
    BROOKESIA_CHECK_FALSE_EXECUTE(value, {goto goto_tag;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Check Error /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Return a value when an ESP-IDF error code is not `ESP_OK`.
 *
 * @param value Expression that yields an ESP-IDF error code.
 * @param ret Value returned when @p value is not `ESP_OK`.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_ESP_ERR_RETURN(value, ret, fmt, ...) \
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(value, {return ret;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Return from the current `void` function when an ESP-IDF error code is not `ESP_OK`.
 *
 * @param value Expression that yields an ESP-IDF error code.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_ESP_ERR_EXIT(value, fmt, ...) \
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(value, {return;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Jump to a label when an ESP-IDF error code is not `ESP_OK`.
 *
 * @param value Expression that yields an ESP-IDF error code.
 * @param goto_tag Label jumped to when @p value is not `ESP_OK`.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_ESP_ERR_GOTO(value, goto_tag, fmt, ...) \
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(value, {goto goto_tag;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Check Exception /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Return a value when an expression throws `std::exception`.
 *
 * @param expression Expression to evaluate inside a `try` block.
 * @param ret Value returned when @p expression throws `std::exception`.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_EXCEPTION_RETURN(expression, ret, fmt, ...) \
    BROOKESIA_CHECK_EXCEPTION_EXECUTE(expression, {return ret;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Return from the current `void` function when an expression throws `std::exception`.
 *
 * @param expression Expression to evaluate inside a `try` block.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_EXCEPTION_EXIT(expression, fmt, ...) \
    BROOKESIA_CHECK_EXCEPTION_EXECUTE(expression, {return;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Jump to a label when an expression throws `std::exception`.
 *
 * @param expression Expression to evaluate inside a `try` block.
 * @param goto_tag Label jumped to when @p expression throws `std::exception`.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_EXCEPTION_GOTO(expression, goto_tag, fmt, ...) \
    BROOKESIA_CHECK_EXCEPTION_EXECUTE(expression, {goto goto_tag;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Check Range /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Log when a value is outside the inclusive range [`min`, `max`].
 *
 * @param value Expression to test.
 * @param min Inclusive lower bound.
 * @param max Inclusive upper bound.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_OUT_RANGE(value, min, max, fmt, ...) \
    BROOKESIA_CHECK_OUT_RANGE_EXECUTE(value, min, max, {}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Return a value when a value is outside the inclusive range [`min`, `max`].
 *
 * @param value Expression to test.
 * @param min Inclusive lower bound.
 * @param max Inclusive upper bound.
 * @param ret Value returned when @p value is outside the range.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_OUT_RANGE_RETURN(value, min, max, ret, fmt, ...) \
    BROOKESIA_CHECK_OUT_RANGE_EXECUTE(value, min, max, {return ret;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Return from the current `void` function when a value is outside the inclusive range [`min`, `max`].
 *
 * @param value Expression to test.
 * @param min Inclusive lower bound.
 * @param max Inclusive upper bound.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_OUT_RANGE_EXIT(value, min, max, fmt, ...) \
    BROOKESIA_CHECK_OUT_RANGE_EXECUTE(value, min, max, {return;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})

/**
 * @brief Jump to a label when a value is outside the inclusive range [`min`, `max`].
 *
 * @param value Expression to test.
 * @param min Inclusive lower bound.
 * @param max Inclusive upper bound.
 * @param goto_tag Label jumped to when @p value is outside the range.
 * @param fmt User log format string forwarded to `BROOKESIA_LOGE`.
 * @param ... Optional format arguments for @p fmt.
 */
#define BROOKESIA_CHECK_OUT_RANGE_GOTO(value, min, max, goto_tag, fmt, ...) \
    BROOKESIA_CHECK_OUT_RANGE_EXECUTE(value, min, max, {goto goto_tag;}, {BROOKESIA_LOGE(fmt, ##__VA_ARGS__);})
