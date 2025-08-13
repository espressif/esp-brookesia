/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/esp_brookesia_app_settings_utils.hpp"
#include "about.hpp"

using namespace std;
using namespace esp_brookesia::systems::speaker;

namespace esp_brookesia::apps {

#define CELL_ELEMENT_CONF_GENERAL() \
    { \
        SettingsUI_WidgetCellElement::MAIN | \
        SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL | \
        SettingsUI_WidgetCellElement::RIGHT_MAIN_LABEL, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_ELEMENT_CONF_RAM() \
    { \
        SettingsUI_WidgetCellElement::MAIN | \
        SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL | \
        SettingsUI_WidgetCellElement::RIGHT_MAIN_LABEL | SettingsUI_WidgetCellElement::RIGHT_MINOR_LABEL, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_CONTAINER_MAP() \
    { \
        { \
            SettingsUI_ScreenAboutContainerIndex::SYSTEM, { \
                {}, \
                { \
                    { SettingsUI_ScreenAboutCellIndex::SYSTEM_FIRMWARE_VERSION, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenAboutCellIndex::SYSTEM_OS_NAME, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenAboutCellIndex::SYSTEM_OS_VERSION, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenAboutCellIndex::SYSTEM_UI_NAME, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenAboutCellIndex::SYSTEM_UI_VERSION, CELL_ELEMENT_CONF_GENERAL() }, \
                }, \
            } \
        }, \
        { \
            SettingsUI_ScreenAboutContainerIndex::DEVICE, { \
                {}, \
                { \
                    { SettingsUI_ScreenAboutCellIndex::DEVICE_MANUFACTURER, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenAboutCellIndex::DEVICE_NAME, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenAboutCellIndex::DEVICE_RESOLUTION, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenAboutCellIndex::DEVICE_FLASH_SIZE, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenAboutCellIndex::DEVICE_RAM_SIZE, CELL_ELEMENT_CONF_RAM() }, \
                    { SettingsUI_ScreenAboutCellIndex::DEVICE_BATTERY_CAPACITY, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenAboutCellIndex::DEVICE_BATTERY_VOLTAGE, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenAboutCellIndex::DEVICE_BATTERY_CURRENT, CELL_ELEMENT_CONF_GENERAL() }, \
                }, \
            } \
        }, \
        { \
            SettingsUI_ScreenAboutContainerIndex::CHIP, { \
                {}, \
                { \
                    { SettingsUI_ScreenAboutCellIndex::CHIP_NAME, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenAboutCellIndex::CHIP_VERSION, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenAboutCellIndex::CHIP_MAC, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenAboutCellIndex::CHIP_FEATURES, CELL_ELEMENT_CONF_GENERAL() }, \
                }, \
            } \
        }, \
    }

SettingsUI_ScreenAbout::SettingsUI_ScreenAbout(App &ui_app, const SettingsUI_ScreenBaseData &base_data,
        const SettingsUI_ScreenAboutData &main_data):
    SettingsUI_ScreenBase(ui_app, base_data, SettingsUI_ScreenBaseType::CHILD),
    data(main_data)
{
}

SettingsUI_ScreenAbout::~SettingsUI_ScreenAbout()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);

    ESP_UTILS_CHECK_FALSE_EXIT(del(), "Delete failed");
}

bool SettingsUI_ScreenAbout::begin()
{
    ESP_UTILS_LOGD("Begin(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(SettingsUI_ScreenBase::begin("About", "Settings"), false, "Screen base begin failed");

    _cell_container_map = CELL_CONTAINER_MAP();
    ESP_UTILS_CHECK_FALSE_GOTO(processCellContainerMapInit(), err, "Process cell container map init failed");
    ESP_UTILS_CHECK_FALSE_GOTO(processDataUpdate(), err, "Process data update failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool SettingsUI_ScreenAbout::del()
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

bool SettingsUI_ScreenAbout::processDataUpdate()
{
    ESP_UTILS_LOGD("Process data update");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(SettingsUI_ScreenBase::processDataUpdate(), false, "Process base data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processCellContainerMapUpdate(), false, "Process cell container map update failed");

    return true;
}

bool SettingsUI_ScreenAbout::processCellContainerMapInit()
{
    ESP_UTILS_LOGD("Process cell container map init");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!SettingsUI_ScreenBase::processCellContainerMapInit<SettingsUI_ScreenAboutContainerIndex,
            SettingsUI_ScreenAboutCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map init failed");
        return false;
    }

    return true;
}

bool SettingsUI_ScreenAbout::processCellContainerMapUpdate()
{
    ESP_UTILS_LOGD("Process cell container map init");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    auto &system_container_conf = _cell_container_map[SettingsUI_ScreenAboutContainerIndex::SYSTEM].first;
    system_container_conf = data.container_confs[(int)SettingsUI_ScreenAboutContainerIndex::SYSTEM];
    auto &system_cell_confs = _cell_container_map[SettingsUI_ScreenAboutContainerIndex::SYSTEM].second;
    for (int i = static_cast<int>(SettingsUI_ScreenAboutCellIndex::SYSTEM_FIRMWARE_VERSION);
            i <= static_cast<int>(SettingsUI_ScreenAboutCellIndex::SYSTEM_UI_VERSION); i++) {
        system_cell_confs[static_cast<SettingsUI_ScreenAboutCellIndex>(i)].second = data.cell_confs[i];
    }

    auto &chip_container_conf = _cell_container_map[SettingsUI_ScreenAboutContainerIndex::DEVICE].first;
    chip_container_conf = data.container_confs[(int)SettingsUI_ScreenAboutContainerIndex::DEVICE];
    auto &chip_cell_confs = _cell_container_map[SettingsUI_ScreenAboutContainerIndex::DEVICE].second;
    for (int i = static_cast<int>(SettingsUI_ScreenAboutCellIndex::DEVICE_MANUFACTURER);
            i <= static_cast<int>(SettingsUI_ScreenAboutCellIndex::DEVICE_BATTERY_CURRENT); i++) {
        chip_cell_confs[static_cast<SettingsUI_ScreenAboutCellIndex>(i)].second = data.cell_confs[i];
    }

    auto &device_container_conf = _cell_container_map[SettingsUI_ScreenAboutContainerIndex::CHIP].first;
    device_container_conf = data.container_confs[(int)SettingsUI_ScreenAboutContainerIndex::CHIP];
    auto &device_cell_confs = _cell_container_map[SettingsUI_ScreenAboutContainerIndex::CHIP].second;
    for (int i = static_cast<int>(SettingsUI_ScreenAboutCellIndex::CHIP_NAME);
            i <= static_cast<int>(SettingsUI_ScreenAboutCellIndex::CHIP_FEATURES); i++) {
        device_cell_confs[static_cast<SettingsUI_ScreenAboutCellIndex>(i)].second = data.cell_confs[i];
    }

    if (!SettingsUI_ScreenBase::processCellContainerMapUpdate<SettingsUI_ScreenAboutContainerIndex,
            SettingsUI_ScreenAboutCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map uipdate failed");
        return false;
    }

    return true;
}

} // namespace esp_brookesia::apps
