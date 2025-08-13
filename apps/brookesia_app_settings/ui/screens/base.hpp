/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <map>
#include "style/esp_brookesia_gui_style.hpp"
#include "private/esp_brookesia_app_settings_utils.hpp"
#include "esp_brookesia.hpp"
#include "gui/lvgl/esp_brookesia_lv.hpp"
#include "../widgets/cell_container.hpp"

namespace esp_brookesia::apps {

enum class SettingsUI_ScreenBaseType {
    ROOT,
    CHILD,
};

enum class SettingsUI_ScreenBaseObject {
    HEADER_OBJECT = 0,
    HEADER_TITLE_LABEL,
    NAVIGATION_MAIN_OBJECT,
    NAVIGATION_ICON_OBJECT,
    NAVIGATION_ICON_IMAGE,
    NAVIGATION_TITLE_LABEL,
    CONTENT_OBJECT,
    MAX,
};

struct SettingsUI_ScreenBaseHeaderData {
    gui::StyleSize size;
    uint16_t align_top_offset;
    gui::StyleColor background_color;
    gui::StyleFont title_text_font;
    gui::StyleColor title_text_color;
};

struct SettingsUI_ScreenBaseContentData {
    gui::StyleSize size;
    uint16_t align_bottom_offset;
    gui::StyleColor background_color;
    uint16_t row_pad;
    uint16_t top_pad;
    uint16_t bottom_pad;
    uint16_t left_pad;
    uint16_t right_pad;
};

struct SettingsUI_ScreenBaseHeaderNavigation {
    gui::StyleAlign align;
    uint16_t main_column_pad;
    gui::StyleSize icon_size;
    gui::StyleImage icon_image;
    gui::StyleFont title_text_font;
    gui::StyleColor title_text_color;
};

struct SettingsUI_ScreenBaseData {
    struct {
        gui::StyleSize size;
        gui::StyleColor background_color;
    } screen;
    SettingsUI_ScreenBaseHeaderData header;
    SettingsUI_ScreenBaseHeaderNavigation header_navigation;
    SettingsUI_ScreenBaseContentData content;
    SettingsUI_WidgetCellContainerData cell_container;
    struct {
        uint8_t enable_header_title: 1;
        uint8_t enable_content_size_flex: 1;
    } flags;
};

template <typename T_CellIndex>
using SettingsUI_ScreenBaseCellMap =
    std::map<T_CellIndex, std::pair<SettingsUI_WidgetCellElement, SettingsUI_WidgetCellConf>>;

template <typename T_ContainerIndex, typename T_CellIndex>
using SettingsUI_ScreenBaseCellContainerMap =
    std::map<T_ContainerIndex, std::pair<SettingsUI_WidgetCellContainerConf, SettingsUI_ScreenBaseCellMap<T_CellIndex>>>;

class SettingsUI_ScreenBase {
public:
    SettingsUI_ScreenBase(
        systems::speaker::App &ui_app, const SettingsUI_ScreenBaseData &base_data, SettingsUI_ScreenBaseType type
    );
    ~SettingsUI_ScreenBase();

    SettingsUI_ScreenBase(const SettingsUI_ScreenBase &) = delete;
    SettingsUI_ScreenBase(SettingsUI_ScreenBase &&) = delete;
    SettingsUI_ScreenBase &operator=(const SettingsUI_ScreenBase &) = delete;
    SettingsUI_ScreenBase &operator=(SettingsUI_ScreenBase &&) = delete;

    bool begin(std::string header_title_name, std::string navigation_title_name);
    bool del();
    bool processDataUpdate();
    SettingsUI_WidgetCellContainer *addCellContainer(int key);
    bool delCellContainer(int key);

    bool checkInitialized() const
    {
        return (app.checkInitialized() && (_screen_object != nullptr));
    }
    lv_obj_t *getScreenObject() const
    {
        return _screen_object;
    }
    lv_obj_t *getObject(SettingsUI_ScreenBaseObject object) const
    {
        ESP_UTILS_CHECK_FALSE_RETURN((int)object < (int)_objects.size(), nullptr, "Invalid object(%d)", (int)object);
        return _objects[(int)object].get();
    }
    SettingsUI_WidgetCellContainer *getCellContainer(int key) const
    {
        auto it = _cell_containers_map.find(key);
        return (it != _cell_containers_map.end()) ? it->second.get() : nullptr;
    }
    SettingsUI_WidgetCell *getCell(int container_key, int cell_key) const;
    lv_obj_t *getElementObject(int container_key, int cell_key, SettingsUI_WidgetCellElement element) const;
    lv_obj_t *getEventObject() const
    {
        return _screen_object;
    }
    systems::base::Event::ID getNavigaitionClickEventID() const
    {
        return _navigation_click_event_id;
    }

    static bool calibrateCommonHeader(
        const gui::StyleSize &parent_size, const systems::base::Display &display,
        SettingsUI_ScreenBaseHeaderData &data
    );
    static bool calibrateCommonContent(
        const gui::StyleSize &parent_size, const systems::base::Display &display,
        SettingsUI_ScreenBaseContentData &data
    );
    static bool calibrateHeaderNavigation(
        const gui::StyleSize &parent_size, const systems::base::Display &display,
        SettingsUI_ScreenBaseHeaderNavigation &data
    );
    static bool calibrateData(
        const gui::StyleSize &parent_size, const systems::base::Display &display,
        SettingsUI_ScreenBaseData &data
    );

    systems::speaker::App &app;
    const SettingsUI_ScreenBaseData &data;

protected:
    template <typename T_ContainerIndex, typename T_CellIndex>
    bool processCellContainerMapInit(const SettingsUI_ScreenBaseCellContainerMap<T_ContainerIndex, T_CellIndex> &map)
    {
        ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

        for (auto &container_it : map) {
            auto container = addCellContainer(static_cast<int>(container_it.first));
            ESP_UTILS_CHECK_NULL_RETURN(container, false, "Add cell container failed");
            auto cells = container_it.second.second;
            for (auto &cell_it : cells) {
                auto cell = container->addCell(static_cast<int>(cell_it.first), cell_it.second.first);
                ESP_UTILS_CHECK_NULL_RETURN(cell, false, "Add cell failed");
            }
        }

        return true;
    }

    template <typename T_ContainerIndex, typename T_CellIndex>
    bool processCellContainerMapUpdate(const SettingsUI_ScreenBaseCellContainerMap<T_ContainerIndex, T_CellIndex> &map)
    {
        ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

        for (auto &container_it : map) {
            auto container = getCellContainer(static_cast<int>(container_it.first));
            ESP_UTILS_CHECK_NULL_RETURN(container, false, "Get cell container failed");
            ESP_UTILS_CHECK_FALSE_RETURN(container->updateConf(container_it.second.first), false,
                                         "Update container(%d) conf failed", static_cast<int>(container_it.first));
            auto cells = container_it.second.second;
            for (auto &cell_it : cells) {
                auto cell = container->getCellByKey(static_cast<int>(cell_it.first));
                ESP_UTILS_CHECK_NULL_RETURN(cell, false, "Get cell failed");
                ESP_UTILS_CHECK_FALSE_RETURN(
                    cell->updateConf(cell_it.second.second), false, "Update cell conf(%d, %d) failed",
                    static_cast<int>(container_it.first), static_cast<int>(cell_it.first)
                );
            }
        }

        return true;
    }

    static void onNavigationTouchEventCallback(lv_event_t *event);

private:
    struct {
        uint8_t is_navigation_pressed_losted: 1;
    } _flags;
    // Don't use smart pointers here, avoid deleting screen twice
    lv_obj_t *_screen_object;
    systems::base::Event::ID _navigation_click_event_id;
    SettingsUI_ScreenBaseType _type;
    std::array<ESP_Brookesia_LvObj_t, (int)SettingsUI_ScreenBaseObject::MAX> _objects;
    std::map<int, std::unique_ptr<SettingsUI_WidgetCellContainer>> _cell_containers_map;
};

} // namespace esp_brookesia::apps
