/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "boost/json/fwd.hpp"

#include "brookesia/gui_interface/runtime.hpp"
#include "brookesia/service_helper/network/http.hpp"
#include "brookesia/service_helper/system/device.hpp"
#include "brookesia/service_helper/system/storage.hpp"

namespace esp_brookesia::app::app_store::detail {

using HttpHelper = service::helper::Http;
using DeviceHelper = service::helper::Device;
using StorageHelper = service::helper::Storage;

inline constexpr const char *APP_ID = "brookesia.general.app_store";
inline constexpr const char *APP_NAME = "App Store";
inline constexpr const char *APP_NAME_ZH_CN = "应用商店";
inline constexpr const char *APP_NAME_I18N_KEY = "app_name";
inline constexpr const char *APP_ICON_ID = "launcher_icon";
inline constexpr const char *APP_ICON_PATH = "res/images/index.json";
inline constexpr const char *LOCALE_EN = "en";
inline constexpr const char *LOCALE_ZH_CN = "zh_CN";
inline constexpr const char *GUI_ROOT = "res/root.json";
inline constexpr const char *FLOW_ID = "app_store";
inline constexpr const char *ACTION_REFRESH = "app_store.refresh";
inline constexpr const char *ACTION_TAB_STORE = "app_store.tab.store";
inline constexpr const char *ACTION_TAB_LOCAL = "app_store.tab.local";
inline constexpr const char *ACTION_TAB_INSTALLED = "app_store.tab.installed";
inline constexpr const char *ACTION_PRIMARY = "app_store.item.primary";
inline constexpr const char *ACTION_LOCAL_DELETE = "app_store.item.local.delete";
inline constexpr const char *ACTION_PAGE_PREV = "app_store.page.prev";
inline constexpr const char *ACTION_PAGE_NEXT = "app_store.page.next";
inline constexpr const char *ITEM_TEMPLATE_ID = "app_store_item";
inline constexpr const char *LOCAL_BPK_ICON_ID = "app_store.icon.bpk";
inline constexpr const char *LIST_PATH = "/app_store/page/list/items";
inline constexpr const char *LIST_TOP_ANCHOR_PATH = "/app_store/page/list/top_anchor";
inline constexpr const char *PAGER_PATH = "/app_store/page/list/pager";
inline constexpr const char *PAGER_PREV_PATH = "/app_store/page/list/pager/prev";
inline constexpr const char *PAGER_LABEL_PATH = "/app_store/page/list/pager/page/label";
inline constexpr const char *PAGER_NEXT_PATH = "/app_store/page/list/pager/next";
inline constexpr const char *TITLE_PATH = "/app_store/page/header/summary/text/title";
inline constexpr const char *STATUS_PATH = "/app_store/page/header/summary/text/status";
inline constexpr const char *STORAGE_PATH = "/app_store/page/header/summary/text/storage";
inline constexpr const char *REFRESH_BUTTON_PATH = "/app_store/page/header/summary/refresh";
inline constexpr const char *REFRESH_LABEL_PATH = "/app_store/page/header/summary/refresh/label";
inline constexpr const char *TAB_STORE_PATH = "/app_store/page/header/tabs/store";
inline constexpr const char *TAB_STORE_LABEL_PATH = "/app_store/page/header/tabs/store/label";
inline constexpr const char *TAB_LOCAL_PATH = "/app_store/page/header/tabs/local";
inline constexpr const char *TAB_LOCAL_LABEL_PATH = "/app_store/page/header/tabs/local/label";
inline constexpr const char *TAB_INSTALLED_PATH = "/app_store/page/header/tabs/installed";
inline constexpr const char *TAB_INSTALLED_LABEL_PATH = "/app_store/page/header/tabs/installed/label";
inline constexpr const char *REFRESH_ICON_TIMER_NAME = "app_store.refresh.icon_step";
inline constexpr const char *SIZE_METADATA_TIMER_NAME = "app_store.size_metadata.step";
inline constexpr const char *REFRESH_REQUEST_TIMEOUT_TIMER_NAME = "app_store.refresh.request_timeout";
inline constexpr const char *METADATA_WATCHDOG_TIMER_NAME = "app_store.metadata.timeout";
inline constexpr const char *DOWNLOAD_WATCHDOG_TIMER_NAME = "app_store.download.stall_timeout";
inline constexpr const char *REFRESH_RESULT_TIMER_NAME = "app_store.refresh.result";
inline constexpr const char *HTTP_EVENT_TIMER_NAME = "app_store.http.event";
inline constexpr const char *STARTUP_LOAD_TIMER_NAME = "app_store.startup.load";
inline constexpr const char *VIEW_MODE_LOAD_TIMER_NAME = "app_store.view_mode.load";
inline constexpr const char *DEFERRED_OPERATION_TIMER_NAME = "app_store.operation.defer";
inline constexpr const char *LOCAL_PACKAGE_SCAN_TIMER_NAME = "app_store.local_package.scan_step";
inline constexpr const char *TIME_SYNC_TIMEOUT_TIMER_NAME = "app_store.time_sync.timeout";
inline constexpr const char *TIME_SYNC_SUCCESS_CLOSE_TIMER_NAME = "app_store.time_sync.success_close";
inline constexpr const char *INDEX_CACHE_FILE = "index.json";
inline constexpr const char *APP_CACHE_DIR = "apps";
inline constexpr const char *DOWNLOAD_DIR = "downloads";
inline constexpr const char *ICON_DIR = "icons";
inline constexpr const char *METADATA_CACHE_FILE = "metadata.json";
inline constexpr const char *ICON_CACHE_FILE = "icon.png";
inline constexpr const char *BPK_PART_EXTENSION = ".part";
inline constexpr uint32_t INDEX_MAX_RESPONSE_SIZE = 512 * 1024;
inline constexpr uint32_t METADATA_MAX_RESPONSE_SIZE = 64 * 1024;
inline constexpr uint32_t BPK_MAX_FILE_SIZE = 64 * 1024 * 1024;
inline constexpr uint64_t DOWNLOAD_FREE_SPACE_MARGIN_BYTES = 256 * 1024;
inline constexpr uint32_t ICON_MAX_FILE_SIZE = 1024 * 1024;
inline constexpr int HTTP_TIMEOUT_MS = 15000;
inline constexpr int ENABLED_OPACITY = 255;
inline constexpr int DISABLED_OPACITY = 110;
inline constexpr int INACTIVE_OPACITY = 150;
inline constexpr int NETWORK_UNAVAILABLE_DIALOG_AUTO_CLOSE_MS = 3000;
inline constexpr int REFRESH_SUCCESS_DIALOG_AUTO_CLOSE_MS = 1500;
inline constexpr int REFRESH_FAILED_DIALOG_AUTO_CLOSE_MS = 4000;
inline constexpr int REFRESH_ICON_STEP_DELAY_MS = 1;
inline constexpr int SIZE_METADATA_STEP_DELAY_MS = 1;
inline constexpr int SIZE_METADATA_RETRY_DELAY_MS = 250;
inline constexpr int REFRESH_REQUEST_TIMEOUT_EXTRA_MS = 1000;
inline constexpr int METADATA_REQUEST_TIMEOUT_EXTRA_MS = 1000;
inline constexpr int DOWNLOAD_STALL_TIMEOUT_MS = 30000;
inline constexpr int HTTP_EVENT_POLL_INTERVAL_MS = 100;
inline constexpr int STARTUP_LOAD_DELAY_MS = 1;
inline constexpr int VIEW_MODE_LOAD_DELAY_MS = 1;
inline constexpr int DEFERRED_OPERATION_DELAY_MS = 1;
inline constexpr int LOCAL_PACKAGE_SCAN_STEP_DELAY_MS = 1;
inline constexpr int VIEW_MODE_LOAD_COMPLETE_DIALOG_AUTO_CLOSE_MS = 700;
inline constexpr int INSTALL_SUCCESS_DIALOG_AUTO_CLOSE_MS = 1500;
inline constexpr int UNINSTALL_SUCCESS_DIALOG_AUTO_CLOSE_MS = 1500;
inline constexpr int DELETE_SUCCESS_DIALOG_AUTO_CLOSE_MS = 1500;
inline constexpr int TIME_SYNC_WAIT_TIMEOUT_MS = 10000;
inline constexpr int TIME_SYNC_SUCCESS_CLOSE_MS = 1000;

std::string make_app_version();
void add_binding(
    std::vector<gui::BindingValueUpdate> &updates,
    std::string absolute_path,
    std::string key,
    std::string value
);
std::string bool_value(bool value);
size_t get_list_page_size();
size_t get_page_count(size_t total_count);
size_t get_page_start(size_t page);
size_t get_page_visible_count(size_t total_count, size_t page);
std::string make_page_text(size_t page, size_t page_count);
std::string join_path(std::string_view parent, std::string_view child);
std::string safe_name(std::string_view value);
std::string read_text_file(const std::filesystem::path &path);
bool write_text_file(const std::filesystem::path &path, std::string_view text);
void append_unique_path(std::vector<std::filesystem::path> &paths, std::filesystem::path path);
bool directory_is_writable(const std::filesystem::path &path);
bool is_http_time_syncing_error(std::string_view error);
std::string string_array_join(const std::vector<std::string> &items);
void append_detail(std::string &detail, std::string item);
std::optional<uint64_t> display_size_from_file_size(uintmax_t bytes);
int compare_versions(std::string_view lhs, std::string_view rhs);
std::string format_bytes(uint64_t bytes);
std::string normalize_path_for_prefix(std::filesystem::path path);
bool path_has_prefix(std::string_view path, std::string_view prefix);
std::string first_utf8_character(std::string_view text);
std::map<std::string, std::string> parse_localized_map(const boost::json::value &value);
std::string get_string_field(const boost::json::object &object, std::string_view key);
std::optional<uint64_t> get_size_field(const boost::json::object &object, std::string_view key);
std::expected<std::unordered_map<std::string, std::string>, std::string> load_i18n_strings(
    const std::filesystem::path &resource_dir,
    std::string_view locale
);
std::string localized_app_name(std::string_view locale);
std::vector<std::string> get_string_array_field(const boost::json::object &object, std::string_view key);
std::optional<std::pair<int32_t, int32_t>> read_png_size(const std::filesystem::path &path);
void remove_invalid_icon_file(const std::filesystem::path &path, std::string_view reason);
void remove_cache_file_if_exists(const std::filesystem::path &path, std::string_view reason);
HttpHelper::HttpRequest make_get_request(std::string url);
bool is_http_url(std::string_view url);
std::string configured_index_url();
std::string resolve_relative_url(std::string_view base_url, std::string_view value);
std::string make_metadata_url(std::string_view index_url, std::string_view package_name, std::string_view version);

} // namespace esp_brookesia::app::app_store::detail
