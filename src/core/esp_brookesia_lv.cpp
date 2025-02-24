#include "esp_brookesia_core_utils.h"
#include "esp_brookesia_lv.hpp"

bool ESP_Brookesia_LvObj::applyStyleAttribute(ESP_Brookesia_StyleSize_t size, lv_style_selector_t selector)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_width(get(), size.width, selector);
    lv_obj_set_style_height(get(), size.height, selector);

    return true;
}

bool ESP_Brookesia_LvObj::applyStyleAttribute(ESP_Brookesia_StyleFont_t font, lv_style_selector_t selector)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_text_font(get(), (lv_font_t*)font.font_resource, selector);

    return true;
}

bool ESP_Brookesia_LvObj::applyStyleAttribute(ESP_Brookesia_StyleAlign_t align, lv_style_selector_t selector)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_align(get(), align.type, selector);

    return true;
}

bool ESP_Brookesia_LvObj::applyStyleAttribute(ESP_Brookesia_StyleLayoutFlex_t layout, lv_style_selector_t selector)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_layout(get(), LV_LAYOUT_FLEX, selector);
    lv_obj_set_style_flex_flow(get(), (lv_flex_flow_t)layout.flow, selector);
    lv_obj_set_style_flex_main_place(get(), (lv_flex_align_t)layout.main_place, selector);
    lv_obj_set_style_flex_cross_place(get(), (lv_flex_align_t)layout.cross_place, selector);
    lv_obj_set_style_flex_track_place(get(), (lv_flex_align_t)layout.track_place, selector);

    return true;
}

bool ESP_Brookesia_LvObj::applyStyleAttribute(ESP_Brookesia_StyleGap_t gap, lv_style_selector_t selector)
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
