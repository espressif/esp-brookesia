/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "private/esp_brookesia_app_settings_utils.hpp"
#include "wlan_softap.hpp"

using namespace std;
using namespace esp_brookesia::systems::speaker;

namespace esp_brookesia::apps {

#define CELL_ELEMENT_CONF_QRCODE_IMAGE() \
    { \
        SettingsUI_WidgetCellElement::MAIN, \
        SettingsUI_WidgetCellConf{} \
    }
#define CELL_CONTAINER_MAP() \
    { \
        { \
            SettingsUI_ScreenWlanSoftAPContainerIndex::QRCODE, { \
                {}, \
                { \
                    { SettingsUI_ScreenWlanSoftAPCellIndex::QRCODE_IMAGE, CELL_ELEMENT_CONF_QRCODE_IMAGE() }, \
                }, \
            } \
        }, \
    }

SettingsUI_ScreenWlanSoftAP::SettingsUI_ScreenWlanSoftAP(
    App &ui_app, const SettingsUI_ScreenBaseData &base_data,
    const SettingsUI_ScreenWlanSoftAPData &main_data
):
    SettingsUI_ScreenBase(ui_app, base_data, SettingsUI_ScreenBaseType::CHILD),
    data(main_data)
{
}

SettingsUI_ScreenWlanSoftAP::~SettingsUI_ScreenWlanSoftAP()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_EXIT(del(), "Delete failed");
}

bool SettingsUI_ScreenWlanSoftAP::begin()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    esp_utils::function_guard del_guard([this]() {
        ESP_UTILS_CHECK_FALSE_EXIT(del(), "Delete failed");
    });

    _cell_container_map = CELL_CONTAINER_MAP();
    ESP_UTILS_CHECK_FALSE_RETURN(SettingsUI_ScreenBase::begin("SoftAP", "WLAN"), false, "Screen base begin failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processCellContainerMapInit(), false, "Process cell container map init failed");

    auto container_object = getElementObject(
                                static_cast<int>(SettingsUI_ScreenWlanSoftAPContainerIndex::QRCODE),
                                static_cast<int>(SettingsUI_ScreenWlanSoftAPCellIndex::QRCODE_IMAGE),
                                SettingsUI_WidgetCellElement::MAIN
                            );
    ESP_UTILS_CHECK_NULL_RETURN(container_object, false, "Get QR code object failed");
    lv_obj_set_height(container_object, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(container_object, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container_object, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(container_object, 10, 0);
    lv_obj_set_style_pad_row(container_object, 10, 0);

    _qrcode_image = lv_qrcode_create(container_object);
    ESP_UTILS_CHECK_NULL_RETURN(_qrcode_image, false, "Create QR code image failed");

    _info_label = lv_label_create(container_object);
    ESP_UTILS_CHECK_NULL_RETURN(_info_label, false, "Create info label failed");

    ESP_UTILS_CHECK_FALSE_RETURN(processDataUpdate(), false, "Process data update failed");

    del_guard.release();

    return true;
}

bool SettingsUI_ScreenWlanSoftAP::del()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    bool ret = true;
    if (!SettingsUI_ScreenBase::del()) {
        ret  = false;
        ESP_UTILS_LOGE("Screen base delete failed");
    }

    _cell_container_map.clear();

    return ret;
}

bool SettingsUI_ScreenWlanSoftAP::processDataUpdate()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(SettingsUI_ScreenBase::processDataUpdate(), false, "Process base data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processCellContainerMapUpdate(), false, "Process cell container map update failed");

    // QR code image
    lv_qrcode_set_size(_qrcode_image, data.qrcode_image.main_size.width);
    lv_qrcode_set_dark_color(_qrcode_image, lv_color_hex(data.qrcode_image.dark_color.color));
    lv_qrcode_set_light_color(_qrcode_image, lv_color_hex(data.qrcode_image.light_color.color));
    lv_obj_set_style_border_color(_qrcode_image, lv_color_hex(data.qrcode_image.light_color.color), 0);
    lv_obj_set_style_border_width(_qrcode_image, data.qrcode_image.border_size.width, 0);

    // Info label
    lv_obj_set_size(_info_label, data.info_label.size.width, data.info_label.size.height);
    lv_obj_set_style_text_font(_info_label, static_cast<const lv_font_t *>(data.info_label.text_font.font_resource), 0);
    lv_obj_set_style_text_color(_info_label, lv_color_hex(data.info_label.text_color.color), 0);
    lv_obj_set_style_text_opa(_info_label, data.info_label.text_color.opacity, 0);

    return true;
}

bool SettingsUI_ScreenWlanSoftAP::calibrateData(
    const gui::StyleSize &parent_size, const systems::base::Display &display,
    SettingsUI_ScreenWlanSoftAPData &data
)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    const gui::StyleSize *compare_size = nullptr;

    // QR code image
    compare_size = &parent_size;
    ESP_UTILS_CHECK_FALSE_RETURN(
        display.calibrateCoreObjectSize(*compare_size, data.qrcode_image.main_size), false,
        "Invalid QR code image size"
    );

    // Info label
    compare_size = &parent_size;
    ESP_UTILS_CHECK_FALSE_RETURN(
        display.calibrateCoreObjectSize(*compare_size, data.info_label.size), false,
        "Invalid info label size"
    );
    compare_size = &data.info_label.size;
    ESP_UTILS_CHECK_FALSE_RETURN(
        display.calibrateCoreFont(compare_size, data.info_label.text_font), false,
        "Invalid info label text font"
    );

    return true;
}

bool SettingsUI_ScreenWlanSoftAP::processCellContainerMapInit()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!SettingsUI_ScreenBase::processCellContainerMapInit<SettingsUI_ScreenWlanSoftAPContainerIndex,
            SettingsUI_ScreenWlanSoftAPCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map init failed");
        return false;
    }

    return true;
}

bool SettingsUI_ScreenWlanSoftAP::processCellContainerMapUpdate()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    auto &qrcode_container_conf = _cell_container_map[SettingsUI_ScreenWlanSoftAPContainerIndex::QRCODE].first;
    qrcode_container_conf = data.container_confs[(int)SettingsUI_ScreenWlanSoftAPContainerIndex::QRCODE];
    auto &qrcode_cell_confs = _cell_container_map[SettingsUI_ScreenWlanSoftAPContainerIndex::QRCODE].second;
    for (int i = static_cast<int>(SettingsUI_ScreenWlanSoftAPCellIndex::QRCODE_IMAGE);
            i <= static_cast<int>(SettingsUI_ScreenWlanSoftAPCellIndex::QRCODE_IMAGE); i++) {
        qrcode_cell_confs[static_cast<SettingsUI_ScreenWlanSoftAPCellIndex>(i)].second = data.cell_confs[i];
    }
    if (!SettingsUI_ScreenBase::processCellContainerMapUpdate<SettingsUI_ScreenWlanSoftAPContainerIndex,
            SettingsUI_ScreenWlanSoftAPCellIndex>(_cell_container_map)) {
        ESP_UTILS_LOGE("Process cell container map update failed");
        return false;
    }

    return true;
}

} // namespace esp_brookesia::apps
