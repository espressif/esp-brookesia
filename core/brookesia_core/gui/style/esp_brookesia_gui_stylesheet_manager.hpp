/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief This file includes "private/esp_brookesia_base_utils.hpp", so it cannot be included by public header files
 */

#pragma once

#include <memory>
#include <string>
#include <list>
#include <map>
#include <unordered_map>
// #include "private/esp_brookesia_base_utils.hpp"
#include "style/esp_brookesia_gui_style.hpp"

namespace esp_brookesia::gui {

template <typename T>
using NameStylesheetMap = std::unordered_map<std::string, std::shared_ptr<T>>;

template <typename T>
using ResolutionNameStylesheetMap = std::map<uint32_t, NameStylesheetMap<T>>;

// *INDENT-OFF*
template <typename T>
class StylesheetManager {
public:
    StylesheetManager();
    virtual ~StylesheetManager();

    virtual bool calibrateScreenSize(StyleSize &size) = 0;

    bool addStylesheet(const char *name, const StyleSize &screen_size, const T &stylesheet);
    bool activateStylesheet(const StyleSize &screen_size, const T &stylesheet);
    bool activateStylesheet(const char *name, const StyleSize &screen_size);

    size_t getStylesheetCount(void) const;
    typename NameStylesheetMap<T>::iterator findNameStylesheetMap(const StyleSize &screen_size);
    typename NameStylesheetMap<T>::iterator getNameStylesheetMapEnd(const StyleSize &screen_size);

    /**
     * @brief Get the active stylesheet
     *
     * @return stylesheet
     *
     */
    const T *getStylesheet(void) const { return &_active_stylesheet; }

    /**
     * @brief Get the stylesheet by name and screen size
     *
     * @param name The name of the stylesheet
     * @param screen_size The screen size of the stylesheet
     *
     * @return stylesheet
     *
     */
    const T *getStylesheet(const char *name, const StyleSize &screen_size);

    /**
     * @brief Get the first stylesheet which matches the screen size
     *
     * @param screen_size The screen size of the stylesheet
     *
     * @return stylesheet
     *
     */
    const T *getStylesheet(const StyleSize &screen_size);

protected:
    T _active_stylesheet;

    virtual bool calibrateStylesheet(const StyleSize &screen_size, T &stylesheet) = 0;

    bool del(void);

private:
    ResolutionNameStylesheetMap<T> _resolution_name_stylesheet_map;

    uint32_t getResolution(const StyleSize &screen_size)
    {
        return (screen_size.width << 16) | screen_size.height;
    }
};
// *INDENT-ON*

template <typename T>
StylesheetManager<T>::StylesheetManager():
    _active_stylesheet{}
{
}

template <typename T>
StylesheetManager<T>::~StylesheetManager()
{
    // ESP_UTILS_LOGD("Delete(0x%p)", this);
    if (!del()) {
        // ESP_UTILS_LOGE("Failed to delete");
    }
}

template <typename T>
bool StylesheetManager<T>::addStylesheet(const char *name, const StyleSize &screen_size, const T &stylesheet)
{
    uint32_t resolution = 0;
    StyleSize calibrate_size = screen_size;
    std::shared_ptr<T> calibration_stylesheet = std::make_shared<T>(stylesheet);

    // ESP_UTILS_CHECK_NULL_RETURN(name, false, "Invalid name");
    // ESP_UTILS_CHECK_NULL_RETURN(calibration_stylesheet, false, "Create stylesheet failed");
    if (name == nullptr || calibration_stylesheet == nullptr) {
        return false;
    }

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), false, "Invalid screen size");
    // ESP_UTILS_LOGD("Add stylesheet(%s - %dx%d)", name, calibrate_size.width, calibrate_size.height);
    if (!calibrateScreenSize(calibrate_size)) {
        return false;
    }

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateStylesheet(calibrate_size, *calibration_stylesheet), false, "Invalid stylesheet");
    if (!calibrateStylesheet(calibrate_size, *calibration_stylesheet)) {
        return false;
    }

    // Check if the resolution is already exist
    resolution = getResolution(calibrate_size);
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
        // ESP_UTILS_LOGW("Stylesheet(%s) already exist, overwrite it", it_name_map->first.c_str());
        it_name_map->second = calibration_stylesheet;
    } else {
        it_resolution_map->second[name] = calibration_stylesheet;
    }

    return true;
}

template <typename T>
bool StylesheetManager<T>::activateStylesheet(const StyleSize &screen_size,
        const T &stylesheet)
{
    StyleSize calibrate_size = screen_size;
    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), false, "Invalid screen size");
    if (!calibrateScreenSize(calibrate_size)) {
        return false;
    }
    // ESP_UTILS_LOGD("Activate stylesheet(%dx%d)", calibrate_size.width, calibrate_size.height);

    std::shared_ptr<T> calibration_stylesheet = std::make_shared<T>(stylesheet);
    // ESP_UTILS_CHECK_NULL_RETURN(calibration_stylesheet, false, "Create stylesheet failed");
    if (calibration_stylesheet == nullptr) {
        return false;
    }
    // ESP_UTILS_CHECK_FALSE_RETURN(
    //     calibrateStylesheet(calibrate_size, *calibration_stylesheet), false, "Invalid stylesheet"
    // );
    if (!calibrateStylesheet(calibrate_size, *calibration_stylesheet)) {
        return false;
    }

    _active_stylesheet = *calibration_stylesheet;

    return true;
}

template <typename T>
bool StylesheetManager<T>::activateStylesheet(const char *name, const StyleSize &screen_size)
{
    StyleSize calibrate_size = screen_size;
    const T *stylesheet = nullptr;

    // ESP_UTILS_CHECK_NULL_RETURN(name, false, "Invalid name");
    if (name == nullptr) {
        return false;
    }

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), false, "Invalid screen size");
    if (!calibrateScreenSize(calibrate_size)) {
        return false;
    }
    // ESP_UTILS_LOGD("Activate stylesheet(%s - %dx%d)", name, calibrate_size.width, calibrate_size.height);

    stylesheet = getStylesheet(name, screen_size);
    // ESP_UTILS_CHECK_NULL_RETURN(stylesheet, false, "Get stylesheet failed");
    if (stylesheet == nullptr) {
        return false;
    }

    _active_stylesheet = *stylesheet;

    return true;
}

template <typename T>
size_t StylesheetManager<T>::getStylesheetCount(void) const
{
    size_t count = 0;

    for (auto &name_stylesheet_map : _resolution_name_stylesheet_map) {
        count += name_stylesheet_map.second.size();
    }

    return count;
}

template <typename T>
typename NameStylesheetMap<T>::iterator StylesheetManager<T>::findNameStylesheetMap(const StyleSize &screen_size)
{
    uint32_t resolution = 0;
    StyleSize calibrate_size = screen_size;

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), false, "Invalid screen size");
    if (!calibrateScreenSize(calibrate_size)) {
        return false;
    }
    resolution = getResolution(calibrate_size);

    return _resolution_name_stylesheet_map.find(resolution);
}

template <typename T>
typename NameStylesheetMap<T>::iterator StylesheetManager<T>::getNameStylesheetMapEnd(const StyleSize &screen_size)
{
    uint32_t resolution = 0;
    StyleSize calibrate_size = screen_size;

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), false, "Invalid screen size");
    if (!calibrateScreenSize(calibrate_size)) {
        return false;
    }
    resolution = getResolution(calibrate_size);

    return _resolution_name_stylesheet_map[resolution].end();
}

template <typename T>
const T *StylesheetManager<T>::getStylesheet(const char *name, const StyleSize &screen_size)
{
    uint32_t resolution = 0;
    StyleSize calibrate_size = screen_size;

    // ESP_UTILS_CHECK_NULL_RETURN(name, nullptr, "Invalid name");
    if (name == nullptr) {
        return nullptr;
    }

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), nullptr, "Invalid screen size");
    if (!calibrateScreenSize(calibrate_size)) {
        return nullptr;
    }

    // Check if the resolution is already exist
    resolution = getResolution(calibrate_size);
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
const T *StylesheetManager<T>::getStylesheet(const StyleSize &screen_size)
{
    uint32_t resolution = 0;
    StyleSize calibrate_size = screen_size;

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), nullptr, "Invalid screen size");
    if (!calibrateScreenSize(calibrate_size)) {
        return nullptr;
    }
    // ESP_UTILS_LOGD("Get stylesheet with resolution(%dx%d)", calibrate_size.width, calibrate_size.height);

    // Check if the resolution is already exist
    resolution = getResolution(calibrate_size);
    auto it_resolution_map = _resolution_name_stylesheet_map.find(resolution);
    if (it_resolution_map == _resolution_name_stylesheet_map.end()) {
        return nullptr;
    }

    auto name_map = it_resolution_map->second;
    if (name_map.empty()) {
        return nullptr;
    }

    return name_map.begin()->second.get();
}

template <typename T>
bool StylesheetManager<T>::del(void)
{
    _active_stylesheet = {};
    _resolution_name_stylesheet_map.clear();

    return true;
}

} // namespace esp_brookesia::gui

template <typename T>
using ESP_Brookesia_NameStylesheetMap [[deprecated("Use `esp_brookesia::gui::NameStylesheetMap` instead")]] =
    esp_brookesia::gui::NameStylesheetMap<T>;
template <typename T>
using ESP_Brookesia_ResolutionNameStylesheetMap [[deprecated("Use `esp_brookesia::gui::ResolutionNameStylesheetMap` instead")]] =
    esp_brookesia::gui::ResolutionNameStylesheetMap<T>;
template <typename T>
using ESP_Brookesia_CoreStylesheetManager [[deprecated("Use `esp_brookesia::gui::StylesheetManager` instead")]] =
    esp_brookesia::gui::StylesheetManager<T>;
