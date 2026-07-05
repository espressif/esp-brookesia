/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/app_store.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <utility>

#include "boost/json.hpp"

#include "brookesia/lib_utils.hpp"
#include "brookesia/service_helper/system/device.hpp"
#include "brookesia/service_helper/network/http.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::app::app_store {
namespace {

using HttpHelper = service::helper::Http;
using DeviceHelper = service::helper::Device;
using StorageHelper = service::helper::Storage;

static constexpr const char *APP_ID = "brookesia.general.app_store";
static constexpr const char *APP_NAME = "App Store";
static constexpr const char *APP_NAME_ZH_CN = "应用商店";
static constexpr const char *APP_NAME_I18N_KEY = "app_name";
static constexpr const char *APP_ICON_ID = "launcher_icon";
static constexpr const char *APP_ICON_PATH = "res/images/index.json";
static constexpr const char *LOCALE_EN = "en";
static constexpr const char *LOCALE_ZH_CN = "zh_CN";
static constexpr const char *GUI_ROOT = "res/root.json";
static constexpr const char *FLOW_ID = "app_store";
static constexpr const char *ACTION_REFRESH = "app_store.refresh";
static constexpr const char *ACTION_TAB_STORE = "app_store.tab.store";
static constexpr const char *ACTION_TAB_LOCAL = "app_store.tab.local";
static constexpr const char *ACTION_TAB_INSTALLED = "app_store.tab.installed";
static constexpr const char *ACTION_PRIMARY = "app_store.item.primary";
static constexpr const char *ACTION_PAGE_PREV = "app_store.page.prev";
static constexpr const char *ACTION_PAGE_NEXT = "app_store.page.next";
static constexpr const char *ITEM_TEMPLATE_ID = "app_store_item";
static constexpr const char *LOCAL_BPK_ICON_ID = "app_store.icon.bpk";
static constexpr const char *LIST_PATH = "/app_store/page/list/items";
static constexpr const char *LIST_TOP_ANCHOR_PATH = "/app_store/page/list/top_anchor";
static constexpr const char *PAGER_PATH = "/app_store/page/list/pager";
static constexpr const char *PAGER_PREV_PATH = "/app_store/page/list/pager/prev";
static constexpr const char *PAGER_LABEL_PATH = "/app_store/page/list/pager/page/label";
static constexpr const char *PAGER_NEXT_PATH = "/app_store/page/list/pager/next";
static constexpr const char *TITLE_PATH = "/app_store/page/header/summary/text/title";
static constexpr const char *STATUS_PATH = "/app_store/page/header/summary/text/status";
static constexpr const char *STORAGE_PATH = "/app_store/page/header/summary/text/storage";
static constexpr const char *REFRESH_BUTTON_PATH = "/app_store/page/header/summary/refresh";
static constexpr const char *REFRESH_LABEL_PATH = "/app_store/page/header/summary/refresh/label";
static constexpr const char *TAB_STORE_PATH = "/app_store/page/header/tabs/store";
static constexpr const char *TAB_STORE_LABEL_PATH = "/app_store/page/header/tabs/store/label";
static constexpr const char *TAB_LOCAL_PATH = "/app_store/page/header/tabs/local";
static constexpr const char *TAB_LOCAL_LABEL_PATH = "/app_store/page/header/tabs/local/label";
static constexpr const char *TAB_INSTALLED_PATH = "/app_store/page/header/tabs/installed";
static constexpr const char *TAB_INSTALLED_LABEL_PATH = "/app_store/page/header/tabs/installed/label";
static constexpr const char *REFRESH_ICON_TIMER_NAME = "app_store.refresh.icon_step";
static constexpr const char *SIZE_METADATA_TIMER_NAME = "app_store.size_metadata.step";
static constexpr const char *REFRESH_REQUEST_TIMEOUT_TIMER_NAME = "app_store.refresh.request_timeout";
static constexpr const char *REFRESH_RESULT_TIMER_NAME = "app_store.refresh.result";
static constexpr const char *VIEW_MODE_LOAD_TIMER_NAME = "app_store.view_mode.load";
static constexpr const char *TIME_SYNC_TIMEOUT_TIMER_NAME = "app_store.time_sync.timeout";
static constexpr const char *TIME_SYNC_SUCCESS_CLOSE_TIMER_NAME = "app_store.time_sync.success_close";
static constexpr const char *INDEX_CACHE_FILE = "index.json";
static constexpr const char *APP_CACHE_DIR = "apps";
static constexpr const char *DOWNLOAD_DIR = "downloads";
static constexpr const char *ICON_DIR = "icons";
static constexpr const char *METADATA_CACHE_FILE = "metadata.json";
static constexpr const char *ICON_CACHE_FILE = "icon.png";
static constexpr const char *BPK_PART_EXTENSION = ".part";
static constexpr uint32_t INDEX_MAX_RESPONSE_SIZE = 512 * 1024;
static constexpr uint32_t METADATA_MAX_RESPONSE_SIZE = 64 * 1024;
static constexpr uint32_t BPK_MAX_FILE_SIZE = 64 * 1024 * 1024;
static constexpr uint32_t ICON_MAX_FILE_SIZE = 1024 * 1024;
static constexpr int HTTP_TIMEOUT_MS = 15000;
static constexpr int ENABLED_OPACITY = 255;
static constexpr int DISABLED_OPACITY = 110;
static constexpr int INACTIVE_OPACITY = 150;
static constexpr int NETWORK_UNAVAILABLE_DIALOG_AUTO_CLOSE_MS = 3000;
static constexpr int REFRESH_SUCCESS_DIALOG_AUTO_CLOSE_MS = 1500;
static constexpr int REFRESH_FAILED_DIALOG_AUTO_CLOSE_MS = 4000;
static constexpr int REFRESH_ICON_STEP_DELAY_MS = 1;
static constexpr int SIZE_METADATA_STEP_DELAY_MS = 1;
static constexpr int SIZE_METADATA_RETRY_DELAY_MS = 250;
static constexpr int REFRESH_REQUEST_TIMEOUT_EXTRA_MS = 1000;
static constexpr int VIEW_MODE_LOAD_DELAY_MS = 1;
static constexpr int VIEW_MODE_LOAD_COMPLETE_DIALOG_AUTO_CLOSE_MS = 700;
static constexpr int INSTALL_SUCCESS_DIALOG_AUTO_CLOSE_MS = 1500;
static constexpr int UNINSTALL_SUCCESS_DIALOG_AUTO_CLOSE_MS = 1500;
static constexpr int TIME_SYNC_WAIT_TIMEOUT_MS = 10000;
static constexpr int TIME_SYNC_SUCCESS_CLOSE_MS = 1000;

std::string make_app_version()
{
    return std::to_string(BROOKESIA_APP_STORE_VER_MAJOR) + "." +
           std::to_string(BROOKESIA_APP_STORE_VER_MINOR) + "." +
           std::to_string(BROOKESIA_APP_STORE_VER_PATCH);
}

void add_binding(
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

std::string bool_value(bool value)
{
    return value ? "true" : "false";
}

size_t get_list_page_size()
{
    return static_cast<size_t>(std::clamp(BROOKESIA_APP_STORE_LIST_PAGE_SIZE, 1, 64));
}

size_t get_page_count(size_t total_count)
{
    const auto page_size = get_list_page_size();
    if (total_count == 0) {
        return 1;
    }
    return (total_count + page_size - 1) / page_size;
}

size_t get_page_start(size_t page)
{
    return page * get_list_page_size();
}

size_t get_page_visible_count(size_t total_count, size_t page)
{
    const auto start = get_page_start(page);
    if (start >= total_count) {
        return 0;
    }
    return std::min(get_list_page_size(), total_count - start);
}

std::string make_page_text(size_t page, size_t page_count)
{
    const auto safe_page_count = std::max<size_t>(page_count, 1);
    const auto display_page = std::min(page + 1, safe_page_count);
    return std::to_string(display_page) + " / " + std::to_string(safe_page_count);
}

std::string join_path(std::string_view parent, std::string_view child)
{
    std::string path(parent);
    if (!path.empty() && path.back() != '/') {
        path += '/';
    }
    path += child;
    return path;
}

std::string safe_name(std::string_view value)
{
    std::string result;
    for (const auto ch : value) {
        const auto uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch) || ch == '_' || ch == '-' || ch == '.') {
            result.push_back(static_cast<char>(ch));
        } else {
            result.push_back('_');
        }
    }
    return result.empty() ? "app" : result;
}

std::string read_text_file(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool write_text_file(const std::filesystem::path &path, std::string_view text)
{
    std::error_code error_code;
    std::filesystem::create_directories(path.parent_path(), error_code);
    if (error_code) {
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(output);
}

void append_unique_path(std::vector<std::filesystem::path> &paths, std::filesystem::path path)
{
    path = path.lexically_normal();
    const auto path_text = path.generic_string();
    const auto exists = std::any_of(
                            paths.begin(),
                            paths.end(),
    [&path_text](const std::filesystem::path & item) {
        return item.generic_string() == path_text;
    }
                        );
    if (!exists) {
        paths.push_back(std::move(path));
    }
}

bool directory_is_writable(const std::filesystem::path &path)
{
    std::error_code error_code;
    std::filesystem::create_directories(path, error_code);
    if (error_code) {
        return false;
    }

    const auto probe_path = path / ".app_store_write_probe";
    {
        std::ofstream output(probe_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            return false;
        }
        output << "ok";
        if (!output) {
            return false;
        }
    }
    std::filesystem::remove(probe_path, error_code);
    return true;
}

bool is_http_time_syncing_error(std::string_view error)
{
    return error.find("state(TimeSyncing)") != std::string_view::npos;
}

std::string string_array_join(const std::vector<std::string> &items)
{
    std::string result;
    for (const auto &item : items) {
        if (!result.empty()) {
            result += ", ";
        }
        result += item;
    }
    return result;
}

void append_detail(std::string &detail, std::string item)
{
    if (item.empty()) {
        return;
    }
    if (!detail.empty()) {
        detail += " | ";
    }
    detail += std::move(item);
}

std::optional<uint64_t> display_size_from_file_size(uintmax_t bytes)
{
    if (bytes == 0) {
        return std::nullopt;
    }
    if (bytes > static_cast<uintmax_t>(std::numeric_limits<uint64_t>::max())) {
        return std::numeric_limits<uint64_t>::max();
    }
    return static_cast<uint64_t>(bytes);
}

bool is_decimal_segment(std::string_view value)
{
    return !value.empty() && std::all_of(value.begin(), value.end(), [](char ch) {
        return std::isdigit(static_cast<unsigned char>(ch));
    });
}

uint64_t parse_decimal_segment(std::string_view value)
{
    uint64_t result = 0;
    for (const auto ch : value) {
        const auto digit = static_cast<uint64_t>(ch - '0');
        if (result > (std::numeric_limits<uint64_t>::max() - digit) / 10) {
            return std::numeric_limits<uint64_t>::max();
        }
        result = result * 10 + digit;
    }
    return result;
}

std::vector<std::string> split_version(std::string_view version)
{
    std::vector<std::string> segments;
    size_t start = 0;
    while (start <= version.size()) {
        const auto end = version.find('.', start);
        if (end == std::string_view::npos) {
            segments.emplace_back(version.substr(start));
            break;
        }
        segments.emplace_back(version.substr(start, end - start));
        start = end + 1;
    }
    return segments;
}

int compare_versions(std::string_view lhs, std::string_view rhs)
{
    if (lhs == rhs) {
        return 0;
    }
    if (lhs.empty() || rhs.empty()) {
        return 0;
    }

    const auto lhs_segments = split_version(lhs);
    const auto rhs_segments = split_version(rhs);
    const auto count = std::max(lhs_segments.size(), rhs_segments.size());
    for (size_t i = 0; i < count; ++i) {
        const std::string lhs_segment = i < lhs_segments.size() ? lhs_segments[i] : "0";
        const std::string rhs_segment = i < rhs_segments.size() ? rhs_segments[i] : "0";
        if (is_decimal_segment(lhs_segment) && is_decimal_segment(rhs_segment)) {
            const auto lhs_value = parse_decimal_segment(lhs_segment);
            const auto rhs_value = parse_decimal_segment(rhs_segment);
            if (lhs_value != rhs_value) {
                return lhs_value < rhs_value ? -1 : 1;
            }
            continue;
        }
        if (lhs_segment != rhs_segment) {
            return lhs_segment < rhs_segment ? -1 : 1;
        }
    }
    return 0;
}

std::string format_bytes(uint64_t bytes)
{
    static constexpr std::array<std::string_view, 4> UNITS = {"B", "KiB", "MiB", "GiB"};
    double value = static_cast<double>(bytes);
    size_t unit_index = 0;
    while (value >= 1024.0 && unit_index + 1 < UNITS.size()) {
        value /= 1024.0;
        ++unit_index;
    }

    std::ostringstream oss;
    if (unit_index == 0 || value >= 100.0) {
        oss << static_cast<uint64_t>(value);
    } else {
        oss << std::fixed << std::setprecision(value >= 10.0 ? 1 : 2) << value;
    }
    oss << ' ' << UNITS[unit_index];
    return oss.str();
}

std::string normalize_path_for_prefix(std::filesystem::path path)
{
    auto text = path.lexically_normal().generic_string();
    while (text.size() > 1 && text.back() == '/') {
        text.pop_back();
    }
    return text;
}

bool path_has_prefix(std::string_view path, std::string_view prefix)
{
    if (prefix.empty() || path.size() < prefix.size() || path.substr(0, prefix.size()) != prefix) {
        return false;
    }
    return path.size() == prefix.size() || path[prefix.size()] == '/';
}

std::string first_utf8_character(std::string_view text)
{
    if (text.empty()) {
        return "A";
    }
    const auto first = static_cast<unsigned char>(text.front());
    size_t length = 1;
    if ((first & 0x80U) == 0) {
        length = 1;
    } else if ((first & 0xE0U) == 0xC0U) {
        length = 2;
    } else if ((first & 0xF0U) == 0xE0U) {
        length = 3;
    } else if ((first & 0xF8U) == 0xF0U) {
        length = 4;
    }
    length = std::min(length, text.size());
    return std::string(text.substr(0, length));
}

std::map<std::string, std::string> parse_localized_map(const boost::json::value &value)
{
    std::map<std::string, std::string> result;
    if (value.is_object()) {
        for (const auto &[key, item] : value.as_object()) {
            if (item.is_string()) {
                auto locale = std::string(key);
                if (locale == "zh-CN") {
                    locale = "zh_CN";
                }
                result[locale] = item.as_string().c_str();
            }
        }
        return result;
    }
    if (value.is_array()) {
        for (const auto &item : value.as_array()) {
            if (!item.is_array() || item.as_array().size() < 2) {
                continue;
            }
            const auto &locale_value = item.as_array()[0];
            const auto &text_value = item.as_array()[1];
            if (!locale_value.is_string() || !text_value.is_string()) {
                continue;
            }
            auto locale = std::string(locale_value.as_string().c_str());
            if (locale == "zh-CN") {
                locale = "zh_CN";
            }
            result[locale] = text_value.as_string().c_str();
        }
    }
    return result;
}

std::string get_string_field(const boost::json::object &object, std::string_view key)
{
    auto it = object.find(key);
    if (it == object.end() || !it->value().is_string()) {
        return {};
    }
    return it->value().as_string().c_str();
}

std::optional<uint64_t> get_size_field(const boost::json::object &object, std::string_view key)
{
    auto it = object.find(key);
    if (it == object.end()) {
        return std::nullopt;
    }

    const auto &value = it->value();
    if (value.is_uint64()) {
        const auto size = value.as_uint64();
        return size > 0 ? std::optional<uint64_t>(size) : std::nullopt;
    }
    if (value.is_int64()) {
        const auto size = value.as_int64();
        return size > 0 ? std::optional<uint64_t>(static_cast<uint64_t>(size)) : std::nullopt;
    }
    return std::nullopt;
}

std::expected<std::unordered_map<std::string, std::string>, std::string> load_i18n_strings(
    const std::filesystem::path &resource_dir,
    std::string_view locale
)
{
    auto path = resource_dir / "res" / "i18n" / (std::string(locale) + ".json");
    auto text = read_text_file(path);
    if (text.empty() && locale != LOCALE_EN) {
        path = resource_dir / "res" / "i18n" / (std::string(LOCALE_EN) + ".json");
        text = read_text_file(path);
    }
    if (text.empty()) {
        return std::unexpected("Failed to read App Store i18n file: " + path.generic_string());
    }

    boost::system::error_code error_code;
    auto parsed = boost::json::parse(text, error_code);
    if (error_code || !parsed.is_object()) {
        return std::unexpected("Failed to parse App Store i18n file: " + path.generic_string());
    }

    const auto &root = parsed.as_object();
    const auto *data_value = root.if_contains("data");
    if (data_value == nullptr || !data_value->is_object()) {
        return std::unexpected("App Store i18n file must contain object data: " + path.generic_string());
    }
    const auto &data = data_value->as_object();
    const auto *app_store_value = data.if_contains("appStore");
    if (app_store_value == nullptr || !app_store_value->is_object()) {
        return std::unexpected("App Store i18n file must contain object data.appStore: " + path.generic_string());
    }
    const auto &app_store = app_store_value->as_object();
    const auto *i18n_value = app_store.if_contains("i18n");
    if (i18n_value == nullptr || !i18n_value->is_object()) {
        return std::unexpected("App Store i18n file must contain object data.appStore.i18n: " + path.generic_string());
    }

    std::unordered_map<std::string, std::string> strings;
    for (const auto &[key, value] : i18n_value->as_object()) {
        if (value.is_string()) {
            strings.emplace(std::string(key), value.as_string().c_str());
        }
    }
    return strings;
}

std::string localized_app_name(std::string_view locale)
{
    const std::array resource_roots = {
        std::filesystem::path(BROOKESIA_APP_STORE_PACKAGE_DIR),
        std::filesystem::path(BROOKESIA_APP_STORE_RESOURCE_DIR),
    };
    for (const auto &resource_root : resource_roots) {
        if (resource_root.empty()) {
            continue;
        }

        auto strings = load_i18n_strings(resource_root, locale);
        if (strings) {
            auto it = strings->find(APP_NAME_I18N_KEY);
            if (it != strings->end()) {
                return it->second;
            }
        }
    }
    return (locale == LOCALE_ZH_CN) ? APP_NAME_ZH_CN : APP_NAME;
}

std::vector<std::string> get_string_array_field(const boost::json::object &object, std::string_view key)
{
    std::vector<std::string> result;
    auto it = object.find(key);
    if (it == object.end() || !it->value().is_array()) {
        return result;
    }
    for (const auto &item : it->value().as_array()) {
        if (item.is_string()) {
            result.push_back(item.as_string().c_str());
        }
    }
    return result;
}

std::optional<std::pair<int32_t, int32_t>> read_png_size(const std::filesystem::path &path)
{
    std::error_code error_code;
    if (!std::filesystem::is_regular_file(path, error_code) || error_code) {
        return std::nullopt;
    }
    const auto file_size = std::filesystem::file_size(path, error_code);
    if (error_code || file_size < 24) {
        return std::nullopt;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    unsigned char header[24] = {};
    input.read(reinterpret_cast<char *>(header), sizeof(header));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(header))) {
        return std::nullopt;
    }
    static constexpr unsigned char PNG_MAGIC[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (!std::equal(std::begin(PNG_MAGIC), std::end(PNG_MAGIC), header)) {
        return std::nullopt;
    }
    const auto width = static_cast<int32_t>(
                           (static_cast<uint32_t>(header[16]) << 24) |
                           (static_cast<uint32_t>(header[17]) << 16) |
                           (static_cast<uint32_t>(header[18]) << 8) |
                           static_cast<uint32_t>(header[19])
                       );
    const auto height = static_cast<int32_t>(
                            (static_cast<uint32_t>(header[20]) << 24) |
                            (static_cast<uint32_t>(header[21]) << 16) |
                            (static_cast<uint32_t>(header[22]) << 8) |
                            static_cast<uint32_t>(header[23])
                        );
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }
    return std::make_pair(width, height);
}

void remove_invalid_icon_file(const std::filesystem::path &path, std::string_view reason)
{
    std::error_code error_code;
    if (std::filesystem::remove(path, error_code)) {
        BROOKESIA_LOGW(
            "Removed invalid App Store icon cache file: path(%1%), reason(%2%)",
            path.generic_string(), reason
        );
    } else if (error_code) {
        BROOKESIA_LOGW(
            "Failed to remove invalid App Store icon cache file: path(%1%), reason(%2%), error(%3%)",
            path.generic_string(), reason, error_code.message()
        );
    }
}

void remove_cache_file_if_exists(const std::filesystem::path &path, std::string_view reason)
{
    if (path.empty()) {
        return;
    }

    std::error_code error_code;
    if (std::filesystem::remove(path, error_code)) {
        BROOKESIA_LOGI(
            "Removed App Store cache file: path(%1%), reason(%2%)",
            path.generic_string(), reason
        );
    } else if (error_code) {
        BROOKESIA_LOGW(
            "Failed to remove App Store cache file: path(%1%), reason(%2%), error(%3%)",
            path.generic_string(), reason, error_code.message()
        );
    }
}

HttpHelper::HttpRequest make_get_request(std::string url)
{
    HttpHelper::HttpRequest request;
    request.url = std::move(url);
    request.method = HttpHelper::HttpMethod::Get;
    request.timeout_ms = HTTP_TIMEOUT_MS;
    request.retry_count = 1;
    request.retry_on_status_codes = {408, 429, 500, 502, 503, 504};
    return request;
}

bool is_http_url(std::string_view url)
{
    return url.starts_with("http://") || url.starts_with("https://");
}

std::string strip_query_and_fragment(std::string_view url)
{
    auto end = url.find_first_of("?#");
    if (end == std::string_view::npos) {
        return std::string(url);
    }
    return std::string(url.substr(0, end));
}

std::string trim_trailing_slash(std::string value)
{
    while (value.size() > std::string("https://").size() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

std::string url_directory(std::string_view url)
{
    const auto clean_url = strip_query_and_fragment(url);
    const auto scheme_end = clean_url.find("://");
    if (scheme_end == std::string::npos) {
        return {};
    }
    const auto authority_start = scheme_end + 3;
    const auto path_start = clean_url.find('/', authority_start);
    if (path_start == std::string::npos) {
        return clean_url;
    }
    const auto last_slash = clean_url.rfind('/');
    if (last_slash == std::string::npos || last_slash <= scheme_end + 2) {
        return clean_url.substr(0, path_start);
    }
    return clean_url.substr(0, last_slash);
}

std::string append_url_path(std::string_view base, std::string_view path)
{
    if (base.empty()) {
        return {};
    }
    auto result = trim_trailing_slash(std::string(base));
    if (!path.empty() && path.front() != '/') {
        result += '/';
    }
    result += path;
    return result;
}

std::string configured_index_url()
{
    const std::string legacy_index_url = BROOKESIA_APP_STORE_INDEX_URL;
    if (!legacy_index_url.empty()) {
        return legacy_index_url;
    }
    return append_url_path(BROOKESIA_APP_STORE_SERVER_ROOT_URL, "index.json");
}

std::string server_root_for_index(std::string_view index_url)
{
    const std::string legacy_index_url = BROOKESIA_APP_STORE_INDEX_URL;
    if (!legacy_index_url.empty()) {
        return url_directory(index_url);
    }
    return trim_trailing_slash(BROOKESIA_APP_STORE_SERVER_ROOT_URL);
}

std::string resolve_relative_url(std::string_view base_url, std::string_view value)
{
    if (value.empty() || is_http_url(value)) {
        return std::string(value);
    }
    if (!is_http_url(base_url)) {
        return {};
    }

    const auto clean_base_url = strip_query_and_fragment(base_url);
    const auto scheme_end = clean_base_url.find("://");
    if (scheme_end == std::string::npos) {
        return {};
    }
    const auto authority_start = scheme_end + 3;
    const auto path_start = clean_base_url.find('/', authority_start);
    const auto origin = path_start == std::string::npos ?
                        clean_base_url :
                        clean_base_url.substr(0, path_start);
    if (value.front() == '/') {
        return origin + std::string(value);
    }

    std::string directory = origin + "/";
    if (path_start != std::string::npos) {
        const auto last_slash = clean_base_url.rfind('/');
        if (last_slash != std::string::npos && last_slash >= path_start) {
            directory = clean_base_url.substr(0, last_slash + 1);
        }
    }
    return directory + std::string(value);
}

std::string make_metadata_url(std::string_view index_url, std::string_view package_name, std::string_view version)
{
    const auto root = server_root_for_index(index_url);
    return append_url_path(
               root,
               "apps/" + std::string(package_name) + "/versions/" + std::string(version) + "/metadata.json"
           );
}

} // namespace

struct AppStoreApp::Impl {
    system::core::AppContext *context = nullptr;
    service::ServiceBinding http_binding;
    service::ServiceBinding device_binding;
    service::ServiceBinding storage_binding;
    std::vector<esp_brookesia::lib_utils::scoped_connection> http_event_connections;
    gui::ScopedConnection primary_action_connection;
    std::vector<StoreEntry> entries;
    std::vector<LocalPackageEntry> local_packages;
    std::vector<InstalledAppEntry> installed_runtime_apps;
    std::vector<std::string> entry_paths;
    std::vector<VisibleItemRef> visible_items;
    std::vector<size_t> refresh_icon_indices;
    std::unordered_map<std::string, VisibleItemRef> instance_to_entry;
    std::unordered_map<std::string, size_t> local_package_by_manifest_id;
    std::unordered_set<std::string> installed_manifest_ids;
    std::unordered_map<std::string, std::string> installed_version_by_manifest_id;
    std::unordered_set<std::string> registered_icon_resource_ids;
    std::string applied_i18n_locale;
    std::unordered_map<std::string, std::string> i18n_strings;
    std::string status_text;
    std::string storage_text;
    std::string refresh_dialog_message;
    ViewMode view_mode = ViewMode::Store;
    size_t list_slot_count = 0;
    size_t list_page = 0;
    system::core::MessageDialogRequestId message_dialog_request_id =
        system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
    system::core::TimerId refresh_icon_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId size_metadata_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId view_mode_load_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId refresh_request_timeout_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId refresh_result_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId time_sync_timeout_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId time_sync_success_close_timer_id = system::core::INVALID_TIMER_ID;
    std::optional<service::helper::Http::GeneralState> http_general_state;
    std::optional<ViewMode> pending_view_mode;
    MessageDialogPurpose message_dialog_purpose = MessageDialogPurpose::None;
    PendingRefreshResultType pending_refresh_result_type = PendingRefreshResultType::None;
    IconUpdatePurpose refresh_icon_purpose = IconUpdatePurpose::None;
    std::optional<boost::json::object> pending_refresh_result_response;
    uint64_t async_generation = 0;
    uint64_t refresh_request_id = 0;
    uint64_t refresh_icon_request_id = 0;
    uint64_t download_dialog_request_id = 0;
    size_t refresh_icon_cursor = 0;
    bool refresh_in_progress = false;
    bool http_available = false;
    bool device_available = false;
    bool storage_available = false;
    bool network_ready = true;
    bool storage_match_warning_logged = false;
    bool time_sync_waiting = false;
    bool processing_pending_refresh_result = false;
};

AppStoreApp::AppStoreApp()
    : impl_(std::make_unique<Impl>())
{
}

AppStoreApp::~AppStoreApp() = default;

#define context_ impl_->context
#define http_binding_ impl_->http_binding
#define device_binding_ impl_->device_binding
#define storage_binding_ impl_->storage_binding
#define http_event_connections_ impl_->http_event_connections
#define primary_action_connection_ impl_->primary_action_connection
#define entries_ impl_->entries
#define local_packages_ impl_->local_packages
#define installed_runtime_apps_ impl_->installed_runtime_apps
#define entry_paths_ impl_->entry_paths
#define visible_items_ impl_->visible_items
#define refresh_icon_indices_ impl_->refresh_icon_indices
#define instance_to_entry_ impl_->instance_to_entry
#define local_package_by_manifest_id_ impl_->local_package_by_manifest_id
#define installed_manifest_ids_ impl_->installed_manifest_ids
#define installed_version_by_manifest_id_ impl_->installed_version_by_manifest_id
#define registered_icon_resource_ids_ impl_->registered_icon_resource_ids
#define applied_i18n_locale_ impl_->applied_i18n_locale
#define i18n_strings_ impl_->i18n_strings
#define status_text_ impl_->status_text
#define storage_text_ impl_->storage_text
#define refresh_dialog_message_ impl_->refresh_dialog_message
#define view_mode_ impl_->view_mode
#define list_slot_count_ impl_->list_slot_count
#define list_page_ impl_->list_page
#define message_dialog_request_id_ impl_->message_dialog_request_id
#define refresh_icon_timer_id_ impl_->refresh_icon_timer_id
#define size_metadata_timer_id_ impl_->size_metadata_timer_id
#define view_mode_load_timer_id_ impl_->view_mode_load_timer_id
#define refresh_request_timeout_timer_id_ impl_->refresh_request_timeout_timer_id
#define refresh_result_timer_id_ impl_->refresh_result_timer_id
#define time_sync_timeout_timer_id_ impl_->time_sync_timeout_timer_id
#define time_sync_success_close_timer_id_ impl_->time_sync_success_close_timer_id
#define http_general_state_ impl_->http_general_state
#define pending_view_mode_ impl_->pending_view_mode
#define message_dialog_purpose_ impl_->message_dialog_purpose
#define pending_refresh_result_type_ impl_->pending_refresh_result_type
#define refresh_icon_purpose_ impl_->refresh_icon_purpose
#define pending_refresh_result_response_ impl_->pending_refresh_result_response
#define async_generation_ impl_->async_generation
#define refresh_request_id_ impl_->refresh_request_id
#define refresh_icon_request_id_ impl_->refresh_icon_request_id
#define download_dialog_request_id_ impl_->download_dialog_request_id
#define refresh_icon_cursor_ impl_->refresh_icon_cursor
#define refresh_in_progress_ impl_->refresh_in_progress
#define http_available_ impl_->http_available
#define device_available_ impl_->device_available
#define storage_available_ impl_->storage_available
#define network_ready_ impl_->network_ready
#define storage_match_warning_logged_ impl_->storage_match_warning_logged
#define time_sync_waiting_ impl_->time_sync_waiting
#define processing_pending_refresh_result_ impl_->processing_pending_refresh_result

#include "app/lifecycle.ipp"
#include "app/view_mode.ipp"
#include "storage/storage.ipp"
#include "storage/catalog_model.ipp"
#include "network/remote_index.ipp"
#include "i18n/locale.ipp"
#include "i18n/text.ipp"
#include "screen/catalog.ipp"
#include "install/download.ipp"
#include "network/http.ipp"
#include "storage/icons.ipp"
#include "app/provider.ipp"

#undef context_
#undef http_binding_
#undef device_binding_
#undef storage_binding_
#undef http_event_connections_
#undef primary_action_connection_
#undef entries_
#undef local_packages_
#undef installed_runtime_apps_
#undef entry_paths_
#undef visible_items_
#undef refresh_icon_indices_
#undef instance_to_entry_
#undef local_package_by_manifest_id_
#undef installed_manifest_ids_
#undef installed_version_by_manifest_id_
#undef registered_icon_resource_ids_
#undef applied_i18n_locale_
#undef i18n_strings_
#undef status_text_
#undef storage_text_
#undef refresh_dialog_message_
#undef view_mode_
#undef list_slot_count_
#undef list_page_
#undef message_dialog_request_id_
#undef refresh_icon_timer_id_
#undef size_metadata_timer_id_
#undef view_mode_load_timer_id_
#undef refresh_request_timeout_timer_id_
#undef refresh_result_timer_id_
#undef time_sync_timeout_timer_id_
#undef time_sync_success_close_timer_id_
#undef http_general_state_
#undef pending_view_mode_
#undef message_dialog_purpose_
#undef pending_refresh_result_type_
#undef refresh_icon_purpose_
#undef pending_refresh_result_response_
#undef async_generation_
#undef refresh_request_id_
#undef refresh_icon_request_id_
#undef download_dialog_request_id_
#undef refresh_icon_cursor_
#undef refresh_in_progress_
#undef http_available_
#undef device_available_
#undef storage_available_
#undef network_ready_
#undef storage_match_warning_logged_
#undef time_sync_waiting_
#undef processing_pending_refresh_result_

} // namespace esp_brookesia::app::app_store
