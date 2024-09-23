/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_core_utils.h"
#include "esp_brookesia_core_home.hpp"
#include "esp_brookesia_core_app.hpp"
#include "esp_brookesia_core.hpp"

#if !ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_HOME
#undef ESP_BROOKESIA_LOGD
#define ESP_BROOKESIA_LOGD(...)
#endif

using namespace std;

ESP_Brookesia_CoreHome::ESP_Brookesia_CoreHome(ESP_Brookesia_Core &core, const ESP_Brookesia_CoreHomeData_t &data):
    _core(core),
    _core_data(data),
    _main_screen(nullptr),
    _system_screen(nullptr),
    _main_screen_obj(nullptr),
    _system_screen_obj(nullptr),
    _container_style_index(0)
{
}

ESP_Brookesia_CoreHome::~ESP_Brookesia_CoreHome()
{
    ESP_BROOKESIA_LOGD("Destroy(@0x%p)", this);
    if (!delCore()) {
        ESP_BROOKESIA_LOGE("Delete failed");
    }
}

bool ESP_Brookesia_CoreHome::showContainerBorder(void)
{
    ESP_BROOKESIA_LOGD("Show container border");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Not initialized");

    for (size_t i = 0; i < _container_styles.size(); i++) {
        lv_style_set_outline_width(&_container_styles[i], _core_data.container.styles[i].outline_width);
    }

    return true;
}

bool ESP_Brookesia_CoreHome::hideContainerBorder(void)
{
    ESP_BROOKESIA_LOGD("Hide container border");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Not initialized");

    for (auto &style : _container_styles) {
        lv_style_set_outline_width(&style, 0);
    }

    return true;
}

lv_style_t *ESP_Brookesia_CoreHome::getCoreContainerStyle(void)
{
    uint8_t index = _container_style_index++;
    if (_container_style_index > (_container_styles.size() - 1)) {
        _container_style_index = 0;
    }

    return &_container_styles[index];
}

const lv_font_t *ESP_Brookesia_CoreHome::getCoreDefaultFontBySize(uint8_t size) const
{
    ESP_BROOKESIA_CHECK_VALUE_RETURN(size, ESP_BROOKESIA_STYLE_FONT_SIZE_MIN, ESP_BROOKESIA_STYLE_FONT_SIZE_MAX, nullptr, "Invalid size");

    auto it = _default_size_font_map.find(size);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(it != _default_size_font_map.end(), nullptr, "Font size(%d) is not found", size);

    return it->second;
}

const lv_font_t *ESP_Brookesia_CoreHome::getCoreDefaultFontByHeight(uint8_t height, uint8_t *size_px) const
{
    uint8_t ret_size = 0;

    ESP_BROOKESIA_CHECK_NULL_RETURN(size_px, nullptr, "Invalid size_px");

    auto lower = _default_height_font_map.lower_bound(height);
    if ((lower->first != height) && (lower != _default_height_font_map.begin())) {
        lower--;
    }
    ESP_BROOKESIA_CHECK_FALSE_RETURN(lower != _update_height_font_map.end(), nullptr, "Font height(%d) is not found", height);

    if (size_px != nullptr) {
        for (auto &it : _default_size_font_map) {
            if (it.second == lower->second) {
                ret_size = it.first;
                break;
            }
        }
        ESP_BROOKESIA_CHECK_FALSE_RETURN(ret_size != 0, nullptr, "Font size is not found");
        *size_px = ret_size;
    }

    return lower->second;
}

bool ESP_Brookesia_CoreHome::calibrateCoreObjectSize(const ESP_Brookesia_StyleSize_t &parent, ESP_Brookesia_StyleSize_t &target) const
{
    uint16_t parent_w = 0;
    uint16_t parent_h = 0;

    parent_w = parent.width;
    parent_h = parent.height;

    // Check width
    if (target.flags.enable_width_percent) {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.width_percent, 1, 100, false, "Invalid width percent");
        target.width = (parent_w * target.width_percent) / 100;
    } else {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.width, 1, parent_w, false, "Invalid width");
    }

    // Check height
    if (target.flags.enable_height_percent) {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.height_percent, 1, 100, false, "Invalid Height percent");
        target.height = (parent_h * target.height_percent) / 100;
    } else {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.height, 1, parent_h, false, "Invalid Height");
    }

    // Process square
    if (target.flags.enable_square) {
        target.width = min(target.width, target.height);
        target.height = target.width;
    }

    return true;
}

bool ESP_Brookesia_CoreHome::calibrateCoreObjectSize(const ESP_Brookesia_StyleSize_t &parent, ESP_Brookesia_StyleSize_t &target,
        bool check_width, bool check_height) const
{
    uint16_t parent_w = 0;
    uint16_t parent_h = 0;

    parent_w = parent.width;
    parent_h = parent.height;

    // Check width
    if (target.flags.enable_width_percent) {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.width_percent, 1, 100, false, "Invalid width percent");
        target.width = (parent_w * target.width_percent) / 100;
    } else if (check_width) {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.width, 1, parent_w, false, "Invalid width");
    }

    // Check height
    if (target.flags.enable_height_percent) {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.height_percent, 1, 100, false, "Invalid Height percent");
        target.height = (parent_h * target.height_percent) / 100;
    } else if (check_height) {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.height, 1, parent_h, false, "Invalid Height");
    }

    // Process square
    if (target.flags.enable_square) {
        target.width = min(target.width, target.height);
        target.height = target.width;
    }

    return true;
}

bool ESP_Brookesia_CoreHome::calibrateCoreObjectSize(const ESP_Brookesia_StyleSize_t &parent, ESP_Brookesia_StyleSize_t &target,
        bool allow_zero) const
{
    uint16_t parent_w = 0;
    uint16_t parent_h = 0;
    uint16_t min_size = allow_zero ? 0 : 1;

    parent_w = parent.width;
    parent_h = parent.height;

    // Check width
    if (target.flags.enable_width_percent) {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.width_percent, min_size, 100, false, "Invalid width percent");
        target.width = (parent_w * target.width_percent) / 100;
    } else {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.width, min_size, parent_w, false, "Invalid width");
    }

    // Check height
    if (target.flags.enable_height_percent) {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.height_percent, min_size, 100, false, "Invalid Height percent");
        target.height = (parent_h * target.height_percent) / 100;
    } else {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.height, min_size, parent_h, false, "Invalid Height");
    }

    // Process square
    if (target.flags.enable_square) {
        target.width = min(target.width, target.height);
        target.height = target.width;
    }

    return true;
}

bool ESP_Brookesia_CoreHome::calibrateCoreFont(const ESP_Brookesia_StyleSize_t *parent, ESP_Brookesia_StyleFont_t &target) const
{
    uint8_t size_px = 0;
    const lv_font_t *font_resource = (const lv_font_t *)target.font_resource;

    if (target.flags.enable_height) {
        goto process_height;
    }

    // Size
    ESP_BROOKESIA_CHECK_VALUE_RETURN(target.size_px, ESP_BROOKESIA_STYLE_FONT_SIZE_MIN, ESP_BROOKESIA_STYLE_FONT_SIZE_MAX, false,
                                     "Invalid size");
    // Font description
    if (font_resource == nullptr) {
        font_resource = getCoreUpdateFontBySize(target.size_px);
        ESP_BROOKESIA_CHECK_NULL_RETURN(font_resource, false, "Get default font failed");
        target.font_resource = font_resource;
        target.height = font_resource->line_height;
    }
    goto end;

process_height:
    // Height
    if (target.flags.enable_height_percent) {
        ESP_BROOKESIA_CHECK_NULL_RETURN(parent, false, "Invalid parent");
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.height_percent, 1, 100, false, "Invalid height percent");
        target.height = (parent->height * target.height_percent) / 100;
    } else if (parent != nullptr) {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(target.height, 1, parent->height, false, "Invalid height");
    }

    // Font description & size
    font_resource = getCoreUpdateFontByHeight(target.height, &size_px);
    ESP_BROOKESIA_CHECK_NULL_RETURN(font_resource, false, "Get default font failed");
    target.font_resource = font_resource;
    target.size_px = size_px;

end:
    return true;
}

bool ESP_Brookesia_CoreHome::calibrateCoreIconImage(const ESP_Brookesia_StyleImage_t &target) const
{
    ESP_BROOKESIA_CHECK_NULL_RETURN(target.resource, false, "Invalid resource");

    return true;
}

bool ESP_Brookesia_CoreHome::processMainScreenLoad(void)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Not initialized");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(lv_obj_is_valid(_main_screen), false, "Invalid main screen");

    lv_scr_load(_main_screen);

    return true;
}

bool ESP_Brookesia_CoreHome::beginCore(void)
{
    lv_obj_t *main_screen = nullptr;
    lv_obj_t *system_screen = nullptr;
    lv_disp_t *display = _core.getDisplayDevice();
    ESP_Brookesia_LvObj_t main_screen_obj = nullptr;
    ESP_Brookesia_LvObj_t system_screen_obj = nullptr;

    ESP_BROOKESIA_LOGD("Begin(0x%p)", this);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(!checkCoreInitialized(), false, "Already initialized");
    ESP_BROOKESIA_CHECK_NULL_RETURN(display, false, "Invalid display device");

    /* Create objects */
    // Main screen
    main_screen = lv_disp_get_scr_act(display);
    ESP_BROOKESIA_CHECK_NULL_RETURN(main_screen, false, "Invalid lvgl current screen");
    main_screen_obj = ESP_BROOKESIA_LV_OBJ(obj, main_screen);
    ESP_BROOKESIA_CHECK_NULL_RETURN(main_screen_obj, false, "Create main screen failed");
    // System screen
    system_screen = lv_disp_get_layer_top(display);
    ESP_BROOKESIA_CHECK_NULL_RETURN(system_screen, false, "Invalid lvgl top screen");
    system_screen_obj = ESP_BROOKESIA_LV_OBJ(obj, system_screen);
    ESP_BROOKESIA_CHECK_NULL_RETURN(system_screen_obj, false, "Create system screen failed");

    /* Setup objects */
    // Container styles
    for (auto &style : _container_styles) {
        lv_style_init(&style);
        lv_style_set_size(&style, LV_SIZE_CONTENT);
        lv_style_set_radius(&style, 0);
        lv_style_set_border_width(&style, 0);
        lv_style_set_pad_all(&style, 0);
        lv_style_set_pad_gap(&style, 0);
        lv_style_set_bg_opa(&style, LV_OPA_TRANSP);
        lv_style_set_outline_width(&style, 0);
    }
    // Main screen
    lv_obj_align(main_screen_obj.get(), LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(main_screen_obj.get(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(main_screen_obj.get(), getCoreContainerStyle(), 0);
    // System screen
    lv_obj_align(system_screen_obj.get(), LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(system_screen_obj.get(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(system_screen_obj.get(), getCoreContainerStyle(), 0);

    /* Save objects */
    _main_screen = main_screen;
    _system_screen = system_screen;
    _main_screen_obj = main_screen_obj;
    _system_screen_obj = system_screen_obj;

    // Update object style
    ESP_BROOKESIA_CHECK_FALSE_GOTO(updateByNewData(), err, "Update object style failed");
    ESP_BROOKESIA_CHECK_FALSE_GOTO(hideContainerBorder(), err, "Hide container border failed");

    return true;

err:
    ESP_BROOKESIA_CHECK_FALSE_RETURN(delCore(), false, "Delete core home failed");

    return false;
}

bool ESP_Brookesia_CoreHome::delCore(void)
{
    ESP_BROOKESIA_LOGD("Delete(0x%p)", this);

    if (!checkCoreInitialized()) {
        return true;
    }

    for (auto &style : _container_styles) {
        lv_style_reset(&style);
    }
    _main_screen_obj.reset();
    _system_screen_obj.reset();
    _main_screen = nullptr;
    _system_screen = nullptr;
    _container_style_index = 0;
    _default_size_font_map.clear();
    _default_height_font_map.clear();
    _update_size_font_map.clear();
    _update_height_font_map.clear();

    return true;
}

bool ESP_Brookesia_CoreHome::updateByNewData(void)
{
    const ESP_Brookesia_StyleSize_t &screen_size = _core.getCoreData().screen_size;

    ESP_BROOKESIA_LOGD("Update core home by new data");

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Not initialized");

    // Change the screen size
    lv_obj_set_size(_main_screen_obj.get(), screen_size.width, screen_size.height);
    lv_obj_set_size(_system_screen_obj.get(), screen_size.width, screen_size.height);

    // Background
    lv_obj_set_style_bg_color(_main_screen_obj.get(), lv_color_hex(_core_data.background.color.color), 0);
    lv_obj_set_style_bg_opa(_main_screen_obj.get(), _core_data.background.color.opacity, 0);
    if (_core_data.background.wallpaper_image_resource.resource != nullptr) {
        lv_obj_set_style_bg_img_src(_main_screen_obj.get(), _core_data.background.wallpaper_image_resource.resource, 0);
    }

    // Text
    _default_size_font_map = _update_size_font_map;

    // Container styles
    for (size_t i = 0; i < _container_styles.size(); i++) {
        lv_style_set_outline_width(&_container_styles[i], _core_data.container.styles[i].outline_width);
        lv_style_set_outline_color(&_container_styles[i],
                                   lv_color_hex(_core_data.container.styles[i].outline_color.color));
        lv_style_set_outline_opa(&_container_styles[i], _core_data.container.styles[i].outline_color.opacity);
    }

    return true;
}

bool ESP_Brookesia_CoreHome::calibrateCoreData(ESP_Brookesia_CoreHomeData_t &data)
{
    const lv_font_t *font_resource = nullptr;

    // Text
    _update_size_font_map.clear();
    _update_height_font_map.clear();
    for (int i = 0; i < data.text.default_fonts_num; i++) {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(data.text.default_fonts[i].size_px, ESP_BROOKESIA_STYLE_FONT_SIZE_MIN,
                                         ESP_BROOKESIA_STYLE_FONT_SIZE_MAX, false, "Invalid default font(%d) size", i);
        ESP_BROOKESIA_CHECK_NULL_RETURN(data.text.default_fonts[i].font_resource, false, "Invalid default font(%d) dsc", i);
        font_resource = (lv_font_t *)data.text.default_fonts[i].font_resource;
        // Save font for function ``
        _update_size_font_map[data.text.default_fonts[i].size_px] = font_resource;
        _update_height_font_map[font_resource->line_height] = font_resource;
    }
    // Check if all default fonts are set, if not, use internal fonts
    for (int i = ESP_BROOKESIA_STYLE_FONT_SIZE_MIN; i <= ESP_BROOKESIA_STYLE_FONT_SIZE_MAX; i += 2) {
        if (_update_size_font_map.find(i) == _update_size_font_map.end()) {
            ESP_BROOKESIA_LOGW("Default font size(%d) is not found, try to use internal font instead", i);
            if (!esp_brookesia_core_utils_get_internal_font_by_size(i, &font_resource)) {
                continue;
            }
            _update_size_font_map[i] = font_resource;
            if (_update_height_font_map.find(font_resource->line_height) == _update_height_font_map.end()) {
                _update_height_font_map[font_resource->line_height] = font_resource;
            }
        }
    }

    return true;
}

const lv_font_t *ESP_Brookesia_CoreHome::getCoreUpdateFontBySize(uint8_t size_px) const
{
    ESP_BROOKESIA_CHECK_VALUE_RETURN(size_px, ESP_BROOKESIA_STYLE_FONT_SIZE_MIN, ESP_BROOKESIA_STYLE_FONT_SIZE_MAX, nullptr, "Invalid size");

    auto it = _update_size_font_map.find(size_px);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(it != _update_size_font_map.end(), nullptr, "Font size(%d) is not found", size_px);

    return it->second;
}

const lv_font_t *ESP_Brookesia_CoreHome::getCoreUpdateFontByHeight(uint8_t height, uint8_t *size_px) const
{
    uint8_t ret_size = 0;

    auto lower = _update_height_font_map.lower_bound(height);
    if ((lower->first != height) && (lower != _update_height_font_map.begin())) {
        lower--;
    }
    ESP_BROOKESIA_CHECK_FALSE_RETURN(lower != _update_height_font_map.end(), nullptr, "Font height(%d) is not found", height);

    if (size_px != nullptr) {
        for (auto &it : _update_size_font_map) {
            if (it.second == lower->second) {
                ret_size = it.first;
                break;
            }
        }
        ESP_BROOKESIA_CHECK_FALSE_RETURN(ret_size != 0, nullptr, "Font size is not found");
        *size_px = ret_size;
    }

    return lower->second;
}
