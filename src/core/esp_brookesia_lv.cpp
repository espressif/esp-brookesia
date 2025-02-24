#include <cmath>
#include "esp_brookesia_core_utils.h"
#include "esp_brookesia_lv.hpp"

bool ESP_Brookesia_LvObject::applyStyle(lv_style_t *style, lv_style_selector_t selector)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");
    ESP_BROOKESIA_CHECK_NULL_RETURN(style, false, "Invalid style");

    lv_obj_add_style(get(), style, selector);

    return true;
}

bool ESP_Brookesia_LvObject::applyStyleAttribute(const ESP_Brookesia_StyleSize_t &size, lv_style_selector_t selector)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_width(get(), size.width, selector);
    lv_obj_set_style_height(get(), size.height, selector);
    lv_obj_set_style_radius(get(), size.radius, selector);

    return true;
}

bool ESP_Brookesia_LvObject::applyStyleAttribute(const ESP_Brookesia_StyleFont_t &font, lv_style_selector_t selector)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_text_font(get(), (lv_font_t*)font.font_resource, selector);

    return true;
}

bool ESP_Brookesia_LvObject::applyStyleAttribute(const ESP_Brookesia_StyleAlign_t &align)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_align(get(), getAlign(align.type), align.offset_x, align.offset_y);

    return true;
}

bool ESP_Brookesia_LvObject::applyStyleAttribute(const ESP_Brookesia_StyleLayoutFlex_t &layout, lv_style_selector_t selector)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_layout(get(), LV_LAYOUT_FLEX, selector);
    lv_obj_set_style_flex_flow(get(), getLayoutFlexFlow(layout.flow), selector);
    lv_obj_set_style_flex_main_place(get(), getLayoutFlexAlign(layout.main_place), selector);
    lv_obj_set_style_flex_cross_place(get(), getLayoutFlexAlign(layout.cross_place), selector);
    lv_obj_set_style_flex_track_place(get(), getLayoutFlexAlign(layout.track_place), selector);

    return true;
}

bool ESP_Brookesia_LvObject::applyStyleAttribute(const ESP_Brookesia_StyleGap_t &gap, lv_style_selector_t selector)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_pad_left(get(), gap.left, selector);
    lv_obj_set_style_pad_right(get(), gap.right, selector);
    lv_obj_set_style_pad_top(get(), gap.top, selector);
    lv_obj_set_style_pad_bottom(get(), gap.bottom, selector);
    lv_obj_set_style_pad_row(get(), gap.row, selector);
    lv_obj_set_style_pad_column(get(), gap.column, selector);

    return true;
}

bool ESP_Brookesia_LvObject::applyStyleAttribute(
    ESP_Brookesia_StyleColorItem_t item, const ESP_Brookesia_StyleColor_t &color, lv_style_selector_t selector
)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    switch (item) {
    case ESP_BROOKESIA_STYLE_COLOR_ITEM_BACKGROUND:
        lv_obj_set_style_bg_color(get(), getColor(color.color), selector);
        lv_obj_set_style_bg_opa(get(), color.opacity, selector);
        break;
    case ESP_BROOKESIA_STYLE_COLOR_ITEM_TEXT:
        lv_obj_set_style_text_color(get(), getColor(color.color), selector);
        lv_obj_set_style_text_opa(get(), color.opacity, selector);
        break;
    default:
        break;
    }

    return true;
}

bool ESP_Brookesia_LvObject::applyStyleAttribute(const ESP_Brookesia_StyleImage_t &image, lv_style_selector_t selector)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_bg_img_src(get(), image.resource, selector);
    lv_obj_set_style_bg_img_recolor(get(), lv_color_hex(image.recolor.color), selector);
    lv_obj_set_style_bg_img_recolor_opa(get(), image.recolor.opacity, selector);

    return true;
}

bool ESP_Brookesia_LvObject::alignTo(ESP_Brookesia_LvObject &target, const ESP_Brookesia_StyleAlign_t &align)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(!target.isValid(), false, "Invalid target");

    lv_obj_align_to(get(), target.get(), getAlign(align.type), align.offset_x, align.offset_y);

    return true;
}

lv_color_t ESP_Brookesia_LvObject::getColor(uint32_t color)
{
    return lv_color_hex(color);
}

lv_align_t ESP_Brookesia_LvObject::getAlign(const ESP_Brookesia_StyleAlignType_t &type)
{
    switch (type) {
    case ESP_BROOKESIA_STYLE_ALIGN_TYPE_TOP_LEFT:
        return LV_ALIGN_TOP_LEFT;
    case ESP_BROOKESIA_STYLE_ALIGN_TYPE_TOP_MID:
        return LV_ALIGN_TOP_MID;
    case ESP_BROOKESIA_STYLE_ALIGN_TYPE_TOP_RIGHT:
        return LV_ALIGN_TOP_RIGHT;
    case ESP_BROOKESIA_STYLE_ALIGN_TYPE_BOTTOM_LEFT:
        return LV_ALIGN_BOTTOM_LEFT;
    case ESP_BROOKESIA_STYLE_ALIGN_TYPE_BOTTOM_MID:
        return LV_ALIGN_BOTTOM_MID;
    case ESP_BROOKESIA_STYLE_ALIGN_TYPE_BOTTOM_RIGHT:
        return LV_ALIGN_BOTTOM_RIGHT;
    case ESP_BROOKESIA_STYLE_ALIGN_TYPE_LEFT_MID:
        return LV_ALIGN_LEFT_MID;
    case ESP_BROOKESIA_STYLE_ALIGN_TYPE_RIGHT_MID:
        return LV_ALIGN_RIGHT_MID;
    case ESP_BROOKESIA_STYLE_ALIGN_TYPE_CENTER:
        return LV_ALIGN_CENTER;
    default:
        return LV_ALIGN_TOP_LEFT;
    }
}

lv_flex_flow_t ESP_Brookesia_LvObject::getLayoutFlexFlow(const ESP_Brookesia_StyleFlexFlow_t &flow)
{
    switch (flow) {
    case ESP_BROOKESIA_STYLE_FLEX_FLOW_ROW:
        return LV_FLEX_FLOW_ROW;
    case ESP_BROOKESIA_STYLE_FLEX_FLOW_COLUMN:
        return LV_FLEX_FLOW_COLUMN;
    case ESP_BROOKESIA_STYLE_FLEX_FLOW_ROW_WRAP:
        return LV_FLEX_FLOW_ROW_WRAP;
    case ESP_BROOKESIA_STYLE_FLEX_FLOW_ROW_REVERSE:
        return LV_FLEX_FLOW_ROW_REVERSE;
    case ESP_BROOKESIA_STYLE_FLEX_FLOW_ROW_WRAP_REVERSE:
        return LV_FLEX_FLOW_ROW_WRAP_REVERSE;
    case ESP_BROOKESIA_STYLE_FLEX_FLOW_COLUMN_WRAP:
        return LV_FLEX_FLOW_COLUMN_WRAP;
    case ESP_BROOKESIA_STYLE_FLEX_FLOW_COLUMN_REVERSE:
        return LV_FLEX_FLOW_COLUMN_REVERSE;
    case ESP_BROOKESIA_STYLE_FLEX_FLOW_COLUMN_WRAP_REVERSE:
        return LV_FLEX_FLOW_COLUMN_WRAP_REVERSE;
    default:
        return LV_FLEX_FLOW_ROW;
    }
}

lv_flex_align_t ESP_Brookesia_LvObject::getLayoutFlexAlign(const ESP_Brookesia_StyleFlexAlign_t &align)
{
    switch (align) {
    case ESP_BROOKESIA_STYLE_FLEX_ALIGN_START:
        return LV_FLEX_ALIGN_START;
    case ESP_BROOKESIA_STYLE_FLEX_ALIGN_END:
        return LV_FLEX_ALIGN_END;
    case ESP_BROOKESIA_STYLE_FLEX_ALIGN_CENTER:
        return LV_FLEX_ALIGN_CENTER;
    case ESP_BROOKESIA_STYLE_FLEX_ALIGN_SPACE_EVENLY:
        return LV_FLEX_ALIGN_SPACE_EVENLY;
    case ESP_BROOKESIA_STYLE_FLEX_ALIGN_SPACE_AROUND:
        return LV_FLEX_ALIGN_SPACE_AROUND;
    case ESP_BROOKESIA_STYLE_FLEX_ALIGN_SPACE_BETWEEN:
        return LV_FLEX_ALIGN_SPACE_BETWEEN;
    default:
        return LV_FLEX_ALIGN_START;
    }
}

bool ESP_Brookesia_LvImage::applyStyleAttribute(const ESP_Brookesia_StyleImage_t &image, lv_style_selector_t selector)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    auto image_obj = _image.get();
    lv_obj_set_style_img_recolor(image_obj, lv_color_hex(image.recolor.color), selector);
    lv_obj_set_style_img_recolor_opa(image_obj, image.recolor.opacity, selector);

    auto container_obj = get();
    lv_obj_set_style_bg_color(container_obj, lv_color_hex(image.container_color.color), selector);
    lv_obj_set_style_bg_opa(container_obj, image.container_color.opacity, selector);

    return true;
}

bool ESP_Brookesia_LvImage::setImage(const void *image)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");
    ESP_BROOKESIA_CHECK_NULL_RETURN(image, false, "Invalid image");

    auto container_obj = get();
    lv_obj_update_layout(container_obj);

    auto width = lv_obj_get_width(container_obj);
    auto height = lv_obj_get_height(container_obj);
    auto image_obj = _image.get();
    float min_factor = std::min(
        (float)(width) / ((lv_img_dsc_t *)image)->header.w,
        (float)(height) / ((lv_img_dsc_t *)image)->header.h
    );
    lv_img_set_zoom(image_obj, LV_IMG_ZOOM_NONE * min_factor);
    lv_img_set_src(image_obj, image);
    lv_obj_refr_size(image_obj);

    return true;
}
