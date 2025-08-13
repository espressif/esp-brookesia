/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/esp_brookesia_app_settings_utils.hpp"
#include "sound.hpp"

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
#define CELL_CONTAINER_MAP() \
    { \
        { \
            SettingsUI_ScreenSoundContainerIndex::VOLUME, { \
                {}, \
                { \
                    { SettingsUI_ScreenSoundCellIndex::VOLUME_SLIDER, CELL_ELEMENT_CONF_SLIDER() }, \
                }, \
            } \
        }, \
    }

SettingsUI_ScreenSound::SettingsUI_ScreenSound(App &ui_app, const SettingsUI_ScreenBaseData &base_data,
        const SettingsUI_ScreenSoundData &main_data):
    SettingsUI_ScreenBase(ui_app, base_data, SettingsUI_ScreenBaseType::CHILD),
    data(main_data)
{
}

SettingsUI_ScreenSound::~SettingsUI_ScreenSound()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);

    ESP_UTILS_CHECK_FALSE_EXIT(del(), "Delete failed");
}

bool SettingsUI_ScreenSound::begin()
{
    ESP_UTILS_LOGD("Begin(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(
        SettingsUI_ScreenBase::begin("Sound", "Settings"), false, "Screen base begin failed"
    );

    _cell_container_map = CELL_CONTAINER_MAP();
    ESP_UTILS_CHECK_FALSE_GOTO(processCellContainerMapInit(), err, "Process cell container map init failed");
    ESP_UTILS_CHECK_FALSE_GOTO(processDataUpdate(), err, "Process data update failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool SettingsUI_ScreenSound::del()
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

bool SettingsUI_ScreenSound::processDataUpdate()
{
    ESP_UTILS_LOGD("Process data update");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(SettingsUI_ScreenBase::processDataUpdate(), false, "Process base data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processCellContainerMapUpdate(), false, "Process cell container map update failed");

    return true;
}

bool SettingsUI_ScreenSound::processCellContainerMapInit()
{
    ESP_UTILS_LOGD("Process cell container map init");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!SettingsUI_ScreenBase::processCellContainerMapInit<SettingsUI_ScreenSoundContainerIndex,
            SettingsUI_ScreenSoundCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map init failed");
        return false;
    }

    return true;
}

bool SettingsUI_ScreenSound::processCellContainerMapUpdate()
{
    ESP_UTILS_LOGD("Process cell container map init");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    auto &system_container_conf = _cell_container_map[SettingsUI_ScreenSoundContainerIndex::VOLUME].first;
    system_container_conf = data.container_confs[(int)SettingsUI_ScreenSoundContainerIndex::VOLUME];
    auto &system_cell_confs = _cell_container_map[SettingsUI_ScreenSoundContainerIndex::VOLUME].second;
    for (int i = static_cast<int>(SettingsUI_ScreenSoundCellIndex::VOLUME_SLIDER);
            i <= static_cast<int>(SettingsUI_ScreenSoundCellIndex::VOLUME_SLIDER); i++) {
        system_cell_confs[static_cast<SettingsUI_ScreenSoundCellIndex>(i)].second = data.cell_confs[i];
    }

    if (!SettingsUI_ScreenBase::processCellContainerMapUpdate<SettingsUI_ScreenSoundContainerIndex,
            SettingsUI_ScreenSoundCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map uipdate failed");
        return false;
    }

    return true;
}

} // namespace esp_brookesia::apps
