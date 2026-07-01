/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/app_files.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <system_error>
#include <unordered_map>

#include "boost/json.hpp"

#include "brookesia/service_helper/system/storage.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::app::files {
namespace {

static constexpr const char *APP_ID = "brookesia.general.files";
static constexpr const char *APP_NAME = "Files";
static constexpr const char *APP_NAME_ZH_CN = "文件";
static constexpr const char *APP_NAME_I18N_KEY = "app_name";
static constexpr const char *APP_ICON_ID = "launcher_icon";
static constexpr const char *LOCALE_EN = "en";
static constexpr const char *LOCALE_ZH_CN = "zh_CN";
static constexpr const char *GUI_ROOT = "res/root.json";
static constexpr const char *FLOW_ID = "files";
static constexpr const char *HEADER_FLOW_ID = "files_header";
static constexpr const char *ACTION_OPEN_BROWSER = "files.open.browser";
static constexpr const char *ACTION_OPEN_OPERATION = "files.open.operation";
static constexpr const char *ACTION_BACK = "files.back";
static constexpr const char *ACTION_ENTRY_CLICK = "files.entry.click";
static constexpr const char *ACTION_ENTRY_LONG = "files.entry.long";
static constexpr const char *ACTION_RENAME = "files.rename";
static constexpr const char *ACTION_DELETE = "files.delete";
static constexpr const char *ENTRY_TEMPLATE_ID = "file_entry";
static constexpr const char *LIST_PATH = "/browser/page/list/items";
static constexpr const char *HEADER_TITLE_PATH = "/files_header/bar/text/title";
static constexpr const char *HEADER_SUBTITLE_PATH = "/files_header/bar/text/subtitle";
static constexpr const char *HEADER_COUNT_PATH = "/files_header/bar/count/label";
static constexpr const char *OP_INFO_TITLE_PATH = "/operations/page/selection_card/selected/content/title";
static constexpr const char *OP_INFO_DETAIL_PATH = "/operations/page/selection_card/selected/content/detail";
static constexpr const char *OP_INFO_ICON_PATH = "/operations/page/selection_card/selected/icon";
static constexpr const char *OP_INFO_CHEVRON_PATH = "/operations/page/selection_card/selected/chevron";
static constexpr const char *OP_RENAME_PATH = "/operations/page/actions_card/rename";
static constexpr const char *OP_DELETE_PATH = "/operations/page/actions_card/delete";
static constexpr const char *INITIAL_REFRESH_TIMER_NAME = "files.initial.refresh";
static constexpr int INITIAL_REFRESH_DELAY_MS = 50;
static constexpr int MESSAGE_AUTO_CLOSE_MS = 3000;
static constexpr size_t ENTRY_POOL_TRIM_MIN_CAPACITY = 128;
static constexpr size_t ENTRY_POOL_TRIM_SPARE = 32;
static constexpr uint8_t ENABLED_OPACITY = 255;
static constexpr uint8_t DISABLED_OPACITY = 110;
static constexpr const char *ICON_DRIVER = "files.icon.driver";
static constexpr const char *ICON_DIRECTORY = "files.icon.directory";
static constexpr const char *ICON_FILE = "files.icon.file";
static constexpr const char *ICON_IMAGE = "files.icon.image";
static constexpr const char *ICON_DOC = "files.icon.doc";
static constexpr const char *ICON_BPK = "files.icon.bpk";
static constexpr const char *ICON_AUDIO = "files.icon.audio";
static constexpr const char *ICON_VIDEO = "files.icon.video";

using StorageHelper = service::helper::Storage;

std::string make_app_version()
{
    return std::to_string(BROOKESIA_APP_FILES_VER_MAJOR) + "." +
           std::to_string(BROOKESIA_APP_FILES_VER_MINOR) + "." +
           std::to_string(BROOKESIA_APP_FILES_VER_PATCH);
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

std::string make_entry_instance_id(size_t index)
{
    return "entry_" + std::to_string(index);
}

std::string make_entry_path(size_t index)
{
    return join_path(LIST_PATH, make_entry_instance_id(index));
}

std::string bool_text(bool value)
{
    return value ? "true" : "false";
}

std::string lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string extension_of(const std::filesystem::path &path)
{
    return lower_copy(path.extension().generic_string());
}

std::string file_icon_for(const std::filesystem::path &path, FileManagerApp::EntryKind kind)
{
    if (kind == FileManagerApp::EntryKind::Volume) {
        return ICON_DRIVER;
    }
    if (kind == FileManagerApp::EntryKind::Directory) {
        return ICON_DIRECTORY;
    }
    const auto ext = extension_of(path);
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
        return ICON_IMAGE;
    }
    if (ext == ".txt" || ext == ".md" || ext == ".doc" || ext == ".docs" || ext == ".json") {
        return ICON_DOC;
    }
    if (ext == ".bpk" || ext == ".rpk") {
        return ICON_BPK;
    }
    if (ext == ".mp3" || ext == ".wav" || ext == ".flac") {
        return ICON_AUDIO;
    }
    if (ext == ".mp4" || ext == ".avi" || ext == ".mov") {
        return ICON_VIDEO;
    }
    return ICON_FILE;
}

std::string format_size(std::uintmax_t size)
{
    static constexpr double KB = 1024.0;
    static constexpr double MB = 1024.0 * KB;
    static constexpr double GB = 1024.0 * MB;
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(1);
    if (size >= static_cast<std::uintmax_t>(GB)) {
        stream << static_cast<double>(size) / GB << " GB";
    } else if (size >= static_cast<std::uintmax_t>(MB)) {
        stream << static_cast<double>(size) / MB << " MB";
    } else if (size >= static_cast<std::uintmax_t>(KB)) {
        stream << static_cast<double>(size) / KB << " KB";
    } else {
        stream.unsetf(std::ios::fixed);
        stream << size << " B";
    }
    return stream.str();
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

    std::ostringstream stream;
    if (unit_index == 0 || value >= 100.0) {
        stream << static_cast<uint64_t>(value);
    } else {
        stream << std::fixed << std::setprecision(value >= 10.0 ? 1 : 2) << value;
    }
    stream << ' ' << UNITS[unit_index];
    return stream.str();
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

std::string describe_volume(const system::core::StorageVolume &volume, std::string_view capacity_text)
{
    auto text = volume.id + " - " + volume.root_path;
    if (!capacity_text.empty()) {
        text += " | ";
        text += capacity_text;
    }
    return text;
}

std::string item_count_text(size_t count, std::string_view empty_text, std::string_view items_suffix)
{
    return count == 0 ? std::string(empty_text) : std::to_string(count) + std::string(items_suffix);
}

bool path_is_same_or_child(const std::filesystem::path &path, const std::filesystem::path &parent)
{
    const auto normalized_path = path.lexically_normal().generic_string();
    const auto normalized_parent = parent.lexically_normal().generic_string();
    return normalized_path == normalized_parent || normalized_path.starts_with(normalized_parent + "/");
}

void add_binding(
    std::vector<gui::BindingValueUpdate> &updates,
    std::string path,
    std::string key,
    std::string value
)
{
    updates.push_back(gui::BindingValueUpdate{
        .absolute_path = std::move(path),
        .key = std::move(key),
        .value = std::move(value),
    });
}

std::string read_text_file(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::filesystem::path resolve_resource_dir(
    system::core::AppContext &context,
    const std::filesystem::path &resource_dir)
{
    if (resource_dir.is_absolute()) {
        return resource_dir.lexically_normal();
    }
    const auto layout = context.system_service().get_storage_layout();
    if (!layout.internal.available || layout.internal.root_path.empty()) {
        return resource_dir.lexically_normal();
    }
    return (std::filesystem::path(layout.internal.root_path) / "apps" / resource_dir).lexically_normal();
}

std::filesystem::path resolve_app_resource_dir(system::core::AppContext &context)
{
    std::filesystem::path resource_dir(BROOKESIA_APP_FILES_RESOURCE_DIR);
    for (const auto &app : context.system_service().list_apps()) {
        if (app.app_id == context.app_id() && !app.manifest.resource_dir.empty()) {
            resource_dir = app.manifest.resource_dir;
            break;
        }
    }
    return resolve_resource_dir(context, resource_dir);
}

std::expected<std::unordered_map<std::string, std::string>, std::string> load_i18n_strings(
    const std::filesystem::path &resource_dir,
    std::string_view locale)
{
    auto path = resource_dir / "res" / "i18n" /
                (std::string(locale) + ".json");
    auto text = read_text_file(path);
    if (text.empty() && locale != "en") {
        path = resource_dir / "res" / "i18n" / "en.json";
        text = read_text_file(path);
    }
    if (text.empty()) {
        return std::unexpected("Failed to read Files i18n file: " + path.generic_string());
    }

    boost::system::error_code error_code;
    auto parsed = boost::json::parse(text, error_code);
    if (error_code || !parsed.is_object()) {
        return std::unexpected("Failed to parse Files i18n file: " + path.generic_string());
    }

    const auto &root = parsed.as_object();
    const auto *data_value = root.if_contains("data");
    if (data_value == nullptr || !data_value->is_object()) {
        return std::unexpected("Files i18n file must contain object data: " + path.generic_string());
    }
    const auto *files_value = data_value->as_object().if_contains("files");
    if (files_value == nullptr || !files_value->is_object()) {
        return std::unexpected("Files i18n file must contain object data.files: " + path.generic_string());
    }
    const auto *i18n_value = files_value->as_object().if_contains("i18n");
    if (i18n_value == nullptr || !i18n_value->is_object()) {
        return std::unexpected("Files i18n file must contain object data.files.i18n: " + path.generic_string());
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
        std::filesystem::path(BROOKESIA_APP_FILES_PACKAGE_DIR),
        std::filesystem::path(BROOKESIA_APP_FILES_RESOURCE_DIR),
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

} // namespace

struct FileManagerApp::Impl {
    std::optional<system::core::StorageLayout> storage_layout;
    std::vector<VolumeEntry> volumes;
    std::vector<Entry> entries;
    size_t entry_view_count = 0;
    std::optional<Entry> selected_entry;
    std::optional<std::string> suppressed_entry_click_path;
    std::optional<size_t> current_volume_index;
    std::filesystem::path current_directory;
    Page current_page = Page::Browser;
    system::core::AppContext *context = nullptr;
    service::ServiceBinding storage_service_binding;
    system::core::TimerId initial_refresh_timer_id = system::core::INVALID_TIMER_ID;
    system::core::MessageDialogRequestId message_dialog_request_id =
        system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
    std::filesystem::path resource_dir;
    std::string applied_i18n_locale;
    std::unordered_map<std::string, std::string> i18n_strings;
    gui::ScopedConnection entry_click_connection;
    gui::ScopedConnection entry_long_press_connection;
    uint64_t storage_capacity_generation = 0;
};

FileManagerApp::FileManagerApp()
    : impl_(std::make_unique<Impl>())
{
}

FileManagerApp::~FileManagerApp() = default;

#define storage_layout_ impl_->storage_layout
#define volumes_ impl_->volumes
#define entries_ impl_->entries
#define entry_view_count_ impl_->entry_view_count
#define selected_entry_ impl_->selected_entry
#define suppressed_entry_click_path_ impl_->suppressed_entry_click_path
#define current_volume_index_ impl_->current_volume_index
#define current_directory_ impl_->current_directory
#define current_page_ impl_->current_page
#define context_ impl_->context
#define storage_service_binding_ impl_->storage_service_binding
#define initial_refresh_timer_id_ impl_->initial_refresh_timer_id
#define message_dialog_request_id_ impl_->message_dialog_request_id
#define resource_dir_ impl_->resource_dir
#define applied_i18n_locale_ impl_->applied_i18n_locale
#define i18n_strings_ impl_->i18n_strings
#define entry_click_connection_ impl_->entry_click_connection
#define entry_long_press_connection_ impl_->entry_long_press_connection
#define storage_capacity_generation_ impl_->storage_capacity_generation

#include "app/lifecycle.ipp"
#include "service/storage.ipp"
#include "storage/volumes.ipp"
#include "storage/browser_entries.ipp"
#include "screen/browser.ipp"
#include "screen/operations.ipp"
#include "operations/file.ipp"
#include "dialog/state.ipp"
#include "dialog/message.ipp"
#include "i18n/locale.ipp"
#include "app/provider.ipp"

#undef storage_layout_
#undef volumes_
#undef entries_
#undef entry_view_count_
#undef selected_entry_
#undef suppressed_entry_click_path_
#undef current_volume_index_
#undef current_directory_
#undef current_page_
#undef context_
#undef storage_service_binding_
#undef initial_refresh_timer_id_
#undef message_dialog_request_id_
#undef resource_dir_
#undef applied_i18n_locale_
#undef i18n_strings_
#undef entry_click_connection_
#undef entry_long_press_connection_
#undef storage_capacity_generation_

} // namespace esp_brookesia::app::files
