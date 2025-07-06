/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <list>
#include <sys/types.h>
#include "esp_brookesia.hpp"

namespace esp_brookesia::speaker_apps {

struct SettingsUI_WidgetCellData {
    struct {
        ESP_Brookesia_StyleSize_t size;
        uint8_t radius;
        ESP_Brookesia_StyleColor_t active_background_color;
        ESP_Brookesia_StyleColor_t inactive_background_color;
    } main;
    struct {
        uint16_t left_align_x_offset;
        uint16_t left_column_pad;
        uint16_t right_align_x_offset;
        uint16_t right_column_pad;
    } area;
    struct {
        ESP_Brookesia_StyleSize_t left_size;
        ESP_Brookesia_StyleSize_t right_size;
    } icon;
    struct {
        ESP_Brookesia_StyleSize_t main_size;
        ESP_Brookesia_StyleColor_t active_indicator_color;
        ESP_Brookesia_StyleColor_t inactive_indicator_color;
        ESP_Brookesia_StyleSize_t knob_size;
        ESP_Brookesia_StyleColor_t knob_color;
    } sw;
    struct {
        uint8_t width;
        ESP_Brookesia_StyleColor_t color;
    } split_line;
    struct {
        uint16_t left_row_pad;
        uint16_t right_row_pad;
        ESP_Brookesia_StyleFont_t left_main_text_font;
        ESP_Brookesia_StyleColor_t left_main_text_color;
        ESP_Brookesia_StyleFont_t left_minor_text_font;
        ESP_Brookesia_StyleColor_t left_minor_text_color;
        ESP_Brookesia_StyleFont_t right_main_text_font;
        ESP_Brookesia_StyleColor_t right_main_text_color;
        ESP_Brookesia_StyleFont_t right_minor_text_font;
        ESP_Brookesia_StyleColor_t right_minor_text_color;
    } label;
    struct {
        ESP_Brookesia_StyleSize_t size;
        ESP_Brookesia_StyleFont_t text_font;
        ESP_Brookesia_StyleColor_t text_color;
        ESP_Brookesia_StyleColor_t cursor_color;
    } text_edit;
    struct {
        ESP_Brookesia_StyleSize_t main_size;
        ESP_Brookesia_StyleColor_t main_color;
        ESP_Brookesia_StyleColor_t indicator_color;
        ESP_Brookesia_StyleSize_t knob_size;
        ESP_Brookesia_StyleColor_t knob_color;
    } slider;
};

enum class SettingsUI_WidgetCellElement {
    MAIN              = 0,
    LEFT_ICON         = (1U << 0),
    LEFT_MAIN_LABEL   = (1U << 1),
    LEFT_MINOR_LABEL  = (1U << 2),
    LEFT_TEXT_EDIT    = (1U << 3),
    _LEFT_LABEL       = ((1U << 4) | LEFT_MAIN_LABEL | LEFT_MINOR_LABEL),
    _LEFT_AREA        = ((1U << 5) | LEFT_ICON | _LEFT_LABEL | LEFT_TEXT_EDIT),
    RIGHT_MAIN_LABEL  = (1U << 6),
    RIGHT_MINOR_LABEL = (1U << 7),
    _RIGHT_LABEL      = ((1U << 8) | RIGHT_MAIN_LABEL | RIGHT_MINOR_LABEL),
    RIGHT_ICONS       = (1U << 9),
    RIGHT_SWITCH      = (1U << 10),
    _RIGHT_AREA       = ((1U << 11) | _RIGHT_LABEL | RIGHT_ICONS | RIGHT_SWITCH),
    CENTER_SLIDER     = (1U << 12),
    _CENTER_AREA      = ((1U << 13) | CENTER_SLIDER),
};

inline SettingsUI_WidgetCellElement operator|(SettingsUI_WidgetCellElement lhs, SettingsUI_WidgetCellElement rhs)
{
    return static_cast<SettingsUI_WidgetCellElement>(
               static_cast<std::underlying_type<SettingsUI_WidgetCellElement>::type>(lhs) |
               static_cast<std::underlying_type<SettingsUI_WidgetCellElement>::type>(rhs)
           );
}

inline bool operator&(SettingsUI_WidgetCellElement lhs, SettingsUI_WidgetCellElement rhs)
{
    return static_cast<bool>(
               static_cast<std::underlying_type<SettingsUI_WidgetCellElement>::type>(lhs) &
               static_cast<std::underlying_type<SettingsUI_WidgetCellElement>::type>(rhs)
           );
}

struct SettingsUI_WidgetCellConf {
    ESP_Brookesia_StyleSize_t left_icon_size;
    ESP_Brookesia_StyleImage_t left_icon_image;
    std::string left_main_label_text;
    std::string left_minor_label_text;
    std::string left_text_edit_placeholder_text;
    std::string right_main_label_text;
    std::string right_minor_label_text;
    ESP_Brookesia_StyleSize_t right_icon_size;
    std::vector<ESP_Brookesia_StyleImage_t> right_icon_images;
    struct {
        uint8_t enable_left_icon: 1;
        uint8_t enable_left_main_label: 1;
        uint8_t enable_left_minor_label: 1;
        uint8_t enable_left_text_edit_placeholder: 1;
        uint8_t enable_right_main_label: 1;
        uint8_t enable_right_minor_label: 1;
        uint8_t enable_right_icons: 1;
        uint8_t enable_clickable: 1;
    } flags;
};

class SettingsUI_WidgetCell {
public:
    SettingsUI_WidgetCell(speaker::App &ui_app, const SettingsUI_WidgetCellData &cell_data,
                          SettingsUI_WidgetCellElement elements);
    ~SettingsUI_WidgetCell();

    bool begin(lv_obj_t *parent);
    bool del();
    bool setSplitLineVisible(bool visible);
    bool processDataUpdate();
    bool updateConf(const SettingsUI_WidgetCellConf &conf);
    bool updateLeftIcon(const ESP_Brookesia_StyleSize_t &size, const ESP_Brookesia_StyleImage_t &image);
    bool updateLeftMainLabel(std::string text);
    bool updateLeftMinorLabel(std::string text);
    bool updateLeftTextEditPlaceholder(std::string text);
    bool updateRightMainLabel(std::string text);
    bool updateRightMinorLabel(std::string text);
    bool updateRightIcons(const ESP_Brookesia_StyleSize_t &size, const std::vector<ESP_Brookesia_StyleImage_t> &right_icons);
    bool updateClickable(bool clickable);

    bool checkElementExist(SettingsUI_WidgetCellElement element) const
    {
        return (_elements_map.find(element) != _elements_map.end());
    }
    bool checkInitialized() const
    {
        return checkElementExist(SettingsUI_WidgetCellElement::MAIN) &&
               (_elements_map.at(SettingsUI_WidgetCellElement::MAIN) != nullptr);
    }
    lv_obj_t *getElementObject(SettingsUI_WidgetCellElement element) const
    {
        if (!checkElementExist(element)) {
            return nullptr;
        }
        return _elements_map.at(element).get();
    }
    void *getEventObject() const
    {
        return (void *)this;
    }
    ESP_Brookesia_CoreEvent::ID getClickEventID() const
    {
        return _click_event_code;
    }

    static bool calibrateData(const ESP_Brookesia_StyleSize_t &parent_size, const ESP_Brookesia_CoreHome &home,
                              SettingsUI_WidgetCellData &data);

    const SettingsUI_WidgetCellData &data;

private:
    bool updateIconImage(lv_obj_t *icon, const ESP_Brookesia_StyleImage_t &image, const ESP_Brookesia_StyleSize_t &size);

    static void onCellTouchEventCallback(lv_event_t *event);

    struct {
        uint8_t is_cell_pressed_losted: 1;
        uint8_t is_cell_click_disable: 1;
    } _flags = {};
    speaker::App &_core_app;
    ESP_Brookesia_LvObj_t _left_icon_object;
    ESP_Brookesia_CoreEvent::ID _click_event_code;
    ESP_Brookesia_LvObj_t _split_line;
    std::array<lv_point_precise_t, 2> _split_line_points;
    SettingsUI_WidgetCellConf _elements_conf;
    SettingsUI_WidgetCellElement _elements;
    std::map<SettingsUI_WidgetCellElement, ESP_Brookesia_LvObj_t> _elements_map;
    std::vector<std::pair<ESP_Brookesia_LvObj_t, ESP_Brookesia_LvObj_t>> _right_icon_object_images;
};

struct SettingsUI_WidgetCellContainerData {
    struct {
        uint16_t row_pad;
    } main;
    struct {
        ESP_Brookesia_StyleFont_t text_font;
        ESP_Brookesia_StyleColor_t text_color;
    } title;
    struct {
        ESP_Brookesia_StyleSize_t size;
        uint8_t radius;
        ESP_Brookesia_StyleColor_t background_color;
        uint16_t top_pad;
        uint16_t bottom_pad;
        uint16_t left_pad;
        uint16_t right_pad;
    } container;
    SettingsUI_WidgetCellData cell;
};

struct SettingsUI_WidgetCellContainerConf {
    std::string title_text;
    struct {
        uint8_t enable_title: 1;
    } flags;
};

class SettingsUI_WidgetCellContainer {
public:
    SettingsUI_WidgetCellContainer(speaker::App &ui_app,
                                   const SettingsUI_WidgetCellContainerData &container_data);
    ~SettingsUI_WidgetCellContainer();

    bool begin(lv_obj_t *parent);
    bool del();
    bool processDataUpdate();
    SettingsUI_WidgetCell *addCell(int key, SettingsUI_WidgetCellElement elements);
    bool cleanCells();
    bool delCellByKey(int key);
    bool delCellByIndex(size_t index);
    bool updateConf(const SettingsUI_WidgetCellContainerConf &conf);

    bool checkInitialized() const
    {
        return (_main_object != nullptr);
    }
    lv_obj_t *getMainObject() const
    {
        return _main_object.get();
    }
    size_t getCellCount() const
    {
        return _cells.size();
    }
    SettingsUI_WidgetCell *getCellByKey(int key) const;
    SettingsUI_WidgetCell *getCellByIndex(size_t index) const;
    int getCellIndex(SettingsUI_WidgetCell *cell) const;

    static bool calibrateData(const ESP_Brookesia_StyleSize_t &parent_size, const ESP_Brookesia_CoreHome &home,
                              SettingsUI_WidgetCellContainerData &data);

    const SettingsUI_WidgetCellContainerData &data;

private:
    speaker::App &_core_app;
    ESP_Brookesia_LvObj_t _main_object;
    ESP_Brookesia_LvObj_t _container_object;
    ESP_Brookesia_LvObj_t _title_label;
    SettingsUI_WidgetCellContainerConf _conf;
    SettingsUI_WidgetCell *_last_cell;
    std::list<std::pair<int, std::unique_ptr<SettingsUI_WidgetCell>>> _cells;
};

} // namespace esp_brookesia::speaker_apps
