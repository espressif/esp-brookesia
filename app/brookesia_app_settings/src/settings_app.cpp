/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/app_settings.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "boost/json.hpp"

#include "brookesia/hal_interface.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/memory_profiler.hpp"
#include "brookesia/service_helper/media/audio.hpp"
#include "brookesia/service_helper/system/device.hpp"
#include "brookesia/service_helper/media/display.hpp"
#include "brookesia/service_helper/network/sntp.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_helper/network/wifi.hpp"

#include "private/utils.hpp"

namespace esp_brookesia::app::settings {
namespace {

using AudioPlaybackHelper = service::helper::AudioPlayback;
using DeviceHelper = service::helper::Device;
using DisplayHelper = service::helper::Display;
using SNTPHelper = service::helper::SNTP;
using StorageHelper = service::helper::Storage;
using WifiHelper = service::helper::Wifi;

static constexpr const char *APP_ID = "brookesia.general.settings";
static constexpr const char *APP_NAME = "Settings";
static constexpr const char *APP_NAME_ZH_CN = "设置";
static constexpr const char *APP_ICON_ID = "launcher_icon";
static constexpr const char *APP_ICON_PATH = "res/images/index.json";
static constexpr const char *GUI_ROOT = "res/root.json";
static constexpr const char *CONTENT_FLOW_ID = "settings_content";
static constexpr const char *HEADER_FLOW_ID = "settings_header";
static constexpr const char *WIFI_TEMPLATE_ID = "wifi_network_item";
static constexpr const char *HARDWARE_GROUP_TEMPLATE_ID = "hardware_group_card";
// Each wifi_network_item instance costs ~25KB PSRAM in per-instance allocations that cannot be
// shared across rows (LVGL widget tree, NodeRecord storage, signal binding subscriptions).
// On PSRAM-constrained targets, eagerly creating every scanned AP exhausts the heap and aborts.
// Stop creating new rows once free external memory drops below this reserve so the system keeps
// working headroom (binding flushes, screen transitions, etc.). The guard is target-adaptive:
// memory-rich targets create the full list, tight targets cap automatically.
static constexpr size_t WIFI_SLOT_MIN_FREE_PSRAM_BYTES = 320 * 1024;
static constexpr const char *LANGUAGE_TEMPLATE_ID = "menu_item";
static constexpr const char *HEADER_BACK_PATH = "/settings_header/bar/back";
static constexpr const char *HEADER_TITLE_PATH = "/settings_header/bar/title";
static constexpr const char *SAVED_NETWORK_PARENT = "/wifi/page/wifi_content/saved_networks/list/items";
static constexpr const char *AVAILABLE_NETWORK_PARENT = "/wifi/page/wifi_content/available_networks/list/items";
static constexpr const char *WIFI_STATUS_CARD_PATH = "/wifi/page/wifi_status_card";
static constexpr const char *WIFI_SWITCH_PATH = "/wifi/page/wifi_status_card/wifi_switch";
static constexpr const char *WIFI_CONTENT_PATH = "/wifi/page/wifi_content";
static constexpr const char *WIFI_SCAN_BUTTON_PATH = "/wifi/page/wifi_content/available_header/available_refresh";
static constexpr const char *WIFI_SCAN_ICON_PATH = "/wifi/page/wifi_content/available_header/available_refresh/icon";
static constexpr const char *WIFI_CONNECTED_CARD_PATH = "/wifi/page/wifi_content/connected";
static constexpr const char *WIFI_CONNECTED_SSID_PATH = "/wifi/page/wifi_content/connected/text/ssid";
static constexpr const char *WIFI_CONNECTED_DETAIL_PATH = "/wifi/page/wifi_content/connected/text/detail";
static constexpr const char *SAVED_NETWORK_EMPTY_PATH = "/wifi/page/wifi_content/saved_networks/list/empty";
static constexpr const char *AVAILABLE_NETWORK_EMPTY_PATH = "/wifi/page/wifi_content/available_networks/list/empty";
static constexpr const char *SAVED_NETWORK_PAGER_PATH = "/wifi/page/wifi_content/saved_networks/list/pager";
static constexpr const char *SAVED_NETWORK_PAGER_PREV_PATH = "/wifi/page/wifi_content/saved_networks/list/pager/prev";
static constexpr const char *SAVED_NETWORK_PAGER_LABEL_PATH = "/wifi/page/wifi_content/saved_networks/list/pager/page";
static constexpr const char *SAVED_NETWORK_PAGER_NEXT_PATH = "/wifi/page/wifi_content/saved_networks/list/pager/next";
static constexpr const char *AVAILABLE_NETWORK_PAGER_PATH = "/wifi/page/wifi_content/available_networks/list/pager";
static constexpr const char *AVAILABLE_NETWORK_PAGER_PREV_PATH =
    "/wifi/page/wifi_content/available_networks/list/pager/prev";
static constexpr const char *AVAILABLE_NETWORK_PAGER_LABEL_PATH =
    "/wifi/page/wifi_content/available_networks/list/pager/page";
static constexpr const char *AVAILABLE_NETWORK_PAGER_NEXT_PATH =
    "/wifi/page/wifi_content/available_networks/list/pager/next";
static constexpr const char *HOME_WIFI_VALUE_PATH = "/settings_home/page/main_list/wifi/value_box/value";
static constexpr const char *WIFI_CONNECT_SSID_PATH = "/wifi_connect/page/network_card/ssid";
static constexpr const char *WIFI_CONNECT_DETAIL_PATH = "/wifi_connect/page/network_card/detail";
static constexpr const char *WIFI_CONNECT_PASSWORD_VALUE_PATH =
    "/wifi_connect/page/password_card/password_row/value";
static constexpr const char *WIFI_CONNECT_STATUS_PATH = "/wifi_connect/page/status";
static constexpr const char *WIFI_CONNECT_BUTTON_PATH = "/wifi_connect/page/actions/connect";
static constexpr const char *LANGUAGE_LIST_PARENT = "/language/page/language_card/list";
static constexpr const char *LANGUAGE_SELECT_ACTION = "settings.language.select";
static constexpr const char *WIFI_SELECT_ACTION = "settings.wifi.select";
static constexpr const char *WIFI_SAVED_FORGET_ACTION = "settings.wifi.saved.forget";
static constexpr const char *WIFI_TOGGLE_ACTION = "settings.wifi.toggle";
static constexpr const char *WIFI_SCAN_ACTION = "settings.wifi.scan";
static constexpr const char *WIFI_DISCONNECT_ACTION = "settings.wifi.disconnect";
static constexpr const char *WIFI_SAVED_PREV_ACTION = "settings.wifi.saved.prev";
static constexpr const char *WIFI_SAVED_NEXT_ACTION = "settings.wifi.saved.next";
static constexpr const char *WIFI_AVAILABLE_PREV_ACTION = "settings.wifi.available.prev";
static constexpr const char *WIFI_AVAILABLE_NEXT_ACTION = "settings.wifi.available.next";
static constexpr const char *LOCALE_EN = "en";
static constexpr const char *LOCALE_ZH_CN = "zh_CN";
static constexpr const char *APP_NAME_I18N_KEY = "app_name";
static constexpr const char *THEME_LIGHT = "light";
static constexpr const char *THEME_DARK = "dark";
static constexpr const char *PAGE_HOME = "settings_home";
static constexpr const char *PAGE_DEVICE = "my_device";
static constexpr const char *PAGE_WIFI = "wifi";
static constexpr const char *PAGE_WIFI_CONNECT = "wifi_connect";
static constexpr const char *PAGE_SOUND = "sound";
static constexpr const char *PAGE_DISPLAY = "display";
static constexpr const char *PAGE_MORE = "more";
static constexpr const char *PAGE_LANGUAGE = "language";
static constexpr const char *PAGE_TIME_ZONE = "time_zone";
static constexpr const char *ACTION_OPEN_HOME = "settings.open.home";
static constexpr const char *ACTION_HEADER_BACK = "settings.header.back";
static constexpr const char *ACTION_BACK_LANGUAGE = "settings.back.language";
static constexpr const char *ACTION_OPEN_WIFI_CONNECT = "settings.open.wifi_connect";
static constexpr const char *ACTION_BACK_WIFI_CONNECT = "settings.back.wifi_connect";
static constexpr const char *ACTION_WIFI_PASSWORD_EDIT = "settings.wifi.password.edit";
static constexpr const char *ACTION_WIFI_CONNECT_CANCEL = "settings.wifi.connect.cancel";
static constexpr const char *ACTION_WIFI_CONNECT_SUBMIT = "settings.wifi.connect.submit";
static constexpr const char *ACTION_DISPLAY_BRIGHTNESS = "settings.display.brightness";
static constexpr const char *ACTION_SOUND_VOLUME = "settings.sound.volume";
static constexpr const char *ACTION_SOUND_MUTE = "settings.sound.mute";
static constexpr const char *LANGUAGE_SWITCH_TIMER_NAME = "settings.language.switch";
static constexpr const char *THEME_SWITCH_TIMER_NAME = "settings.theme.switch";
static constexpr const char *WIFI_SCAN_RETRY_TIMER_NAME = "settings.wifi.scan.retry";
static constexpr const char *WIFI_CONNECTED_HIDE_TIMER_NAME = "settings.wifi.connected.hide";
static constexpr const char *WIFI_CONNECTED_SCROLL_TIMER_NAME = "settings.wifi.connected.scroll";
static constexpr const char *TIME_ZONE_OPTION_UTC_ACTION = "settings.time_zone.utc";
static constexpr const char *TIME_ZONE_OPTION_CHINA_ACTION = "settings.time_zone.china";
static constexpr const char *TIME_ZONE_OPTION_JAPAN_ACTION = "settings.time_zone.japan";
static constexpr const char *TIME_ZONE_OPTION_EASTERN_ACTION = "settings.time_zone.eastern";
static constexpr const char *TIME_ZONE_OPTION_PACIFIC_ACTION = "settings.time_zone.pacific";
static constexpr const char *TIME_ZONE_OPTION_CENTRAL_EUROPE_ACTION = "settings.time_zone.central_europe";
static constexpr const char *MORE_TIME_ZONE_VALUE_PATH = "/more/page/language_card/time_zone/value_box/value";
static constexpr const char *TIME_ZONE_CURRENT_VALUE_PATH = "/time_zone/page/status_card/current/value";
static constexpr const char *TIME_ZONE_STATE_VALUE_PATH = "/time_zone/page/status_card/state/value_box/value";
static constexpr const char *TIME_ZONE_OPTION_UTC_VALUE_PATH =
    "/time_zone/page/options_card/utc/value_box/value";
static constexpr const char *TIME_ZONE_OPTION_CHINA_VALUE_PATH =
    "/time_zone/page/options_card/china/value_box/value";
static constexpr const char *TIME_ZONE_OPTION_JAPAN_VALUE_PATH =
    "/time_zone/page/options_card/japan/value_box/value";
static constexpr const char *TIME_ZONE_OPTION_EASTERN_VALUE_PATH =
    "/time_zone/page/options_card/eastern/value_box/value";
static constexpr const char *TIME_ZONE_OPTION_PACIFIC_VALUE_PATH =
    "/time_zone/page/options_card/pacific/value_box/value";
static constexpr const char *TIME_ZONE_OPTION_CENTRAL_EUROPE_VALUE_PATH =
    "/time_zone/page/options_card/central_europe/value_box/value";
static constexpr const char *MY_DEVICE_SYSTEM_NAME_PATH = "/my_device/page/hero/hero_content/os";
static constexpr const char *MY_DEVICE_SYSTEM_VERSION_PATH = "/my_device/page/hero/hero_content/version";
static constexpr const char *MY_DEVICE_DEVICE_VALUE_PATH = "/my_device/page/summary_card/device_value";
static constexpr const char *MY_DEVICE_HARDWARE_GROUP_PARENT = "/my_device/page/hardware_groups";
static constexpr const char *DISPLAY_BRIGHTNESS_ROW_PATH = "/display/page/brightness_card/brightness_row";
static constexpr const char *DISPLAY_BRIGHTNESS_SLIDER_PATH = "/display/page/brightness_card/brightness_row/slider";
static constexpr const char *SOUND_VOLUME_ROW_PATH = "/sound/page/volume_card/media_volume_row";
static constexpr const char *SOUND_VOLUME_SLIDER_PATH = "/sound/page/volume_card/media_volume_row/slider";
static constexpr const char *SOUND_MUTE_ROW_PATH = "/sound/page/silent_card/silent_mode";
static constexpr const char *SOUND_MUTE_TOGGLE_PATH = "/sound/page/silent_card/silent_mode/toggle";
static constexpr const char *HOME_SOUND_VALUE_PATH = "/settings_home/page/main_list/sound/value_box/value";
static constexpr int SETTINGS_SWITCH_DEFER_MS = 32;
static constexpr int WIFI_CONNECTED_SCROLL_DELAY_MS = 50;
static constexpr uint32_t DEVICE_SERVICE_TIMEOUT_MS = 500;
static constexpr uint32_t WIFI_SERVICE_TIMEOUT_MS = 1000;
static constexpr uint32_t SNTP_SERVICE_TIMEOUT_MS = 500;
static constexpr uint32_t WIFI_SCAN_WINDOW_MS = 10000;
static constexpr size_t WIFI_SCAN_AP_LIMIT = 64;
static constexpr int WIFI_SCAN_RETRY_DELAY_MS = 250;
static constexpr size_t WIFI_SCAN_INTERRUPTED_RETRY_COUNT = 1;
static constexpr int WIFI_DISCONNECTED_HIDE_DELAY_MS = 3000;
static constexpr int DISABLED_OPACITY = 96;
static constexpr int ENABLED_OPACITY = 255;
static constexpr int MESSAGE_DIALOG_SUCCESS_AUTO_CLOSE_MS = 700;
static constexpr int MESSAGE_DIALOG_FAILED_AUTO_CLOSE_MS = 3000;
static constexpr bool SETTINGS_HARDWARE_OPERATIONS_ENABLED = true;
static constexpr const char *MY_DEVICE_FALLBACK_SYSTEM_NAME = "System";
static constexpr const char *MY_DEVICE_FALLBACK_SYSTEM_VERSION = "--";
static constexpr const char *SETTINGS_STORAGE_NAMESPACE = "app.settings";
static constexpr const char *WIFI_ENABLED_STORAGE_KEY = "WifiEnabled";
static constexpr uint32_t SETTINGS_STORAGE_TIMEOUT_MS = WIFI_SERVICE_TIMEOUT_MS;

static constexpr std::array<const char *, 16> NAVIGATION_ACTIONS = {
    ACTION_OPEN_HOME,
    "settings.back.device",
    "settings.back.wifi",
    ACTION_BACK_WIFI_CONNECT,
    "settings.back.sound",
    "settings.back.display",
    ACTION_BACK_LANGUAGE,
    "settings.back.time_zone",
    "settings.open.device",
    "settings.open.wifi",
    ACTION_OPEN_WIFI_CONNECT,
    "settings.open.sound",
    "settings.open.display",
    "settings.open.more",
    "settings.open.language",
    "settings.open.time_zone",
};

static constexpr std::array<const char *, 2> THEME_ACTIONS = {
    "settings.display.light",
    "settings.display.dark",
};

static constexpr std::array<const char *, 6> TIME_ZONE_ACTIONS = {
    TIME_ZONE_OPTION_UTC_ACTION,
    TIME_ZONE_OPTION_CHINA_ACTION,
    TIME_ZONE_OPTION_JAPAN_ACTION,
    TIME_ZONE_OPTION_EASTERN_ACTION,
    TIME_ZONE_OPTION_PACIFIC_ACTION,
    TIME_ZONE_OPTION_CENTRAL_EUROPE_ACTION,
};

struct NavigationTarget {
    std::string_view action;
    std::string_view page;
};

struct SettingsKvName {
    std::string nspace;
    std::string key;
};

std::expected<std::string, std::string> make_settings_kv_namespace(std::string_view raw_namespace)
{
    auto result = StorageHelper::make_kv_namespace({raw_namespace}, ".", SETTINGS_STORAGE_TIMEOUT_MS);
    if (!result) {
        return std::unexpected(
                   "Failed to make Settings KV namespace '" + std::string(raw_namespace) + "': " + result.error()
               );
    }
    if (result->hashed) {
        BROOKESIA_LOGD("%1%", result->warning);
    }
    return result->name;
}

std::expected<std::string, std::string> make_settings_kv_key(std::string_view raw_key)
{
    auto result = StorageHelper::make_kv_key({raw_key}, ".", SETTINGS_STORAGE_TIMEOUT_MS);
    if (!result) {
        return std::unexpected(
                   "Failed to make Settings KV key '" + std::string(raw_key) + "': " + result.error()
               );
    }
    if (result->hashed) {
        BROOKESIA_LOGD("%1%", result->warning);
    }
    return result->name;
}

std::expected<SettingsKvName, std::string> make_settings_kv_name(
    std::string_view raw_namespace,
    std::string_view raw_key
)
{
    auto namespace_result = make_settings_kv_namespace(raw_namespace);
    if (!namespace_result) {
        return std::unexpected(namespace_result.error());
    }

    auto key_result = make_settings_kv_key(raw_key);
    if (!key_result) {
        return std::unexpected(key_result.error());
    }

    return SettingsKvName{
        .nspace = std::move(namespace_result.value()),
        .key = std::move(key_result.value()),
    };
}

struct SelectableThemeTokens {
    std::string border_width;
    std::string border_color;
    std::string label_color;
    std::string selected_border_width;
    std::string selected_border_color;
    std::string selected_label_color;
};

struct HardwareGroup {
    std::string key;
    std::string title;
    std::vector<std::string> lines;
};

struct TimeZoneOption {
    std::string_view action;
    std::string_view timezone;
    std::string_view value_path;
};

struct I18nBundle {
    std::vector<gui::BindingValueUpdate> updates;
    std::unordered_map<std::string, std::string> strings;
};

std::unordered_map<std::string, std::unordered_map<std::string, std::string>> i18n_string_tables;

std::string make_app_version()
{
    return std::to_string(BROOKESIA_APP_SETTINGS_VER_MAJOR) + "." +
           std::to_string(BROOKESIA_APP_SETTINGS_VER_MINOR) + "." +
           std::to_string(BROOKESIA_APP_SETTINGS_VER_PATCH);
}

static constexpr std::array<NavigationTarget, 16> NAVIGATION_TARGETS = {
    NavigationTarget{ACTION_OPEN_HOME, PAGE_HOME},
    NavigationTarget{"settings.back.device", PAGE_HOME},
    NavigationTarget{"settings.back.wifi", PAGE_HOME},
    NavigationTarget{ACTION_BACK_WIFI_CONNECT, PAGE_WIFI},
    NavigationTarget{"settings.back.sound", PAGE_HOME},
    NavigationTarget{"settings.back.display", PAGE_HOME},
    NavigationTarget{ACTION_BACK_LANGUAGE, PAGE_MORE},
    NavigationTarget{"settings.back.time_zone", PAGE_MORE},
    NavigationTarget{"settings.open.device", PAGE_DEVICE},
    NavigationTarget{"settings.open.wifi", PAGE_WIFI},
    NavigationTarget{ACTION_OPEN_WIFI_CONNECT, PAGE_WIFI_CONNECT},
    NavigationTarget{"settings.open.sound", PAGE_SOUND},
    NavigationTarget{"settings.open.display", PAGE_DISPLAY},
    NavigationTarget{"settings.open.more", PAGE_MORE},
    NavigationTarget{"settings.open.language", PAGE_LANGUAGE},
    NavigationTarget{"settings.open.time_zone", PAGE_TIME_ZONE},
};

static constexpr std::array<TimeZoneOption, 6> TIME_ZONE_OPTIONS = {
    TimeZoneOption{TIME_ZONE_OPTION_UTC_ACTION, "UTC0", TIME_ZONE_OPTION_UTC_VALUE_PATH},
    TimeZoneOption{TIME_ZONE_OPTION_CHINA_ACTION, "CST-8", TIME_ZONE_OPTION_CHINA_VALUE_PATH},
    TimeZoneOption{TIME_ZONE_OPTION_JAPAN_ACTION, "JST-9", TIME_ZONE_OPTION_JAPAN_VALUE_PATH},
    TimeZoneOption{
        TIME_ZONE_OPTION_EASTERN_ACTION,
        "EST5EDT,M3.2.0/2,M11.1.0/2",
        TIME_ZONE_OPTION_EASTERN_VALUE_PATH
    },
    TimeZoneOption{
        TIME_ZONE_OPTION_PACIFIC_ACTION,
        "PST8PDT,M3.2.0/2,M11.1.0/2",
        TIME_ZONE_OPTION_PACIFIC_VALUE_PATH
    },
    TimeZoneOption{
        TIME_ZONE_OPTION_CENTRAL_EUROPE_ACTION,
        "CET-1CEST,M3.5.0/2,M10.5.0/3",
        TIME_ZONE_OPTION_CENTRAL_EUROPE_VALUE_PATH
    },
};

bool is_navigation_action(std::string_view action)
{
    for (const auto *known_action : NAVIGATION_ACTIONS) {
        if (action == known_action) {
            return true;
        }
    }
    return false;
}

bool is_theme_action(std::string_view action)
{
    for (const auto *known_action : THEME_ACTIONS) {
        if (action == known_action) {
            return true;
        }
    }
    return false;
}

std::optional<std::string_view> get_time_zone_for_action(std::string_view action)
{
    for (const auto &option : TIME_ZONE_OPTIONS) {
        if (action == option.action) {
            return option.timezone;
        }
    }
    return std::nullopt;
}

int clamp_percent(double value)
{
    return std::clamp(static_cast<int>(std::lround(value)), 0, 100);
}

std::string make_percent_text(int value)
{
    return std::to_string(std::clamp(value, 0, 100)) + "%";
}

std::optional<std::string> find_localized_text(std::string_view locale, std::string_view key);
std::string localized_text(std::string_view locale, std::string_view key);
std::string localized_text_or(std::string_view locale, std::string_view key, std::string_view fallback);

std::string make_localized_unavailable_text(std::string_view locale)
{
    return localized_text(locale, "text.unavailable");
}

std::string make_localized_muted_text(std::string_view locale)
{
    return localized_text(locale, "text.muted");
}

std::string make_localized_unknown_text(std::string_view locale)
{
    return localized_text(locale, "text.unknown");
}

std::string make_localized_hardware_title(std::string_view locale, std::string_view key)
{
    if (key == "System") {
        return localized_text(locale, "text.hardware.system");
    }
    if (key == "Display") {
        return localized_text(locale, "text.hardware.display");
    }
    if (key == "Audio") {
        return localized_text(locale, "text.hardware.audio");
    }
    if (key == "Video") {
        return localized_text(locale, "text.hardware.video");
    }
    if (key == "Network") {
        return localized_text(locale, "text.hardware.network");
    }
    if (key == "Power") {
        return localized_text(locale, "text.hardware.power");
    }
    if (key == "Storage") {
        return localized_text(locale, "text.hardware.storage");
    }
    return localized_text(locale, "text.hardware.other");
}

std::string to_lower_copy(std::string_view value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

std::string classify_hardware_interface(std::string_view type_name, std::string_view instance_name)
{
    const auto combined = to_lower_copy(std::string(type_name) + " " + std::string(instance_name));
    if (combined.find("display") != std::string::npos || combined.find("touch") != std::string::npos ||
            combined.find("panel") != std::string::npos || combined.find("backlight") != std::string::npos) {
        return "Display";
    }
    if (combined.find("audio") != std::string::npos || combined.find("codec") != std::string::npos ||
            combined.find("player") != std::string::npos) {
        return "Audio";
    }
    if (combined.find("camera") != std::string::npos || combined.find("video") != std::string::npos) {
        return "Video";
    }
    if (combined.find("network") != std::string::npos || combined.find("wifi") != std::string::npos ||
            combined.find("connectivity") != std::string::npos) {
        return "Network";
    }
    if (combined.find("battery") != std::string::npos || combined.find("power") != std::string::npos) {
        return "Power";
    }
    if (combined.find("storage") != std::string::npos || combined.find("filesystem") != std::string::npos ||
            combined.find("file") != std::string::npos) {
        return "Storage";
    }
    if (combined.find("system") != std::string::npos || combined.find("board") != std::string::npos) {
        return "System";
    }
    return "Other";
}

HardwareGroup &find_or_create_hardware_group(
    std::vector<HardwareGroup> &groups,
    std::string key,
    std::string_view locale
)
{
    auto it = std::find_if(groups.begin(), groups.end(), [&key](const HardwareGroup & group) {
        return group.key == key;
    });
    if (it != groups.end()) {
        return *it;
    }
    groups.push_back(HardwareGroup{
        .key = key,
        .title = make_localized_hardware_title(locale, key),
        .lines = {},
    });
    return groups.back();
}

std::string join_hardware_lines(const std::vector<std::string> &lines)
{
    std::ostringstream stream;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            stream << '\n';
        }
        stream << lines[i];
    }
    return stream.str();
}

std::optional<std::string_view> get_navigation_page(std::string_view action)
{
    for (const auto &target : NAVIGATION_TARGETS) {
        if (target.action == action) {
            return target.page;
        }
    }
    return std::nullopt;
}

std::string_view get_header_back_action_for_page(std::string_view page)
{
    if (page == PAGE_WIFI_CONNECT) {
        return ACTION_BACK_WIFI_CONNECT;
    }
    if (page == PAGE_LANGUAGE) {
        return ACTION_BACK_LANGUAGE;
    }
    if (page == PAGE_TIME_ZONE) {
        return "settings.back.time_zone";
    }
    if (page == PAGE_DEVICE) {
        return "settings.back.device";
    }
    if (page == PAGE_WIFI) {
        return "settings.back.wifi";
    }
    if (page == PAGE_SOUND) {
        return "settings.back.sound";
    }
    if (page == PAGE_DISPLAY) {
        return "settings.back.display";
    }
    return ACTION_OPEN_HOME;
}

std::string join_path(std::string_view parent, std::string_view child)
{
    std::string path(parent);
    path += "/";
    path += child;
    return path;
}

std::string make_language_instance_id(std::string_view locale)
{
    std::string id("lang_");
    for (char ch : locale) {
        const auto unsigned_ch = static_cast<unsigned char>(ch);
        id.push_back(std::isalnum(unsigned_ch) != 0 ? ch : '_');
    }
    return id;
}

std::string normalize_locale(std::string_view locale)
{
    if (locale.empty()) {
        return LOCALE_EN;
    }
    if (locale == "zh") {
        return LOCALE_ZH_CN;
    }
    return std::string(locale);
}

std::string make_runtime_language(std::string_view locale)
{
    return std::string(locale);
}

std::string make_wifi_instance_id(std::string_view prefix, std::string_view ssid, size_t index)
{
    std::string id(prefix);
    id += "_";
    for (char ch : ssid) {
        const auto unsigned_ch = static_cast<unsigned char>(ch);
        id.push_back(std::isalnum(unsigned_ch) != 0 ? static_cast<char>(std::tolower(unsigned_ch)) : '_');
    }
    id += "_";
    id += std::to_string(index);
    return id;
}

std::string get_wifi_band_text(int channel)
{
    if (channel >= 36) {
        return "5G";
    }
    if (channel > 0) {
        return "2.4G";
    }
    return "";
}

std::string get_wifi_signal_text(int signal_level)
{
    if (signal_level <= 0) {
        return "";
    }
    return std::to_string(std::clamp(signal_level, 0, 4)) + "/4";
}

size_t get_wifi_list_page_size()
{
    return static_cast<size_t>(std::clamp(
                                   BROOKESIA_APP_SETTINGS_WIFI_MAX_VISIBLE_APS,
                                   1,
                                   64
                               ));
}

size_t get_page_count(size_t total_count)
{
    const auto page_size = get_wifi_list_page_size();
    if (total_count == 0) {
        return 1;
    }
    return (total_count + page_size - 1) / page_size;
}

size_t get_page_start(size_t page)
{
    return page * get_wifi_list_page_size();
}

size_t get_page_visible_count(size_t total_count, size_t page)
{
    const auto start = get_page_start(page);
    if (start >= total_count) {
        return 0;
    }
    return std::min(get_wifi_list_page_size(), total_count - start);
}

std::string make_page_text(size_t page, size_t page_count)
{
    const auto safe_page_count = std::max<size_t>(page_count, 1);
    const auto display_page = std::min(page + 1, safe_page_count);
    return std::to_string(display_page) + " / " + std::to_string(safe_page_count);
}

std::string extract_child_instance_id(std::string_view parent, std::string_view path)
{
    if (!path.starts_with(parent)) {
        return {};
    }
    auto instance_id = std::string(path.substr(parent.size()));
    if (!instance_id.empty() && instance_id.front() == '/') {
        instance_id.erase(instance_id.begin());
    }
    const auto child_separator = instance_id.find('/');
    if (child_separator != std::string::npos) {
        instance_id.resize(child_separator);
    }
    return instance_id;
}

bool is_wifi_connecting_state(std::string_view state)
{
    return state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connecting);
}

bool is_wifi_started_state(std::string_view state)
{
    return state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Started) ||
           state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connecting) ||
           state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connected) ||
           state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Disconnecting);
}

bool is_wifi_started_event(std::string_view event)
{
    return event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Started) ||
           event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Connected);
}

std::string make_password_summary(std::string_view password, std::string_view empty_text)
{
    if (password.empty()) {
        return std::string(empty_text);
    }
    return std::string(std::clamp<size_t>(password.size(), 4, 12), '*');
}

std::string get_language_display_name(std::string_view locale, std::string_view language_locale)
{
    const auto key = std::string("language.display.") + std::string(language_locale);
    return localized_text_or(locale, key, language_locale);
}

std::string get_language_summary_name(std::string_view locale)
{
    const auto key = std::string("language.summary.") + std::string(locale);
    return localized_text_or(locale, key, locale);
}

std::string get_time_zone_summary_name(std::string_view locale, std::string_view timezone)
{
    if (timezone.empty()) {
        return localized_text(locale, "text.unavailable");
    }
    const auto key = std::string("time_zone.name.") + std::string(timezone);
    return localized_text_or(locale, key, timezone);
}

std::vector<std::string> normalize_language_list(std::vector<std::string> languages)
{
    if (languages.empty()) {
        languages.push_back(LOCALE_EN);
    }

    std::vector<std::string> normalized_languages;
    normalized_languages.reserve(languages.size());
    for (const auto &locale : languages) {
        const auto normalized_locale = normalize_locale(locale);
        if (std::find(normalized_languages.begin(), normalized_languages.end(), normalized_locale) ==
                normalized_languages.end()) {
            normalized_languages.push_back(normalized_locale);
        }
    }
    languages = std::move(normalized_languages);

    std::vector<std::string> ordered;
    for (std::string_view preferred : {
                std::string_view(LOCALE_EN), std::string_view(LOCALE_ZH_CN)
            }) {
        const std::string preferred_locale(preferred);
        auto it = std::find(languages.begin(), languages.end(), preferred_locale);
        if (it != languages.end()) {
            ordered.push_back(*it);
            languages.erase(it);
        }
    }
    std::sort(languages.begin(), languages.end());
    ordered.insert(ordered.end(), languages.begin(), languages.end());
    return ordered;
}

std::string make_resource_path(system::core::AppContext &context, std::string_view relative_path)
{
    auto app_paths = context.system_service().get_app_storage_paths();
    if (!app_paths || !app_paths->internal.available) {
        return std::filesystem::path(relative_path).lexically_normal().generic_string();
    }
    return (
               std::filesystem::path(app_paths->internal.root) /
               std::filesystem::path(relative_path)
           ).lexically_normal().generic_string();
}

std::expected<std::string, std::string> read_text_file(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::unexpected("Failed to open file: " + path);
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

std::string json_value_to_string(const boost::json::value &value)
{
    if (value.is_string()) {
        return value.as_string().c_str();
    }
    return boost::json::serialize(value);
}

std::optional<std::string> load_manifest_i18n_string(std::string_view locale, std::string_view key)
{
    const std::array resource_roots = {
        std::filesystem::path(BROOKESIA_APP_SETTINGS_PACKAGE_DIR),
        std::filesystem::path(BROOKESIA_APP_SETTINGS_RESOURCE_DIR),
    };
    for (const auto &resource_root : resource_roots) {
        if (resource_root.empty()) {
            continue;
        }

        const auto path = resource_root / "res" / "i18n" / (std::string(locale) + ".json");
        auto file_content = read_text_file(path.generic_string());
        if (!file_content) {
            continue;
        }

        boost::system::error_code error_code;
        auto parsed = boost::json::parse(*file_content, error_code);
        if (error_code || !parsed.is_object()) {
            continue;
        }

        const auto *strings = parsed.as_object().if_contains("strings");
        if (strings == nullptr || !strings->is_object()) {
            continue;
        }

        const auto *value = strings->as_object().if_contains(key);
        if (value != nullptr && value->is_string()) {
            return std::string(value->as_string().c_str());
        }
    }
    return std::nullopt;
}

std::string localized_manifest_text(std::string_view locale, std::string_view key, std::string_view fallback)
{
    if (auto text = load_manifest_i18n_string(locale, key); text.has_value()) {
        return *text;
    }
    if (locale != LOCALE_EN) {
        if (auto text = load_manifest_i18n_string(LOCALE_EN, key); text.has_value()) {
            return *text;
        }
    }
    return std::string(fallback);
}

std::expected<SelectableThemeTokens, std::string> load_selectable_theme_tokens(
    system::core::AppContext &context,
    std::string_view theme_id
)
{
    (void)context;
    (void)theme_id;
    return SelectableThemeTokens{
        .border_width = "1dp",
        .border_color = "${color.border.default}",
        .label_color = "${color.text.default}",
        .selected_border_width = "3dp",
        .selected_border_color = "${color.primary.fill}",
        .selected_label_color = "${color.primary.fill}",
    };
}

std::expected<I18nBundle, std::string> load_i18n_bundle(
    system::core::AppContext &context,
    std::string_view locale
)
{
    std::string relative_path = "res/i18n/";
    relative_path += locale;
    relative_path += ".json";

    const auto path = make_resource_path(context, relative_path);
    auto file_content = read_text_file(path);
    if (!file_content) {
        return std::unexpected(file_content.error());
    }

    boost::system::error_code error_code;
    auto parsed = boost::json::parse(*file_content, error_code);
    if (error_code) {
        return std::unexpected("Failed to parse i18n file '" + path + "': " + error_code.message());
    }
    if (!parsed.is_object()) {
        return std::unexpected("I18N file root must be an object: " + path);
    }
    const auto &root = parsed.as_object();
    const auto *updates_value = root.if_contains("updates");
    if (updates_value == nullptr || !updates_value->is_array()) {
        return std::unexpected("I18N file must contain an array field 'updates': " + path);
    }

    I18nBundle bundle;
    const auto *strings_value = root.if_contains("strings");
    if (strings_value != nullptr) {
        if (!strings_value->is_object()) {
            return std::unexpected("I18N file field 'strings' must be an object: " + path);
        }
        for (const auto &entry : strings_value->as_object()) {
            if (!entry.value().is_string()) {
                return std::unexpected("I18N string entry must be a string: " + path);
            }
            bundle.strings.emplace(entry.key(), entry.value().as_string().c_str());
        }
    }

    for (const auto &entry_value : updates_value->as_array()) {
        if (!entry_value.is_object()) {
            return std::unexpected("I18N update entry must be an object: " + path);
        }
        const auto &entry = entry_value.as_object();
        const auto *path_value = entry.if_contains("path");
        const auto *key_value = entry.if_contains("key");
        const auto *value = entry.if_contains("value");
        if (path_value == nullptr || !path_value->is_string() ||
                key_value == nullptr || !key_value->is_string() ||
                value == nullptr) {
            return std::unexpected("I18N update entry must contain string path/key and value: " + path);
        }
        bundle.updates.push_back(gui::BindingValueUpdate{
            .absolute_path = path_value->as_string().c_str(),
            .key = key_value->as_string().c_str(),
            .value = json_value_to_string(*value),
        });
    }

    return bundle;
}

void add_binding_update(
    std::vector<gui::BindingValueUpdate> &updates,
    std::string absolute_path,
    std::string key,
    std::string value
)
{
    updates.push_back(gui::BindingValueUpdate{
        .absolute_path = std::move(absolute_path),
        .key = std::move(key),
        .value = std::move(value),
    });
}

void add_theme_mode_updates(
    std::vector<gui::BindingValueUpdate> &updates,
    std::string_view path,
    bool selected,
    const SelectableThemeTokens &tokens,
    std::string_view preview_bg_color
)
{
    const std::string path_string(path);
    add_binding_update(
        updates,
        path_string,
        "borderWidth",
        selected ? tokens.selected_border_width : tokens.border_width
    );
    add_binding_update(
        updates,
        path_string,
        "borderColor",
        selected ? tokens.selected_border_color : tokens.border_color
    );
    add_binding_update(
        updates,
        path_string,
        "clickable",
        selected ? "false" : "true"
    );
    add_binding_update(
        updates,
        path_string + "/preview",
        "previewBgColor",
        std::string(preview_bg_color)
    );
    add_binding_update(
        updates,
        path_string + "/label",
        "textColor",
        selected ? tokens.selected_label_color : tokens.label_color
    );
}

void add_control_state_updates(
    std::vector<gui::BindingValueUpdate> &updates,
    std::string_view row_path,
    std::string_view control_path,
    bool enabled
)
{
    add_binding_update(updates, std::string(row_path), "disabled", enabled ? "false" : "true");
    add_binding_update(
        updates,
        std::string(row_path),
        "opacity",
        std::to_string(enabled ? ENABLED_OPACITY : DISABLED_OPACITY)
    );
    add_binding_update(updates, std::string(control_path), "disabled", enabled ? "false" : "true");
}

std::optional<std::string> find_localized_text(std::string_view locale, std::string_view key)
{
    auto locale_it = i18n_string_tables.find(std::string(locale));
    if (locale_it != i18n_string_tables.end()) {
        auto text_it = locale_it->second.find(std::string(key));
        if (text_it != locale_it->second.end()) {
            return text_it->second;
        }
    }
    if (locale != LOCALE_EN) {
        auto english_it = i18n_string_tables.find(LOCALE_EN);
        if (english_it != i18n_string_tables.end()) {
            auto text_it = english_it->second.find(std::string(key));
            if (text_it != english_it->second.end()) {
                return text_it->second;
            }
        }
    }
    return std::nullopt;
}

std::string localized_text(std::string_view locale, std::string_view key)
{
    if (auto text = find_localized_text(locale, key); text.has_value()) {
        return *text;
    }
    BROOKESIA_LOGD("Missing Settings i18n string '%1%' for locale '%2%'", key, locale);
    return std::string(key);
}

std::string localized_text_or(std::string_view locale, std::string_view key, std::string_view fallback)
{
    if (auto text = find_localized_text(locale, key); text.has_value()) {
        return *text;
    }
    BROOKESIA_LOGD("Missing Settings i18n string '%1%' for locale '%2%'", key, locale);
    return std::string(fallback);
}

} // namespace

struct SettingsApp::Impl {
    system::core::AppContext *context = nullptr;
    std::vector<std::string> dynamic_wifi_paths;
    std::vector<std::string> dynamic_hardware_group_paths;
    size_t saved_wifi_slot_count = 0;
    size_t available_wifi_slot_count = 0;
    size_t saved_wifi_page = 0;
    size_t available_wifi_page = 0;
    std::vector<std::string> dynamic_language_paths;
    std::unordered_map<std::string, WifiNetworkState> wifi_instance_to_network;
    std::unordered_map<std::string, std::string> language_instance_to_locale;
    std::optional<WifiNetworkState> selected_wifi_network;
    std::string wifi_forget_click_suppression_id;
    std::string wifi_forget_click_suppression_ssid;
    std::string wifi_connect_password;
    std::string connected_wifi_ssid;
    gui::ScopedConnection language_action_connection;
    gui::ScopedConnection wifi_select_action_connection;
    gui::ScopedConnection wifi_forget_action_connection;
    gui::ScopedConnection brightness_action_connection;
    gui::ScopedConnection volume_action_connection;
    gui::ScopedConnection mute_action_connection;
    service::ServiceBinding storage_service_binding;
    service::ServiceBinding device_service_binding;
    service::ServiceBinding display_service_binding;
    service::ServiceBinding audio_playback_service_binding;
    service::ServiceBinding wifi_service_binding;
    service::ServiceBinding sntp_service_binding;
    std::vector<esp_brookesia::lib_utils::scoped_connection> device_event_connections;
    std::vector<esp_brookesia::lib_utils::scoped_connection> wifi_event_connections;
    std::vector<esp_brookesia::lib_utils::scoped_connection> sntp_event_connections;
    uint64_t wifi_operation_generation = 0;
    DeviceUiState device_ui_state;
    WifiUiState wifi_ui_state;
    TimeZoneUiState time_zone_ui_state;
    std::vector<WifiNetworkState> saved_wifi_networks;
    std::vector<WifiNetworkState> available_wifi_networks;
    bool saved_wifi_networks_loaded = false;
    bool saved_wifi_networks_refreshing = false;
    bool available_wifi_rows_hidden_for_scan = false;
    bool available_wifi_scan_results_ready = false;
    bool wifi_scan_stop_after_first_result = false;
    size_t wifi_scan_retry_remaining = 0;
    std::string pending_language_locale;
    std::string pending_theme_id;
    system::core::TimerId pending_language_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId pending_theme_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId pending_wifi_scan_retry_timer_id = system::core::INVALID_TIMER_ID;
    uint64_t pending_wifi_scan_retry_generation = 0;
    system::core::TimerId pending_wifi_connected_hide_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId pending_wifi_connected_scroll_timer_id = system::core::INVALID_TIMER_ID;
    system::core::MessageDialogRequestId message_dialog_request_id =
        system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
    system::core::KeyboardRequestId wifi_keyboard_request_id =
        system::core::INVALID_KEYBOARD_REQUEST_ID;
    gui::SubscriptionId wifi_refresh_animation_id = 0;
    std::string current_page;
    std::string current_locale;
    std::string current_theme_id;
    bool language_switch_in_progress = false;
    bool theme_switch_in_progress = false;
    bool language_loading_visible = false;
    bool theme_loading_visible = false;
};

SettingsApp::SettingsApp()
    : impl_(std::make_unique<Impl>())
{
}

SettingsApp::~SettingsApp() = default;

#define context_ impl_->context
#define dynamic_wifi_paths_ impl_->dynamic_wifi_paths
#define dynamic_hardware_group_paths_ impl_->dynamic_hardware_group_paths
#define saved_wifi_slot_count_ impl_->saved_wifi_slot_count
#define available_wifi_slot_count_ impl_->available_wifi_slot_count
#define saved_wifi_page_ impl_->saved_wifi_page
#define available_wifi_page_ impl_->available_wifi_page
#define dynamic_language_paths_ impl_->dynamic_language_paths
#define wifi_instance_to_network_ impl_->wifi_instance_to_network
#define language_instance_to_locale_ impl_->language_instance_to_locale
#define selected_wifi_network_ impl_->selected_wifi_network
#define wifi_forget_click_suppression_id_ impl_->wifi_forget_click_suppression_id
#define wifi_forget_click_suppression_ssid_ impl_->wifi_forget_click_suppression_ssid
#define wifi_connect_password_ impl_->wifi_connect_password
#define connected_wifi_ssid_ impl_->connected_wifi_ssid
#define language_action_connection_ impl_->language_action_connection
#define wifi_select_action_connection_ impl_->wifi_select_action_connection
#define wifi_forget_action_connection_ impl_->wifi_forget_action_connection
#define brightness_action_connection_ impl_->brightness_action_connection
#define volume_action_connection_ impl_->volume_action_connection
#define mute_action_connection_ impl_->mute_action_connection
#define storage_service_binding_ impl_->storage_service_binding
#define device_service_binding_ impl_->device_service_binding
#define display_service_binding_ impl_->display_service_binding
#define audio_playback_service_binding_ impl_->audio_playback_service_binding
#define wifi_service_binding_ impl_->wifi_service_binding
#define sntp_service_binding_ impl_->sntp_service_binding
#define device_event_connections_ impl_->device_event_connections
#define wifi_event_connections_ impl_->wifi_event_connections
#define sntp_event_connections_ impl_->sntp_event_connections
#define wifi_operation_generation_ impl_->wifi_operation_generation
#define device_ui_state_ impl_->device_ui_state
#define wifi_ui_state_ impl_->wifi_ui_state
#define time_zone_ui_state_ impl_->time_zone_ui_state
#define saved_wifi_networks_ impl_->saved_wifi_networks
#define available_wifi_networks_ impl_->available_wifi_networks
#define saved_wifi_networks_loaded_ impl_->saved_wifi_networks_loaded
#define saved_wifi_networks_refreshing_ impl_->saved_wifi_networks_refreshing
#define available_wifi_rows_hidden_for_scan_ impl_->available_wifi_rows_hidden_for_scan
#define available_wifi_scan_results_ready_ impl_->available_wifi_scan_results_ready
#define wifi_scan_stop_after_first_result_ impl_->wifi_scan_stop_after_first_result
#define wifi_scan_retry_remaining_ impl_->wifi_scan_retry_remaining
#define pending_language_locale_ impl_->pending_language_locale
#define pending_theme_id_ impl_->pending_theme_id
#define pending_language_timer_id_ impl_->pending_language_timer_id
#define pending_theme_timer_id_ impl_->pending_theme_timer_id
#define pending_wifi_scan_retry_timer_id_ impl_->pending_wifi_scan_retry_timer_id
#define pending_wifi_scan_retry_generation_ impl_->pending_wifi_scan_retry_generation
#define pending_wifi_connected_hide_timer_id_ impl_->pending_wifi_connected_hide_timer_id
#define pending_wifi_connected_scroll_timer_id_ impl_->pending_wifi_connected_scroll_timer_id
#define message_dialog_request_id_ impl_->message_dialog_request_id
#define wifi_keyboard_request_id_ impl_->wifi_keyboard_request_id
#define wifi_refresh_animation_id_ impl_->wifi_refresh_animation_id
#define current_page_ impl_->current_page
#define current_locale_ impl_->current_locale
#define current_theme_id_ impl_->current_theme_id
#define language_switch_in_progress_ impl_->language_switch_in_progress
#define theme_switch_in_progress_ impl_->theme_switch_in_progress
#define language_loading_visible_ impl_->language_loading_visible
#define theme_loading_visible_ impl_->theme_loading_visible

#include "app/lifecycle.ipp"
#include "i18n/locale.ipp"
#include "screen/common.ipp"
#include "service/device.ipp"
#include "service/services.ipp"
#include "i18n/language_theme.ipp"
#include "dialog/message.ipp"
#include "service/wifi.ipp"

#undef context_
#undef dynamic_wifi_paths_
#undef dynamic_hardware_group_paths_
#undef saved_wifi_slot_count_
#undef available_wifi_slot_count_
#undef saved_wifi_page_
#undef available_wifi_page_
#undef dynamic_language_paths_
#undef wifi_instance_to_network_
#undef language_instance_to_locale_
#undef selected_wifi_network_
#undef wifi_forget_click_suppression_id_
#undef wifi_forget_click_suppression_ssid_
#undef wifi_connect_password_
#undef connected_wifi_ssid_
#undef language_action_connection_
#undef wifi_select_action_connection_
#undef wifi_forget_action_connection_
#undef brightness_action_connection_
#undef volume_action_connection_
#undef mute_action_connection_
#undef storage_service_binding_
#undef device_service_binding_
#undef display_service_binding_
#undef audio_playback_service_binding_
#undef wifi_service_binding_
#undef sntp_service_binding_
#undef device_event_connections_
#undef wifi_event_connections_
#undef sntp_event_connections_
#undef wifi_operation_generation_
#undef device_ui_state_
#undef wifi_ui_state_
#undef time_zone_ui_state_
#undef saved_wifi_networks_
#undef available_wifi_networks_
#undef saved_wifi_networks_loaded_
#undef saved_wifi_networks_refreshing_
#undef available_wifi_rows_hidden_for_scan_
#undef available_wifi_scan_results_ready_
#undef wifi_scan_stop_after_first_result_
#undef wifi_scan_retry_remaining_
#undef pending_language_locale_
#undef pending_theme_id_
#undef pending_language_timer_id_
#undef pending_theme_timer_id_
#undef pending_wifi_scan_retry_timer_id_
#undef pending_wifi_scan_retry_generation_
#undef pending_wifi_connected_hide_timer_id_
#undef pending_wifi_connected_scroll_timer_id_
#undef message_dialog_request_id_
#undef wifi_keyboard_request_id_
#undef wifi_refresh_animation_id_
#undef current_page_
#undef current_locale_
#undef current_theme_id_
#undef language_switch_in_progress_
#undef theme_switch_in_progress_
#undef language_loading_visible_
#undef theme_loading_visible_

} // namespace esp_brookesia::app::settings
