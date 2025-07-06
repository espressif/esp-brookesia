/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/esp_brookesia_app_settings_utils.hpp"
#include "../../assets/esp_brookesia_app_settings_assets.h"
#include "wlan.hpp"

using namespace std;
using namespace esp_brookesia::speaker;

namespace esp_brookesia::speaker_apps {

#define CELL_ELEMENT_CONF_SW() \
    { \
        SettingsUI_WidgetCellElement::MAIN \
        | SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL \
        | SettingsUI_WidgetCellElement::RIGHT_SWITCH, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_ELEMENT_CONF_AP() \
    { \
        SettingsUI_WidgetCellElement::MAIN \
        | SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL \
        | SettingsUI_WidgetCellElement::LEFT_MINOR_LABEL \
        | SettingsUI_WidgetCellElement::RIGHT_ICONS, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_CONTAINER_MAP() \
    { \
        { \
            SettingsUI_ScreenWlanContainerIndex::CONTROL, { \
                {}, \
                { \
                    { SettingsUI_ScreenWlanCellIndex::CONTROL_SW, CELL_ELEMENT_CONF_SW() }, \
                }, \
            } \
        }, \
        { \
            SettingsUI_ScreenWlanContainerIndex::CONNECTED, { \
                {}, \
                { \
                    { SettingsUI_ScreenWlanCellIndex::CONNECTED_AP, CELL_ELEMENT_CONF_AP() }, \
                }, \
            }, \
        },\
        { \
            SettingsUI_ScreenWlanContainerIndex::AVAILABLE, { {}, {}, }, \
        },\
    }

// *INDENT-OFF*
SettingsUI_ScreenWlan::SettingsUI_ScreenWlan(speaker::App &ui_app, const SettingsUI_ScreenBaseData &base_data,
                          const SettingsUI_ScreenWlanData &main_data):
    SettingsUI_ScreenBase(ui_app, base_data, SettingsUI_ScreenBaseType::CHILD),
    data(main_data)
{
}
// *INDENT-OFF*

SettingsUI_ScreenWlan::~SettingsUI_ScreenWlan()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);

    ESP_UTILS_CHECK_FALSE_EXIT(del(), "Delete failed");
}

bool SettingsUI_ScreenWlan::begin()
{
    ESP_UTILS_LOGD("Begin(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(SettingsUI_ScreenBase::begin("WLAN", "Settings"), false, "Screen base begin failed");

    _cell_container_map = CELL_CONTAINER_MAP();
    ESP_UTILS_CHECK_FALSE_GOTO(processCellContainerMapInit(), err, "Process cell container map init failed");

    // Update UI
    ESP_UTILS_CHECK_FALSE_GOTO(processDataUpdate(), err, "Process data update failed");
    ESP_UTILS_CHECK_FALSE_GOTO(setConnectedVisible(false), err, "Set connected visible failed");
    ESP_UTILS_CHECK_FALSE_GOTO(setAvailableVisible(false), err, "Set available visible failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool SettingsUI_ScreenWlan::del()
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

bool SettingsUI_ScreenWlan::processDataUpdate()
{
    ESP_UTILS_LOGD("Process data update");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(SettingsUI_ScreenBase::processDataUpdate(), false, "Process base data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processCellContainerMapUpdate(), false, "Process cell container map update failed");

    return true;
}

bool SettingsUI_ScreenWlan::setConnectedVisible(bool visible)
{
    ESP_UTILS_LOGD("Set connected visible(%d)", visible);

    SettingsUI_WidgetCellContainer *container = getCellContainer(
        static_cast<int>(SettingsUI_ScreenWlanContainerIndex::CONNECTED)
    );
    ESP_UTILS_CHECK_NULL_RETURN(container, false, "Get container failed");

    lv_obj_t *object = container->getMainObject();
    ESP_UTILS_CHECK_NULL_RETURN(object, false, "Get main object failed");

    if (visible) {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }

    return true;
}

bool SettingsUI_ScreenWlan::updateConnectedData(const WlanData &wlan_data)
{
    ESP_UTILS_CHECK_FALSE_RETURN(!wlan_data.ssid.empty(), false, "Invalid data ssid");

    auto container = getCellContainer(static_cast<int>(SettingsUI_ScreenWlanContainerIndex::CONNECTED));
    ESP_UTILS_CHECK_NULL_RETURN(container, false, "Get connected cell container failed");

    auto cell = container->getCellByIndex(0);
    ESP_UTILS_CHECK_NULL_RETURN(cell, false, "Get connected cell failed");

    ESP_UTILS_CHECK_FALSE_RETURN(
        updateCellWlanData(cell, wlan_data), false, "Update WLAN connected cell failed"
    );

    return true;
}

bool SettingsUI_ScreenWlan::updateConnectedState(ConnectState state)
{
    ESP_UTILS_LOGD("Update connected state: %d", state);

    auto container = getCellContainer(static_cast<int>(SettingsUI_ScreenWlanContainerIndex::CONNECTED));
    ESP_UTILS_CHECK_NULL_RETURN(container, false, "Get connected cell container failed");

    auto cell = container->getCellByIndex(0);
    ESP_UTILS_CHECK_NULL_RETURN(cell, false, "Get connected cell failed");

    std::string text = "";
    switch (state) {
    case ConnectState::CONNECTED:
        text = "Connected";
        break;
    case ConnectState::CONNECTING:
        text = "Connecting...";
        break;
    case ConnectState::DISCONNECT:
        text = "Disconnected";
        break;
    default:
        break;
    }
    ESP_UTILS_CHECK_FALSE_RETURN(
        cell->updateLeftMinorLabel(text), false, "Update left minor label failed"
    );

    return true;
}

bool SettingsUI_ScreenWlan::scrollConnectedToView()
{
    ESP_UTILS_LOGD("Scroll connected to view");

    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    lv_obj_t *connect_container = getCellContainer(
                                      static_cast<int>(SettingsUI_ScreenWlanContainerIndex::CONNECTED)
                                  )->getMainObject();
    ESP_UTILS_CHECK_NULL_RETURN(connect_container, false, "Get connect container object failed");

    lv_obj_scroll_to_view_recursive(connect_container, LV_ANIM_ON);

    return true;
}

bool SettingsUI_ScreenWlan::checkConnectedVisible()
{
    auto container = getCellContainer(static_cast<int>(SettingsUI_ScreenWlanContainerIndex::CONNECTED));
    ESP_UTILS_CHECK_NULL_RETURN(container, false, "Get connected cell container failed");

    return !lv_obj_has_flag(container->getMainObject(), LV_OBJ_FLAG_HIDDEN);
}

bool SettingsUI_ScreenWlan::setAvailableVisible(bool visible)
{
    ESP_UTILS_LOGD("Set available visible(%d)", visible);

    SettingsUI_WidgetCellContainer *container = getCellContainer(
        static_cast<int>(SettingsUI_ScreenWlanContainerIndex::AVAILABLE)
    );
    ESP_UTILS_CHECK_NULL_RETURN(container, false, "Get container failed");

    lv_obj_t *object = container->getMainObject();
    ESP_UTILS_CHECK_NULL_RETURN(object, false, "Get main object failed");

    if (visible) {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }

    return true;
}

bool SettingsUI_ScreenWlan::updateAvailableData(
    const std::vector<WlanData> &wlan_data, ESP_Brookesia_CoreEvent::Handler event_handler, void *user_data
)
{
    auto cell_container = getCellContainer(static_cast<int>(SettingsUI_ScreenWlanContainerIndex::AVAILABLE));
    ESP_UTILS_CHECK_NULL_RETURN(cell_container, false, "Get cell container failed");

    int cell_number = cell_container->getCellCount();
    int data_number = wlan_data.size();
    bool ret = true;
    SettingsUI_WidgetCell *cell = nullptr;

    // Update cells number, create or remove cell
    for (int i = 0; (i < cell_number) || (i < data_number); i++) {
        if (i >= cell_number) {
            cell = cell_container->addCell(
                       cell_number, SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL | SettingsUI_WidgetCellElement::RIGHT_ICONS
                   );
            ESP_UTILS_CHECK_FALSE_GOTO(ret = (cell != nullptr), end, "Add cell(%d) failed", cell_number);

            ESP_UTILS_CHECK_FALSE_GOTO(
                ret = app.getCore()->getCoreEvent()->registerEvent(
                          cell->getEventObject(), event_handler, cell->getClickEventID(), user_data
                      ), end, "Register cell(%d) click event failed", cell_number
            );
        } else if (i >= data_number) {
            ESP_UTILS_CHECK_FALSE_GOTO(
                ret = cell_container->delCellByIndex(cell_container->getCellCount() - 1), end,
                "Remove cell(%d) failed", i
            );
        } else {
            cell = cell_container->getCellByIndex(i);
            ESP_UTILS_CHECK_NULL_GOTO(cell, end, "Get cell(%d) failed", i);
        }

        // Update cells data
        if (i < data_number) {
            ESP_UTILS_CHECK_FALSE_GOTO(
                ret = updateCellWlanData(cell, wlan_data[i]), end, "Update WLAN available cell(%d) failed", i
            );
            ESP_UTILS_CHECK_FALSE_GOTO(
                ret = cell->setSplitLineVisible(i < (data_number - 1)), end, "Set split line visible failed"
            );
        }
    }

end:
    if (!ret) {
        ESP_UTILS_CHECK_FALSE_RETURN(cleanAvailable(), false, "Clean WLAN available failed");
    }

    return ret;
}

bool SettingsUI_ScreenWlan::updateCellWlanData(SettingsUI_WidgetCell *cell, const WlanData &wlan_data)
{
    ESP_UTILS_LOGD("Update cell(%p) WLAN data", cell);
    ESP_UTILS_CHECK_NULL_RETURN(cell, false, "Invalid cell");
    ESP_UTILS_CHECK_FALSE_RETURN(!wlan_data.ssid.empty(), false, "Invalid data ssid");

    SettingsUI_WidgetCellConf cell_conf = {
        .left_main_label_text = wlan_data.ssid,
        .flags = {
            .enable_left_main_label = true,
            .enable_right_icons = true,
            .enable_clickable = true,
        },
    };
    switch (wlan_data.signal_level) {
    case SignalLevel::WEAK:
        cell_conf.right_icon_images.push_back(data.icon_wlan_signals[0]);
        break;
    case SignalLevel::MODERATE:
        cell_conf.right_icon_images.push_back(data.icon_wlan_signals[1]);
        break;
    case SignalLevel::GOOD:
        cell_conf.right_icon_images.push_back(data.icon_wlan_signals[2]);
        break;
    default:
        ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid signal level");
        break;
    }
    if (wlan_data.is_locked) {
        cell_conf.right_icon_images.push_back(data.icon_wlan_lock);
    }

    ESP_UTILS_CHECK_FALSE_RETURN(cell->updateConf(cell_conf), false, "Cell update conf failed");

    return true;
}

bool SettingsUI_ScreenWlan::cleanAvailable()
{
    auto cell_container = getCellContainer(static_cast<int>(SettingsUI_ScreenWlanContainerIndex::AVAILABLE));
    ESP_UTILS_CHECK_NULL_RETURN(cell_container, false, "Get cell container failed");
    ESP_UTILS_CHECK_FALSE_RETURN(cell_container->cleanCells(), false, "Clean cells failed");

    return true;
}

bool SettingsUI_ScreenWlan::setAvaliableClickable(bool clickable)
{
    auto cell_container = getCellContainer(static_cast<int>(SettingsUI_ScreenWlanContainerIndex::AVAILABLE));
    ESP_UTILS_CHECK_NULL_RETURN(cell_container, false, "Get cell container failed");

    ESP_UTILS_LOGD("Set available clickable(%d)", clickable);

    for (size_t i = 0; i < cell_container->getCellCount(); i++) {
        SettingsUI_WidgetCell *cell = cell_container->getCellByIndex(i);
        ESP_UTILS_CHECK_NULL_RETURN(cell, false, "Get cell failed");

        ESP_UTILS_CHECK_FALSE_RETURN(cell->updateClickable(clickable), false, "Update clickable failed");
    }

    return true;
}

bool SettingsUI_ScreenWlan::processCellContainerMapInit()
{
    ESP_UTILS_LOGD("Process cell container map init");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!SettingsUI_ScreenBase::processCellContainerMapInit<SettingsUI_ScreenWlanContainerIndex,
         SettingsUI_ScreenWlanCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map init failed");
        return false;
    }

    return true;
}

bool SettingsUI_ScreenWlan::processCellContainerMapUpdate()
{
    ESP_UTILS_LOGD("Process cell container map init");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    _cell_container_map[SettingsUI_ScreenWlanContainerIndex::CONTROL].first =
        data.container_confs[(int)SettingsUI_ScreenWlanContainerIndex::CONTROL];
    _cell_container_map[SettingsUI_ScreenWlanContainerIndex::CONTROL].second[SettingsUI_ScreenWlanCellIndex::CONTROL_SW].second =
        data.cell_confs[(int)SettingsUI_ScreenWlanCellIndex::CONTROL_SW];
    _cell_container_map[SettingsUI_ScreenWlanContainerIndex::CONTROL].second[SettingsUI_ScreenWlanCellIndex::CONTROL_SW].second =
        data.cell_confs[(int)SettingsUI_ScreenWlanCellIndex::CONTROL_SW];

    _cell_container_map[SettingsUI_ScreenWlanContainerIndex::CONNECTED].first =
        data.container_confs[(int)SettingsUI_ScreenWlanContainerIndex::CONNECTED];
    _cell_container_map[SettingsUI_ScreenWlanContainerIndex::CONNECTED].second[SettingsUI_ScreenWlanCellIndex::CONNECTED_AP].second =
        data.cell_confs[(int)SettingsUI_ScreenWlanCellIndex::CONNECTED_AP];
    _cell_container_map[SettingsUI_ScreenWlanContainerIndex::CONNECTED].second[SettingsUI_ScreenWlanCellIndex::CONNECTED_AP].second =
        data.cell_confs[(int)SettingsUI_ScreenWlanCellIndex::CONNECTED_AP];

    _cell_container_map[SettingsUI_ScreenWlanContainerIndex::AVAILABLE].first =
        data.container_confs[(int)SettingsUI_ScreenWlanContainerIndex::AVAILABLE];

    if (!SettingsUI_ScreenBase::processCellContainerMapUpdate<SettingsUI_ScreenWlanContainerIndex,
         SettingsUI_ScreenWlanCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map uipdate failed");
        return false;
    }

    return true;
}

} // namespace esp_brookesia::speaker
