/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/esp_brookesia_app_settings_utils.hpp"
#include "display.hpp"

using namespace std;
using namespace esp_brookesia::systems::speaker;

namespace esp_brookesia::apps {

#define CELL_ELEMENT_CONF_SLIDER() \
    { \
        SettingsUI_WidgetCellElement::MAIN | \
        SettingsUI_WidgetCellElement::LEFT_ICON | \
        SettingsUI_WidgetCellElement::CENTER_SLIDER | \
        SettingsUI_WidgetCellElement::RIGHT_ICONS, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_ELEMENT_CONF_AUTO() \
    { \
        SettingsUI_WidgetCellElement::MAIN | \
        SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL | \
        SettingsUI_WidgetCellElement::RIGHT_SWITCH, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_CONTAINER_MAP() \
    { \
        { \
            SettingsUI_ScreenDisplayContainerIndex::BRIGHTNESS, { \
                {}, \
                { \
                    { SettingsUI_ScreenDisplayCellIndex::BRIGHTNESS_SLIDER, CELL_ELEMENT_CONF_SLIDER() }, \
                    { SettingsUI_ScreenDisplayCellIndex::BRIGHTNESS_AUTO, CELL_ELEMENT_CONF_AUTO() }, \
                }, \
            } \
        }, \
    }

SettingsUI_ScreenDisplay::SettingsUI_ScreenDisplay(App &ui_app, const SettingsUI_ScreenBaseData &base_data,
        const SettingsUI_ScreenDisplayData &main_data):
    SettingsUI_ScreenBase(ui_app, base_data, SettingsUI_ScreenBaseType::CHILD),
    data(main_data)
{
}

SettingsUI_ScreenDisplay::~SettingsUI_ScreenDisplay()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);

    ESP_UTILS_CHECK_FALSE_EXIT(del(), "Delete failed");
}

bool SettingsUI_ScreenDisplay::begin()
{
    ESP_UTILS_LOGD("Begin(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(
        SettingsUI_ScreenBase::begin("Display", "Settings"), false, "Screen base begin failed"
    );

    _cell_container_map = CELL_CONTAINER_MAP();
    ESP_UTILS_CHECK_FALSE_GOTO(processCellContainerMapInit(), err, "Process cell container map init failed");
    ESP_UTILS_CHECK_FALSE_GOTO(processDataUpdate(), err, "Process data update failed");

    {
        auto cell = getCell(
                        static_cast<int>(SettingsUI_ScreenDisplayContainerIndex::BRIGHTNESS),
                        static_cast<int>(SettingsUI_ScreenDisplayCellIndex::BRIGHTNESS_SLIDER)
                    );
        ESP_UTILS_CHECK_NULL_GOTO(cell, err, "Get cell failed");
        cell->setSplitLineVisible(false);
    }

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool SettingsUI_ScreenDisplay::del()
{
    ESP_UTILS_LOGD("Delete(0x%p)", this);
    if (!checkInitialized()) {
        return true;
    }

    bool ret = true;
    if (!SettingsUI_ScreenBase::del()) {
        ret  = false;
        ESP_UTILS_LOGE("Screen base delete failed");
    }

    _cell_container_map.clear();

    return ret;
}

bool SettingsUI_ScreenDisplay::processDataUpdate()
{
    ESP_UTILS_LOGD("Process data update");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(SettingsUI_ScreenBase::processDataUpdate(), false, "Process base data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processCellContainerMapUpdate(), false, "Process cell container map update failed");

    return true;
}

bool SettingsUI_ScreenDisplay::processCellContainerMapInit()
{
    ESP_UTILS_LOGD("Process cell container map init");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!SettingsUI_ScreenBase::processCellContainerMapInit<SettingsUI_ScreenDisplayContainerIndex,
            SettingsUI_ScreenDisplayCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map init failed");
        return false;
    }

    return true;
}

bool SettingsUI_ScreenDisplay::processCellContainerMapUpdate()
{
    ESP_UTILS_LOGD("Process cell container map init");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    auto &system_container_conf = _cell_container_map[SettingsUI_ScreenDisplayContainerIndex::BRIGHTNESS].first;
    system_container_conf = data.container_confs[(int)SettingsUI_ScreenDisplayContainerIndex::BRIGHTNESS];
    auto &system_cell_confs = _cell_container_map[SettingsUI_ScreenDisplayContainerIndex::BRIGHTNESS].second;
    for (int i = static_cast<int>(SettingsUI_ScreenDisplayCellIndex::BRIGHTNESS_SLIDER);
            i <= static_cast<int>(SettingsUI_ScreenDisplayCellIndex::BRIGHTNESS_AUTO); i++) {
        system_cell_confs[static_cast<SettingsUI_ScreenDisplayCellIndex>(i)].second = data.cell_confs[i];
    }

    if (!SettingsUI_ScreenBase::processCellContainerMapUpdate<SettingsUI_ScreenDisplayContainerIndex,
            SettingsUI_ScreenDisplayCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map uipdate failed");
        return false;
    }

    return true;
}

} // namespace esp_brookesia::apps
