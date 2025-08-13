/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/esp_brookesia_app_settings_utils.hpp"
#include "../../assets/esp_brookesia_app_settings_assets.h"
#include "wlan.hpp"

using namespace std;
using namespace esp_brookesia::systems::speaker;

namespace esp_brookesia::apps {

#define CELL_ELEMENT_CONF_SW() \
    { \
        SettingsUI_WidgetCellElement::MAIN \
        | SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL \
        | SettingsUI_WidgetCellElement::RIGHT_SWITCH, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_ELEMENT_CONF_PROVISIONING_SOFTAP() \
    { \
        SettingsUI_WidgetCellElement::MAIN \
        | SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL \
        | SettingsUI_WidgetCellElement::RIGHT_ICONS, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_ELEMENT_CONF_CONNECTED_AP() \
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
                    { SettingsUI_ScreenWlanCellIndex::CONNECTED_AP, CELL_ELEMENT_CONF_CONNECTED_AP() }, \
                }, \
            }, \
        },\
        { \
            SettingsUI_ScreenWlanContainerIndex::AVAILABLE, { {}, {}, }, \
        },\
        { \
            SettingsUI_ScreenWlanContainerIndex::PROVISIONING, { \
                {}, \
                { \
                    { SettingsUI_ScreenWlanCellIndex::PROVISIONING_SOFTAP, CELL_ELEMENT_CONF_PROVISIONING_SOFTAP() }, \
                }, \
            } \
        }, \
    }

// *INDENT-OFF*
SettingsUI_ScreenWlan::SettingsUI_ScreenWlan(App &ui_app, const SettingsUI_ScreenBaseData &base_data,
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

    esp_utils::function_guard end_guard([this]() {
        ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();
        ESP_UTILS_CHECK_FALSE_EXIT(del(), "Delete failed");
    });

    _cell_container_map = CELL_CONTAINER_MAP();
    ESP_UTILS_CHECK_FALSE_RETURN(processCellContainerMapInit(), false, "Process cell container map init failed");

    auto connected_left_main_label = getElementObject(
        static_cast<int>(SettingsUI_ScreenWlanContainerIndex::CONNECTED),
        static_cast<int>(SettingsUI_ScreenWlanCellIndex::CONNECTED_AP),
        SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL
    );
    ESP_UTILS_CHECK_NULL_RETURN(connected_left_main_label, false, "Get connected left main label failed");
    lv_label_set_long_mode(connected_left_main_label, LV_LABEL_LONG_SCROLL);

    // Update UI
    ESP_UTILS_CHECK_FALSE_RETURN(processDataUpdate(), false, "Process data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(setConnectedVisible(false), false, "Set connected visible failed");
    ESP_UTILS_CHECK_FALSE_RETURN(setAvailableVisible(false), false, "Set available visible failed");

    end_guard.release();

    return true;
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

    auto connected_left_main_label = getElementObject(
        static_cast<int>(SettingsUI_ScreenWlanContainerIndex::CONNECTED),
        static_cast<int>(SettingsUI_ScreenWlanCellIndex::CONNECTED_AP),
        SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL
    );
    ESP_UTILS_CHECK_NULL_RETURN(connected_left_main_label, false, "Get connected left main label failed");
    lv_obj_set_width(connected_left_main_label, data.cell_left_main_label_size.width);

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

bool SettingsUI_ScreenWlan::updateConnectedData(WlanData wlan_data)
{
    ESP_UTILS_CHECK_FALSE_RETURN(!wlan_data.ssid.empty(), false, "Invalid data ssid");

    auto container = getCellContainer(static_cast<int>(SettingsUI_ScreenWlanContainerIndex::CONNECTED));
    ESP_UTILS_CHECK_NULL_RETURN(container, false, "Get connected cell container failed");

    auto cell = container->getCellByIndex(0);
    ESP_UTILS_CHECK_NULL_RETURN(cell, false, "Get connected cell failed");

    auto left_main_label = cell->getElementObject(SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL);
    ESP_UTILS_CHECK_NULL_RETURN(left_main_label, false, "Get left main label failed");
    lv_label_set_long_mode(left_main_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_width(left_main_label, data.cell_left_main_label_size.width);

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
    _connected_state = state;

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
    std::vector<WlanData> &&wlan_data, systems::base::Event::Handler event_handler, void *user_data
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

            auto left_main_label = cell->getElementObject(SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL);
            ESP_UTILS_CHECK_NULL_GOTO(left_main_label, end, "Get left main label failed");
            lv_label_set_long_mode(left_main_label, LV_LABEL_LONG_SCROLL);
            lv_obj_set_width(left_main_label, data.cell_left_main_label_size.width);

            ESP_UTILS_CHECK_FALSE_GOTO(
                ret = app.getSystemContext()->getEvent().registerEvent(
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

bool SettingsUI_ScreenWlan::updateCellWlanData(SettingsUI_WidgetCell *cell, WlanData wlan_data)
{
    // ESP_UTILS_LOGD("Update cell(%p) WLAN data", cell);
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

bool SettingsUI_ScreenWlan::setSoftAPVisible(bool visible)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    SettingsUI_WidgetCellContainer *container = getCellContainer(
        static_cast<int>(SettingsUI_ScreenWlanContainerIndex::PROVISIONING)
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

    auto &control_container_conf = _cell_container_map[SettingsUI_ScreenWlanContainerIndex::CONTROL].first;
    control_container_conf = data.container_confs[(int)SettingsUI_ScreenWlanContainerIndex::CONTROL];
    auto &control_cell_confs = _cell_container_map[SettingsUI_ScreenWlanContainerIndex::CONTROL].second;
    for (int i = static_cast<int>(SettingsUI_ScreenWlanCellIndex::CONTROL_SW);
            i <= static_cast<int>(SettingsUI_ScreenWlanCellIndex::CONTROL_SW); i++) {
        control_cell_confs[static_cast<SettingsUI_ScreenWlanCellIndex>(i)].second = data.cell_confs[i];
    }

    auto &connected_container_conf = _cell_container_map[SettingsUI_ScreenWlanContainerIndex::CONNECTED].first;
    connected_container_conf = data.container_confs[(int)SettingsUI_ScreenWlanContainerIndex::CONNECTED];
    auto &connected_cell_confs = _cell_container_map[SettingsUI_ScreenWlanContainerIndex::CONNECTED].second;
    for (int i = static_cast<int>(SettingsUI_ScreenWlanCellIndex::CONNECTED_AP);
            i <= static_cast<int>(SettingsUI_ScreenWlanCellIndex::CONNECTED_AP); i++) {
        connected_cell_confs[static_cast<SettingsUI_ScreenWlanCellIndex>(i)].second = data.cell_confs[i];
    }

    auto &available_container_conf = _cell_container_map[SettingsUI_ScreenWlanContainerIndex::AVAILABLE].first;
    available_container_conf = data.container_confs[(int)SettingsUI_ScreenWlanContainerIndex::AVAILABLE];

    auto &softap_container_conf = _cell_container_map[SettingsUI_ScreenWlanContainerIndex::PROVISIONING].first;
    softap_container_conf = data.container_confs[(int)SettingsUI_ScreenWlanContainerIndex::PROVISIONING];
    auto &softap_cell_confs = _cell_container_map[SettingsUI_ScreenWlanContainerIndex::PROVISIONING].second;
    for (int i = static_cast<int>(SettingsUI_ScreenWlanCellIndex::PROVISIONING_SOFTAP);
            i <= static_cast<int>(SettingsUI_ScreenWlanCellIndex::PROVISIONING_SOFTAP); i++) {
        softap_cell_confs[static_cast<SettingsUI_ScreenWlanCellIndex>(i)].second = data.cell_confs[i];
    }

    if (!SettingsUI_ScreenBase::processCellContainerMapUpdate<SettingsUI_ScreenWlanContainerIndex,
         SettingsUI_ScreenWlanCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map uipdate failed");
        return false;
    }

    return true;
}

} // namespace esp_brookesia::apps
