/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/esp_brookesia_app_settings_utils.hpp"
#include "settings.hpp"

using namespace std;
using namespace esp_brookesia::systems::speaker;

namespace esp_brookesia::apps {

#define CELL_ELEMENT_CONF_WIRELESS() \
    { \
        SettingsUI_WidgetCellElement::MAIN | \
        SettingsUI_WidgetCellElement::LEFT_ICON | SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL | \
        SettingsUI_WidgetCellElement::RIGHT_ICONS | SettingsUI_WidgetCellElement::RIGHT_MAIN_LABEL, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_ELEMENT_CONF_MORE_RESTORE() \
    { \
        SettingsUI_WidgetCellElement::MAIN | \
        SettingsUI_WidgetCellElement::LEFT_ICON | SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_ELEMENT_CONF_GENERAL() \
    { \
        SettingsUI_WidgetCellElement::MAIN | \
        SettingsUI_WidgetCellElement::LEFT_ICON | SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL | \
        SettingsUI_WidgetCellElement::RIGHT_ICONS, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_ELEMENT_CONF_SWITCH() \
    { \
        SettingsUI_WidgetCellElement::MAIN | \
        SettingsUI_WidgetCellElement::LEFT_ICON | SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL | \
        SettingsUI_WidgetCellElement::RIGHT_SWITCH, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_CONTAINER_MAP() \
    { \
        { \
            SettingsUI_ScreenSettingsContainerIndex::WIRELESS, { \
                {}, \
                { \
                    { SettingsUI_ScreenSettingsCellIndex::WIRELESS_WLAN, CELL_ELEMENT_CONF_WIRELESS() }, \
                }, \
            } \
        }, \
        { \
            SettingsUI_ScreenSettingsContainerIndex::MEDIA, { \
                {}, \
                { \
                    { SettingsUI_ScreenSettingsCellIndex::MEDIA_SOUND, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenSettingsCellIndex::MEDIA_DISPLAY, CELL_ELEMENT_CONF_GENERAL() }, \
                }, \
            }, \
        },\
        { \
            SettingsUI_ScreenSettingsContainerIndex::INPUT, { \
                {}, \
                { \
                    { SettingsUI_ScreenSettingsCellIndex::INPUT_TOUCH, CELL_ELEMENT_CONF_SWITCH() }, \
                }, \
            }, \
        },\
        { \
            SettingsUI_ScreenSettingsContainerIndex::MORE, { \
                {}, \
                { \
                    { SettingsUI_ScreenSettingsCellIndex::MORE_ABOUT, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenSettingsCellIndex::MORE_DEVELOPER_MODE, CELL_ELEMENT_CONF_GENERAL() }, \
                    { SettingsUI_ScreenSettingsCellIndex::MORE_RESTORE, CELL_ELEMENT_CONF_MORE_RESTORE() }, \
                }, \
            }, \
        }, \
    }

// *INDENT-OFF*
SettingsUI_ScreenSettings::SettingsUI_ScreenSettings(App &ui_app, const SettingsUI_ScreenBaseData &base_data,
                          const SettingsUI_ScreenSettingsData &main_data):
    SettingsUI_ScreenBase(ui_app, base_data, SettingsUI_ScreenBaseType::ROOT),
    data(main_data)
{
}
// *INDENT-OFF*

SettingsUI_ScreenSettings::~SettingsUI_ScreenSettings()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);

    ESP_UTILS_CHECK_FALSE_EXIT(del(), "Delete failed");
}

bool SettingsUI_ScreenSettings::begin()
{
    ESP_UTILS_LOGD("Begin(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(SettingsUI_ScreenBase::begin("Settings", ""), false, "Screen base begin failed");

    _cell_container_map = CELL_CONTAINER_MAP();
    ESP_UTILS_CHECK_FALSE_GOTO(processCellContainerMapInit(), err, "Process cell container map init failed");
    ESP_UTILS_CHECK_FALSE_GOTO(processDataUpdate(), err, "Process data update failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool SettingsUI_ScreenSettings::del()
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

bool SettingsUI_ScreenSettings::processDataUpdate()
{
    ESP_UTILS_LOGD("Process data update");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(SettingsUI_ScreenBase::processDataUpdate(), false, "Process base data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processCellContainerMapUpdate(), false, "Process cell container map update failed");

    return true;
}

bool SettingsUI_ScreenSettings::processCellContainerMapInit()
{
    ESP_UTILS_LOGD("Process cell container map init");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!SettingsUI_ScreenBase::processCellContainerMapInit<SettingsUI_ScreenSettingsContainerIndex,
         SettingsUI_ScreenSettingsCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map init failed");
        return false;
    }

    return true;
}

bool SettingsUI_ScreenSettings::processCellContainerMapUpdate()
{
    ESP_UTILS_LOGD("Process cell container map init");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    auto &wireless_container_conf = _cell_container_map[SettingsUI_ScreenSettingsContainerIndex::WIRELESS].first;
    wireless_container_conf = data.container_confs[(int)SettingsUI_ScreenSettingsContainerIndex::WIRELESS];
    auto &wireless_cell_confs = _cell_container_map[SettingsUI_ScreenSettingsContainerIndex::WIRELESS].second;
    for (int i = static_cast<int>(SettingsUI_ScreenSettingsCellIndex::WIRELESS_WLAN);
            i <= static_cast<int>(SettingsUI_ScreenSettingsCellIndex::WIRELESS_WLAN); i++) {
        wireless_cell_confs[static_cast<SettingsUI_ScreenSettingsCellIndex>(i)].second = data.cell_confs[i];
    }

    auto &media_container_conf = _cell_container_map[SettingsUI_ScreenSettingsContainerIndex::MEDIA].first;
    media_container_conf = data.container_confs[(int)SettingsUI_ScreenSettingsContainerIndex::MEDIA];
    auto &media_cell_confs = _cell_container_map[SettingsUI_ScreenSettingsContainerIndex::MEDIA].second;
    for (int i = static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MEDIA_SOUND);
            i <= static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MEDIA_DISPLAY); i++) {
        media_cell_confs[static_cast<SettingsUI_ScreenSettingsCellIndex>(i)].second = data.cell_confs[i];
    }

    auto &input_container_conf = _cell_container_map[SettingsUI_ScreenSettingsContainerIndex::INPUT].first;
    input_container_conf = data.container_confs[(int)SettingsUI_ScreenSettingsContainerIndex::INPUT];
    auto &input_cell_confs = _cell_container_map[SettingsUI_ScreenSettingsContainerIndex::INPUT].second;
    for (int i = static_cast<int>(SettingsUI_ScreenSettingsCellIndex::INPUT_TOUCH);
            i <= static_cast<int>(SettingsUI_ScreenSettingsCellIndex::INPUT_TOUCH); i++) {
        input_cell_confs[static_cast<SettingsUI_ScreenSettingsCellIndex>(i)].second = data.cell_confs[i];
    }

    auto &more_container_conf = _cell_container_map[SettingsUI_ScreenSettingsContainerIndex::MORE].first;
    more_container_conf = data.container_confs[(int)SettingsUI_ScreenSettingsContainerIndex::MORE];
    auto &more_cell_confs = _cell_container_map[SettingsUI_ScreenSettingsContainerIndex::MORE].second;
    for (int i = static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MORE_ABOUT);
            i <= static_cast<int>(SettingsUI_ScreenSettingsCellIndex::MORE_RESTORE); i++) {
        more_cell_confs[static_cast<SettingsUI_ScreenSettingsCellIndex>(i)].second = data.cell_confs[i];
    }

    if (!SettingsUI_ScreenBase::processCellContainerMapUpdate<SettingsUI_ScreenSettingsContainerIndex,
         SettingsUI_ScreenSettingsCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map uipdate failed");
        return false;
    }

    return true;
}

} // namespace esp_brookesia::apps
