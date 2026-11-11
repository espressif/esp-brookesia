/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "private/settings_surface.hpp"

#include "boost/thread/mutex.hpp"
#include "boost/thread/lock_guard.hpp"
#include "boost/json/array.hpp"
#include "boost/json/value.hpp"

#include "brookesia/app_settings.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_interface/interfaces/system/general.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/memory_profiler.hpp"
#include "brookesia/service_helper/framework/utils.hpp"
#include "brookesia/service_helper/media/audio.hpp"
#include "brookesia/service_helper/media/display.hpp"
#include "brookesia/service_helper/network/sntp.hpp"
#include "brookesia/service_helper/network/wifi.hpp"
#include "brookesia/service_helper/system/device.hpp"
#include "brookesia/service_helper/system/storage.hpp"

#include "private/utils.hpp"

#if BROOKESIA_APP_SETTINGS_ENABLE_PROFILE_LOG
#   define APP_SETTINGS_PROFILE_LOGI(...) BROOKESIA_LOGI(__VA_ARGS__)
#else
#   define APP_SETTINGS_PROFILE_LOGI(...) do { if (false) { BROOKESIA_LOGI(__VA_ARGS__); } } while (0)
#endif

namespace esp_brookesia::app::settings::detail {

using AudioPlaybackHelper = service::helper::AudioPlayback;
using DeviceHelper = service::helper::Device;
using DisplayHelper = service::helper::Display;
using SNTPHelper = service::helper::SNTP;
using StorageHelper = service::helper::Storage;
using UtilsHelper = service::helper::Utils;
using WifiHelper = service::helper::Wifi;
using SteadyClock = std::chrono::steady_clock;
using SteadyTimePoint = SteadyClock::time_point;

struct SettingsKvName {
    std::string nspace;
    std::string key;
};

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

extern std::unordered_map<std::string, std::unordered_map<std::string, std::string>> i18n_string_tables;

inline constexpr const char *APP_ID = "brookesia.general.settings";
inline constexpr const char *APP_NAME = "Settings";
inline constexpr const char *APP_NAME_ZH_CN = "设置";
inline constexpr const char *APP_ICON_ID = "launcher_icon";
inline constexpr const char *APP_ICON_PATH = "res/images/index.json";
inline constexpr const char *GUI_ROOT = "res/root.json";
inline constexpr const char *CONTENT_FLOW_ID = "settings_content";
inline constexpr const char *HEADER_FLOW_ID = "settings_header";
inline constexpr const char *WIFI_TEMPLATE_ID = "wifi_network_item";
inline constexpr const char *HARDWARE_GROUP_TEMPLATE_ID = "hardware_group_card";
// Each wifi_network_item instance costs ~25KB PSRAM in per-instance allocations that cannot be
// shared across rows (LVGL widget tree, NodeRecord storage, signal binding subscriptions).
// On PSRAM-constrained targets, eagerly creating every scanned AP exhausts the heap and aborts.
// Stop creating new rows once free external memory drops below this reserve so the system keeps
// working headroom (binding flushes, screen transitions, etc.). The guard is target-adaptive:
// memory-rich targets create the full list, tight targets cap automatically.
inline constexpr size_t WIFI_SLOT_MIN_FREE_PSRAM_BYTES = 320 * 1024;
inline constexpr const char *LANGUAGE_TEMPLATE_ID = "menu_item";
inline constexpr const char *HEADER_BACK_PATH = "/settings_header/bar/back";
inline constexpr const char *HEADER_TITLE_PATH = "/settings_header/bar/title";
inline constexpr const char *SAVED_NETWORK_PARENT = "/wifi/page/wifi_content/saved_networks/list/items";
inline constexpr const char *AVAILABLE_NETWORK_PARENT = "/wifi/page/wifi_content/available_networks/list/items";
inline constexpr const char *WIFI_STATUS_CARD_PATH = "/wifi/page/wifi_status_card";
inline constexpr const char *WIFI_SWITCH_PATH = "/wifi/page/wifi_status_card/wifi_switch";
inline constexpr const char *WIFI_CONTENT_PATH = "/wifi/page/wifi_content";
inline constexpr const char *WIFI_SCAN_BUTTON_PATH = "/wifi/page/wifi_content/available_header/available_refresh";
inline constexpr const char *WIFI_SCAN_ICON_PATH = "/wifi/page/wifi_content/available_header/available_refresh/icon";
inline constexpr const char *WIFI_CONNECTED_CARD_PATH = "/wifi/page/wifi_content/connected";
inline constexpr const char *WIFI_CONNECTED_SSID_PATH = "/wifi/page/wifi_content/connected/text/ssid";
inline constexpr const char *WIFI_CONNECTED_DETAIL_PATH = "/wifi/page/wifi_content/connected/text/detail";
inline constexpr const char *SAVED_NETWORK_EMPTY_PATH = "/wifi/page/wifi_content/saved_networks/list/empty";
inline constexpr const char *AVAILABLE_NETWORK_EMPTY_PATH = "/wifi/page/wifi_content/available_networks/list/empty";
inline constexpr const char *SAVED_NETWORK_PAGER_PATH = "/wifi/page/wifi_content/saved_networks/list/pager";
inline constexpr const char *SAVED_NETWORK_PAGER_PREV_PATH = "/wifi/page/wifi_content/saved_networks/list/pager/prev";
inline constexpr const char *SAVED_NETWORK_PAGER_LABEL_PATH = "/wifi/page/wifi_content/saved_networks/list/pager/page";
inline constexpr const char *SAVED_NETWORK_PAGER_NEXT_PATH = "/wifi/page/wifi_content/saved_networks/list/pager/next";
inline constexpr const char *AVAILABLE_NETWORK_PAGER_PATH = "/wifi/page/wifi_content/available_networks/list/pager";
inline constexpr const char *AVAILABLE_NETWORK_PAGER_PREV_PATH =
    "/wifi/page/wifi_content/available_networks/list/pager/prev";
inline constexpr const char *AVAILABLE_NETWORK_PAGER_LABEL_PATH =
    "/wifi/page/wifi_content/available_networks/list/pager/page";
inline constexpr const char *AVAILABLE_NETWORK_PAGER_NEXT_PATH =
    "/wifi/page/wifi_content/available_networks/list/pager/next";
inline constexpr const char *HOME_WIFI_VALUE_PATH = "/settings_home/page/main_list/wifi/value_box/value";
inline constexpr const char *WIFI_CONNECT_SSID_PATH = "/wifi_connect/page/network_card/ssid";
inline constexpr const char *WIFI_CONNECT_DETAIL_PATH = "/wifi_connect/page/network_card/detail";
inline constexpr const char *WIFI_CONNECT_PASSWORD_VALUE_PATH =
    "/wifi_connect/page/password_card/password_row/value";
inline constexpr const char *WIFI_CONNECT_STATUS_PATH = "/wifi_connect/page/status";
inline constexpr const char *WIFI_CONNECT_BUTTON_PATH = "/wifi_connect/page/actions/connect";
inline constexpr const char *LANGUAGE_LIST_PARENT = "/language/page/language_card/list";
inline constexpr const char *LANGUAGE_SELECT_ACTION = "settings.language.select";
inline constexpr const char *WIFI_SELECT_ACTION = "settings.wifi.select";
inline constexpr const char *WIFI_SAVED_FORGET_ACTION = "settings.wifi.saved.forget";
inline constexpr const char *WIFI_TOGGLE_ACTION = "settings.wifi.toggle";
inline constexpr const char *WIFI_SCAN_ACTION = "settings.wifi.scan";
inline constexpr const char *WIFI_DISCONNECT_ACTION = "settings.wifi.disconnect";
inline constexpr const char *WIFI_SAVED_PREV_ACTION = "settings.wifi.saved.prev";
inline constexpr const char *WIFI_SAVED_NEXT_ACTION = "settings.wifi.saved.next";
inline constexpr const char *WIFI_AVAILABLE_PREV_ACTION = "settings.wifi.available.prev";
inline constexpr const char *WIFI_AVAILABLE_NEXT_ACTION = "settings.wifi.available.next";
inline constexpr const char *LOCALE_EN = "en";
inline constexpr const char *LOCALE_ZH_CN = "zh_CN";
inline constexpr const char *APP_NAME_I18N_KEY = "app_name";
inline constexpr const char *THEME_LIGHT = "light";
inline constexpr const char *THEME_DARK = "dark";
inline constexpr const char *PAGE_HOME = "settings_home";
inline constexpr const char *PAGE_DEVICE = "my_device";
inline constexpr const char *PAGE_WIFI = "wifi";
inline constexpr const char *PAGE_WIFI_CONNECT = "wifi_connect";
inline constexpr const char *PAGE_SOUND = "sound";
inline constexpr const char *PAGE_DISPLAY = "display";
inline constexpr const char *PAGE_MORE = "more";
inline constexpr const char *PAGE_LANGUAGE = "language";
inline constexpr const char *PAGE_THEME = "theme";
inline constexpr const char *SOUND_UI_TIMER_NAME = "settings.sound.ui";
inline constexpr uint32_t SOUND_UI_POLL_INTERVAL_MS = 100;
inline constexpr const char *PAGE_TIME_ZONE = "time_zone";
inline constexpr const char *PAGE_DEBUG = "debug";
inline constexpr const char *ACTION_OPEN_HOME = "settings.open.home";
inline constexpr const char *ACTION_HEADER_BACK = "settings.header.back";
inline constexpr const char *ACTION_BACK_LANGUAGE = "settings.back.language";
inline constexpr const char *ACTION_OPEN_DEBUG = "settings.open.debug";
inline constexpr const char *ACTION_BACK_DEBUG = "settings.back.debug";
inline constexpr const char *ACTION_DEBUG_DEVICE_NAME_CLICK = "settings.debug.device_name.click";
inline constexpr const char *ACTION_OPEN_WIFI_CONNECT = "settings.open.wifi_connect";
inline constexpr const char *ACTION_BACK_WIFI_CONNECT = "settings.back.wifi_connect";
inline constexpr const char *ACTION_WIFI_PASSWORD_EDIT = "settings.wifi.password.edit";
inline constexpr const char *ACTION_WIFI_CONNECT_CANCEL = "settings.wifi.connect.cancel";
inline constexpr const char *ACTION_WIFI_CONNECT_SUBMIT = "settings.wifi.connect.submit";
inline constexpr const char *ACTION_DISPLAY_BRIGHTNESS = "settings.display.brightness";
inline constexpr const char *ACTION_SOUND_VOLUME = "settings.sound.volume";
inline constexpr const char *ACTION_SOUND_MUTE = "settings.sound.mute";
inline constexpr const char *ACTION_DEBUG_MEMORY_TOGGLE = "settings.debug.memory.toggle";
inline constexpr const char *ACTION_DEBUG_THREAD_TOGGLE = "settings.debug.thread.toggle";
inline constexpr const char *ACTION_DEBUG_GUI_TOGGLE = "settings.debug.gui.toggle";
inline constexpr const char *ACTION_DEBUG_MEMORY_SAMPLE_INTERVAL = "settings.debug.memory.sample_interval";
inline constexpr const char *ACTION_DEBUG_MEMORY_INTERNAL_FREE_PERCENT =
    "settings.debug.memory.internal_free_percent";
inline constexpr const char *ACTION_DEBUG_MEMORY_INTERNAL_LARGEST =
    "settings.debug.memory.internal_largest";
inline constexpr const char *ACTION_DEBUG_MEMORY_EXTERNAL_FREE_PERCENT =
    "settings.debug.memory.external_free_percent";
inline constexpr const char *ACTION_DEBUG_MEMORY_EXTERNAL_LARGEST =
    "settings.debug.memory.external_largest";
inline constexpr const char *ACTION_DEBUG_THREAD_PROFILING_INTERVAL =
    "settings.debug.thread.profiling_interval";
inline constexpr const char *ACTION_DEBUG_THREAD_SAMPLING_DURATION =
    "settings.debug.thread.sampling_duration";
inline constexpr const char *ACTION_DEBUG_THREAD_IDLE_CPU =
    "settings.debug.thread.idle_cpu";
inline constexpr const char *ACTION_DEBUG_THREAD_STACK_HWM =
    "settings.debug.thread.stack_hwm";
inline constexpr const char *WIFI_SCAN_RETRY_TIMER_NAME = "settings.wifi.scan.retry";
inline constexpr const char *WIFI_UI_TIMER_NAME = "settings.wifi.ui";
inline constexpr int WIFI_UI_POLL_INTERVAL_MS = 100;
inline constexpr const char *BRIGHTNESS_UI_TIMER_NAME = "settings.display.brightness";
inline constexpr int BRIGHTNESS_UI_POLL_INTERVAL_MS = 100;
inline constexpr const char *WIFI_CONNECTED_HIDE_TIMER_NAME = "settings.wifi.connected.hide";
inline constexpr const char *WIFI_CONNECTED_SCROLL_TIMER_NAME = "settings.wifi.connected.scroll";
inline constexpr const char *TIME_ZONE_OPTION_UTC_ACTION = "settings.time_zone.utc";
inline constexpr const char *TIME_ZONE_OPTION_CHINA_ACTION = "settings.time_zone.china";
inline constexpr const char *TIME_ZONE_OPTION_JAPAN_ACTION = "settings.time_zone.japan";
inline constexpr const char *TIME_ZONE_OPTION_EASTERN_ACTION = "settings.time_zone.eastern";
inline constexpr const char *TIME_ZONE_OPTION_PACIFIC_ACTION = "settings.time_zone.pacific";
inline constexpr const char *TIME_ZONE_OPTION_CENTRAL_EUROPE_ACTION = "settings.time_zone.central_europe";
inline constexpr const char *MORE_TIME_ZONE_VALUE_PATH = "/more/page/language_card/time_zone/value_box/value";
inline constexpr const char *TIME_ZONE_CURRENT_VALUE_PATH = "/time_zone/page/status_card/current/value";
inline constexpr const char *TIME_ZONE_STATE_VALUE_PATH = "/time_zone/page/status_card/state/value_box/value";
inline constexpr const char *TIME_ZONE_OPTION_UTC_VALUE_PATH =
    "/time_zone/page/options_card/utc/value_box/value";
inline constexpr const char *TIME_ZONE_OPTION_CHINA_VALUE_PATH =
    "/time_zone/page/options_card/china/value_box/value";
inline constexpr const char *TIME_ZONE_OPTION_JAPAN_VALUE_PATH =
    "/time_zone/page/options_card/japan/value_box/value";
inline constexpr const char *TIME_ZONE_OPTION_EASTERN_VALUE_PATH =
    "/time_zone/page/options_card/eastern/value_box/value";
inline constexpr const char *TIME_ZONE_OPTION_PACIFIC_VALUE_PATH =
    "/time_zone/page/options_card/pacific/value_box/value";
inline constexpr const char *TIME_ZONE_OPTION_CENTRAL_EUROPE_VALUE_PATH =
    "/time_zone/page/options_card/central_europe/value_box/value";
inline constexpr const char *MY_DEVICE_SYSTEM_NAME_PATH = "/my_device/page/hero/hero_content/os";
inline constexpr const char *MY_DEVICE_SYSTEM_VERSION_PATH = "/my_device/page/hero/hero_content/version";
inline constexpr const char *MY_DEVICE_DEVICE_VALUE_PATH = "/my_device/page/summary_card/device_value";
inline constexpr const char *MY_DEVICE_HARDWARE_GROUP_PARENT = "/my_device/page/hardware_groups";
inline constexpr const char *DISPLAY_BRIGHTNESS_ROW_PATH = "/display/page/brightness_card/brightness_row";
inline constexpr const char *DISPLAY_BRIGHTNESS_SLIDER_PATH = "/display/page/brightness_card/brightness_row/slider";
inline constexpr const char *SOUND_VOLUME_ROW_PATH = "/sound/page/volume_card/media_volume_row";
inline constexpr const char *SOUND_VOLUME_SLIDER_PATH = "/sound/page/volume_card/media_volume_row/slider";
inline constexpr const char *SOUND_MUTE_ROW_PATH = "/sound/page/silent_card/silent_mode";
inline constexpr const char *SOUND_MUTE_TOGGLE_PATH = "/sound/page/silent_card/silent_mode/toggle";
inline constexpr const char *HOME_SOUND_VALUE_PATH = "/settings_home/page/main_list/sound/value_box/value";
inline constexpr const char *DEBUG_MEMORY_SWITCH_PATH = "/debug/page/switch_card/memory_debug/toggle";
inline constexpr const char *DEBUG_THREAD_SWITCH_ROW_PATH = "/debug/page/switch_card/thread_debug";
inline constexpr const char *DEBUG_THREAD_SWITCH_PATH = "/debug/page/switch_card/thread_debug/toggle";
inline constexpr const char *DEBUG_GUI_SWITCH_PATH = "/debug/page/switch_card/gui_debug/toggle";
inline constexpr const char *DEBUG_MEMORY_SAMPLE_ROW_PATH =
    "/debug/page/memory_card/memory_sample_interval";
inline constexpr const char *DEBUG_MEMORY_SAMPLE_SLIDER_PATH =
    "/debug/page/memory_card/memory_sample_interval/slider";
inline constexpr const char *DEBUG_MEMORY_INTERNAL_FREE_ROW_PATH =
    "/debug/page/memory_card/memory_internal_free";
inline constexpr const char *DEBUG_MEMORY_INTERNAL_FREE_SLIDER_PATH =
    "/debug/page/memory_card/memory_internal_free/slider";
inline constexpr const char *DEBUG_MEMORY_INTERNAL_LARGEST_ROW_PATH =
    "/debug/page/memory_card/memory_internal_largest";
inline constexpr const char *DEBUG_MEMORY_INTERNAL_LARGEST_SLIDER_PATH =
    "/debug/page/memory_card/memory_internal_largest/slider";
inline constexpr const char *DEBUG_MEMORY_EXTERNAL_FREE_ROW_PATH =
    "/debug/page/memory_card/memory_external_free";
inline constexpr const char *DEBUG_MEMORY_EXTERNAL_FREE_SLIDER_PATH =
    "/debug/page/memory_card/memory_external_free/slider";
inline constexpr const char *DEBUG_MEMORY_EXTERNAL_LARGEST_ROW_PATH =
    "/debug/page/memory_card/memory_external_largest";
inline constexpr const char *DEBUG_MEMORY_EXTERNAL_LARGEST_SLIDER_PATH =
    "/debug/page/memory_card/memory_external_largest/slider";
inline constexpr const char *DEBUG_THREAD_INTERVAL_ROW_PATH =
    "/debug/page/thread_card/thread_interval";
inline constexpr const char *DEBUG_THREAD_INTERVAL_SLIDER_PATH =
    "/debug/page/thread_card/thread_interval/slider";
inline constexpr const char *DEBUG_THREAD_DURATION_ROW_PATH =
    "/debug/page/thread_card/thread_duration";
inline constexpr const char *DEBUG_THREAD_DURATION_SLIDER_PATH =
    "/debug/page/thread_card/thread_duration/slider";
inline constexpr const char *DEBUG_THREAD_IDLE_ROW_PATH =
    "/debug/page/thread_card/thread_idle";
inline constexpr const char *DEBUG_THREAD_IDLE_SLIDER_PATH =
    "/debug/page/thread_card/thread_idle/slider";
inline constexpr const char *DEBUG_THREAD_STACK_ROW_PATH =
    "/debug/page/thread_card/thread_stack";
inline constexpr const char *DEBUG_THREAD_STACK_SLIDER_PATH =
    "/debug/page/thread_card/thread_stack/slider";
inline constexpr int WIFI_CONNECTED_SCROLL_DELAY_MS = 50;
inline constexpr int DEBUG_ENTRY_CLICK_TIMEOUT_MS = 1500;
inline constexpr size_t DEBUG_ENTRY_CLICK_COUNT = 3;
inline constexpr uint32_t DEVICE_SERVICE_TIMEOUT_MS = 500;
inline constexpr uint32_t WIFI_SERVICE_TIMEOUT_MS = 1000;
inline constexpr uint32_t SNTP_SERVICE_TIMEOUT_MS = 500;
inline constexpr const char *SNTP_UI_TIMER_NAME = "settings_sntp_ui";
inline constexpr uint32_t SNTP_UI_POLL_INTERVAL_MS = 2000;
inline constexpr uint32_t UTILS_DEBUG_SERVICE_TIMEOUT_MS = 500;
inline constexpr const char *WIFI_PAGE_TIMER_NAME = "settings.wifi.page";
inline constexpr const char *WIFI_CONNECT_TIMER_NAME = "settings.wifi.connect";
inline constexpr int WIFI_PAGE_POLL_INTERVAL_MS = 100;
inline constexpr int WIFI_CONNECT_POLL_INTERVAL_MS = 500;
inline constexpr int WIFI_CONNECT_TIMEOUT_MS = 65000;
inline constexpr int WIFI_STATE_POLL_INTERVAL_MS = 1000;
inline constexpr int WIFI_TOGGLE_TIMEOUT_MS = 15000;
inline constexpr int WIFI_SCAN_SETTLE_TIMEOUT_MS = 2000;
inline constexpr uint32_t WIFI_SCAN_WINDOW_MS = 10000;
inline constexpr size_t WIFI_SCAN_AP_LIMIT = 64;
inline constexpr int WIFI_SCAN_RETRY_DELAY_MS = 250;
inline constexpr size_t WIFI_SCAN_INTERRUPTED_RETRY_COUNT = 1;
inline constexpr int WIFI_DISCONNECTED_HIDE_DELAY_MS = 3000;
inline constexpr int DISABLED_OPACITY = 96;
inline constexpr int ENABLED_OPACITY = 255;
inline constexpr int MESSAGE_DIALOG_SUCCESS_AUTO_CLOSE_MS = 700;
inline constexpr int MESSAGE_DIALOG_FAILED_AUTO_CLOSE_MS = 3000;
inline constexpr bool SETTINGS_HARDWARE_OPERATIONS_ENABLED = true;
inline constexpr const char *MY_DEVICE_FALLBACK_SYSTEM_NAME = "System";
inline constexpr const char *MY_DEVICE_FALLBACK_SYSTEM_VERSION = "--";
inline constexpr const char *SETTINGS_STORAGE_NAMESPACE = "app.settings";
inline constexpr const char *WIFI_ENABLED_STORAGE_KEY = "WifiEnabled";
inline constexpr const char *DEBUG_KEY_MEMORY_ENABLED = "Debug.MemoryEnabled";
inline constexpr const char *DEBUG_KEY_THREAD_ENABLED = "Debug.ThreadEnabled";
inline constexpr const char *DEBUG_KEY_GUI_VIEW_DEBUG_ENABLED = "Debug.GuiViewDebugEnabled";
inline constexpr const char *DEBUG_KEY_MEMORY_SAMPLE_INTERVAL_MS = "Debug.MemorySampleIntervalMs";
inline constexpr const char *DEBUG_KEY_MEMORY_INTERNAL_FREE_PERCENT_THRESHOLD =
    "Debug.MemoryInternalFreePercentThreshold";
inline constexpr const char *DEBUG_KEY_MEMORY_INTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_KB =
    "Debug.MemoryInternalLargestFreeBlockThresholdKb";
inline constexpr const char *DEBUG_KEY_MEMORY_EXTERNAL_FREE_PERCENT_THRESHOLD =
    "Debug.MemoryExternalFreePercentThreshold";
inline constexpr const char *DEBUG_KEY_MEMORY_EXTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_KB =
    "Debug.MemoryExternalLargestFreeBlockThresholdKb";
inline constexpr const char *DEBUG_KEY_THREAD_PROFILING_INTERVAL_MS = "Debug.ThreadProfilingIntervalMs";
inline constexpr const char *DEBUG_KEY_THREAD_SAMPLING_DURATION_MS = "Debug.ThreadSamplingDurationMs";
inline constexpr const char *DEBUG_KEY_THREAD_IDLE_CPU_PERCENT_THRESHOLD = "Debug.ThreadIdleCpuPercentThreshold";
inline constexpr const char *DEBUG_KEY_THREAD_STACK_HIGH_WATER_MARK_THRESHOLD_BYTES =
    "Debug.ThreadStackHighWaterMarkThresholdBytes";
inline constexpr uint32_t SETTINGS_STORAGE_TIMEOUT_MS = WIFI_SERVICE_TIMEOUT_MS;

inline constexpr std::array<const char *, 18> NAVIGATION_ACTIONS = {
    ACTION_OPEN_HOME,
    "settings.back.device",
    "settings.back.wifi",
    ACTION_BACK_WIFI_CONNECT,
    "settings.back.sound",
    "settings.back.display",
    ACTION_BACK_LANGUAGE,
    "settings.back.time_zone",
    ACTION_BACK_DEBUG,
    "settings.open.device",
    "settings.open.wifi",
    ACTION_OPEN_WIFI_CONNECT,
    "settings.open.sound",
    "settings.open.display",
    "settings.open.more",
    "settings.open.language",
    "settings.open.time_zone",
    ACTION_OPEN_DEBUG,
};

inline constexpr std::array<const char *, 2> THEME_ACTIONS = {
    "settings.display.light",
    "settings.display.dark",
};

inline constexpr std::array<const char *, 6> TIME_ZONE_ACTIONS = {
    TIME_ZONE_OPTION_UTC_ACTION,
    TIME_ZONE_OPTION_CHINA_ACTION,
    TIME_ZONE_OPTION_JAPAN_ACTION,
    TIME_ZONE_OPTION_EASTERN_ACTION,
    TIME_ZONE_OPTION_PACIFIC_ACTION,
    TIME_ZONE_OPTION_CENTRAL_EUROPE_ACTION,
};

inline constexpr std::array<TimeZoneOption, 6> TIME_ZONE_OPTIONS = {
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

int64_t elapsed_ms_since(
    SteadyTimePoint started_at,
    SteadyTimePoint ended_at = SteadyClock::now()
);
void log_start_profile(
    const char *stage,
    SteadyTimePoint stage_started_at,
    SteadyTimePoint total_started_at
);
std::vector<std::string> make_default_action_subscriptions();
std::expected<std::string, std::string> make_settings_kv_namespace(std::string_view raw_namespace);
std::expected<std::string, std::string> make_settings_kv_key(std::string_view raw_key);
std::expected<SettingsKvName, std::string> make_settings_kv_name(
    std::string_view raw_namespace,
    std::string_view raw_key
);
std::string make_app_version();
bool is_navigation_action(std::string_view action);
bool is_theme_action(std::string_view action);
std::optional<std::string_view> get_time_zone_for_action(std::string_view action);
int clamp_percent(double value);
std::string make_percent_text(int value);
std::string make_localized_unavailable_text(std::string_view locale);
std::string make_localized_muted_text(std::string_view locale);
std::string make_localized_unknown_text(std::string_view locale);
std::string make_localized_hardware_title(std::string_view locale, std::string_view key);
std::string classify_hardware_interface(std::string_view type_name, std::string_view instance_name);
HardwareGroup &find_or_create_hardware_group(
    std::vector<HardwareGroup> &groups,
    std::string key,
    std::string_view locale
);
std::string join_hardware_lines(const std::vector<std::string> &lines);
std::optional<std::string_view> get_navigation_page(std::string_view action);
std::string_view get_header_back_action_for_page(std::string_view page);
std::string join_path(std::string_view parent, std::string_view child);
std::string make_language_instance_id(std::string_view locale);
std::string normalize_locale(std::string_view locale);
std::string make_runtime_language(std::string_view locale);
std::string make_wifi_instance_id(std::string_view prefix, std::string_view ssid, size_t index);
std::string get_wifi_band_text(int channel);
std::string get_wifi_signal_text(int signal_level);
size_t get_wifi_list_page_size();
size_t get_page_count(size_t total_count);
size_t get_page_start(size_t page);
size_t get_page_visible_count(size_t total_count, size_t page);
std::string make_page_text(size_t page, size_t page_count);
std::string extract_child_instance_id(std::string_view parent, std::string_view path);
bool is_wifi_connecting_state(std::string_view state);
bool is_wifi_started_state(std::string_view state);
bool is_wifi_started_event(std::string_view event);
std::string make_password_summary(std::string_view password, std::string_view empty_text);
std::string get_language_display_name(std::string_view locale, std::string_view language_locale);
std::string get_language_summary_name(std::string_view locale);
std::string get_time_zone_summary_name(std::string_view locale, std::string_view timezone);
std::vector<std::string> normalize_language_list(std::vector<std::string> languages);
std::string make_resource_path(system::core::AppContext &context, std::string_view relative_path);
std::expected<std::string, std::string> read_text_file(const std::string &path);
std::string json_value_to_string(const boost::json::value &value);
std::optional<std::string> load_manifest_i18n_string(std::string_view locale, std::string_view key);
std::string localized_manifest_text(
    std::string_view locale,
    std::string_view key,
    std::string_view fallback
);
std::expected<SelectableThemeTokens, std::string> load_selectable_theme_tokens(
    system::core::AppContext &context,
    std::string_view theme_id
);
std::expected<I18nBundle, std::string> load_i18n_bundle(
    system::core::AppContext &context,
    std::string_view locale
);
void add_binding_update(
    std::vector<gui::BindingValueUpdate> &updates,
    std::string absolute_path,
    std::string key,
    std::string value
);
void add_theme_mode_updates(
    std::vector<gui::BindingValueUpdate> &updates,
    std::string_view path,
    bool selected,
    const SelectableThemeTokens &tokens,
    std::string_view preview_bg_color,
    std::string label
);
void add_control_state_updates(
    std::vector<gui::BindingValueUpdate> &updates,
    std::string_view row_path,
    std::string_view control_path,
    bool enabled
);
std::optional<std::string> find_localized_text(std::string_view locale, std::string_view key);
std::string localized_text(std::string_view locale, std::string_view key);
std::string localized_text_or(
    std::string_view locale,
    std::string_view key,
    std::string_view fallback
);
const service::EventItem *find_event_item(
    const service::EventItemMap &items,
    std::string_view name
);

} // namespace esp_brookesia::app::settings::detail

namespace esp_brookesia::app::settings {

// Display callbacks only update this mailbox; the app task updates the UI.
struct BrightnessCallbackState {
    std::mutex mutex;
    bool open = false;
    uint32_t output_id = 0;
    bool output_valid = false;
    bool sync_queued = false;
    int pending_brightness = 0;
    system::core::TimerId timer_id = system::core::INVALID_TIMER_ID;
};

// Each page visit owns a separate mailbox. Service callbacks never retain the App or GUI.
struct WifiPageMailbox {
    boost::mutex mutex;
    bool open = true;
    bool state_changed = false;
    bool scan_started = false;
    bool scan_stopped = false;
    std::optional<boost::json::array> scan_results;
};

// Service threads enqueue bounded work; only the System app task touches Settings state or GUI.
struct WifiUiMailbox {
    static constexpr size_t MAX_PENDING = 64;
    boost::mutex mutex;
    bool open = true;
    bool overflowed = false;
    std::deque<std::function<void(SettingsApp &)>> pending;

    void enqueue(std::function<void(SettingsApp &)> work)
    {
        boost::lock_guard lock(mutex);
        if (!open) {
            return;
        }
        if (pending.size() == MAX_PENDING) {
            overflowed = true;
            return;
        }
        pending.push_back(std::move(work));
    }

    void close()
    {
        boost::lock_guard lock(mutex);
        open = false;
        pending.clear();
    }
};

template<typename Handler>
auto make_wifi_ui_result_handler(const std::shared_ptr<WifiUiMailbox> &mailbox, Handler handler)
{
    const std::weak_ptr<WifiUiMailbox> weak_mailbox = mailbox;
    return [weak_mailbox, handler = std::move(handler)](service::FunctionResult && result) mutable {
        if (auto active = weak_mailbox.lock())
        {
            auto payload = std::make_shared<service::FunctionResult>(std::move(result));
            active->enqueue([handler, payload](SettingsApp &) mutable {
                handler(std::move(*payload));
            });
        }
    };
}

struct WifiSelectionOperation {
    boost::mutex mutex;
    bool valid = true;
    bool done = false;
    bool success = false;
    bool saved = false;
    std::string ssid;
    std::string password;
};

struct WifiPageState {
    std::shared_ptr<WifiPageMailbox> mailbox;
    std::shared_ptr<WifiSelectionOperation> selection;
    system::core::TimerId timer_id = system::core::INVALID_TIMER_ID;
    detail::SteadyTimePoint next_state_read{};
    detail::SteadyTimePoint deadline{};
    std::optional<bool> toggle_target;
    bool scan_requested = false;
    bool scan_received = false;
    bool scan_owned = false;
    bool connect_pending = false;
    std::string connect_ssid;
    bool saved_list = false;
    std::string forget_ssid;
    std::string error;
};

// Service callbacks retain this small state, never SettingsApp or GUI objects.
struct WifiConnectOperation {
    enum class Phase { Saving, SettingTarget, SendingConnect, WaitingForConnection, Failed };
    boost::mutex mutex;
    bool valid = true;
    bool connected_event_armed = false;
    uint64_t connected_event_epoch = 0;
    uint64_t connect_start_epoch = 0;
    Phase phase = Phase::Saving;
    std::string ssid;
    std::string password;
    std::string error;
    detail::SteadyTimePoint deadline{};
    esp_brookesia::lib_utils::scoped_connection connected_event_connection;
};

struct SoundUiMailbox {
    boost::mutex mutex;
    bool open = true;
    std::optional<double> volume;
    std::optional<bool> muted;
    std::optional<std::string> error;
    bool playback_started = false;
    bool playback_finished = false;
    uint64_t control_result_generation = 0;
    bool control_result_pending = false;
    bool control_result_success = false;

    void close()
    {
        boost::lock_guard lock(mutex);
        open = false;
        volume.reset();
        muted.reset();
        error.reset();
    }
};

struct SettingsApp::Impl {
    system::core::AppContext *context = nullptr;
    // Owned by the App record; retained until uninstall for compiled bundle selection.
    system::core::AppContext *installed_context = nullptr;
    std::vector<std::string> dynamic_wifi_paths;
    std::vector<std::string> dynamic_hardware_group_paths;
    std::vector<detail::HardwareGroup> cached_hardware_groups;
    std::string cached_device_name;
    size_t my_device_page = 0;
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
    std::vector<gui::ScopedConnection> action_handler_connections;
    service::ServiceBinding storage_service_binding;
    service::ServiceBinding device_service_binding;
    service::ServiceBinding display_service_binding;
    service::ServiceBinding audio_playback_service_binding;
    service::ServiceBinding wifi_service_binding;
    service::ServiceBinding sntp_service_binding;
    service::ServiceBinding utils_debug_service_binding;
    std::vector<esp_brookesia::lib_utils::scoped_connection> device_event_connections;
    std::vector<esp_brookesia::lib_utils::scoped_connection> wifi_event_connections;
    std::vector<esp_brookesia::lib_utils::scoped_connection> sntp_event_connections;
    std::shared_ptr<std::atomic<uint64_t>> wifi_operation_generation =
                                            std::make_shared<std::atomic<uint64_t>>(0);
    DeviceUiState device_ui_state;
    std::shared_ptr<SoundUiMailbox> sound_ui_mailbox;
    system::core::TimerId sound_ui_timer_id = system::core::INVALID_TIMER_ID;
    bool settings_tone_active = false;
    detail::SteadyTimePoint settings_tone_deadline{};
    uint64_t sound_control_generation = 0;
    WifiUiState wifi_ui_state;
    WifiPageState wifi_page;
    std::shared_ptr<WifiUiMailbox> wifi_ui_mailbox;
    system::core::TimerId wifi_ui_timer_id = system::core::INVALID_TIMER_ID;
    std::shared_ptr<WifiConnectOperation> wifi_connect_operation;
    system::core::TimerId wifi_connect_timer_id = system::core::INVALID_TIMER_ID;
    TimeZoneUiState time_zone_ui_state;
    system::core::TimerId sntp_ui_timer_id = system::core::INVALID_TIMER_ID;
    DebugUiState debug_ui_state;
    detail::UtilsHelper::DebugCapabilities utils_debug_capabilities;
    std::vector<WifiNetworkState> saved_wifi_networks;
    std::vector<WifiNetworkState> available_wifi_networks;
    std::vector<detail::WifiHelper::ScanApInfo> last_wifi_scan_ap_infos;
    bool saved_wifi_networks_loaded = false;
    bool saved_wifi_networks_refreshing = false;
    bool available_wifi_rows_hidden_for_scan = false;
    bool available_wifi_scan_results_ready = false;
    bool wifi_scan_stop_after_first_result = false;
    size_t wifi_scan_retry_remaining = 0;
    std::string pending_language_locale;
    std::string pending_theme_id;
    std::string preference_error;
    hal::InterfaceHandle<hal::system::GeneralIface> restart_iface;
    SettingsApp::RestartPromptKind restart_prompt_kind = SettingsApp::RestartPromptKind::None;
    bool restart_in_progress = false;
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
    bool theme_from_display = false;
    size_t debug_entry_click_count = 0;
    detail::SteadyTimePoint debug_entry_last_click_at{};
    bool utils_debug_unavailable_logged = false;
    std::unique_ptr<SettingsSurface> surface;
    std::shared_ptr<BrightnessCallbackState> brightness_callbacks = std::make_shared<BrightnessCallbackState>();
    int logged_brightness = -1;
    bool logged_brightness_enabled = false;
};

} // namespace esp_brookesia::app::settings

#define context_ impl_->context
#define dynamic_wifi_paths_ impl_->dynamic_wifi_paths
#define dynamic_hardware_group_paths_ impl_->dynamic_hardware_group_paths
#define cached_hardware_groups_ impl_->cached_hardware_groups
#define cached_device_name_ impl_->cached_device_name
#define my_device_page_ impl_->my_device_page
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
#define action_handler_connections_ impl_->action_handler_connections
#define storage_service_binding_ impl_->storage_service_binding
#define device_service_binding_ impl_->device_service_binding
#define display_service_binding_ impl_->display_service_binding
#define audio_playback_service_binding_ impl_->audio_playback_service_binding
#define wifi_service_binding_ impl_->wifi_service_binding
#define sntp_service_binding_ impl_->sntp_service_binding
#define utils_debug_service_binding_ impl_->utils_debug_service_binding
#define device_event_connections_ impl_->device_event_connections
#define wifi_event_connections_ impl_->wifi_event_connections
#define sntp_event_connections_ impl_->sntp_event_connections
#define wifi_operation_generation_ (*impl_->wifi_operation_generation)
#define device_ui_state_ impl_->device_ui_state
#define wifi_ui_state_ impl_->wifi_ui_state
#define wifi_page_ impl_->wifi_page
#define wifi_ui_mailbox_ impl_->wifi_ui_mailbox
#define wifi_ui_timer_id_ impl_->wifi_ui_timer_id
#define wifi_connect_operation_ impl_->wifi_connect_operation
#define wifi_connect_timer_id_ impl_->wifi_connect_timer_id
#define time_zone_ui_state_ impl_->time_zone_ui_state
#define sntp_ui_timer_id_ impl_->sntp_ui_timer_id
#define debug_ui_state_ impl_->debug_ui_state
#define utils_debug_capabilities_ impl_->utils_debug_capabilities
#define saved_wifi_networks_ impl_->saved_wifi_networks
#define available_wifi_networks_ impl_->available_wifi_networks
#define last_wifi_scan_ap_infos_ impl_->last_wifi_scan_ap_infos
#define saved_wifi_networks_loaded_ impl_->saved_wifi_networks_loaded
#define saved_wifi_networks_refreshing_ impl_->saved_wifi_networks_refreshing
#define available_wifi_rows_hidden_for_scan_ impl_->available_wifi_rows_hidden_for_scan
#define available_wifi_scan_results_ready_ impl_->available_wifi_scan_results_ready
#define wifi_scan_stop_after_first_result_ impl_->wifi_scan_stop_after_first_result
#define wifi_scan_retry_remaining_ impl_->wifi_scan_retry_remaining
#define pending_language_locale_ impl_->pending_language_locale
#define pending_theme_id_ impl_->pending_theme_id
#define restart_iface_ impl_->restart_iface
#define restart_prompt_kind_ impl_->restart_prompt_kind
#define restart_in_progress_ impl_->restart_in_progress
#define pending_wifi_scan_retry_timer_id_ impl_->pending_wifi_scan_retry_timer_id
#define pending_wifi_scan_retry_generation_ impl_->pending_wifi_scan_retry_generation
#define pending_wifi_connected_hide_timer_id_ impl_->pending_wifi_connected_hide_timer_id
#define pending_wifi_connected_scroll_timer_id_ impl_->pending_wifi_connected_scroll_timer_id
#define message_dialog_request_id_ impl_->message_dialog_request_id
#define wifi_keyboard_request_id_ impl_->wifi_keyboard_request_id
#define wifi_refresh_animation_id_ impl_->wifi_refresh_animation_id
#define current_page_ impl_->current_page
#define current_locale_ impl_->current_locale
#define theme_from_display_ impl_->theme_from_display
#define preference_error_ impl_->preference_error
#define sound_ui_mailbox_ impl_->sound_ui_mailbox
#define sound_ui_timer_id_ impl_->sound_ui_timer_id
#define settings_tone_active_ impl_->settings_tone_active
#define settings_tone_deadline_ impl_->settings_tone_deadline
#define sound_control_generation_ impl_->sound_control_generation
#define current_theme_id_ impl_->current_theme_id
#define debug_entry_click_count_ impl_->debug_entry_click_count
#define debug_entry_last_click_at_ impl_->debug_entry_last_click_at
#define utils_debug_unavailable_logged_ impl_->utils_debug_unavailable_logged
#define surface_ impl_->surface
#define brightness_callbacks_ impl_->brightness_callbacks
#define logged_brightness_ impl_->logged_brightness
#define logged_brightness_enabled_ impl_->logged_brightness_enabled
