/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_ui_phone_app.hpp"
#include "esp_ui_phone_home.hpp"

#if !ESP_UI_LOG_ENABLE_DEBUG_PHONE_HOME
#undef ESP_UI_LOGD
#define ESP_UI_LOGD(...)
#endif

using namespace std;

LV_IMG_DECLARE(esp_ui_phone_app_launcher_image_default);

ESP_UI_PhoneHome::ESP_UI_PhoneHome(ESP_UI_Core &core, const ESP_UI_PhoneHomeData_t &data):
    ESP_UI_CoreHome(core, core.getCoreData().home),
    _data(data),
    _app_launcher(core, data.app_launcher.data),
    _status_bar(nullptr),
    _navigation_bar(nullptr),
    _recents_screen(nullptr)
{
}

ESP_UI_PhoneHome::~ESP_UI_PhoneHome()
{
    ESP_UI_LOGD("Destroy(@0x%p)", this);
    if (!del()) {
        ESP_UI_LOGE("Failed to delete");
    }
}

bool ESP_UI_PhoneHome::begin(void)
{
    lv_obj_t *main_screen_obj = _core.getCoreHome().getMainScreenObject();
    lv_obj_t *system_screen_obj = _core.getCoreHome().getSystemScreenObject();
    shared_ptr<ESP_UI_StatusBar> status_bar = nullptr;
    shared_ptr<ESP_UI_NavigationBar> navigation_bar = nullptr;
    shared_ptr<ESP_UI_RecentsScreen> recents_screen = nullptr;

    ESP_UI_LOGD("Begin(@0x%p)", this);
    ESP_UI_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    // Recents Screen
    if (_data.flags.enable_recents_screen) {
        recents_screen = std::make_shared<ESP_UI_RecentsScreen>(_core, _data.recents_screen.data);
        ESP_UI_CHECK_NULL_RETURN(recents_screen, false, "Create recents_screen failed");
        ESP_UI_CHECK_FALSE_RETURN(recents_screen->begin(system_screen_obj), false, "Begin recents_screen failed");
    }

    // Status bar
    if (_data.flags.enable_status_bar) {
        status_bar = std::make_shared<ESP_UI_StatusBar>(_core, _data.status_bar.data,
                     _core.getCoreManager().getAppFreeId(), _core.getCoreManager().getAppFreeId());
        ESP_UI_CHECK_NULL_RETURN(status_bar, false, "Create status bar failed");
        ESP_UI_CHECK_FALSE_RETURN(status_bar->begin(system_screen_obj), false, "Begin status bar failed");
        ESP_UI_CHECK_FALSE_RETURN(status_bar->setVisualMode(_data.status_bar.visual_mode), false,
                                  "Status bar set visual mode failed");
    }

    // Navigation bar
    if (_data.flags.enable_navigation_bar) {
        navigation_bar = std::make_shared<ESP_UI_NavigationBar>(_core, _data.navigation_bar.data);
        ESP_UI_CHECK_NULL_RETURN(navigation_bar, false, "Create navigation bar failed");
        ESP_UI_CHECK_FALSE_RETURN(navigation_bar->begin(system_screen_obj), false, "Begin navigation bar failed");
        ESP_UI_CHECK_FALSE_RETURN(navigation_bar->setVisualMode(_data.navigation_bar.visual_mode), false,
                                  "Navigation bar set visual mode failed");
    }

    // App table
    ESP_UI_CHECK_FALSE_RETURN(_app_launcher.begin(main_screen_obj), false, "Begin app launcher failed");

    _status_bar = status_bar;
    _navigation_bar = navigation_bar;
    _recents_screen = recents_screen;

    return true;
}

bool ESP_UI_PhoneHome::del(void)
{
    ESP_UI_LOGD("Delete(@0x%p)", this);

    if (!checkInitialized()) {
        return true;
    }

    if (_status_bar) {
        _status_bar.reset();
    }
    if (_navigation_bar) {
        _navigation_bar.reset();
    }
    if (_recents_screen) {
        _recents_screen.reset();
    }
    if (!_app_launcher.del()) {
        ESP_UI_LOGE("Delete app launcher failed");
    }

    return true;
}

bool ESP_UI_PhoneHome::processAppInstall(ESP_UI_CoreApp *app)
{
    ESP_UI_PhoneApp *phone_app = static_cast<ESP_UI_PhoneApp *>(app);
    ESP_UI_AppLauncherIconInfo_t icon_info = {};

    ESP_UI_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UI_LOGD("Process when app(%d) install", phone_app->getId());
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Process app launcher
    icon_info = (ESP_UI_AppLauncherIconInfo_t) {
        phone_app->getName(), phone_app->getLauncherIcon(), phone_app->getId()
    };
    if (phone_app->getLauncherIcon().resource == nullptr) {
        ESP_UI_LOGW("No launcher icon provided, use default icon");
        icon_info.image.resource = &esp_ui_phone_app_launcher_image_default;
        phone_app->setLauncherIconImage(ESP_UI_STYLE_IMAGE(&esp_ui_phone_app_launcher_image_default));
    }
    ESP_UI_CHECK_FALSE_RETURN(_app_launcher.addIcon(phone_app->getActiveData().app_launcher_page_index, icon_info),
                              false, "Add launcher icon failed");

    return true;
}

bool ESP_UI_PhoneHome::processAppUninstall(ESP_UI_CoreApp *app)
{
    ESP_UI_PhoneApp *phone_app = static_cast<ESP_UI_PhoneApp *>(app);

    ESP_UI_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UI_LOGD("Process when app(%d) uninstall", phone_app->getId());
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Process app launcher
    ESP_UI_CHECK_FALSE_RETURN(_app_launcher.removeIcon(phone_app->getId()), false, "Remove launcher icon failed");

    return true;
}

bool ESP_UI_PhoneHome::processAppRun(ESP_UI_CoreApp *app)
{
    ESP_UI_PhoneApp *phone_app = static_cast<ESP_UI_PhoneApp *>(app);

    ESP_UI_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UI_LOGD("Process when app(%d) run", phone_app->getId());
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    const ESP_UI_PhoneAppData_t &app_data = phone_app->getActiveData();
    // Process status bar
    if (_status_bar == nullptr) {
        ESP_UI_LOGD("No status_bar");
    } else {
        // Add status bar icon if needed
        if (app_data.status_icon_data.icon.image_num > 0) {
            if (app_data.flags.enable_status_icon_common_size) {
                ESP_UI_LOGD("Use common size for status icon");
                phone_app->_active_data.status_icon_data.size = _data.status_bar.data.icon_common_size;
            }
            ESP_UI_CHECK_FALSE_RETURN(
                ESP_UI_StatusBar::calibrateIconData(_data.status_bar.data, *this, phone_app->_active_data.status_icon_data),
                false, "Calibrate status icon data failed"
            );
            ESP_UI_CHECK_FALSE_RETURN(
                _status_bar->addIcon(app_data.status_icon_data, app_data.status_icon_area_index, phone_app->getId()),
                false, "Add status icon failed"
            );
        }
        // Change visibility
        ESP_UI_CHECK_FALSE_RETURN(_status_bar->setVisualMode(app_data.status_bar_visual_mode), false,
                                  "Status bar set visual mode failed");
    }

    // Process navigation bar
    if (_navigation_bar == nullptr) {
        ESP_UI_LOGD("No navigation_bar");
    } else {
        // Change visibility
        ESP_UI_CHECK_FALSE_RETURN(_navigation_bar->setVisualMode(app_data.navigation_bar_visual_mode), false,
                                  "Navigation bar set visual mode failed");
    }

    // Process recents_screen
    if (_recents_screen == nullptr) {
        ESP_UI_LOGD("No recents_screen");
    } else {
        ESP_UI_LOGD("Add recents_screen snapshot");
        ESP_UI_CHECK_FALSE_RETURN(phone_app->updateRecentsScreenSnapshotConf(nullptr), false,
                                  "Update snapshot conf failed");
        ESP_UI_CHECK_FALSE_RETURN(_recents_screen->addSnapshot(phone_app->_recents_screen_snapshot_conf), false,
                                  "RecentsScreen add snapshot failed");
    }

    return true;
}

bool ESP_UI_PhoneHome::processAppResume(ESP_UI_CoreApp *app)
{
    ESP_UI_PhoneApp *phone_app = static_cast<ESP_UI_PhoneApp *>(app);

    ESP_UI_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UI_LOGD("Process when app(%d) resume", phone_app->getId());
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    const ESP_UI_PhoneAppData_t &app_data = phone_app->getActiveData();
    // Process status bar
    if (_status_bar == nullptr) {
        ESP_UI_LOGD("No status_bar");
    } else {
        // Change visibility
        ESP_UI_CHECK_FALSE_RETURN(_status_bar->setVisualMode(app_data.status_bar_visual_mode), false,
                                  "Status bar set visual mode failed");
    }

    // Process navigation bar
    if (_navigation_bar == nullptr) {
        ESP_UI_LOGD("No navigation_bar");
    } else {
        // Change visibility
        ESP_UI_CHECK_FALSE_RETURN(_navigation_bar->setVisualMode(app_data.navigation_bar_visual_mode), false,
                                  "Navigation bar set visual mode failed");
    }

    return true;
}

bool ESP_UI_PhoneHome::processAppClose(ESP_UI_CoreApp *app)
{
    ESP_UI_PhoneApp *phone_app = static_cast<ESP_UI_PhoneApp *>(app);

    ESP_UI_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UI_LOGD("Process when app(%d) close", phone_app->getId());
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Process status bar
    if (_status_bar == nullptr) {
        ESP_UI_LOGD("No status_bar");
    } else if (phone_app->getActiveData().status_icon_data.icon.image_num > 0) {
        // Remove status bar icon if created
        ESP_UI_CHECK_FALSE_RETURN(_status_bar->removeIcon(phone_app->getId()), false, "Remove status icon failed");
    }

    // Process recents_screen
    if (_recents_screen == nullptr) {
        ESP_UI_LOGD("No recents_screen");
    } else {
        if (_recents_screen->checkSnapshotExist(phone_app->getId())) {
            ESP_UI_CHECK_FALSE_RETURN(_recents_screen->removeSnapshot(phone_app->getId()), false,
                                      "Remove snapshot failed");
        }
    }

    return true;
}

bool ESP_UI_PhoneHome::processMainScreenLoad(void)
{
    lv_obj_t *main_screen = _core.getCoreHome().getMainScreen();

    ESP_UI_LOGD("Process when load home");
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Process status bar
    if (_status_bar == nullptr) {
        ESP_UI_LOGD("No status_bar");
    } else {
        ESP_UI_CHECK_FALSE_RETURN(_status_bar->setVisualMode(_data.status_bar.visual_mode), false,
                                  "Status bar set visual mode failed");
    }

    // Process navigation bar
    if (_navigation_bar == nullptr) {
        ESP_UI_LOGD("No navigation_bar");
    } else {
        ESP_UI_CHECK_FALSE_RETURN(_navigation_bar->setVisualMode(_data.navigation_bar.visual_mode), false,
                                  "Navigation bar set visual mode failed");
    }

    ESP_UI_CHECK_FALSE_RETURN(lv_obj_is_valid(main_screen), false, "Invalid main screen");
    lv_scr_load(main_screen);

    return true;
}

bool ESP_UI_PhoneHome::getAppVisualArea(ESP_UI_CoreApp *app, lv_area_t &app_visual_area) const
{
    ESP_UI_PhoneApp *phone_app = static_cast<ESP_UI_PhoneApp *>(app);

    ESP_UI_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");

    lv_area_t visual_area = {
        .x1 = 0,
        .y1 = 0,
        .x2 = (lv_coord_t)(_core.getCoreData().screen_size.width - 1),
        .y2 = (lv_coord_t)(_core.getCoreData().screen_size.height - 1),
    };
    const ESP_UI_PhoneAppData_t &app_data = phone_app->getActiveData();

    // Process status bar
    if ((_status_bar != nullptr) && (app_data.status_bar_visual_mode == ESP_UI_STATUS_BAR_VISUAL_MODE_SHOW_FIXED)) {
        visual_area.y1 = _data.status_bar.data.main.size.height;
    }

    // Process navigation bar
    if ((_navigation_bar != nullptr) &&
            (app_data.navigation_bar_visual_mode == ESP_UI_NAVIGATION_BAR_VISUAL_MODE_SHOW_FIXED)) {
        visual_area.y2 -= _data.navigation_bar.data.main.size.height;
    }

    app_visual_area = visual_area;

    return true;
}

bool ESP_UI_PhoneHome::processRecentsScreenShow(void)
{
    ESP_UI_LOGD("Process when show recents_screen");
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_CHECK_NULL_RETURN(_recents_screen, false, "No recents_screen");

    // Process status bar
    if (_status_bar == nullptr) {
        ESP_UI_LOGD("No status_bar");
    } else {
        ESP_UI_CHECK_FALSE_RETURN(_status_bar->setVisualMode(_data.recents_screen.status_bar_visual_mode), false,
                                  "Status bar set visual mode failed");
    }

    // Process navigation bar
    if (_navigation_bar == nullptr) {
        ESP_UI_LOGD("No navigation_bar");
    } else {
        ESP_UI_CHECK_FALSE_RETURN(_navigation_bar->setVisualMode(_data.recents_screen.navigation_bar_visual_mode),
                                  false, "Navigation bar set visual mode failed");
    }

    ESP_UI_CHECK_FALSE_RETURN(_recents_screen->setVisible(true), false, "RecentsScreen show failed");

    return true;
}

bool ESP_UI_PhoneHome::calibrateData(const ESP_UI_StyleSize_t &screen_size, ESP_UI_PhoneHomeData_t &data)
{
    ESP_UI_LOGD("Calibrate data");

    // Initialize the size of flex widgets
    if (data.flags.enable_app_launcher_flex_size) {
        data.app_launcher.data.main.y_start = 0;
        data.app_launcher.data.main.size.flags.enable_height_percent = 0;
        data.app_launcher.data.main.size.height = screen_size.height;
    }
    if (data.flags.enable_recents_screen && data.flags.enable_recents_screen_flex_size) {
        data.recents_screen.data.main.y_start = 0;
        data.recents_screen.data.main.size.flags.enable_height_percent = 0;
        data.recents_screen.data.main.size.height = screen_size.height;
    }

    // Status bar
    if (data.flags.enable_status_bar) {
        ESP_UI_CHECK_FALSE_RETURN(ESP_UI_StatusBar::calibrateData(screen_size, *this, data.status_bar.data),
                                  false, "Calibrate status bar data failed");
        if (data.flags.enable_app_launcher_flex_size &&
                (data.status_bar.visual_mode == ESP_UI_STATUS_BAR_VISUAL_MODE_SHOW_FIXED)) {
            data.app_launcher.data.main.y_start += data.status_bar.data.main.size.height;
            data.app_launcher.data.main.size.height -= data.status_bar.data.main.size.height;
        }
        if (data.flags.enable_recents_screen && data.flags.enable_recents_screen_flex_size &&
                (data.recents_screen.status_bar_visual_mode == ESP_UI_STATUS_BAR_VISUAL_MODE_SHOW_FIXED)) {
            data.recents_screen.data.main.y_start += data.status_bar.data.main.size.height;
            data.recents_screen.data.main.size.height -= data.status_bar.data.main.size.height;
        }
    }
    // Navigation bar
    if (data.flags.enable_navigation_bar) {
        ESP_UI_CHECK_FALSE_RETURN(ESP_UI_NavigationBar::calibrateData(screen_size, *this, data.navigation_bar.data),
                                  false, "Calibrate navigation bar data failed");
        if (data.flags.enable_app_launcher_flex_size &&
                (data.navigation_bar.visual_mode == ESP_UI_NAVIGATION_BAR_VISUAL_MODE_SHOW_FIXED)) {
            ESP_UI_CHECK_VALUE_RETURN(data.app_launcher.data.main.y_start + data.navigation_bar.data.main.size.height,
                                      1, screen_size.height, false, "Invalid app launcher height flex");
            data.app_launcher.data.main.size.height -= data.navigation_bar.data.main.size.height;
        }
        if (data.flags.enable_recents_screen && data.flags.enable_recents_screen_flex_size &&
                (data.recents_screen.navigation_bar_visual_mode == ESP_UI_NAVIGATION_BAR_VISUAL_MODE_SHOW_FIXED)) {
            ESP_UI_CHECK_VALUE_RETURN(data.recents_screen.data.main.y_start + data.recents_screen.data.main.size.height,
                                      1, screen_size.height, false, "Invalid app launcher height flex");
            data.recents_screen.data.main.size.height -= data.navigation_bar.data.main.size.height;
        }
    }
    // Recents Screen
    if (data.flags.enable_recents_screen) {
        ESP_UI_CHECK_FALSE_RETURN(ESP_UI_RecentsScreen::calibrateData(screen_size, *this, data.recents_screen.data),
                                  false, "Calibrate recents_screen data failed");
    }
    // App table
    ESP_UI_CHECK_FALSE_RETURN(ESP_UI_AppLauncher::calibrateData(screen_size, *this, data.app_launcher.data),
                              false, "Calibrate app launcher data failed");

    return true;
}
