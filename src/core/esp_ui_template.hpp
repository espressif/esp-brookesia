/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <string>
#include <list>
#include <map>
#include "esp_ui_core_utils.h"
#include "esp_ui_core_type.h"
#include "esp_ui_core.hpp"

template <typename T>
using ESP_UI_NameStylesheetMap_t = std::unordered_map<std::string, std::shared_ptr<T>>;

template <typename T>
using ESP_UI_ResolutionNameStylesheetMap_t = std::map<uint32_t, ESP_UI_NameStylesheetMap_t<T>>;

// *INDENT-OFF*
template <typename T>
class ESP_UI_Template: public ESP_UI_Core {
public:
    ESP_UI_Template(const ESP_UI_CoreData_t &core_data, ESP_UI_CoreHome &home, ESP_UI_CoreManager &manager,
                    lv_disp_t *display);
    virtual ~ESP_UI_Template();

    virtual bool calibrateStylesheet(const ESP_UI_StyleSize_t &screen_size, T &stylesheet) = 0;

    bool addStylesheet(const char *name, const ESP_UI_StyleSize_t &screen_size, const T &stylesheet);
    bool activateStylesheet(const char *name, const ESP_UI_StyleSize_t &screen_size);

    size_t getStylesheetCount(void) const;
    typename ESP_UI_NameStylesheetMap_t<T>::iterator findNameStylesheetMap(const ESP_UI_StyleSize_t &screen_size);
    typename ESP_UI_NameStylesheetMap_t<T>::iterator getNameStylesheetMapEnd(const ESP_UI_StyleSize_t &screen_size);

    /**
     * @brief Get the active stylesheet
     *
     * @return stylesheet
     *
     */
    const T *getStylesheet(void) const { return &_stylesheet; }

    /**
     * @brief Get the stylesheet by name and screen size
     *
     * @param name The name of the stylesheet
     * @param screen_size The screen size of the stylesheet
     *
     * @return stylesheet
     *
     */
    const T *getStylesheet(const char *name, const ESP_UI_StyleSize_t &screen_size) const;

    /**
     * @brief Get the first stylesheet which matches the screen size
     *
     * @param screen_size The screen size of the stylesheet
     *
     * @return stylesheet
     *
     */
    const T *getStylesheet(const ESP_UI_StyleSize_t &screen_size) const;

protected:
    T _stylesheet;

    bool calibrateResolutionSize(uint32_t &resolution, ESP_UI_StyleSize_t size) const;
    bool delTemplate(void);

private:
    ESP_UI_ResolutionNameStylesheetMap_t<T> _resolution_name_stylesheet_map;
};
// *INDENT-OFF*

template <typename T>
ESP_UI_Template<T>::ESP_UI_Template(const ESP_UI_CoreData_t &core_data, ESP_UI_CoreHome &home,
        ESP_UI_CoreManager &manager, lv_disp_t *display):
    ESP_UI_Core(core_data, home, manager, display),
    _stylesheet{}
{
}

template <typename T>
ESP_UI_Template<T>::~ESP_UI_Template()
{
    ESP_UI_LOGD("Delete(0x%p)", this);
    if (!delTemplate()) {
        ESP_UI_LOGE("Failed to delete");
    }
}

template <typename T>
bool ESP_UI_Template<T>::addStylesheet(const char *name, const ESP_UI_StyleSize_t &screen_size, const T &stylesheet)
{
    uint32_t resolution = 0;
    ESP_UI_StyleSize_t calibrate_size = screen_size;
    std::shared_ptr<T> calibration_stylesheet = std::make_shared<T>(stylesheet);

    ESP_UI_CHECK_NULL_RETURN(name, false, "Invalid name");

    // Check if the display is set. If not, use the default display
    if (_display == nullptr) {
        ESP_UI_LOGW("Display is not set, use default display");
        _display = lv_disp_get_default();
        ESP_UI_CHECK_NULL_RETURN(_display, false, "Display device is not initialized");
    }

    ESP_UI_CHECK_FALSE_RETURN(calibrateResolutionSize(resolution, calibrate_size), false, "Invalid screen size");
    ESP_UI_LOGD("Add stylesheet(%s - %dx%d)", name, calibrate_size.width, calibrate_size.height);

    ESP_UI_CHECK_FALSE_RETURN(calibrateStylesheet(calibrate_size, *calibration_stylesheet), false, "Invalid stylesheet");

    // Check if the resolution is already exist
    auto it_resolution_map = _resolution_name_stylesheet_map.find(resolution);
    // If not exist, create a new map which contains the name and data
    if (it_resolution_map == _resolution_name_stylesheet_map.end()) {
        _resolution_name_stylesheet_map[resolution][std::string(name)] = calibration_stylesheet;
        return true;
    }

    // If exist, check if the name is already exist
    auto it_name_map = it_resolution_map->second.find(name);
    // If exist, overwrite it, else add it
    if (it_name_map != it_resolution_map->second.end()) {
        ESP_UI_LOGW("Stylesheet(%s) already exist, overwrite it", it_name_map->first.c_str());
        it_name_map->second = calibration_stylesheet;
    } else {
        it_resolution_map->second[name] = calibration_stylesheet;
    }

    return true;
}

template <typename T>
bool ESP_UI_Template<T>::activateStylesheet(const char *name, const ESP_UI_StyleSize_t &screen_size)
{
    uint32_t resolution = 0;
    ESP_UI_StyleSize_t calibrate_size = screen_size;
    const T *stylesheet = nullptr;

    ESP_UI_CHECK_NULL_RETURN(name, false, "Invalid name");

    ESP_UI_CHECK_FALSE_RETURN(calibrateResolutionSize(resolution, calibrate_size), false, "Invalid screen size");
    ESP_UI_LOGD("Activate stylesheet(%s - %dx%d)", name, calibrate_size.width, calibrate_size.height);

    stylesheet = getStylesheet(name, screen_size);
    ESP_UI_CHECK_NULL_RETURN(stylesheet, false, "Get stylesheet failed");

    ESP_UI_Template<T>::_stylesheet = *stylesheet;

    if (checkCoreInitialized() && !sendDataUpdateEvent()) {
        ESP_UI_LOGE("Send update data event failed");
    }

    return true;
}

template <typename T>
size_t ESP_UI_Template<T>::getStylesheetCount(void) const
{
    size_t count = 0;

    for (auto &name_stylesheet_map : _resolution_name_stylesheet_map) {
        count += name_stylesheet_map.second.size();
    }

    return count;
}

template <typename T>
typename ESP_UI_NameStylesheetMap_t<T>::iterator ESP_UI_Template<T>::findNameStylesheetMap(const ESP_UI_StyleSize_t &screen_size)
{
    uint32_t resolution = 0;
    ESP_UI_StyleSize_t calibrate_size = screen_size;

    ESP_UI_CHECK_FALSE_RETURN(calibrateResolutionSize(resolution, calibrate_size), false, "Invalid screen size");

    return _resolution_name_stylesheet_map.find(resolution);
}

template <typename T>
typename ESP_UI_NameStylesheetMap_t<T>::iterator ESP_UI_Template<T>::getNameStylesheetMapEnd(const ESP_UI_StyleSize_t &screen_size)
{
    uint32_t resolution = 0;
    ESP_UI_StyleSize_t calibrate_size = screen_size;

    ESP_UI_CHECK_FALSE_RETURN(calibrateResolutionSize(resolution, calibrate_size), false, "Invalid screen size");

    return _resolution_name_stylesheet_map[resolution].end();
}

template <typename T>
const T *ESP_UI_Template<T>::getStylesheet(const char *name, const ESP_UI_StyleSize_t &screen_size) const
{
    uint32_t resolution = 0;
    ESP_UI_StyleSize_t calibrate_size = screen_size;

    ESP_UI_CHECK_NULL_RETURN(name, nullptr, "Invalid name");

    ESP_UI_CHECK_FALSE_RETURN(calibrateResolutionSize(resolution, calibrate_size), nullptr, "Invalid screen size");

    // Check if the resolution is already exist
    auto it_resolution_map = _resolution_name_stylesheet_map.find(resolution);
    if (it_resolution_map == _resolution_name_stylesheet_map.end()) {
        return nullptr;
    }

    // If exist, check if the name is already exist
    auto it_name_map = it_resolution_map->second.find(name);
    if (it_name_map == it_resolution_map->second.end()) {
        return nullptr;
    }

    return it_name_map->second.get();
}

template <typename T>
const T *ESP_UI_Template<T>::getStylesheet(const ESP_UI_StyleSize_t &screen_size) const
{
    uint32_t resolution = 0;
    ESP_UI_StyleSize_t calibrate_size = screen_size;

    ESP_UI_CHECK_FALSE_RETURN(calibrateResolutionSize(resolution, calibrate_size), nullptr, "Invalid screen size");
    ESP_UI_LOGD("Get stylesheet with resolution(%dx%d)", calibrate_size.width, calibrate_size.height);

    // Check if the resolution is already exist
    auto it_resolution_map = _resolution_name_stylesheet_map.find(resolution);
    auto name_map = it_resolution_map->second;
    if ((it_resolution_map == _resolution_name_stylesheet_map.end()) || name_map.empty()) {
        return nullptr;
    }

    return name_map.begin()->second.get();
}

template <typename T>
bool ESP_UI_Template<T>::calibrateResolutionSize(uint32_t &resolution, ESP_UI_StyleSize_t size) const
{
    ESP_UI_StyleSize_t display_size = {};

    ESP_UI_CHECK_NULL_RETURN(_display, false, "Display device is not initialized");

    display_size.width = lv_disp_get_hor_res(_display);
    display_size.height = lv_disp_get_ver_res(_display);
    ESP_UI_CHECK_FALSE_RETURN(_core_home.calibrateCoreObjectSize(display_size, size), false, "Invalid screen size");

    resolution = (((uint32_t)size.width) << 16) | size.height;

    return true;
}

template <typename T>
bool ESP_UI_Template<T>::delTemplate(void)
{
    _stylesheet = {};
    _resolution_name_stylesheet_map.clear();

    return true;
}
