/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

/* Font */
#define ESP_BROOKESIA_STYLE_FONT_SIZE_MIN    8
#define ESP_BROOKESIA_STYLE_FONT_SIZE_MAX    48

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Style size structure, used to define the size of UI elements. Users can set these dimensions using the
 *        `ESP_BROOKESIA_STYLE_SIZE_*` macros.
 */
typedef struct {
    uint16_t width;                         /*!< Width in pixels */
    uint16_t height;                        /*!< Height in pixels */
    uint8_t width_percent;                  /*!< Percentage of the parent width */
    uint8_t height_percent;                 /*!< Percentage of the parent height */
    struct {
        uint8_t enable_width_percent: 1;    /*!< If set, the `width` will be calculated based on `width_percent` */
        uint8_t enable_height_percent: 1;   /*!< If set, the `height` will be calculated based on `height_percent` */
        uint8_t enable_square: 1;           /*!< If set, `width` and `height` will be equal, taking the smaller value */
    } flags;                                /*!< Style size flags */
} ESP_Brookesia_StyleSize_t;

/**
 * @brief Initializer for style size structure with specified width and height in pixels.
 *
 * @param w The width in pixels
 * @param h The height in pixels
 */
#define ESP_BROOKESIA_STYLE_SIZE_RECT(w, h) \
    {                                \
        .width = w,                  \
        .height = h,                 \
    }

/**
 * @brief Initializer for style size structure with width and height as percentages of the parent size.
 *
 * @param w_percent The percentage of the parent width
 * @param h_percent The percentage of the parent height
 */
#define ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(w_percent, h_percent) \
    {                                                        \
        .width_percent = w_percent,                          \
        .height_percent = h_percent,                         \
        .flags = {                                           \
            .enable_width_percent = 1,                       \
            .enable_height_percent = 1,                      \
        },                                                   \
    }

/**
 * @brief Initializer for style size structure with width as a percentage of the parent size and height in pixels.
 *
 * @param w_percent The percentage of the parent width
 * @param h The height in pixels
 */
#define ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(w_percent, h) \
    {                                                  \
        .height = h,                                   \
        .width_percent = w_percent,                    \
        .flags = {                                     \
            .enable_width_percent = 1,                 \
        },                                             \
    }

/**
 * @brief Initializer for style size structure with width in pixels and height as a percentage of the parent size.
 *
 * @param w The width in pixels
 * @param h_percent The percentage of the parent height
 */
#define ESP_BROOKESIA_STYLE_SIZE_RECT_H_PERCENT(w, h_percent) \
    {                                                  \
        .width = w,                                    \
        .height_percent = h_percent,                   \
        .flags = {                                     \
            .enable_height_percent = 1,                \
        },                                             \
    }

/**
 * @brief Initializer for style size structure with width and height equal to the specified size in pixels.
 *
 * @param size The size in pixels
 */
#define ESP_BROOKESIA_STYLE_SIZE_SQUARE(size) \
    {                                  \
        .width = size,                 \
        .height = size,                \
    }

/**
 * @brief Initializer for style size structure with width and height equal to the specified percentage of the
 *        parent size.
 *
 * @param percent The percentage of the parent size
 */
#define ESP_BROOKESIA_STYLE_SIZE_SQUARE_PERCENT(percent) \
    {                                             \
        .width_percent = percent,                 \
        .height_percent = percent,                \
        .flags = {                                \
            .enable_width_percent = 1,            \
            .enable_height_percent = 1,           \
            .enable_square = 1,                   \
        },                                        \
    }

/**
 * @brief Style font structure, used to define the UI fonts. Users can set these dimensions using the
 *        `ESP_BROOKESIA_STYLE_FONT_*` macros.
 */
typedef struct {
    uint8_t size_px;                        /*!< Font size in pixels. The font size must be between
                                                 `ESP_BROOKESIA_STYLE_FONT_SIZE_MIN` and `ESP_BROOKESIA_STYLE_FONT_SIZE_MAX` */
    uint8_t height;                         /*!< Font height in pixels */
    uint8_t height_percent;                 /*!< Font height as a percentage of the parent height */
    const void *font_resource;              /*!< Custom font resource */
    struct {
        uint8_t enable_height: 1;           /*!< If set, the `size` will be calculated based on `height` */
        uint8_t enable_height_percent: 1;   /*!< If set, the `size` will be calculated based on `height_percent` */
    } flags;                                /*!< Style font flags */
} ESP_Brookesia_StyleFont_t;

/**
 * @brief Initializer for style font structure with specified font size in pixels.
 *
 * @param size The font size in pixels
 */
#define ESP_BROOKESIA_STYLE_FONT_SIZE(size) \
    {                                \
        .size_px = size,             \
    }

/**
 * @brief Initializer for style font structure with specified font height in pixels.
 *
 * @param h The font height in pixels
 */
#define ESP_BROOKESIA_STYLE_FONT_HEIGHT(h) \
    {                               \
        .height = h,                \
        .flags = {                  \
            .enable_height = 1,     \
        },                          \
    }

/**
 * @brief Initializer for style font structure with height as a percentage of the parent height
 *
 * @param percent The percentage of the parent height
 */
#define ESP_BROOKESIA_STYLE_FONT_HEIGHT_PERCENT(percent) \
    {                                             \
        .height_percent = percent,                \
        .flags = {                                \
            .enable_height = 1,                   \
            .enable_height_percent = 1,           \
        },                                        \
    }

/**
 * @brief Initializer for style font structure with custom font resource and specified font size in pixels.
 *
 * @param size The font size in pixels
 * @param font The custom font resource
 *
 */
#define ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(size, font) \
    {                                             \
        .size_px = size,                          \
        .font_resource = font,                    \
    }

/**
 * @brief Style color structure, used to define the color of UI elements. Users can set these colors using the
 *       `ESP_BROOKESIA_STYLE_COLOR*` macros.
 */
typedef struct {
    uint32_t color:  24;
    uint32_t opacity: 8;
} ESP_Brookesia_StyleColor_t;

/**
 * @brief Initializer for style color structure with specified color
 *
 * @param color24 The color in 24-bit RGB format (MSB-> R[7:0], G[7:0], B[7:0] <-LSB)
 */
#define ESP_BROOKESIA_STYLE_COLOR(color24) \
    {                               \
        .color = color24,           \
        .opacity = 255,             \
    }

/**
 * @brief Initializer for style color structure with specified color and opacity
 *
 * @param color24 The color in 24-bit RGB format (MSB-> R[7:0], G[7:0], B[7:0] <-LSB)
 * @param opa The opacity value (0-255)
 */
#define ESP_BROOKESIA_STYLE_COLOR_WITH_OPACIRY(color24, opa) \
    {                                                 \
        .color = color24,                             \
        .opacity = opa,                               \
    }

/**
 * @brief Style image structure, used to define the image resources for UI elements. Users can set these
 *        dimensions using the `ESP_BROOKESIA_STYLE_IMAGE*` macros.
 *
 */
typedef struct {
    const void *resource;           /*!< Pointer to the image resource */
    ESP_Brookesia_StyleColor_t recolor;    /*!< Color to recolor the image */
    struct {
        uint8_t enable_recolor: 1;  /*!< Flag to enable image recoloring */
    } flags;                        /*!< Style image flags */
} ESP_Brookesia_StyleImage_t;

/**
 * @brief Initializer for the style image structure with the specified image resource.
 *
 * @param image The image resource
 */
#define ESP_BROOKESIA_STYLE_IMAGE(image) \
    {                             \
        .resource = image,        \
    }

/**
 * @brief Initializer for the style image structure with the specified image resource and recolor color.
 *
 * @param image The image resource
 * @param color The color to recolor the image
 */
#define ESP_BROOKESIA_STYLE_IMAGE_RECOLOR(image, color) \
    {                                            \
        .resource = image,                       \
        .recolor = ESP_BROOKESIA_STYLE_COLOR(color),    \
        .flags = {                               \
            .enable_recolor = 1,                 \
        },                                       \
    }

/**
 * @brief Initializer for the style image structure with the specified image resource and white recolor.
 *
 * @param image The image resource
 */
#define ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(image)  \
    {                                            \
        .resource = image,                       \
        .recolor = ESP_BROOKESIA_STYLE_COLOR(0xFFFFFF), \
        .flags = {                               \
            .enable_recolor = 1,                 \
        },                                       \
    }

/**
 * @brief Initializer for the style image structure with the specified image resource and black recolor.
 *
 * @param image The image resource
 */
#define ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_BLACK(image) \
    {                                           \
        .resource = image,                      \
        .recolor = ESP_BROOKESIA_STYLE_COLOR(0x000000),\
        .flags = {                              \
            .enable_recolor = 1,                \
        },                                      \
    }

#ifdef __cplusplus
}
#endif
