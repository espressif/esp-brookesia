/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "esp_brookesia_app_settings_data.hpp"
#include "esp_brookesia_app_settings_ui.hpp"
#include "esp_brookesia_app_settings_manager.hpp"
#include "stylesheets/stylesheets.hpp"
#include "assets/esp_brookesia_app_settings_assets.h"

namespace esp_brookesia::apps {

using SettingsStylesheet = gui::StylesheetManager<SettingsStylesheetData>;

class Settings: public systems::speaker::App, public SettingsStylesheet {
public:
    Settings(const Settings &) = delete;
    Settings(Settings &&) = delete;
    Settings &operator=(const Settings &) = delete;
    Settings &operator=(Settings &&) = delete;

    ~Settings();

    bool addStylesheet(const SettingsStylesheetData *data);
    bool addStylesheet(speaker::Speaker *speaker, const SettingsStylesheetData *data);
    bool activateStylesheet(const SettingsStylesheetData *data);
    bool activateStylesheet(const char *name, const gui::StyleSize &screen_size);

    static Settings *requestInstance();

    SettingsUI ui;
    SettingsManager manager;

protected:
    bool run() override;
    bool back() override;
    bool close() override;
    bool init() override;
    bool deinit() override;

    bool isStarting() const
    {
        return _is_starting;
    }
    bool isStopping() const
    {
        return _is_stopping;
    }

private:
    Settings();

    bool calibrateStylesheet(const gui::StyleSize &screen_size, SettingsStylesheetData &sheetstyle) override;
    bool calibrateScreenSize(gui::StyleSize &size) override;

    const SettingsStylesheetData _default_stylesheet_dark = {};

    std::atomic<bool> _is_starting = false;
    std::atomic<bool> _is_stopping = false;

    inline static Settings *_instance = nullptr;
};

} // namespace esp_brookesia::apps
