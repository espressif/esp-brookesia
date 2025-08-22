/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <functional>
#include <limits>
#include <stdint.h>

namespace esp_brookesia::gui {

enum StyleWidthItem {
    STYLE_WIDTH_ITEM_BORDER = 0,        /*!< Border width of UI elements */
    STYLE_WIDTH_ITEM_OUTLINE,           /*!< Outline width of UI elements */
    STYLE_WIDTH_ITEM_MAX,
};

/**
 * @brief Style size structure, used to define the size of UI elements. Users can set these dimensions using the
 *        `*`.
 */
struct StyleSize {
    static constexpr int LENGTH_AUTO = std::numeric_limits<int>::max();
    static constexpr int RADIUS_CIRCLE = std::numeric_limits<int>::max();

    /**
    * @brief Initializer for style size structure with specified width and height in pixels.
    *
    * @param w The width in pixels
    * @param h The height in pixels
    */
    static constexpr StyleSize RECT(int w, int h)
    {
        return {
            .width = w,
            .height = h,
        };
    }

    /**
    * @brief Initializer for style size structure with width and height as percentages of the parent size.
    *
    * @param w_percent The percentage of the parent width
    * @param h_percent The percentage of the parent height
    */
    static constexpr StyleSize RECT_PERCENT(int w_percent, int h_percent)
    {
        return {
            .width_percent = w_percent,
            .height_percent = h_percent,
            .flags = {
                .enable_width_percent = true,
                .enable_height_percent = true,
            },
        };
    }

    /**
    * @brief Initializer for style size structure with width as a percentage of the parent size and height in pixels.
    *
    * @param w_percent The percentage of the parent width
    * @param h The height in pixels
    */
    static constexpr StyleSize RECT_W_PERCENT(int w_percent, int h)
    {
        return {
            .height = h,
            .width_percent = w_percent,
            .flags = {
                .enable_width_percent = true,
            },
        };
    }

    /**
    * @brief Initializer for style size structure with width in pixels and height as a percentage of the parent size.
    *
    * @param w The width in pixels
    * @param h_percent The percentage of the parent height
    */
    static constexpr StyleSize RECT_H_PERCENT(int w, int h_percent)
    {
        return {
            .width = w,
            .height_percent = h_percent,
            .flags = {
                .enable_height_percent = true,
            },
        };
    }

    /**
    * @brief Initializer for style size structure with width and height equal to the specified size in pixels.
    *
    * @param size The size in pixels
    */
    static constexpr StyleSize SQUARE(int size)
    {
        return {
            .width = size,
            .height = size,
        };
    }

    /**
    * @brief Initializer for style size structure with width and height equal to the specified percentage of the
    *        parent size.
    *
    * @param percent The percentage of the parent size
    */
    static constexpr StyleSize SQUARE_PERCENT(int percent)
    {
        return {
            .width_percent = percent,
            .height_percent = percent,
            .flags = {
                .enable_width_percent = true,
                .enable_height_percent = true,
                .enable_square = true,
            },
        };
    }

    /**
    * @brief Initializer for style size structure with circular shape of specified diameter in pixels.
    *
    * @param size The diameter in pixels
    */
    static constexpr StyleSize CIRCLE(int size)
    {
        return {
            .width = size,
            .height = size,
            .flags = {
                .enable_circle = true,
            },
        };
    }

    /**
    * @brief Initializer for style size structure with circular shape of specified diameter as percentage of parent size.
    *
    * @param percent The percentage of the parent size for the diameter
    */
    static constexpr StyleSize CIRCLE_PERCENT(int percent)
    {
        return {
            .width_percent = percent,
            .height_percent = percent,
            .flags = {
                .enable_width_percent = true,
                .enable_height_percent = true,
                .enable_circle = true,
            },
        };
    }

    bool calibrate(const StyleSize &parent);
    bool calibrate(const StyleSize &parent, bool check_width, bool check_height);
    bool calibrate(const StyleSize &parent, bool allow_zero);

    int width;                         /*!< Width in pixels */
    int height;                        /*!< Height in pixels */
    int radius;                        /*!< Radius in pixels */
    int width_percent;                  /*!< Percentage of the parent width */
    int height_percent;                 /*!< Percentage of the parent height */
    struct {
        int enable_width_percent: 1;    /*!< If set, the `width` will be calculated based on `width_percent` */
        int enable_width_auto: 1;       /*!< If set, the `width` will be ignored */
        int enable_height_percent: 1;   /*!< If set, the `height` will be calculated based on `height_percent` */
        int enable_height_auto: 1;      /*!< If set, the `height` will be ignored */
        int enable_square: 1;           /*!< If set, `width` and `height` will be equal, taking the smaller value */
        int enable_circle: 1;           /*!< If set, `width` and `height` will be equal, taking the smaller value,
                                              *  and `radius` will be set to `LV_RADIUS_CIRCLE`
                                              */
    } flags;                                /*!< Style size flags */
};

/**
 * @brief Style font structure, used to define the UI fonts. Users can set these dimensions using the
 *        `*`.
 */
struct StyleFont {
    static constexpr int FONT_SIZE_MIN = 8;    /*!< Minimum font size in pixels */
    static constexpr int FONT_SIZE_MAX = 48;   /*!< Maximum font size in pixels */
    static constexpr int FONT_SIZE_NUM = ((FONT_SIZE_MAX - FONT_SIZE_MIN) / 2 + 1);

    using FindResourceBySizeMethod = std::function<const void *(int size_px)>;
    using FindResourceByHeightMethod = std::function<const void *(int height, int *size_px)>;
    using GetFontLineHeightMethod = std::function<int (const void *font_resource)>;

    /**
    * @brief Initializer for style font structure with specified font size in pixels.
    *
    * @param size The font size in pixels
    */
    static constexpr StyleFont SIZE(int size)
    {
        return {
            .size_px = size,
        };
    }

    /**
    * @brief Initializer for style font structure with specified font height in pixels.
    *
    * @param h The font height in pixels
    */
    static constexpr StyleFont HEIGHT(int h)
    {
        return {
            .height = h,
            .flags = {
                .enable_height = true,
            },
        };
    }

    /**
    * @brief Initializer for style font structure with height as a percentage of the parent height
    *
    * @param percent The percentage of the parent height
    */
    static constexpr StyleFont HEIGHT_PERCENT(int percent)
    {
        return {
            .height_percent = percent,
            .flags = {
                .enable_height = true,
                .enable_height_percent = true,
            },
        };
    }

    /**
    * @brief Initializer for style font structure with custom font resource and specified font size in pixels.
    *
    * @param size The font size in pixels
    * @param font The custom font resource
    *
    */
    static constexpr StyleFont CUSTOM_SIZE(int size, const void *font)
    {
        return {
            .size_px = size,
            .font_resource = font,
        };
    }

    bool calibrate(
        const StyleSize *parent, FindResourceBySizeMethod find_resource_by_size,
        FindResourceByHeightMethod find_resource_by_height, GetFontLineHeightMethod get_font_line_height
    );

    int size_px;                        /*!< Font size in pixels. The font size must be between
                                                 `FONT_SIZE_MIN` and `FONT_SIZE_MAX` */
    int height;                         /*!< Font height in pixels */
    int height_percent;                 /*!< Font height as a percentage of the parent height */
    const void *font_resource;              /*!< Custom font resource */
    struct {
        int enable_height: 1;           /*!< If set, the `size` will be calculated based on `height` */
        int enable_height_percent: 1;   /*!< If set, the `size` will be calculated based on `height_percent` */
    } flags;                                /*!< Style font flags */
};

enum StyleFontType {
    TYPE_EN = 0,
    TYPE_CN,
    TYPE_MAX,
};

/**
 * @brief Enumeration of style color items for different UI elements
 */
enum StyleColorItem {
    STYLE_COLOR_ITEM_BACKGROUND = 0,        /*!< Background color of UI elements */
    STYLE_COLOR_ITEM_TEXT,                  /*!< Text color of UI elements */
    STYLE_COLOR_ITEM_BORDER,                /*!< Border color of UI elements */
    STYLE_COLOR_ITEM_MAX,
};

/**
 * @brief Style color structure, used to define the color of UI elements. Users can set these colors using the
 *       `STYLE_COLOR*`.
 */
struct StyleColor {
    /**
    * @brief Initializer for style color structure with specified color
    *
    * @param color24 The color in 24-bit RGB format (MSB-> R[7:0], G[7:0], B[7:0] <-LSB)
    */
    static constexpr StyleColor COLOR(uint32_t color24)
    {
        return {
            .color = color24,
            .opacity = 255,
        };
    }

    /**
    * @brief Initializer for style color structure with specified color and opacity
    *
    * @param color24 The color in 24-bit RGB format (MSB-> R[7:0], G[7:0], B[7:0] <-LSB)
    * @param opa The opacity value (0-255)
    */
    static constexpr StyleColor COLOR_WITH_OPACITY(uint32_t color24, uint8_t opa)
    {
        return {
            .color = color24,
            .opacity = opa,
        };
    }

    uint32_t color:  24;                    /*!< Color in 24-bit RGB format (R[7:0], G[7:0], B[7:0]) */
    uint32_t opacity: 8;                    /*!< Opacity value (0-255, where 0 is transparent and 255 is opaque) */
};

/**
 * @brief Style image structure, used to define the image resources for UI elements. Users can set these
 *        dimensions using the `STYLE_IMAGE*`.
 *
 */
struct StyleImage {
    /**
    * @brief Initializer for the style image structure with the specified image resource.
    *
    * @param image The image resource
    */
    static constexpr StyleImage IMAGE(const void *image)
    {
        return {
            .resource = image,
        };
    }

    /**
    * @brief Initializer for the style image structure with the specified image resource and recolor color.
    *
    * @param image The image resource
    * @param color The color to recolor the image
    */
    static constexpr StyleImage IMAGE_RECOLOR(const void *image, uint32_t color)
    {
        return {
            .resource = image,
            .recolor = StyleColor::COLOR(color),
            .flags = {
                .enable_recolor = true,
            },
        };
    }

    /**
    * @brief Initializer for the style image structure with the specified image resource and white recolor.
    *
    * @param image The image resource
    */
    static constexpr StyleImage IMAGE_RECOLOR_WHITE(const void *image)
    {
        return {
            .resource = image,
            .recolor = StyleColor::COLOR(0xFFFFFF),
            .flags = {
                .enable_recolor = true,
            },
        };
    }

    /**
    * @brief Initializer for the style image structure with the specified image resource and black recolor.
    *
    * @param image The image resource
    */
    static constexpr StyleImage IMAGE_RECOLOR_BLACK(const void *image)
    {
        return {
            .resource = image,
            .recolor = StyleColor::COLOR(0x000000),
            .flags = {
                .enable_recolor = true,
            },
        };
    }

    bool calibrate(void) const;

    const void *resource;                   /*!< Pointer to the image resource */
    StyleColor recolor;                     /*!< Color to recolor the image */
    StyleColor container_color;             /*!< Color of the container */
    struct {
        int enable_recolor: 1;          /*!< Flag to enable image recoloring */
        int enable_container_color: 1;  /*!< Flag to enable container color */
    } flags;                                /*!< Style image flags */
};

/**
 * @brief Enumeration of alignment types for positioning UI elements
 */
enum StyleAlignType {
    STYLE_ALIGN_TYPE_TOP_LEFT = 0,          /*!< Align to top-left corner */
    STYLE_ALIGN_TYPE_TOP_MID,               /*!< Align to top-middle */
    STYLE_ALIGN_TYPE_TOP_RIGHT,             /*!< Align to top-right corner */
    STYLE_ALIGN_TYPE_BOTTOM_LEFT,           /*!< Align to bottom-left corner */
    STYLE_ALIGN_TYPE_BOTTOM_MID,            /*!< Align to bottom-middle */
    STYLE_ALIGN_TYPE_BOTTOM_RIGHT,          /*!< Align to bottom-right corner */
    STYLE_ALIGN_TYPE_LEFT_MID,              /*!< Align to middle of left side */
    STYLE_ALIGN_TYPE_RIGHT_MID,             /*!< Align to middle of right side */
    STYLE_ALIGN_TYPE_CENTER,                /*!< Align to center */
};

/**
 * @brief Style alignment structure for positioning UI elements
 */
struct StyleAlign {
    StyleAlignType type;                    /*!< Alignment type */
    int offset_x;                           /*!< Horizontal offset in pixels */
    int offset_y;                           /*!< Vertical offset in pixels */
};

/**
 * @brief Style gap structure for defining spacing between UI elements
 */
struct StyleGap {
    /**
    * @brief Initializer for style gap structure with specified gaps around an element.
    *
    * @param top_v The gap at the top in pixels
    * @param bottom_v The gap at the bottom in pixels
    * @param left_v The gap at the left in pixels
    * @param right_v The gap at the right in pixels
    */
    static constexpr StyleGap AROUND(int top_v, int bottom_v, int left_v, int right_v)
    {
        return {
            .top = top_v,
            .bottom = bottom_v,
            .left = left_v,
            .right = right_v,
        };
    }

    /**
    * @brief Initializer for style gap structure with specified gap between rows.
    *
    * @param value The gap between rows in pixels
    */
    static constexpr StyleGap ROW(int value)
    {
        return {
            .row = value,
        };
    }

    /**
    * @brief Initializer for style gap structure with specified gap between columns.
    *
    * @param value The gap between columns in pixels
    */
    static constexpr StyleGap COLUMN(int value)
    {
        return {
            .column = value,
        };
    }

    int top;                                /*!< Gap at the top in pixels */
    int bottom;                             /*!< Gap at the bottom in pixels */
    int left;                               /*!< Gap at the left in pixels */
    int right;                              /*!< Gap at the right in pixels */
    int row;                                /*!< Gap between rows in pixels */
    int column;                             /*!< Gap between columns in pixels */
};

/**
 * @brief Style layout flex structure for defining flex container properties
 */
struct StyleLayoutFlex {
    /**
     * @brief Enumeration of flex flow types for layout direction
     */
    enum FlowType {
        FLOW_ROW = 0,                /*!< Items arranged in a row */
        FLOW_COLUMN,                 /*!< Items arranged in a column */
        FLOW_ROW_WRAP,               /*!< Items arranged in a row with wrapping */
        FLOW_ROW_REVERSE,            /*!< Items arranged in a row in reverse order */
        FLOW_ROW_WRAP_REVERSE,       /*!< Items arranged in a row with wrapping in reverse order */
        FLOW_COLUMN_WRAP,            /*!< Items arranged in a column with wrapping */
        FLOW_COLUMN_REVERSE,         /*!< Items arranged in a column in reverse order */
        FLOW_COLUMN_WRAP_REVERSE,    /*!< Items arranged in a column with wrapping in reverse order */
    };

    /**
     * @brief Enumeration of flex alignment types for positioning items within a flex container
     */
    enum AlignType {
        ALIGN_START,                 /*!< Align items to the start of the container */
        ALIGN_END,                   /*!< Align items to the end of the container */
        ALIGN_CENTER,                /*!< Align items to the center of the container */
        ALIGN_SPACE_EVENLY,          /*!< Distribute items evenly with equal space around them */
        ALIGN_SPACE_AROUND,          /*!< Distribute items with equal space around them */
        ALIGN_SPACE_BETWEEN,         /*!< Distribute items with equal space between them */
    };

    FlowType flow;                     /*!< Flow direction of flex items */
    AlignType main_place;              /*!< Alignment along the main axis */
    AlignType cross_place;             /*!< Alignment along the cross axis */
    AlignType track_place;             /*!< Alignment of tracks (for multi-line layouts) */
};

struct StyleAnimation {
    /**
    * @brief Enumeration of animation path types for controlling animation timing
    */
    enum AnimationPathType {
        ANIM_PATH_TYPE_LINEAR = 0,              /*!< Linear animation path (constant speed) */
        ANIM_PATH_TYPE_EASE_IN,                 /*!< Animation starts slowly and accelerates */
        ANIM_PATH_TYPE_EASE_OUT,                /*!< Animation starts quickly and decelerates */
        ANIM_PATH_TYPE_EASE_IN_OUT,             /*!< Animation starts and ends slowly with acceleration in the middle */
        ANIM_PATH_TYPE_OVERSHOOT,               /*!< Animation goes beyond the target and then settles */
        ANIM_PATH_TYPE_BOUNCE,                  /*!< Animation bounces at the end */
        ANIM_PATH_TYPE_STEP,                    /*!< Animation proceeds in discrete steps */
        ANIM_PATH_TYPE_MAX,                     /*!< Maximum value for animation path types */
    };

    int start_value;
    int end_value;
    int duration_ms;
    int delay_ms;
    AnimationPathType path_type;
};

enum StyleFlag {
    STYLE_FLAG_HIDDEN         = (1ULL << 0),
    STYLE_FLAG_CLICKABLE      = (1ULL << 1),
    STYLE_FLAG_SCROLLABLE     = (1ULL << 2),
    STYLE_FLAG_EVENT_BUBBLE   = (1ULL << 3),
    STYLE_FLAG_CLIP_CORNER    = (1ULL << 4),
    STYLE_FLAG_SEND_DRAW_TASK_EVENTS = (1ULL << 5),
};

inline StyleFlag operator|(StyleFlag a, StyleFlag b)
{
    return static_cast<StyleFlag>((static_cast<int>(a) | static_cast<int>(b)));
}

} // namespace esp_brookesia::gui

/**
 * @brief Backward compatibility for macro definitions
 */
#define ESP_BROOKESIA_STYLE_SIZE_RECT(w, h)                 esp_brookesia::gui::StyleSize::RECT(w, h)
#define ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(w_p, h_p)     esp_brookesia::gui::StyleSize::RECT_PERCENT(w_p, h_p)
#define ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(w_p, h)     esp_brookesia::gui::StyleSize::RECT_W_PERCENT(w_p, h)
#define ESP_BROOKESIA_STYLE_SIZE_RECT_H_PERCENT(w, h_p)     esp_brookesia::gui::StyleSize::RECT_H_PERCENT(w, h_p)
#define ESP_BROOKESIA_STYLE_SIZE_SQUARE(s)                  esp_brookesia::gui::StyleSize::SQUARE(s)
#define ESP_BROOKESIA_STYLE_SIZE_SQUARE_PERCENT(p)          esp_brookesia::gui::StyleSize::SQUARE_PERCENT(p)
#define ESP_BROOKESIA_STYLE_SIZE_CIRCLE(s)                  esp_brookesia::gui::StyleSize::CIRCLE(s)
#define ESP_BROOKESIA_STYLE_SIZE_CIRCLE_PERCENT(p)          esp_brookesia::gui::StyleSize::CIRCLE_PERCENT(p)
#define ESP_BROOKESIA_FONT_SIZE_MIN                         esp_brookesia::gui::StyleFont::FONT_SIZE_MIN
#define ESP_BROOKESIA_FONT_SIZE_MAX                         esp_brookesia::gui::StyleFont::FONT_SIZE_MAX
#define ESP_BROOKESIA_STYLE_FONT_SIZE(s)                    esp_brookesia::gui::StyleFont::SIZE(s)
#define ESP_BROOKESIA_STYLE_FONT_HEIGHT(h)                  esp_brookesia::gui::StyleFont::HEIGHT(h)
#define ESP_BROOKESIA_STYLE_FONT_HEIGHT_PERCENT(p)          esp_brookesia::gui::StyleFont::HEIGHT_PERCENT(p)
#define ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(s, f)          esp_brookesia::gui::StyleFont::CUSTOM_SIZE(s, f)
#define ESP_BROOKESIA_STYLE_COLOR(c)                        esp_brookesia::gui::StyleColor::COLOR(c)
#define ESP_BROOKESIA_STYLE_COLOR_WITH_OPACITY(c, o)        esp_brookesia::gui::StyleColor::COLOR_WITH_OPACITY(c, o)
#define ESP_BROOKESIA_STYLE_IMAGE(r)                        esp_brookesia::gui::StyleImage::IMAGE(r)
#define ESP_BROOKESIA_STYLE_IMAGE_RECOLOR(r, c)             esp_brookesia::gui::StyleImage::IMAGE_RECOLOR(r, c)
#define ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(r)          esp_brookesia::gui::StyleImage::IMAGE_RECOLOR_WHITE(r)
#define ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_BLACK(r)          esp_brookesia::gui::StyleImage::IMAGE_RECOLOR_BLACK(r)
#define ESP_BROOKESIA_STYLE_ALIGN(t, x, y)                  esp_brookesia::gui::StyleAlign{t, x, y}
#define ESP_BROOKESIA_STYLE_GAP_AROUND(t, b, l, r)          esp_brookesia::gui::StyleGap::AROUND(t, b, l, r)
#define ESP_BROOKESIA_STYLE_GAP_ROW(v)                      esp_brookesia::gui::StyleGap::ROW(v)
#define ESP_BROOKESIA_STYLE_GAP_COLUMN(v)                   esp_brookesia::gui::StyleGap::COLUMN(v)
#define ESP_BROOKESIA_STYLE_LAYOUT_FLEX(f, m, c, t)         esp_brookesia::gui::StyleLayoutFlex{f, m, c, t}

/**
 * @brief Backward compatibility for enum values
 */
/* StyleAlignType */
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_CENTER               esp_brookesia::gui::STYLE_ALIGN_TYPE_CENTER
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_TOP_LEFT             esp_brookesia::gui::STYLE_ALIGN_TYPE_TOP_LEFT
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_TOP_MID              esp_brookesia::gui::STYLE_ALIGN_TYPE_TOP_MID
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_TOP_RIGHT            esp_brookesia::gui::STYLE_ALIGN_TYPE_TOP_RIGHT
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_BOTTOM_LEFT          esp_brookesia::gui::STYLE_ALIGN_TYPE_BOTTOM_LEFT
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_BOTTOM_MID           esp_brookesia::gui::STYLE_ALIGN_TYPE_BOTTOM_MID
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_BOTTOM_RIGHT         esp_brookesia::gui::STYLE_ALIGN_TYPE_BOTTOM_RIGHT
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_LEFT_MID             esp_brookesia::gui::STYLE_ALIGN_TYPE_LEFT_MID
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_RIGHT_MID            esp_brookesia::gui::STYLE_ALIGN_TYPE_RIGHT_MID
/* StyleFlexFlow */
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_ROW                   esp_brookesia::gui::StyleLayoutFlex::FLOW_ROW
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_COLUMN                esp_brookesia::gui::StyleLayoutFlex::FLOW_COLUMN
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_ROW_WRAP              esp_brookesia::gui::StyleLayoutFlex::FLOW_ROW_WRAP
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_ROW_REVERSE           esp_brookesia::gui::StyleLayoutFlex::FLOW_ROW_REVERSE
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_ROW_WRAP_REVERSE      esp_brookesia::gui::StyleLayoutFlex::FLOW_ROW_WRAP_REVERSE
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_COLUMN_WRAP           esp_brookesia::gui::StyleLayoutFlex::FLOW_COLUMN_WRAP
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_COLUMN_REVERSE        esp_brookesia::gui::StyleLayoutFlex::FLOW_COLUMN_REVERSE
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_COLUMN_WRAP_REVERSE   esp_brookesia::gui::StyleLayoutFlex::FLOW_COLUMN_WRAP_REVERSE
/* StyleFlexAlign */
#define ESP_BROOKESIA_STYLE_FLEX_ALIGN_START                esp_brookesia::gui::StyleLayoutFlex::ALIGN_START
#define ESP_BROOKESIA_STYLE_FLEX_ALIGN_END                  esp_brookesia::gui::StyleLayoutFlex::ALIGN_END
#define ESP_BROOKESIA_STYLE_FLEX_ALIGN_CENTER               esp_brookesia::gui::StyleLayoutFlex::ALIGN_CENTER
#define ESP_BROOKESIA_STYLE_FLEX_ALIGN_SPACE_EVENLY         esp_brookesia::gui::StyleLayoutFlex::ALIGN_SPACE_EVENLY
#define ESP_BROOKESIA_STYLE_FLEX_ALIGN_SPACE_AROUND         esp_brookesia::gui::StyleLayoutFlex::ALIGN_SPACE_AROUND
#define ESP_BROOKESIA_STYLE_FLEX_ALIGN_SPACE_BETWEEN        esp_brookesia::gui::StyleLayoutFlex::ALIGN_SPACE_BETWEEN
/* StyleColorItem */
#define ESP_BROOKESIA_STYLE_COLOR_ITEM_BACKGROUND           esp_brookesia::gui::STYLE_COLOR_ITEM_BACKGROUND
#define ESP_BROOKESIA_STYLE_COLOR_ITEM_TEXT                 esp_brookesia::gui::STYLE_COLOR_ITEM_TEXT
/* AnimationPathType */
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_LINEAR              esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_LINEAR
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_IN             esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_EASE_IN
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_OUT            esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_EASE_OUT
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_IN_OUT         esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_EASE_IN_OUT
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_OVERSHOOT           esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_OVERSHOOT
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_BOUNCE              esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_BOUNCE
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_STEP                esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_STEP
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_MAX                 esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_MAX

/**
 * @brief Backward compatibility for type definitions
 */
using ESP_Brookesia_StyleSize_t       [[deprecated("Use `esp_brookesia::gui::StyleSize` instead")]] =
    esp_brookesia::gui::StyleSize;
using ESP_Brookesia_StyleFont_t       [[deprecated("Use `esp_brookesia::gui::StyleFont` instead")]] =
    esp_brookesia::gui::StyleFont;
using ESP_Brookesia_StyleColorItem_t  [[deprecated("Use `esp_brookesia::gui::StyleColorItem` instead")]] =
    esp_brookesia::gui::StyleColorItem;
using ESP_Brookesia_StyleColor_t      [[deprecated("Use `esp_brookesia::gui::StyleColor` instead")]] =
    esp_brookesia::gui::StyleColor;
using ESP_Brookesia_StyleImage_t      [[deprecated("Use `esp_brookesia::gui::StyleImage` instead")]] =
    esp_brookesia::gui::StyleImage;
using ESP_Brookesia_StyleAlignType_t  [[deprecated("Use `esp_brookesia::gui::StyleAlignType` instead")]] =
    esp_brookesia::gui::StyleAlignType;
using ESP_Brookesia_StyleAlign_t      [[deprecated("Use `esp_brookesia::gui::StyleAlign` instead")]] =
    esp_brookesia::gui::StyleAlign;
using ESP_Brookesia_StyleGap_t        [[deprecated("Use `esp_brookesia::gui::StyleGap` instead")]] =
    esp_brookesia::gui::StyleGap;
using ESP_Brookesia_StyleFlexFlow_t   [[deprecated("Use `esp_brookesia::gui::StyleLayoutFlex::FlowType` instead")]] =
    esp_brookesia::gui::StyleLayoutFlex::FlowType;
using ESP_Brookesia_StyleFlexAlign_t  [[deprecated("Use `esp_brookesia::gui::StyleLayoutFlex::AlignType` instead")]] =
    esp_brookesia::gui::StyleLayoutFlex::AlignType;
using ESP_Brookesia_StyleLayoutFlex_t [[deprecated("Use `esp_brookesia::gui::StyleLayoutFlex` instead")]] =
    esp_brookesia::gui::StyleLayoutFlex;
using ESP_Brookesia_AnimPathType_t    [[deprecated("Use `esp_brookesia::gui::StyleAnimation::AnimationPathType` instead")]] =
    esp_brookesia::gui::StyleAnimation::AnimationPathType;
using ESP_Brookesia_StyleAnimation_t  [[deprecated("Use `esp_brookesia::gui::StyleAnimation` instead")]] =
    esp_brookesia::gui::StyleAnimation;
