/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "private/esp_brookesia_gui_style_utils.hpp"
#include "esp_brookesia_gui_style.hpp"

namespace esp_brookesia::gui {

bool StyleSize::calibrate(const StyleSize &parent)
{
    int parent_w = 0;
    int parent_h = 0;

    parent_w = parent.width;
    parent_h = parent.height;

    // Check width
    if (flags.enable_width_auto) {
        width = StyleSize::LENGTH_AUTO;
    } else if (flags.enable_width_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(width_percent, 1, 100, false, "Invalid width percent");
        width = (parent_w != StyleSize::LENGTH_AUTO) ? (parent_w * width_percent) / 100 : StyleSize::LENGTH_AUTO;
    } else if (width != StyleSize::LENGTH_AUTO) {
        ESP_UTILS_CHECK_VALUE_RETURN(width, 1, parent_w, false, "Invalid width");
    }

    // Check height
    if (flags.enable_height_auto) {
        height = StyleSize::LENGTH_AUTO;
    } else if (flags.enable_height_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(height_percent, 1, 100, false, "Invalid Height percent");
        height = (parent_h != StyleSize::LENGTH_AUTO) ? (parent_h * height_percent) / 100 : StyleSize::LENGTH_AUTO;
    } else {
        ESP_UTILS_CHECK_VALUE_RETURN(height, 1, parent_h, false, "Invalid Height");
    }

    // Process special size
    if (flags.enable_square || flags.enable_circle) {
        width = std::min(width, height);
        height = width;
    }

    // Process circle
    if (flags.enable_circle) {
        radius = StyleSize::RADIUS_CIRCLE;
    }

    return true;
}

bool StyleSize::calibrate(const StyleSize &parent, bool check_width, bool check_height)
{
    int parent_w = 0;
    int parent_h = 0;

    parent_w = parent.width;
    parent_h = parent.height;

    // Check width
    if (flags.enable_width_auto) {
        width = StyleSize::LENGTH_AUTO;
    } else if (flags.enable_width_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(width_percent, 1, 100, false, "Invalid width percent");
        width = (parent_w * width_percent) / 100;
    } else if (check_width) {
        ESP_UTILS_CHECK_VALUE_RETURN(width, 1, parent_w, false, "Invalid width");
    }

    // Check height
    if (flags.enable_height_auto) {
        height = StyleSize::LENGTH_AUTO;
    } else if (flags.enable_height_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(height_percent, 1, 100, false, "Invalid Height percent");
        height = (parent_h * height_percent) / 100;
    } else if (check_height) {
        ESP_UTILS_CHECK_VALUE_RETURN(height, 1, parent_h, false, "Invalid Height");
    }

    // Process special size
    if (flags.enable_square || flags.enable_circle) {
        width = std::min(width, height);
        height = width;
    }

    // Process circle
    if (flags.enable_circle) {
        radius = StyleSize::RADIUS_CIRCLE;
    }

    return true;
}

bool StyleSize::calibrate(const StyleSize &parent, bool allow_zero)
{
    int parent_w = 0;
    int parent_h = 0;
    int min_size = allow_zero ? 0 : 1;

    parent_w = parent.width;
    parent_h = parent.height;

    // Check width
    if (flags.enable_width_auto) {
        width = StyleSize::LENGTH_AUTO;
    } else if (flags.enable_width_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(width_percent, min_size, 100, false, "Invalid width percent");
        width = (parent_w * width_percent) / 100;
    } else {
        ESP_UTILS_CHECK_VALUE_RETURN(width, min_size, parent_w, false, "Invalid width");
    }

    // Check height
    if (flags.enable_height_auto) {
        height = StyleSize::LENGTH_AUTO;
    } else if (flags.enable_height_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(height_percent, min_size, 100, false, "Invalid Height percent");
        height = (parent_h * height_percent) / 100;
    } else {
        ESP_UTILS_CHECK_VALUE_RETURN(height, min_size, parent_h, false, "Invalid Height");
    }

    // Process special size
    if (flags.enable_square || flags.enable_circle) {
        width = std::min(width, height);
        height = width;
    }

    // Process circle
    if (flags.enable_circle) {
        radius = StyleSize::RADIUS_CIRCLE;
    }

    return true;
}

bool StyleFont::calibrate(
    const StyleSize *parent, FindResourceBySizeMethod find_resource_by_size,
    FindResourceByHeightMethod find_resource_by_height, GetFontLineHeightMethod get_font_line_height
)
{
    if (flags.enable_height) {
        goto process_height;
    }

    // Size
    ESP_UTILS_CHECK_VALUE_RETURN(
        size_px, StyleFont::FONT_SIZE_MIN, StyleFont::FONT_SIZE_MAX, false, "Invalid size"
    );
    // Font description
    if (font_resource == nullptr) {
        font_resource = find_resource_by_size(size_px);
        ESP_UTILS_CHECK_NULL_RETURN(font_resource, false, "Get default font failed");
        height = get_font_line_height(font_resource);
    }
    goto end;

process_height:
    // Height
    if (flags.enable_height_percent) {
        ESP_UTILS_CHECK_NULL_RETURN(parent, false, "Invalid parent");
        ESP_UTILS_CHECK_VALUE_RETURN(height_percent, 1, 100, false, "Invalid height percent");
        height = (parent->height * height_percent) / 100;
    } else if (parent != nullptr) {
        ESP_UTILS_CHECK_VALUE_RETURN(height, 1, parent->height, false, "Invalid height");
    }

    // Font description & size
    font_resource = find_resource_by_height(height, &size_px);
    ESP_UTILS_CHECK_NULL_RETURN(font_resource, false, "Get default font failed");

end:
    return true;
}

bool StyleImage::calibrate(void) const
{
    ESP_UTILS_CHECK_NULL_RETURN(resource, false, "Invalid resource");

    return true;
}

} // namespace esp_brookesia::gui
