// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.1
// LVGL version: 8.3.11
// Project name: Smart_Gadget

#include "esp_brookesia_conf_internal.h"
#include "ui_comp.h"

#if ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_COMP
static uint32_t lv_event_get_comp_child;

typedef struct {
    uint32_t child_idx;
    lv_obj_t * child;
} ui_comp_get_child_t;

lv_obj_t * ui_comp_get_child(lv_obj_t * comp, uint32_t child_idx)
{
    ui_comp_get_child_t info;
    info.child = NULL;
    info.child_idx = child_idx;
    lv_event_send(comp, lv_event_get_comp_child, &info);
    return info.child;
}

void get_component_child_event_cb(lv_event_t * e)
{
    lv_obj_t ** c = lv_event_get_user_data(e);
    ui_comp_get_child_t * info = lv_event_get_param(e);
    info->child = c[info->child_idx];
}

void del_component_child_event_cb(lv_event_t * e)
{
    lv_obj_t ** c = lv_event_get_user_data(e);
    lv_mem_free(c);
}

void esp_brookesia_squareline_ui_comp_init(void)
{
    lv_event_get_comp_child = lv_event_register_id();
}

lv_event_code_t ui_comp_get_event_code(void)
{
    return lv_event_get_comp_child;
}
#endif /* ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_COMP */