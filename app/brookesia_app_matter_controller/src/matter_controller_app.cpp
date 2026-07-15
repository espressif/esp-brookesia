/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/app_matter_controller.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "boost/json.hpp"
#include "boost/thread/lock_guard.hpp"
#include "matter_backend.hpp"
#include "private/utils.hpp"

#if defined(ESP_PLATFORM)
#include <sdkconfig.h>
#endif


namespace esp_brookesia::app::matter_controller {
namespace {

static constexpr const char *APP_ID = "brookesia.app.matter_controller";
static constexpr const char *APP_NAME = "Matter Controller";
static constexpr const char *APP_VERSION = "0.2.0";
static constexpr const char *APP_ICON_ID = "matter_controller";
static constexpr const char *GUI_ROOT = "res/root.json";
static constexpr const char *MAIN_FLOW_ID = "matter_controller_main";
static constexpr const char *LOCALE_EN = "en";
static constexpr const char *LOCALE_ZH_CN = "zh_CN";

static constexpr const char *SCREEN_PATH = "/matter_controller";
static constexpr const char *STAGE_PATH = "/matter_controller/stage";
static constexpr const char *HEADER_TIME_PATH = "/matter_controller/header/header_clock/header_time";
static constexpr const char *HEADER_DATE_PATH = "/matter_controller/header/header_clock/header_date";
static constexpr const char *THREAD_TOPO_PATH = "/matter_controller/stage/page_thread/thread_content/thread_topo";
static constexpr const char *THREAD_HUB_PATH = "/matter_controller/stage/page_thread/thread_content/thread_topo/thread_hub";
static constexpr const char *THREAD_DEVICE_NODE_LIST_PATH = THREAD_TOPO_PATH;
static constexpr const char *PAIRING_STATUS_TITLE_PATH =
    "/matter_controller/stage/page_pairing/pairing_content/pairing_info/pairing_status/pairing_status_title";
static constexpr const char *PAIRING_STATUS_DESC_PATH =
    "/matter_controller/stage/page_pairing/pairing_content/pairing_info/pairing_status/pairing_status_desc";
static constexpr const char *PAIRING_STATUS_PATH =
    "/matter_controller/stage/page_pairing/pairing_content/pairing_info/pairing_status";
static constexpr const char *PAIRING_QR_IMAGE_PATH = "/matter_controller/stage/page_pairing/pairing_content/qr_panel/qr_wrapper/qr_image";
static constexpr const char *PAIRING_QR_PLACEHOLDER_PATH =
    "/matter_controller/stage/page_pairing/pairing_content/qr_panel/qr_wrapper/qr_placeholder";
static constexpr const char *PAIRING_QR_PANEL_PATH =
    "/matter_controller/stage/page_pairing/pairing_content/qr_panel";
static constexpr const char *PAIRING_WAITING_PATH =
    "/matter_controller/stage/page_pairing/pairing_content/pairing_info/pairing_waiting";
static constexpr const char *PAIRING_SUCCESS_PATH =
    "/matter_controller/stage/page_pairing/pairing_content/pairing_info/commission_success";

static constexpr const char *TEMPLATE_BASIC_CARD = "device_card_basic";
// Matter standard light types have separate trees so each endpoint allocates
// only the controls it can expose: On/Off, Dimmable, Color Temperature, or
// Extended Color (Brightness + CCT + Hue + Saturation).
static constexpr const char *TEMPLATE_LIGHT_DIMMABLE_CARD = "device_card_light_dimmable";
static constexpr const char *TEMPLATE_LIGHT_COLOR_TEMP_CARD = "device_card_light_color_temp";
static constexpr const char *TEMPLATE_LIGHT_EXTENDED_CARD = "device_card_light_extended";
static constexpr const char *TEMPLATE_ROOM_CARD = "room_card";
static constexpr const char *TEMPLATE_ROOM_DEVICE_OPTION = "room_device_option";
static constexpr const char *TEMPLATE_THREAD_DEVICE_NODE = "thread_device_node";
static constexpr const char *TEMPLATE_MODAL = "modal_overlay";
static constexpr const char *TEMPLATE_ROOM_MODAL = "room_modal";

static constexpr const char *TEMPLATE_PAGE_DEVICES = "page_devices";
static constexpr const char *TEMPLATE_PAGE_ROOMS = "page_rooms";
static constexpr const char *TEMPLATE_PAGE_THREAD = "page_thread";
static constexpr const char *TEMPLATE_PAGE_PAIRING = "page_pairing";
static constexpr const char *TEMPLATE_PAGE_SETTINGS = "page_settings";
static constexpr const char *TEMPLATE_PAGE_NETWORK = "page_network";

static constexpr const char *PAGE_DEVICES = "devices";
static constexpr const char *PAGE_ROOMS = "rooms";
static constexpr const char *PAGE_THREAD = "thread";
static constexpr const char *PAGE_PAIRING = "pairing";
static constexpr const char *PAGE_SETTINGS = "settings";
static constexpr const char *PAGE_NETWORK = "network";

static const char *page_template_id(std::string_view page)
{
    if (page == PAGE_DEVICES) return TEMPLATE_PAGE_DEVICES;
    if (page == PAGE_ROOMS) return TEMPLATE_PAGE_ROOMS;
    if (page == PAGE_THREAD) return TEMPLATE_PAGE_THREAD;
    if (page == PAGE_PAIRING) return TEMPLATE_PAGE_PAIRING;
    if (page == PAGE_NETWORK) return TEMPLATE_PAGE_NETWORK;
    if (page == PAGE_SETTINGS) return TEMPLATE_PAGE_SETTINGS;
    return nullptr;
}

static constexpr const char *KEY_HIDDEN = "hidden";
static constexpr const char *KEY_TEXT = "text";
static constexpr const char *KEY_VALUE = "value";
static constexpr const char *KEY_COLOR = "color";
static constexpr const char *KEY_CHECKED = "checked";
static constexpr const char *KEY_NAV_BG = "nav_bg";
static constexpr const char *KEY_NAV_COLOR = "nav_color";
static constexpr const char *KEY_STATUS_BG = "status_bg";
static constexpr const char *KEY_DOT_COLOR = "dot_color";
static constexpr const char *KEY_POWER_HIDDEN = "power_hidden";
static constexpr const char *KEY_BTN_BG = "btn_bg";
static constexpr const char *KEY_MSG_HIDDEN = "msg_hidden";
static constexpr const char *KEY_BORDER_COLOR = "border_color";
static constexpr const char *KEY_POWER_BG = "power_bg";
static constexpr const char *KEY_POWER_BORDER = "power_border";
static constexpr const char *KEY_NODE_BG = "node_bg";
static constexpr const char *KEY_NODE_BORDER = "node_border";
static constexpr const char *KEY_NODE_X = "node_x";
static constexpr const char *KEY_NODE_Y = "node_y";
static constexpr const char *KEY_IMAGE_SRC = "imageProps.src";

static constexpr const char *TIMER_CLOCK = "clock";
static constexpr const char *I18N_RESOURCE_DIR = "res/i18n/";
static constexpr const char *JSON_EXTENSION = ".json";

static constexpr int THREAD_TOPO_FALLBACK_WIDTH = 300;
static constexpr int THREAD_TOPO_FALLBACK_HEIGHT = 410;
static constexpr int THREAD_HUB_SIZE = 60;
static constexpr int THREAD_NODE_WIDTH = 76;
static constexpr int THREAD_NODE_HEIGHT = 62;
static constexpr int THREAD_NODE_MARGIN = 12;

static constexpr const char *PAIRING_QR_RUNTIME_IMAGE_ID = "pairing_qr_runtime";

const char *device_card_template_id(const ControllerDevice &device)
{
    // Prefer discovered Hue capability over a stale conservative template.
    // Device-list refreshes used to leave supports_hue=true while card_template
    // stayed on light_color_temp; trusting supports_hue keeps RGB sliders.
    if (device.supports_hue || device.card_template == "light_extended") {
        return TEMPLATE_LIGHT_EXTENDED_CARD;
    }
    if (device.card_template == "light_color_temp") {
        return TEMPLATE_LIGHT_COLOR_TEMP_CARD;
    }
    if (device.card_template == "light_dimmable") {
        return TEMPLATE_LIGHT_DIMMABLE_CARD;
    }
    // On/Off Light and an On/Off Plug-in Unit both use the minimal basic tree.
    return TEMPLATE_BASIC_CARD;
}

// Actions
static constexpr const char *ACT_DEVICES = "nav.devices";
static constexpr const char *ACT_ROOMS = "nav.rooms";
static constexpr const char *ACT_THREAD = "nav.thread";
static constexpr const char *ACT_PAIRING = "nav.pairing";
static constexpr const char *ACT_SETTINGS = "nav.settings";
static constexpr const char *ACT_NETWORK = "nav.network";
static constexpr const char *ACT_TOGGLE_BASIC = "device.toggle.basic";
static constexpr const char *ACT_TOGGLE_LIGHT_DIMMABLE = "device.toggle.light_dimmable";
static constexpr const char *ACT_TOGGLE_LIGHT_COLOR_TEMP = "device.toggle.light_color_temp";
static constexpr const char *ACT_TOGGLE_LIGHT_EXTENDED = "device.toggle.light_extended";
static constexpr const char *ACT_REMOVE_BASIC = "device.remove.basic";
static constexpr const char *ACT_REMOVE_LIGHT_DIMMABLE = "device.remove.light_dimmable";
static constexpr const char *ACT_REMOVE_LIGHT_COLOR_TEMP = "device.remove.light_color_temp";
static constexpr const char *ACT_REMOVE_LIGHT_EXTENDED = "device.remove.light_extended";
// Actions must be unique across all loaded templates.
static constexpr const char *ACT_LIGHT_DIMMABLE_BRIGHTNESS = "device.light_dimmable.brightness";
static constexpr const char *ACT_LIGHT_COLOR_TEMP_BRIGHTNESS = "device.light_color_temp.brightness";
static constexpr const char *ACT_LIGHT_COLOR_TEMP_CCT = "device.light_color_temp.cct";
static constexpr const char *ACT_LIGHT_EXTENDED_BRIGHTNESS = "device.light_extended.brightness";
static constexpr const char *ACT_LIGHT_EXTENDED_CCT = "device.light_extended.cct";
static constexpr const char *ACT_LIGHT_EXTENDED_HUE = "device.light_extended.hue";
static constexpr const char *ACT_LIGHT_EXTENDED_SATURATION = "device.light_extended.saturation";
static constexpr const char *ACT_ROOM_ADD = "rooms.add";
static constexpr const char *ACT_ROOM_TYPE_LIVING_ROOM = "room_modal.type.living_room";
static constexpr const char *ACT_ROOM_TYPE_KITCHEN = "room_modal.type.kitchen";
static constexpr const char *ACT_ROOM_TYPE_STUDY = "room_modal.type.study";
static constexpr const char *ACT_ROOM_TYPE_BEDROOM = "room_modal.type.bedroom";
static constexpr const char *ACT_ROOM_MODAL_TOGGLE_DEVICE = "room_modal.toggle_device";
static constexpr const char *ACT_ROOM_MODAL_BACK = "room_modal.back";
static constexpr const char *ACT_ROOM_MODAL_CANCEL = "room_modal.cancel";
static constexpr const char *ACT_ROOM_MODAL_CONFIRM = "room_modal.confirm";
static constexpr const char *ACT_SETTINGS_CHECK_UPDATE = "settings.check_update";
static constexpr const char *ACT_SETTINGS_RESET = "settings.reset";
static constexpr const char *ACT_SETTINGS_LANGUAGE = "settings.language";
static constexpr const char *ACT_SETTINGS_BRIGHTNESS = "settings.brightness";
static constexpr const char *ACT_SETTINGS_ABOUT = "settings.about";
static constexpr const char *ACT_NETWORK_REFRESH = "network.refresh";
static constexpr const char *ACT_MODAL_CANCEL = "modal.cancel";
static constexpr const char *ACT_MODAL_CONFIRM = "modal.confirm";

static constexpr std::array<const char *, 4> ROOM_TYPE_KEYS = {
    "room.living_room",
    "room.kitchen",
    "room.study",
    "room.bedroom",
};
static constexpr std::array<const char *, 4> ROOM_TYPE_IDS = {
    "room_type_living_room",
    "room_type_kitchen",
    "room_type_study",
    "room_type_bedroom",
};

struct I18nPackage {
    std::vector<gui::BindingValueUpdate> updates;
    std::unordered_map<std::string, std::string> strings;
};

std::string int_str(int v)
{
    return std::to_string(v);
}
std::string bool_str(bool v)
{
    return v ? "true" : "false";
}

int clamp_int(int value, int min_value, int max_value)
{
    return std::max(min_value, std::min(value, max_value));
}

int cct_kelvin(int ui_cct)
{
    return clamp_int(ui_cct, 27, 65) * 100;
}

std::string trim_copy(std::string_view value)
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) {
        return "";
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(begin, end - begin + 1));
}

std::string normalize_locale(std::string_view locale)
{
    return locale == LOCALE_ZH_CN ? LOCALE_ZH_CN : LOCALE_EN;
}

std::string firmware_chip_display_name()
{
#if defined(CONFIG_IDF_TARGET_ESP32S31)
    return "ESP32-S31";
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
    return "ESP32-P4";
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    return "ESP32-S3";
#elif defined(CONFIG_IDF_TARGET)
    return CONFIG_IDF_TARGET;
#else
    return "PC";
#endif
}

std::string make_resource_path(system::core::AppContext &context, std::string_view relative_path)
{
    const auto app_paths = context.system_service().get_app_storage_paths();
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

std::string replace_all(std::string value, std::string_view pattern, std::string_view replacement)
{
    if (pattern.empty()) {
        return value;
    }

    size_t position = 0;
    while ((position = value.find(pattern, position)) != std::string::npos) {
        value.replace(position, pattern.size(), replacement);
        position += replacement.size();
    }
    return value;
}

std::expected<I18nPackage, std::string> load_i18n_package(
    system::core::AppContext &context,
    std::string_view locale
)
{
    std::string relative_path(I18N_RESOURCE_DIR);
    relative_path += locale;
    relative_path += JSON_EXTENSION;

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
    const auto *strings_value = root.if_contains("strings");
    if (strings_value == nullptr || !strings_value->is_object()) {
        return std::unexpected("I18N file must contain an object field 'strings': " + path);
    }

    I18nPackage package;
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
        package.updates.push_back(gui::BindingValueUpdate{
            .absolute_path = path_value->as_string().c_str(),
            .key = key_value->as_string().c_str(),
            .value = json_value_to_string(*value),
        });
    }

    for (const auto &[key, value] : strings_value->as_object()) {
        package.strings.emplace(key, json_value_to_string(value));
    }
    return package;
}

std::string binding_cache_key(std::string_view path, std::string_view key)
{
    std::string ck(path); ck += "|"; ck += key; return ck;
}

template <typename Container>
void release_container_memory(Container &container)
{
    Container empty;
    container.swap(empty);
}

} // namespace

MatterControllerApp::~MatterControllerApp() = default;

system::core::AppManifest MatterControllerApp::get_manifest() const
{
    return {
        .id = APP_ID, .name = APP_NAME, .version = APP_VERSION,
        .kind = system::core::AppKind::Native, .visible = true, .icon_id = APP_ICON_ID,
        .supported_systems = {}, .icon_path = "",
        .runtime_type = runtime::BackendType::Unknown, .app_path = BROOKESIA_APP_MATTER_CONTROLLER_RESOURCE_DIR, .entry = "",
        .resource_dir = BROOKESIA_APP_MATTER_CONTROLLER_RESOURCE_DIR,
        .arguments = {},
    };
}

system::core::AppGuiDescriptor MatterControllerApp::get_gui_descriptor() const
{
    return {
        .root_kind = system::core::GuiRootKind::File,
        .root = GUI_ROOT,
        .resources = {},
        .screen_flows = {
            system::core::GuiScreenFlowEntry{
                .screen_flow = MAIN_FLOW_ID,
                .layer = system::core::GuiAppLayer::AppTop,
            },
        },
    };
}

// --- Binding System ---

void MatterControllerApp::set_binding(system::core::AppContext &, std::string_view p, std::string_view k, std::string v) const
{
    queue_binding(p, k, std::move(v), false);
}

void MatterControllerApp::force_binding(system::core::AppContext &, std::string_view p, std::string_view k, std::string v) const
{
    queue_binding(p, k, std::move(v), true);
}

void MatterControllerApp::queue_binding(std::string_view p, std::string_view k, std::string v, bool force) const
{
    const auto ck = binding_cache_key(p, k);
    if (!force) {
        auto it = binding_cache_.find(ck);
        if (it != binding_cache_.end() && it->second == v) {
            return;
        }
    }
    binding_cache_[ck] = v;
    auto pit = pending_binding_indexes_.find(ck);
    if (pit != pending_binding_indexes_.end()) {
        pending_binding_updates_[pit->second].value = std::move(v);
        return;
    }
    pending_binding_indexes_[ck] = pending_binding_updates_.size();
    pending_binding_updates_.push_back({.absolute_path = std::string(p), .key = std::string(k), .value = std::move(v)});
}

void MatterControllerApp::invalidate_binding(std::string_view p, std::string_view k) const
{
    binding_cache_.erase(binding_cache_key(p, k));
}

void MatterControllerApp::forget_binding_prefix(std::string_view path_prefix) const
{
    const std::string prefix(path_prefix);
    const std::string child_prefix = prefix + "/";
    auto belongs_to_prefix = [&](std::string_view path) {
        return path == prefix || path.starts_with(child_prefix);
    };

    for (auto it = binding_cache_.begin(); it != binding_cache_.end(); ) {
        const auto separator = it->first.rfind('|');
        const std::string_view path(
            it->first.data(), separator == std::string::npos ? it->first.size() : separator
        );
        if (belongs_to_prefix(path)) {
            it = binding_cache_.erase(it);
        } else {
            ++it;
        }
    }

    if (pending_binding_updates_.empty()) {
        return;
    }
    std::erase_if(pending_binding_updates_, [&](const gui::BindingValueUpdate &update) {
        return belongs_to_prefix(update.absolute_path);
    });
    pending_binding_indexes_.clear();
    for (size_t i = 0; i < pending_binding_updates_.size(); ++i) {
        const auto &update = pending_binding_updates_[i];
        pending_binding_indexes_[binding_cache_key(update.absolute_path, update.key)] = i;
    }
}

std::expected<void, std::string> MatterControllerApp::flush_bindings(system::core::AppContext &ctx) const
{
    if (pending_binding_updates_.empty()) return {};
    auto u = std::move(pending_binding_updates_);
    pending_binding_updates_.clear();
    pending_binding_indexes_.clear();
    auto r = ctx.gui().set_binding_values(u);
    if (!r) {
        BROOKESIA_LOGW("Set bindings failed: %1%", r.error());
        return r;
    }
    return {};
}

// --- I18N ---

std::expected<bool, std::string> MatterControllerApp::refresh_locale_if_needed(
    system::core::AppContext &ctx, bool force)
{
    const auto locale = normalize_locale(ctx.gui().get_language());
    if (!force && locale == current_locale_) {
        return false;
    }

    auto package = load_i18n_package(ctx, locale);
    auto loaded_locale = locale;
    if (!package && locale != LOCALE_EN) {
        BROOKESIA_LOGW("Failed to load locale '%1%': %2%; fallback to '%3%'", locale, package.error(), LOCALE_EN);
        package = load_i18n_package(ctx, LOCALE_EN);
        loaded_locale = LOCALE_EN;
    }
    if (!package) {
        return std::unexpected(package.error());
    }

    current_locale_ = loaded_locale;
    localized_strings_ = std::move(package->strings);
    pending_locale_updates_ = std::move(package->updates);

    // Only the shell nodes exist when the app starts. Sending every localized
    // value at once makes the runtime look up all lazy page paths, emits a
    // warning for each miss, and holds the GUI task long enough to make launch
    // feel stalled. Apply shell strings now and consume each page's strings
    // immediately after that page is created instead.
    auto root_result = apply_pending_locale_updates(ctx, SCREEN_PATH);
    if (!root_result) {
        return std::unexpected(root_result.error());
    }
    for (const auto &view_id : created_stage_views_) {
        auto view_result = apply_pending_locale_updates(
                               ctx, std::string(STAGE_PATH) + "/" + view_id
                           );
        if (!view_result) {
            return std::unexpected(view_result.error());
        }
    }
    return true;
}

std::expected<void, std::string> MatterControllerApp::apply_pending_locale_updates(
    system::core::AppContext &ctx, std::string_view path_prefix)
{
    if (pending_locale_updates_.empty()) {
        return {};
    }

    const std::string prefix(path_prefix);
    const std::string child_prefix = prefix + "/";
    const bool root_scope = prefix == SCREEN_PATH;
    const std::string stage_prefix = std::string(STAGE_PATH) + "/";
    std::vector<gui::BindingValueUpdate> selected;
    selected.reserve(pending_locale_updates_.size());

    auto keep = [&](gui::BindingValueUpdate &update) {
        const bool in_scope = update.absolute_path == prefix ||
                              update.absolute_path.starts_with(child_prefix);
        const bool belongs_to_lazy_stage_view = root_scope &&
                                                update.absolute_path.starts_with(stage_prefix);
        if (in_scope && !belongs_to_lazy_stage_view) {
            selected.push_back(std::move(update));
            return false;
        }
        return true;
    };
    pending_locale_updates_.erase(
        std::remove_if(
            pending_locale_updates_.begin(), pending_locale_updates_.end(),
            [&](gui::BindingValueUpdate &update) { return !keep(update); }
        ),
        pending_locale_updates_.end()
    );

    if (selected.empty()) {
        return {};
    }
    auto result = ctx.gui().set_binding_values(selected);
    if (!result) {
        return std::unexpected(result.error());
    }
    return {};
}

void MatterControllerApp::refresh_visible_content(system::core::AppContext &ctx)
{
    update_nav_highlight(ctx);
    if (active_page_ == PAGE_DEVICES) {
        setup_device_grid(ctx);
    } else if (active_page_ == PAGE_ROOMS) {
        setup_rooms_list(ctx);
    } else if (active_page_ == PAGE_THREAD) {
        setup_thread_map(ctx);
    } else if (active_page_ == PAGE_PAIRING) {
        setup_pairing_page(ctx);
    } else if (active_page_ == PAGE_NETWORK) {
        setup_network_page(ctx);
    } else if (active_page_ == PAGE_SETTINGS) {
        setup_settings_page(ctx);
    }

    if (room_modal_visible_) {
        setup_room_modal(ctx);
    }
    refresh_active_modal(ctx);
}

void MatterControllerApp::refresh_active_modal(system::core::AppContext &ctx)
{
    if (active_modal_title_key_.empty()) {
        return;
    }
    const auto title_key = active_modal_title_key_;
    const auto message_key = active_modal_message_key_;
    const auto confirm_key = active_modal_confirm_key_;
    const auto confirm_bg = active_modal_confirm_bg_;
    show_modal(ctx, title_key, message_key, confirm_key, confirm_bg);
}

std::string MatterControllerApp::tr(std::string_view key) const
{
    auto it = localized_strings_.find(std::string(key));
    if (it != localized_strings_.end()) {
        return it->second;
    }
    return std::string(key);
}

std::string MatterControllerApp::format_tr(
    std::string_view key, std::string_view placeholder, std::string_view value) const
{
    std::string token("{");
    token += placeholder;
    token += "}";
    return replace_all(tr(key), token, value);
}

std::string MatterControllerApp::localized_room_name(std::string_view room_key) const
{
    return tr(room_key);
}

std::string MatterControllerApp::localized_device_status(const ControllerDevice &device) const
{
    if (device.device_type == "light") {
        if (device.status_key == "status.bright") {
            return format_tr(device.status_key, "value", int_str(device.brightness));
        }
        if (device.status_key == "status.cct") {
            return format_tr(device.status_key, "value", int_str(cct_kelvin(device.cct)));
        }
        if (device.status_key == "status.hue") {
            return format_tr(device.status_key, "value", int_str(device.hue));
        }
        if (device.status_key == "status.saturation") {
            return format_tr(device.status_key, "value", int_str(device.saturation));
        }
        return tr(device.status_key);
    }

    if (!device.powered_on) {
        return tr("status.off");
    }
    if (device.device_type == "socket") {
        return tr("status.active");
    }
    return tr("status.on");
}

std::string MatterControllerApp::localized_device_name(const ControllerDevice &device) const
{
    if (!device.name_key.empty()) {
        return tr(device.name_key);
    }
    if (!device.display_name.empty()) {
        return device.display_name;
    }
    return tr("device.generic");
}

// --- Page Management ---

std::expected<void, std::string> MatterControllerApp::ensure_stage_view_created(
    system::core::AppContext &ctx, std::string_view template_id)
{
    const std::string view_id(template_id);
    if (created_stage_views_.contains(view_id)) {
        return {};
    }
    auto result = ctx.gui().create_view(template_id, STAGE_PATH, view_id);
    if (!result) {
        return std::unexpected(
                   "Failed to create Matter Controller view '" + view_id + "': " + result.error()
               );
    }
    created_stage_views_.insert(view_id);
    auto locale_result = apply_pending_locale_updates(
                             ctx, std::string(STAGE_PATH) + "/" + view_id
                         );
    if (!locale_result) {
        destroy_stage_view(ctx, view_id);
        return locale_result;
    }
    return {};
}

void MatterControllerApp::destroy_stage_view(
    system::core::AppContext &ctx, std::string_view template_id)
{
    const std::string view_id(template_id);
    if (!created_stage_views_.contains(view_id)) {
        return;
    }
    const std::string path = std::string(STAGE_PATH) + "/" + view_id;
    if (!ctx.gui().destroy_view(path)) {
        BROOKESIA_LOGW("Failed to destroy Matter Controller view: %1%", path);
        return;
    }
    created_stage_views_.erase(view_id);
    forget_binding_prefix(path);
}

void MatterControllerApp::release_page_resources(
    system::core::AppContext &ctx, std::string_view page)
{
    if (page == PAGE_DEVICES) {
        release_all_cards(ctx);
        path_to_device_map_.clear();
    } else if (page == PAGE_THREAD) {
        clear_thread_nodes(ctx);
    } else if (page == PAGE_ROOMS) {
        for (const auto &path : room_row_paths_) {
            ctx.gui().destroy_view(path);
        }
        room_row_paths_.clear();
        for (const auto &path : room_option_paths_) {
            ctx.gui().destroy_view(path);
        }
        room_option_paths_.clear();
        room_option_path_to_device_map_.clear();
        room_modal_visible_ = false;
        destroy_stage_view(ctx, TEMPLATE_ROOM_MODAL);
    } else if (page == PAGE_PAIRING && pairing_qr_image_registered_) {
        if (!ctx.gui().unregister_image(PAIRING_QR_RUNTIME_IMAGE_ID)) {
            BROOKESIA_LOGW("Failed to unregister pairing QR image: %1%", PAIRING_QR_RUNTIME_IMAGE_ID);
        }
        pairing_qr_image_registered_ = false;
        release_matter_pairing_qr_image();
    }
}

std::expected<void, std::string> MatterControllerApp::show_page(
    system::core::AppContext &ctx, std::string_view page)
{
    const char *template_id = page_template_id(page);
    if (template_id == nullptr) {
        return std::unexpected("Unknown Matter Controller page: " + std::string(page));
    }


    // A normal modal must never survive a page transition. Retaining hidden
    // overlays leaves another view subtree and its binding/style state resident.
    if (created_stage_views_.contains(TEMPLATE_MODAL)) {
        hide_modal(ctx);
    }

    // Keep a single heavyweight page mounted. The previous implementation
    // accumulated every visited page (and the pairing QR bitmap) under stage;
    // after one successful switch, creating the next 40-60 node page could
    // exhaust PSRAM and throw std::bad_alloc.
    if (!active_page_.empty() &&
            (active_page_ != page || active_stage_view_ != template_id)) {
        const std::string previous_page = active_page_;
        release_page_resources(ctx, previous_page);
        if (!active_stage_view_.empty()) {
            destroy_stage_view(ctx, active_stage_view_);
        }
        active_page_.clear();
        active_stage_view_.clear();
    }

    auto create_result = ensure_stage_view_created(ctx, template_id);
    if (!create_result) {
        return create_result;
    }

    active_page_ = page;
    active_stage_view_ = template_id;
    set_binding(ctx, std::string(STAGE_PATH) + "/" + template_id, KEY_HIDDEN, "false");
    update_nav_highlight(ctx);
    std::string title_key = "nav.dashboard";
    if (page == PAGE_PAIRING) title_key = "nav.commissioning";
    else if (page == PAGE_NETWORK) title_key = "nav.network";
    else if (page == PAGE_SETTINGS) title_key = "nav.settings";
    else if (page == PAGE_ROOMS) title_key = "nav.rooms";
    else if (page == PAGE_THREAD) title_key = "nav.thread";
    set_binding(ctx, "/matter_controller/header/header_identity/header_title", KEY_TEXT, tr(title_key));

    if (page == PAGE_DEVICES) {
        setup_device_grid(ctx);
    } else if (page == PAGE_ROOMS) {
        setup_rooms_list(ctx);
    } else if (page == PAGE_THREAD) {
        setup_thread_map(ctx);
    } else if (page == PAGE_PAIRING) {
        setup_pairing_page(ctx);
    } else if (page == PAGE_NETWORK) {
        setup_network_page(ctx);
    } else if (page == PAGE_SETTINGS) {
        setup_settings_page(ctx);
    }
    return {};
}

void MatterControllerApp::update_nav_highlight(system::core::AppContext &ctx) const
{
    auto set_nav = [&](std::string_view id, bool active) {
        const std::string p = std::string("/matter_controller/bottom_nav/") + std::string(id);
        set_binding(ctx, p, KEY_NAV_BG, active ? "#6C63FF" : "#252545");
        set_binding(ctx, p + "/" + std::string(id) + "_label", KEY_COLOR, active ? "#EAEAFF" : "#9292B8");
        set_binding(ctx, p + "/" + std::string(id) + "_dot", KEY_DOT_COLOR, active ? "#EAEAFF" : "#3A3A60");
    };
    const bool dashboard_active = active_page_ == PAGE_DEVICES || active_page_ == PAGE_ROOMS ||
                                  active_page_ == PAGE_THREAD;
    set_nav("nav_devices", dashboard_active);
    set_nav("nav_pairing", active_page_ == PAGE_PAIRING);
    set_nav("nav_network", active_page_ == PAGE_NETWORK);
    set_nav("nav_settings", active_page_ == PAGE_SETTINGS);
}

void MatterControllerApp::setup_network_page(system::core::AppContext &ctx)
{
    const std::string p = std::string(STAGE_PATH) + "/page_network/network_grid";
    set_binding(ctx, p + "/network_wifi/wifi_status_value", KEY_TEXT,
                format_tr("network.line.status", "value", tr("network.connected")));
    set_binding(ctx, p + "/network_wifi/wifi_ssid_value", KEY_TEXT,
                format_tr("network.line.ssid", "value", tr("network.ssid.demo")));
    set_binding(ctx, p + "/network_wifi/wifi_signal_value", KEY_TEXT,
                format_tr("network.line.signal", "value", tr("network.signal.excellent")));

    set_binding(ctx, p + "/network_fabric/fabric_status_value", KEY_TEXT,
                format_tr("network.line.status", "value", tr("network.ready")));
    set_binding(ctx, p + "/network_fabric/fabric_id_value", KEY_TEXT,
                format_tr("network.line.fabric", "value", tr("network.fabric.primary")));
    set_binding(ctx, p + "/network_fabric/fabric_devices_value", KEY_TEXT,
                format_tr(
                    "network.line.devices", "value",
                    format_tr("network.commissioned", "count", int_str(static_cast<int>(devices_.size())))
                ));

    set_binding(ctx, p + "/network_thread/network_thread_status_value", KEY_TEXT,
                format_tr("network.line.status", "value", tr("network.available")));
    set_binding(ctx, p + "/network_thread/network_thread_role_value", KEY_TEXT,
                format_tr("network.line.border_router", "value", tr("network.thread.active")));
    set_binding(ctx, p + "/network_health/network_health_latency_value", KEY_TEXT,
                format_tr("network.line.controller", "value", tr("network.online")));
    set_binding(ctx, p + "/network_health/network_health_updated_value", KEY_TEXT,
                format_tr("network.line.last_check", "value", tr("network.just_now")));
}

void MatterControllerApp::setup_settings_page(system::core::AppContext &ctx)
{
    const std::string p = std::string(STAGE_PATH) + "/page_settings";
    const bool chinese = current_locale_ == LOCALE_ZH_CN;
    set_binding(ctx, p + "/settings_content/settings_preferences/language_row/language_info/language_value",
                KEY_TEXT, chinese ? tr("language.chinese") : tr("language.english"));
    set_binding(ctx, p + "/settings_content/settings_preferences/language_row/language_toggle",
                KEY_CHECKED, bool_str(chinese));
    set_binding(ctx, p + "/settings_content/settings_preferences/brightness_group/brightness_header/brightness_value",
                KEY_TEXT, int_str(ui_brightness_) + "%");
    set_binding(ctx, p + "/settings_content/settings_preferences/brightness_group/brightness_slider",
                KEY_VALUE, int_str(ui_brightness_));
    set_binding(
        ctx, p + "/settings_content/settings_firmware/fw_version_summary", KEY_TEXT,
        format_tr("firmware.version_summary", "chip", firmware_chip_display_name())
    );
}

// --- Device Grid ---


void MatterControllerApp::select_device_by_path(std::string_view event_path)
{
    auto it = path_to_device_map_.find(std::string(event_path));
    if (it != path_to_device_map_.end()) {
        selected_device_index_ = it->second;
        return;
    }
    for (const auto &[path, idx] : path_to_device_map_) {
        if (event_path.find(path + "/") == 0) {
            selected_device_index_ = idx;
            return;
        }
    }

    // Keep the current selection when an event arrives from a stale view path.
    // Falling back to index zero could send a control command to an unrelated
    // device while the dashboard is being rebuilt.
    BROOKESIA_LOGW("Ignore unmapped device event path: %1%", event_path);
}

void MatterControllerApp::setup_device_grid(system::core::AppContext &ctx)
{

    // Reconcile cards by Matter endpoint instead of destroying the entire grid
    // whenever commissioning changes the device list. Retaining existing trees
    // avoids a large transient LVGL allocation peak and prevents the remaining
    // card from fragmenting PSRAM while a new card is being parsed.
    std::vector<DeviceInfo *> previous_cards;
    previous_cards.swap(active_cards_);
    active_cards_.assign(devices_.size(), nullptr);
    path_to_device_map_.clear();
    while (card_pool_.size() < devices_.size()) {
        card_pool_.emplace_back();
    }

    const std::string shell = STAGE_PATH + std::string("/page_devices/page_devices_shell");
    const std::string grid = shell + "/devices_body/devices_grid";
    set_binding(ctx, shell + "/devices_header/devices_heading/page_devices_count", KEY_TEXT,
                format_tr("dashboard.device_count", "count", int_str(static_cast<int>(devices_.size()))));
    const bool has_devices = !devices_.empty();
    set_binding(ctx, shell + "/devices_body/devices_empty", KEY_HIDDEN, bool_str(has_devices));
    set_binding(ctx, grid, KEY_HIDDEN, bool_str(!has_devices));
    set_binding(ctx, shell + "/devices_body/devices_empty/empty_title", KEY_TEXT, tr("dashboard.empty.title"));
    set_binding(ctx, shell + "/devices_body/devices_empty/empty_desc", KEY_TEXT, tr("dashboard.empty.desc"));

    // Commit the cheap visibility/count updates before allocating dynamic card
    // subtrees. If a card allocation later fails, the stale No Devices overlay
    // must not remain on top of an already paired device.
    if (auto visibility_result = flush_bindings(ctx); !visibility_result) {
        BROOKESIA_LOGW("Failed to update dashboard visibility: %1%", visibility_result.error());
    }

    // Release endpoints that disappeared before allocating new card trees. This
    // is especially important when a device-list refresh replaces one endpoint
    // with another at the same time.
    for (auto *&card : previous_cards) {
        if (card == nullptr) {
            continue;
        }
        const bool still_present = std::any_of(devices_.begin(), devices_.end(), [&](const ControllerDevice &device) {
            return device.node_id == card->node_id && device.endpoint_id == card->endpoint_id;
        });
        if (!still_present) {
            release_card(ctx, *card);
            card = nullptr;
        }
    }

    auto take_existing_card = [&](const ControllerDevice &device) -> DeviceInfo * {
        for (auto *&card : previous_cards) {
            if (card != nullptr && card->node_id == device.node_id &&
                    card->endpoint_id == device.endpoint_id) {
                auto *result = card;
                card = nullptr;
                return result;
            }
        }
        return nullptr;
    };

    auto create_or_refresh_card = [&](size_t device_index) -> bool {
        const auto &device = devices_[device_index];
        const char *template_id = device_card_template_id(device);
        DeviceInfo *card = take_existing_card(device);
        if (card != nullptr && card->template_id != template_id) {
            // A capability change needs a different tree, but only that one
            // endpoint is rebuilt rather than every device on the dashboard.
            release_card(ctx, *card);
            card = nullptr;
        }

        if (card == nullptr) {
            card = acquire_card();
            if (card == nullptr) {
                BROOKESIA_LOGW("No reusable device card available");
                return false;
            }
            const std::string card_id = "card_" + std::to_string(device.node_id) + "_" +
                                        std::to_string(device.endpoint_id);
            auto create_result = ctx.gui().create_view(template_id, grid, card_id);
            if (!create_result) {
                BROOKESIA_LOGW("create_view failed: %1%", create_result.error());
                return false;
            }
            card->path = grid + "/" + card_id;
            card->template_id = template_id;
            card->in_use = true;
            card->node_id = device.node_id;
            card->endpoint_id = device.endpoint_id;
        }

        active_cards_[device_index] = card;
        path_to_device_map_[card->path] = device_index;
        apply_card(ctx, *card, device);
        set_binding(ctx, card->path, KEY_HIDDEN, "false");

        // Applying a whole RGB/CW card in one batch previously queued over 30
        // bindings. Flush each card as soon as it is ready so temporary GUI
        // conversion buffers do not overlap across all devices.
        if (auto binding_result = flush_bindings(ctx); !binding_result) {
            BROOKESIA_LOGW("Failed to apply device card bindings: %1%", binding_result.error());
            return false;
        }
        return true;
    };

    // New cards retain the prior reachable-first placement. Existing cards are
    // intentionally left in place: moving them would require a destroy/recreate
    // cycle and defeat the memory-saving incremental update.
    auto reconcile_cards = [&](bool reachable) {
        for (size_t device_index = 0; device_index < devices_.size(); ++device_index) {
            if (devices_[device_index].reachable == reachable &&
                    !create_or_refresh_card(device_index)) {
                return false;
            }
        }
        return true;
    };
    if (reconcile_cards(true)) {
        reconcile_cards(false);
    }

    // Any unconsumed pointer was not represented by the latest backend list.
    for (auto *card : previous_cards) {
        if (card != nullptr) {
            release_card(ctx, *card);
        }
    }
}

void MatterControllerApp::apply_card(system::core::AppContext &ctx, DeviceInfo &card, const ControllerDevice &dev) const
{
    const std::string p = card.path;
    set_binding(ctx, p, KEY_BORDER_COLOR, dev.powered_on ? "#6C63FF" : "#3A3A60");

    set_binding(ctx, p + "/card_header/card_title_area/card_name", KEY_TEXT, localized_device_name(dev));
    set_binding(ctx, p + "/card_header/card_meta_row/card_room", KEY_TEXT, localized_room_name(dev.room_key));
    set_binding(ctx, p + "/card_header/card_meta_row/card_tag", KEY_TEXT, tr(dev.tag_key));
    set_binding(ctx, p + "/card_header/card_meta_row/card_online", KEY_TEXT, dev.reachable ? tr("network.online") : tr("network.offline"));
    set_binding(ctx, p + "/card_header/card_meta_row/card_online", KEY_COLOR, dev.reachable ? "#4ECDC4" : "#9292B8");
    set_binding(ctx, p + "/card_header/card_meta_row/card_online", KEY_STATUS_BG, dev.reachable ? "#254744" : "#3A3A60");
    set_binding(ctx, p + "/card_footer/card_status", KEY_TEXT, localized_device_status(dev));
    set_binding(ctx, p + "/card_footer/card_status", KEY_COLOR,
                dev.powered_on ? "#4ECDC4" : "#9292B8");
    set_binding(ctx, p + "/card_footer/card_remove_btn/card_remove_label", KEY_TEXT,
                tr("device.remove"));
    set_binding(ctx, p + "/card_footer/card_power_btn", KEY_POWER_BG,
                dev.powered_on ? "#6C63FF" : "#2E2E5A");
    set_binding(ctx, p + "/card_footer/card_power_btn", KEY_POWER_BORDER,
                dev.powered_on ? "#8B85FF" : "#3A3A60");
    set_binding(ctx, p + "/card_footer/card_power_btn/card_power_label", KEY_TEXT,
                dev.powered_on ? tr("status.on") : tr("status.off"));

    if (card.template_id == TEMPLATE_LIGHT_DIMMABLE_CARD) {
        apply_light_controls(ctx, p, dev, false, false);
    } else if (card.template_id == TEMPLATE_LIGHT_COLOR_TEMP_CARD) {
        apply_light_controls(ctx, p, dev, true, false);
    } else if (card.template_id == TEMPLATE_LIGHT_EXTENDED_CARD) {
        apply_light_controls(ctx, p, dev, true, true);
    }
}

void MatterControllerApp::apply_light_controls(
    system::core::AppContext &ctx, std::string_view card_path, const ControllerDevice &dev,
    bool has_color_temp_controls, bool has_hue_controls) const
{
    const std::string controls_path = std::string(card_path) + "/card_controls";
    // Every light template has a fixed control tree matching its Matter Device
    // Type. This avoids both hidden, unused LVGL controls and runtime layout
    // resizing while attributes are reported.
    set_binding(ctx, controls_path + "/control_brightness/control_brightness_label", KEY_TEXT,
                tr("slider.brightness"));
    set_binding(ctx, controls_path + "/control_brightness/control_brightness_value", KEY_TEXT,
                int_str(dev.brightness) + "%");
    set_binding(ctx, controls_path + "/control_brightness/control_brightness_slider", KEY_VALUE,
                int_str(dev.brightness));

    if (has_color_temp_controls) {
        set_binding(ctx, controls_path + "/control_cct/control_cct_label", KEY_TEXT,
                    tr("slider.color_temp"));
        set_binding(ctx, controls_path + "/control_cct/control_cct_value", KEY_TEXT,
                    int_str(cct_kelvin(dev.cct)) + "K");
        set_binding(ctx, controls_path + "/control_cct/control_cct_slider", KEY_VALUE,
                    int_str(dev.cct));
    }

    if (has_hue_controls) {
        set_binding(ctx, controls_path + "/control_hue/control_hue_label", KEY_TEXT, tr("slider.hue"));
        set_binding(ctx, controls_path + "/control_hue/control_hue_value", KEY_TEXT,
                    int_str(dev.hue) + "°");
        set_binding(ctx, controls_path + "/control_hue/control_hue_slider", KEY_VALUE,
                    int_str(dev.hue));
        set_binding(ctx, controls_path + "/control_saturation/control_saturation_label", KEY_TEXT,
                    tr("slider.saturation"));
        set_binding(ctx, controls_path + "/control_saturation/control_saturation_value", KEY_TEXT,
                    int_str(dev.saturation) + "%");
        set_binding(ctx, controls_path + "/control_saturation/control_saturation_slider", KEY_VALUE,
                    int_str(dev.saturation));
    }
}

DeviceInfo *MatterControllerApp::acquire_card()
{
    for (auto &c : card_pool_) if (!c.in_use) {
            return &c;
        }
    return nullptr;
}

void MatterControllerApp::refresh_device_card(system::core::AppContext &ctx, size_t device_index)
{
    if (device_index >= devices_.size() || device_index >= active_cards_.size() || active_cards_[device_index] == nullptr) {
        setup_device_grid(ctx);
        return;
    }
    apply_card(ctx, *active_cards_[device_index], devices_[device_index]);
}

void MatterControllerApp::release_card(system::core::AppContext &ctx, DeviceInfo &card)
{
    card.in_use = false;
    if (!card.path.empty()) {
        ctx.gui().destroy_view(card.path);
        forget_binding_prefix(card.path);
        card.path.clear();
    }
    card.template_id.clear();
    card.node_id = 0;
    card.endpoint_id = 0;
}

void MatterControllerApp::release_all_cards(system::core::AppContext &ctx)
{
    for (auto *c : active_cards_) {
        if (c != nullptr) {
            release_card(ctx, *c);
        }
    }
    active_cards_.clear();
}

// --- Rooms List ---

void MatterControllerApp::setup_rooms_list(system::core::AppContext &ctx)
{
    // Clear old room rows
    for (auto &p : room_row_paths_) {
        ctx.gui().destroy_view(p);
    }
    room_row_paths_.clear();

    std::string list_path = STAGE_PATH + std::string("/page_rooms/page_rooms_shell/rooms_list");
    for (size_t i = 0; i < rooms_.size(); ++i) {
        auto rr = ctx.gui().create_view(TEMPLATE_ROOM_CARD, list_path, "room_" + std::to_string(i));
        if (!rr) {
            break;
        }
        std::string rp = list_path + "/room_" + std::to_string(i);
        room_row_paths_.push_back(rp);
        auto &room = rooms_[i];
        set_binding(ctx, rp + "/room_texts/room_name", KEY_TEXT, localized_room_name(room.name_key));
        int cnt = 0;
        for (const auto &d : devices_) if (d.room_key == room.name_key) {
                cnt++;
            }
        set_binding(ctx, rp + "/room_texts/room_count", KEY_TEXT, format_tr("room.associated_devices", "count", int_str(cnt)));
        set_binding(ctx, rp + "/room_chip", KEY_TEXT, tr("room.chip"));
        set_binding(ctx, rp, KEY_HIDDEN, "false");
    }
}

void MatterControllerApp::show_room_modal(system::core::AppContext &ctx)
{
    auto create_result = ensure_stage_view_created(ctx, TEMPLATE_ROOM_MODAL);
    if (!create_result) {
        BROOKESIA_LOGE("Failed to create room modal: %1%", create_result.error());
        return;
    }
    room_draft_error_key_.clear();
    room_draft_device_selected_.assign(devices_.size(), false);
    selected_room_option_index_ = 0;
    selected_room_type_index_ = 0;
    for (size_t i = 0; i < ROOM_TYPE_KEYS.size(); ++i) {
        const bool room_exists = std::any_of(rooms_.begin(), rooms_.end(), [&](const ControllerRoom &room) {
            return room.name_key == ROOM_TYPE_KEYS[i];
        });
        if (!room_exists) {
            selected_room_type_index_ = i;
            break;
        }
    }
    room_draft_name_ = ROOM_TYPE_KEYS[selected_room_type_index_];
    setup_room_modal(ctx);
    room_modal_visible_ = true;
    set_binding(ctx, STAGE_PATH + std::string("/room_modal"), KEY_HIDDEN, "false");
}

void MatterControllerApp::hide_room_modal(system::core::AppContext &ctx)
{
    room_modal_visible_ = false;
    // Destroy nested dynamic options first so their runtime instance snapshots
    // are removed as well, then release the modal subtree instead of retaining
    // it invisibly until the app exits.
    for (const auto &path : room_option_paths_) {
        ctx.gui().destroy_view(path);
        forget_binding_prefix(path);
    }
    room_option_paths_.clear();
    room_option_path_to_device_map_.clear();
    destroy_stage_view(ctx, TEMPLATE_ROOM_MODAL);
}

void MatterControllerApp::select_room_device_by_path(std::string_view event_path)
{
    auto it = room_option_path_to_device_map_.find(std::string(event_path));
    if (it != room_option_path_to_device_map_.end()) {
        selected_room_option_index_ = it->second;
        return;
    }
    for (const auto &[path, idx] : room_option_path_to_device_map_) {
        if (event_path.find(path + "/") == 0) {
            selected_room_option_index_ = idx;
            return;
        }
    }
    selected_room_option_index_ = 0;
}

void MatterControllerApp::apply_room_device_option(system::core::AppContext &ctx, size_t device_index) const
{
    if (device_index >= devices_.size() || device_index >= room_option_paths_.size()) {
        return;
    }
    const auto &path = room_option_paths_[device_index];
    const auto &device = devices_[device_index];
    const bool selected = device_index < room_draft_device_selected_.size() &&
                          room_draft_device_selected_[device_index];
    set_binding(ctx, path + "/room_device_texts/room_device_name", KEY_TEXT, localized_device_name(device));
    set_binding(
        ctx,
        path + "/room_device_texts/room_device_detail",
        KEY_TEXT,
        format_tr("room.current_room", "room", localized_room_name(device.room_key))
    );
    set_binding(ctx, path + "/room_device_toggle", KEY_CHECKED, bool_str(selected));
    set_binding(ctx, path, KEY_BORDER_COLOR, selected ? "#6C63FF" : "#252833");
    set_binding(ctx, path, KEY_HIDDEN, "false");
}

void MatterControllerApp::apply_room_type_options(system::core::AppContext &ctx) const
{
    const std::string list_path = STAGE_PATH + std::string("/room_modal/room_modal_box/room_type_list");
    for (size_t i = 0; i < ROOM_TYPE_KEYS.size(); ++i) {
        const std::string path = list_path + "/" + ROOM_TYPE_IDS[i];
        const bool selected = i == selected_room_type_index_;
        set_binding(ctx, path, KEY_BTN_BG, selected ? "#34306A" : "#252545");
        set_binding(ctx, path, KEY_BORDER_COLOR, selected ? "#8B85FF" : "#3A3A60");
        set_binding(ctx, path + "/room_type_name", KEY_TEXT, localized_room_name(ROOM_TYPE_KEYS[i]));
    }
}

void MatterControllerApp::setup_room_modal(system::core::AppContext &ctx)
{
    for (const auto &path : room_option_paths_) {
        ctx.gui().destroy_view(path);
    }
    room_option_paths_.clear();
    room_option_path_to_device_map_.clear();

    const std::string modal_path = STAGE_PATH + std::string("/room_modal/room_modal_box");
    const std::string list_path = modal_path + "/room_device_list";
    apply_room_type_options(ctx);
    set_binding(ctx, modal_path + "/room_modal_error", KEY_TEXT, tr(room_draft_error_key_));
    set_binding(ctx, modal_path + "/room_modal_error", KEY_HIDDEN, bool_str(room_draft_error_key_.empty()));

    for (size_t i = 0; i < devices_.size(); ++i) {
        auto result = ctx.gui().create_view(
                          TEMPLATE_ROOM_DEVICE_OPTION,
                          list_path,
                          "room_device_" + std::to_string(devices_[i].node_id)
                      );
        if (!result) {
            BROOKESIA_LOGW("create_view failed: %1%", result.error());
            break;
        }
        const auto path = list_path + "/room_device_" + std::to_string(devices_[i].node_id);
        room_option_paths_.push_back(path);
        room_option_path_to_device_map_[path] = i;
        apply_room_device_option(ctx, i);
    }
}

// --- Thread Map ---

void MatterControllerApp::clear_thread_nodes(system::core::AppContext &ctx)
{
    for (const auto &path : thread_node_paths_) {
        ctx.gui().destroy_view(path);
        forget_binding_prefix(path);
    }
    thread_node_paths_.clear();
}

void MatterControllerApp::setup_thread_map(system::core::AppContext &ctx)
{
    clear_thread_nodes(ctx);

    const auto topo_frame = ctx.gui().get_view_frame(THREAD_TOPO_PATH);
    const auto hub_frame = ctx.gui().get_view_frame(THREAD_HUB_PATH);
    const int topo_width = topo_frame ? topo_frame->width : THREAD_TOPO_FALLBACK_WIDTH;
    const int topo_height = topo_frame ? topo_frame->height : THREAD_TOPO_FALLBACK_HEIGHT;
    const float center_x = (topo_frame && hub_frame)
                           ? static_cast<float>(hub_frame->x - topo_frame->x) + (hub_frame->width / 2.0F)
                           : topo_width / 2.0F;
    const float center_y = (topo_frame && hub_frame)
                           ? static_cast<float>(hub_frame->y - topo_frame->y) + (hub_frame->height / 2.0F)
                           : topo_height / 2.0F;
    const float node_half_width = THREAD_NODE_WIDTH / 2.0F;
    const float node_half_height = THREAD_NODE_HEIGHT / 2.0F;
    const float node_radius = std::max(node_half_width, node_half_height);
    const float min_radius = (THREAD_HUB_SIZE / 2.0F) + node_radius + 18.0F;
    const float max_radius = std::max(min_radius,
                                      (std::min(topo_width, topo_height) / 2.0F) - node_radius - THREAD_NODE_MARGIN);
    const float radius = std::min(max_radius, std::max(min_radius, max_radius * 0.82F));
    constexpr float pi = 3.14159265358979323846F;

    auto clamp_x = [&](float x) {
        return clamp_int(static_cast<int>(std::lround(x)), THREAD_NODE_MARGIN,
                         std::max(THREAD_NODE_MARGIN, topo_width - THREAD_NODE_WIDTH - THREAD_NODE_MARGIN));
    };
    auto clamp_y = [&](float y) {
        return clamp_int(static_cast<int>(std::lround(y)), THREAD_NODE_MARGIN,
                         std::max(THREAD_NODE_MARGIN, topo_height - THREAD_NODE_HEIGHT - THREAD_NODE_MARGIN));
    };

    for (size_t i = 0; i < devices_.size(); ++i) {
        const auto &device = devices_[i];
        const std::string view_id = "thread_device_" + std::to_string(device.node_id) + "_" +
                                    std::to_string(device.endpoint_id);
        auto result = ctx.gui().create_view(TEMPLATE_THREAD_DEVICE_NODE, THREAD_DEVICE_NODE_LIST_PATH, view_id);
        if (!result) {
            BROOKESIA_LOGW("Failed to create Thread device node: %1%", result.error());
            break;
        }

        const std::string path = std::string(THREAD_DEVICE_NODE_LIST_PATH) + "/" + view_id;
        thread_node_paths_.push_back(path);
        const float angle = -pi / 2.0F + (2.0F * pi * static_cast<float>(i) /
                                          static_cast<float>(std::max<size_t>(devices_.size(), 1)));
        set_binding(ctx, path, KEY_NODE_X, int_str(clamp_x(center_x + std::cos(angle) * radius - node_half_width)));
        set_binding(ctx, path, KEY_NODE_Y, int_str(clamp_y(center_y + std::sin(angle) * radius - node_half_height)));
        set_binding(ctx, path, KEY_NODE_BG, device.reachable ? "#254744" : "#252545");
        set_binding(ctx, path, KEY_NODE_BORDER, device.reachable ? "#4ECDC4" : "#3A3A60");

        std::string label = localized_device_name(device);
        if (label.empty()) {
            label = tr("thread.node.device");
        }
        const auto first = std::find_if(label.begin(), label.end(), [](unsigned char c) {
            return std::isalnum(c) != 0;
        });
        const std::string initial = first == label.end()
                                    ? "?"
                                    : std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(*first))));
        set_binding(ctx, path + "/thread_device_node_label", KEY_TEXT, initial);
        set_binding(ctx, path + "/thread_device_node_name", KEY_TEXT, label);
    }
}

void MatterControllerApp::setup_pairing_page(system::core::AppContext &ctx)
{
    const auto runtime = get_matter_runtime_state();
    const bool commissioned = runtime.network_provisioned;
    set_binding(ctx, PAIRING_STATUS_TITLE_PATH, KEY_TEXT,
                tr(commissioned ? "pairing.status.commissioned" : "pairing.status.broadcasting"));
    set_binding(ctx, PAIRING_STATUS_DESC_PATH, KEY_TEXT,
                tr(commissioned ? "pairing.status.commissioned_desc" : "pairing.status.broadcasting_desc"));

    // A successful controller commissioning must show an explicit success
    // state, never the generic QR placeholder. The provisioning QR is no
    // longer useful at this point, so promptly release its runtime resource.
    if (commissioned) {
        // The commissioned state is represented by one explicit success card.
        // Keeping the generic status card visible here previously produced two
        // visually identical green strips when nested flex items were measured.
        set_binding(ctx, PAIRING_STATUS_PATH, KEY_HIDDEN, "true");
        set_binding(ctx, PAIRING_QR_PANEL_PATH, KEY_HIDDEN, "true");
        set_binding(ctx, PAIRING_WAITING_PATH, KEY_HIDDEN, "true");
        set_binding(ctx, PAIRING_SUCCESS_PATH, KEY_HIDDEN, "false");
        // Keep a QR bitmap that is currently bound to the page alive until
        // this page is released. Hiding and freeing it in the same queued
        // binding transaction could leave LVGL with a stale image source.
        // release_page_resources() frees it immediately on page exit.
        return;
    }

    set_binding(ctx, PAIRING_STATUS_PATH, KEY_HIDDEN, "false");
    set_binding(ctx, PAIRING_QR_PANEL_PATH, KEY_HIDDEN, "false");
    set_binding(ctx, PAIRING_WAITING_PATH, KEY_HIDDEN, "false");
    set_binding(ctx, PAIRING_SUCCESS_PATH, KEY_HIDDEN, "true");

    auto payload = get_matter_pairing_payload();
    if (!payload) {
        // BLE provisioning advertises asynchronously. Opening this page before
        // APP_NETWORK_EVENT_QR_DISPLAY is normal, so keep the compact waiting
        // state while the actual QR payload is being generated.
        constexpr const char *kQrPendingPrefix = "Provisioning QR not yet available";
        if (payload.error().find(kQrPendingPrefix) == 0) {
            set_binding(ctx, PAIRING_QR_IMAGE_PATH, KEY_HIDDEN, "true");
            set_binding(ctx, PAIRING_QR_PLACEHOLDER_PATH, KEY_HIDDEN, "false");
        } else {
            BROOKESIA_LOGW("Matter pairing payload unavailable: %1%", payload.error());
        }
        return;
    }

    if (!pairing_qr_image_registered_) {
        auto register_result = ctx.gui().register_image(gui::RuntimeImageResource{
            .id = PAIRING_QR_RUNTIME_IMAGE_ID,
            .primary_src = "",
            .native_src = payload->qr_native_src,
            .width = payload->qr_width,
            .height = payload->qr_height,
        });
        if (!register_result) {
            BROOKESIA_LOGW("Failed to register pairing QR image: %1%", register_result.error());
            return;
        }
        pairing_qr_image_registered_ = true;
    }

    set_binding(ctx, PAIRING_QR_IMAGE_PATH, KEY_IMAGE_SRC, PAIRING_QR_RUNTIME_IMAGE_ID);
    set_binding(ctx, PAIRING_QR_IMAGE_PATH, KEY_HIDDEN, "false");
    set_binding(ctx, PAIRING_QR_PLACEHOLDER_PATH, KEY_HIDDEN, "true");
}

// --- Modal ---

void MatterControllerApp::show_modal(
    system::core::AppContext &ctx, std::string_view title_key, std::string_view message_key,
    std::string_view confirm_key, std::string_view confirm_bg)
{
    auto create_result = ensure_stage_view_created(ctx, TEMPLATE_MODAL);
    if (!create_result) {
        BROOKESIA_LOGE("Failed to create modal: %1%", create_result.error());
        return;
    }
    active_modal_title_key_ = title_key;
    active_modal_message_key_ = message_key;
    active_modal_confirm_key_ = confirm_key;
    active_modal_confirm_bg_ = confirm_bg;

    std::string mp = STAGE_PATH + std::string("/modal_overlay/modal_box");
    set_binding(ctx, mp + "/modal_title", KEY_TEXT, tr(active_modal_title_key_));
    set_binding(ctx, mp + "/modal_message", KEY_TEXT, tr(active_modal_message_key_));
    set_binding(ctx, mp + "/modal_message", KEY_MSG_HIDDEN, bool_str(active_modal_message_key_.empty()));
    set_binding(ctx, mp + "/modal_actions/modal_confirm/modal_confirm_label", KEY_TEXT, tr(active_modal_confirm_key_));
    set_binding(ctx, mp + "/modal_actions/modal_confirm", KEY_BTN_BG, std::string(confirm_bg));
    set_binding(ctx, STAGE_PATH + std::string("/modal_overlay"), KEY_HIDDEN, "false");
}

void MatterControllerApp::hide_modal(system::core::AppContext &ctx)
{
    active_modal_title_key_.clear();
    active_modal_message_key_.clear();
    active_modal_confirm_key_.clear();
    active_modal_confirm_bg_.clear();
    destroy_stage_view(ctx, TEMPLATE_MODAL);
}

// --- Backend ---

std::expected<void, std::string> MatterControllerApp::start_backend()
{
    backend_ = create_default_matter_backend();
    if (!backend_) {
        return std::unexpected("Failed to create Matter backend");
    }
    auto result = backend_->start();
    if (!result) {
        return result;
    }
    devices_ = backend_->devices();
    rooms_ = backend_->rooms();
    while (card_pool_.size() < devices_.size()) {
        card_pool_.emplace_back();
    }
    return {};
}

void MatterControllerApp::stop_backend()
{
    if (backend_) {
        backend_->stop();
        backend_.reset();
    }
    devices_.clear();
    rooms_.clear();
}

bool MatterControllerApp::sync_backend_state(system::core::AppContext &ctx, bool force_refresh)
{
    if (!backend_) {
        return false;
    }
    if (!force_refresh) {
        auto tick_result = backend_->tick();
        if (!tick_result) {
            BROOKESIA_LOGW("Matter backend tick failed: %1%", tick_result.error());
        }
        if (!backend_->consume_dirty()) {
            return false;
        }
    }

    const auto &next_devices = backend_->devices();
    const auto &next_rooms = backend_->rooms();
    if (!force_refresh && devices_ == next_devices && rooms_ == next_rooms) {
        return false;
    }

    const bool device_count_changed = devices_.size() != next_devices.size();
    if (force_refresh || device_count_changed) {
    }

    bool device_layout_changed = device_count_changed;
    if (!device_layout_changed) {
        for (size_t i = 0; i < devices_.size(); ++i) {
            const auto &old_device = devices_[i];
            const auto &new_device = next_devices[i];
            if (old_device.node_id != new_device.node_id ||
                    old_device.endpoint_id != new_device.endpoint_id ||
                    old_device.device_type != new_device.device_type ||
                    old_device.card_template != new_device.card_template ||
                    device_card_template_id(old_device) != device_card_template_id(new_device)) {
                device_layout_changed = true;
                break;
            }
        }
    }
    const bool rooms_changed = rooms_ != next_rooms;
    bool thread_map_changed = device_layout_changed;

    std::optional<std::pair<uint64_t, uint16_t>> selected_device_id;
    if (selected_device_index_ < devices_.size()) {
        const auto &selected = devices_[selected_device_index_];
        selected_device_id = std::make_pair(selected.node_id, selected.endpoint_id);
    }

    // Keep the existing device objects when the topology is unchanged. A
    // Matter report only changes scalar state in this case; assigning the
    // whole vector would copy every std::string on every report and create
    // avoidable PSRAM allocations/fragmentation while a light and socket are
    // both subscribed.
    if (device_layout_changed) {
        devices_ = next_devices;
    } else {
        for (size_t i = 0; i < devices_.size(); ++i) {
            auto &current = devices_[i];
            const auto &next = next_devices[i];
            // Keep the hot report path allocation-free for unchanged strings,
            // but still propagate an occasional name/room/capability update.
            if (current.name_key != next.name_key) {
                current.name_key = next.name_key;
                thread_map_changed = true;
            }
            if (current.display_name != next.display_name) {
                current.display_name = next.display_name;
                thread_map_changed = true;
            }
            if (current.device_type != next.device_type) {
                current.device_type = next.device_type;
                thread_map_changed = true;
            }
            if (current.card_template != next.card_template) current.card_template = next.card_template;
            if (current.tag_key != next.tag_key) current.tag_key = next.tag_key;
            if (current.room_key != next.room_key) current.room_key = next.room_key;
            current.powered_on = next.powered_on;
            current.brightness = next.brightness;
            current.cct = next.cct;
            current.hue = next.hue;
            current.saturation = next.saturation;
            if (current.status_key != next.status_key) current.status_key = next.status_key;
            if (current.reachable != next.reachable || current.is_commissioned != next.is_commissioned) {
                thread_map_changed = true;
            }
            current.reachable = next.reachable;
            current.is_commissioned = next.is_commissioned;
            current.supports_onoff = next.supports_onoff;
            current.supports_level = next.supports_level;
            current.supports_color_temp = next.supports_color_temp;
            current.supports_hue = next.supports_hue;
        }
    }
    if (force_refresh || device_layout_changed) {
    }
    if (rooms_changed) {
        rooms_ = next_rooms;
    }
    if (selected_device_id) {
        const auto selected = std::find_if(devices_.begin(), devices_.end(), [&](const ControllerDevice &device) {
            return device.node_id == selected_device_id->first &&
                   device.endpoint_id == selected_device_id->second;
        });
        if (selected != devices_.end()) {
            selected_device_index_ = static_cast<size_t>(std::distance(devices_.begin(), selected));
        }
    }
    while (card_pool_.size() < devices_.size()) {
        card_pool_.emplace_back();
    }
    if (selected_device_index_ >= devices_.size()) {
        selected_device_index_ = devices_.empty() ? 0 : (devices_.size() - 1);
    }
    if (selected_room_option_index_ >= devices_.size()) {
        selected_room_option_index_ = devices_.empty() ? 0 : (devices_.size() - 1);
    }
    if (room_draft_device_selected_.size() < devices_.size()) {
        room_draft_device_selected_.resize(devices_.size(), false);
    }
    if (active_page_ == PAGE_DEVICES) {
        // Heal a desynced pool card (supports_hue true but tree still color-temp)
        // without waiting for another topology change.
        if (!device_layout_changed) {
            for (size_t i = 0; i < devices_.size() && i < active_cards_.size(); ++i) {
                const auto *card = active_cards_[i];
                if (card != nullptr &&
                        card->template_id != device_card_template_id(devices_[i])) {
                    device_layout_changed = true;
                    break;
                }
            }
        }
        if (device_layout_changed) {
            setup_device_grid(ctx);
        } else {
            // Attribute reports are frequent. Update the existing card nodes
            // in place instead of destroying/re-parsing every card subtree.
            for (size_t i = 0; i < devices_.size(); ++i) {
                refresh_device_card(ctx, i);
            }
        }
    } else if (active_page_ == PAGE_ROOMS && (device_layout_changed || rooms_changed || force_refresh)) {
        setup_rooms_list(ctx);
    } else if (active_page_ == PAGE_NETWORK && (device_layout_changed || force_refresh)) {
        setup_network_page(ctx);
    } else if (active_page_ == PAGE_THREAD && (thread_map_changed || force_refresh)) {
        setup_thread_map(ctx);
    }
    if (room_modal_visible_ && (device_layout_changed || rooms_changed)) {
        setup_room_modal(ctx);
    }
    if (force_refresh || device_layout_changed) {
    }
    return true;
}

std::vector<std::pair<uint64_t, uint16_t>> MatterControllerApp::selected_room_device_ids() const
{
    std::vector<std::pair<uint64_t, uint16_t>> selected_ids;
    for (size_t i = 0; i < devices_.size() && i < room_draft_device_selected_.size(); ++i) {
        if (!room_draft_device_selected_[i]) {
            continue;
        }
        selected_ids.emplace_back(devices_[i].node_id, devices_[i].endpoint_id);
    }
    return selected_ids;
}

// --- Lifecycle ---

std::expected<void, std::string> MatterControllerApp::on_start(system::core::AppContext &ctx)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    boost::lock_guard<boost::recursive_mutex> lock(mutex_);

    auto locale_result = refresh_locale_if_needed(ctx, true);
    if (!locale_result) {
        return std::unexpected(locale_result.error());
    }
    auto backend_result = start_backend();
    if (!backend_result) {
        return backend_result;
    }

    auto device_path_handler = [this](const gui::Event & evt) {
        boost::lock_guard<boost::recursive_mutex> lock(mutex_);
        select_device_by_path(evt.path);
    };
    event_connections_.clear();
    event_connections_.emplace_back(ctx.gui().subscribe_action(ACT_TOGGLE_BASIC, device_path_handler));
    event_connections_.emplace_back(ctx.gui().subscribe_action(ACT_TOGGLE_LIGHT_DIMMABLE, device_path_handler));
    event_connections_.emplace_back(ctx.gui().subscribe_action(ACT_TOGGLE_LIGHT_COLOR_TEMP, device_path_handler));
    event_connections_.emplace_back(ctx.gui().subscribe_action(ACT_TOGGLE_LIGHT_EXTENDED, device_path_handler));
    event_connections_.emplace_back(ctx.gui().subscribe_action(ACT_REMOVE_BASIC, device_path_handler));
    event_connections_.emplace_back(ctx.gui().subscribe_action(ACT_REMOVE_LIGHT_DIMMABLE, device_path_handler));
    event_connections_.emplace_back(ctx.gui().subscribe_action(ACT_REMOVE_LIGHT_COLOR_TEMP, device_path_handler));
    event_connections_.emplace_back(ctx.gui().subscribe_action(ACT_REMOVE_LIGHT_EXTENDED, device_path_handler));
    auto subscribe_light_value = [this, &ctx](const char *action, std::optional<int> &pending,
                                                int minimum, int maximum) {
        event_connections_.emplace_back(ctx.gui().subscribe_action(action,
            [this, pending = &pending, minimum, maximum](const gui::Event & evt) {
                boost::lock_guard<boost::recursive_mutex> lock(mutex_);
                select_device_by_path(evt.path);
                if (auto value = evt.get_int64("value"); value.has_value()) {
                    *pending = clamp_int(static_cast<int>(*value), minimum, maximum);
                }
            }));
    };
    subscribe_light_value(ACT_LIGHT_DIMMABLE_BRIGHTNESS, pending_brightness_value_, 0, 100);
    subscribe_light_value(ACT_LIGHT_COLOR_TEMP_BRIGHTNESS, pending_brightness_value_, 0, 100);
    subscribe_light_value(ACT_LIGHT_COLOR_TEMP_CCT, pending_cct_value_, 27, 65);
    subscribe_light_value(ACT_LIGHT_EXTENDED_BRIGHTNESS, pending_brightness_value_, 0, 100);
    subscribe_light_value(ACT_LIGHT_EXTENDED_CCT, pending_cct_value_, 27, 65);
    subscribe_light_value(ACT_LIGHT_EXTENDED_HUE, pending_hue_value_, 0, 359);
    subscribe_light_value(ACT_LIGHT_EXTENDED_SATURATION, pending_saturation_value_, 0, 100);
    event_connections_.emplace_back(ctx.gui().subscribe_action(ACT_SETTINGS_BRIGHTNESS, [this](const gui::Event & evt) {
        boost::lock_guard<boost::recursive_mutex> lock(mutex_);
        if (auto value = evt.get_int64("value"); value.has_value()) {
            ui_brightness_ = clamp_int(static_cast<int>(*value), 20, 100);
        }
    }));
    event_connections_.emplace_back(ctx.gui().subscribe_action(ACT_ROOM_MODAL_TOGGLE_DEVICE, [this](const gui::Event & evt) {
        boost::lock_guard<boost::recursive_mutex> lock(mutex_);
        select_room_device_by_path(evt.path);
    }));
    // Subscribe to all other actions
    for (auto a : {
                ACT_DEVICES, ACT_ROOMS, ACT_THREAD, ACT_PAIRING, ACT_NETWORK, ACT_SETTINGS,
                ACT_TOGGLE_BASIC, ACT_TOGGLE_LIGHT_DIMMABLE, ACT_TOGGLE_LIGHT_COLOR_TEMP,
                ACT_TOGGLE_LIGHT_EXTENDED, ACT_REMOVE_BASIC, ACT_REMOVE_LIGHT_DIMMABLE,
                ACT_REMOVE_LIGHT_COLOR_TEMP, ACT_REMOVE_LIGHT_EXTENDED,
                ACT_LIGHT_DIMMABLE_BRIGHTNESS, ACT_LIGHT_COLOR_TEMP_BRIGHTNESS,
                ACT_LIGHT_COLOR_TEMP_CCT, ACT_LIGHT_EXTENDED_BRIGHTNESS, ACT_LIGHT_EXTENDED_CCT,
                ACT_LIGHT_EXTENDED_HUE, ACT_LIGHT_EXTENDED_SATURATION,
                ACT_ROOM_ADD, ACT_ROOM_TYPE_LIVING_ROOM, ACT_ROOM_TYPE_KITCHEN,
                ACT_ROOM_TYPE_STUDY, ACT_ROOM_TYPE_BEDROOM, ACT_ROOM_MODAL_TOGGLE_DEVICE,
                ACT_ROOM_MODAL_BACK, ACT_ROOM_MODAL_CANCEL, ACT_ROOM_MODAL_CONFIRM,
                ACT_NETWORK_REFRESH, ACT_SETTINGS_CHECK_UPDATE, ACT_SETTINGS_LANGUAGE,
                ACT_SETTINGS_BRIGHTNESS, ACT_SETTINGS_ABOUT, ACT_SETTINGS_RESET,
                ACT_MODAL_CANCEL, ACT_MODAL_CONFIRM
            }) {
        auto res = ctx.gui().subscribe_action(a);
        if (!res) {
            return res;
        }
    }

    // Show initial page
    auto page_result = show_page(ctx, PAGE_DEVICES);
    if (!page_result) {
        return page_result;
    }
    auto fr = flush_bindings(ctx);
    if (!fr) {
        return fr;
    }

    // Timers
    auto ct = ctx.timer().start_periodic(TIMER_CLOCK, 1000);
    if (!ct) {
        return std::unexpected(ct.error());
    }
    clock_timer_id_ = *ct;
    return {};
}

std::expected<void, std::string> MatterControllerApp::on_resume(system::core::AppContext &ctx)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    boost::lock_guard<boost::recursive_mutex> lock(mutex_);
    auto locale_result = refresh_locale_if_needed(ctx, false);
    if (!locale_result) {
        return std::unexpected(locale_result.error());
    }
    if (*locale_result) {
        refresh_visible_content(ctx);
        sync_backend_state(ctx, true);
        return flush_bindings(ctx);
    }
    if (sync_backend_state(ctx, true)) {
        return flush_bindings(ctx);
    }
    return {};
}

std::expected<void, std::string> MatterControllerApp::on_stop(system::core::AppContext &ctx)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    boost::lock_guard<boost::recursive_mutex> lock(mutex_);
    if (clock_timer_id_ != system::core::INVALID_TIMER_ID) {
        ctx.timer().stop(clock_timer_id_);
        clock_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    stop_backend();
    if (!active_page_.empty()) {
        release_page_resources(ctx, active_page_);
    }
    if (room_modal_visible_ || created_stage_views_.contains(TEMPLATE_ROOM_MODAL)) {
        hide_room_modal(ctx);
    }
    if (created_stage_views_.contains(TEMPLATE_MODAL)) {
        hide_modal(ctx);
    }
    if (pairing_qr_image_registered_) {
        if (!ctx.gui().unregister_image(PAIRING_QR_RUNTIME_IMAGE_ID)) {
            BROOKESIA_LOGW("Failed to unregister pairing QR image: %1%", PAIRING_QR_RUNTIME_IMAGE_ID);
        }
        pairing_qr_image_registered_ = false;
    }
    release_matter_pairing_qr_image();
    const std::vector<std::string> remaining_views(created_stage_views_.begin(), created_stage_views_.end());
    for (const auto &view_id : remaining_views) {
        destroy_stage_view(ctx, view_id);
    }
    // The Brookesia app object may remain alive briefly after on_stop(). Merely
    // calling clear() keeps vector/map/deque capacity allocated and leaves the
    // already fragmented PSRAM without a block large enough for the next JSON
    // resource read. Swap with empty containers so a second launch starts with
    // the maximum possible contiguous heap.
    release_container_memory(event_connections_);
    release_container_memory(binding_cache_);
    release_container_memory(pending_binding_indexes_);
    release_container_memory(pending_binding_updates_);
    release_container_memory(path_to_device_map_);
    release_container_memory(created_stage_views_);
    release_container_memory(localized_strings_);
    release_container_memory(pending_locale_updates_);
    release_container_memory(card_pool_);
    release_container_memory(active_cards_);
    release_container_memory(devices_);
    release_container_memory(rooms_);
    release_container_memory(room_row_paths_);
    release_container_memory(room_option_paths_);
    release_container_memory(room_option_path_to_device_map_);
    release_container_memory(room_draft_device_selected_);

    release_container_memory(current_locale_);
    release_container_memory(active_modal_title_key_);
    release_container_memory(active_modal_message_key_);
    release_container_memory(active_modal_confirm_key_);
    release_container_memory(active_modal_confirm_bg_);
    release_container_memory(active_page_);
    release_container_memory(active_stage_view_);
    release_container_memory(modal_action_);
    release_container_memory(room_draft_name_);
    release_container_memory(room_draft_error_key_);

    room_modal_visible_ = false;
    selected_device_index_ = 0;
    selected_room_option_index_ = 0;
    pending_brightness_value_.reset();
    pending_cct_value_.reset();
    pending_hue_value_.reset();
    pending_saturation_value_.reset();
    pending_remove_device_id_.reset();
    return {};
}

std::expected<void, std::string> MatterControllerApp::on_action(system::core::AppContext &ctx, std::string_view a)
{
    boost::lock_guard<boost::recursive_mutex> lock(mutex_);
    auto locale_result = refresh_locale_if_needed(ctx, false);
    if (!locale_result) {
        return std::unexpected(locale_result.error());
    }
    if (*locale_result) {
        refresh_visible_content(ctx);
    }

    if (a == ACT_DEVICES) {
        auto result = show_page(ctx, PAGE_DEVICES);
        if (!result) return result;
        return flush_bindings(ctx);
    }
    if (a == ACT_ROOMS) {
        auto result = show_page(ctx, PAGE_ROOMS);
        if (!result) return result;
        return flush_bindings(ctx);
    }
    if (a == ACT_THREAD) {
        auto result = show_page(ctx, PAGE_THREAD);
        if (!result) return result;
        return flush_bindings(ctx);
    }
    if (a == ACT_PAIRING) {
        auto result = show_page(ctx, PAGE_PAIRING);
        if (!result) return result;
        return flush_bindings(ctx);
    }
    if (a == ACT_NETWORK) {
        auto result = show_page(ctx, PAGE_NETWORK);
        if (!result) return result;
        return flush_bindings(ctx);
    }
    if (a == ACT_SETTINGS) {
        auto result = show_page(ctx, PAGE_SETTINGS);
        if (!result) return result;
        return flush_bindings(ctx);
    }
    if (a == ACT_TOGGLE_BASIC || a == ACT_TOGGLE_LIGHT_DIMMABLE ||
            a == ACT_TOGGLE_LIGHT_COLOR_TEMP || a == ACT_TOGGLE_LIGHT_EXTENDED) {
        if (backend_ && selected_device_index_ < devices_.size()) {
            const auto &device = devices_[selected_device_index_];
            auto result = backend_->toggle_device(device.node_id, device.endpoint_id);
            if (!result) {
                BROOKESIA_LOGW("Toggle device failed: %1%", result.error());
            }
            sync_backend_state(ctx, false);
        }
        return flush_bindings(ctx);
    }

    if (a == ACT_LIGHT_DIMMABLE_BRIGHTNESS || a == ACT_LIGHT_COLOR_TEMP_BRIGHTNESS ||
            a == ACT_LIGHT_EXTENDED_BRIGHTNESS) {
        if (backend_ && selected_device_index_ < devices_.size() &&
                devices_[selected_device_index_].supports_level) {
            auto &dev = devices_[selected_device_index_];
            const int brightness = pending_brightness_value_.value_or(dev.brightness);
            pending_brightness_value_.reset();
            auto result = backend_->set_brightness(dev.node_id, dev.endpoint_id, brightness);
            if (!result) {
                BROOKESIA_LOGW("Set brightness failed: %1%", result.error());
            } else {
                dev.brightness = brightness;
                dev.powered_on = true;
                dev.status_key = "status.bright";
                refresh_device_card(ctx, selected_device_index_);
            }
        }
        return flush_bindings(ctx);
    }

    if (a == ACT_LIGHT_COLOR_TEMP_CCT || a == ACT_LIGHT_EXTENDED_CCT) {
        if (backend_ && selected_device_index_ < devices_.size() &&
                devices_[selected_device_index_].supports_color_temp) {
            auto &dev = devices_[selected_device_index_];
            const int cct = pending_cct_value_.value_or(dev.cct);
            pending_cct_value_.reset();
            auto result = backend_->set_cct(dev.node_id, dev.endpoint_id, cct);
            if (!result) {
                BROOKESIA_LOGW("Set color temperature failed: %1%", result.error());
            } else {
                dev.cct = cct;
                dev.powered_on = true;
                dev.status_key = "status.cct";
                refresh_device_card(ctx, selected_device_index_);
            }
        }
        return flush_bindings(ctx);
    }

    if (a == ACT_LIGHT_EXTENDED_HUE) {
        if (backend_ && selected_device_index_ < devices_.size() &&
                devices_[selected_device_index_].supports_hue) {
            auto &dev = devices_[selected_device_index_];
            const int hue = clamp_int(pending_hue_value_.value_or(dev.hue), 0, 359);
            pending_hue_value_.reset();
            auto result = backend_->set_hue(dev.node_id, dev.endpoint_id, hue);
            if (!result) {
                BROOKESIA_LOGW("Set hue failed: %1%", result.error());
            } else {
                dev.hue = hue;
                dev.powered_on = true;
                dev.status_key = "status.hue";
                refresh_device_card(ctx, selected_device_index_);
            }
        }
        return flush_bindings(ctx);
    }

    if (a == ACT_LIGHT_EXTENDED_SATURATION) {
        if (backend_ && selected_device_index_ < devices_.size() &&
                devices_[selected_device_index_].supports_hue) {
            auto &dev = devices_[selected_device_index_];
            const int saturation = clamp_int(pending_saturation_value_.value_or(dev.saturation), 0, 100);
            pending_saturation_value_.reset();
            auto result = backend_->set_saturation(dev.node_id, dev.endpoint_id, saturation);
            if (!result) {
                BROOKESIA_LOGW("Set saturation failed: %1%", result.error());
            } else {
                dev.saturation = saturation;
                dev.powered_on = true;
                dev.status_key = "status.saturation";
                refresh_device_card(ctx, selected_device_index_);
            }
        }
        return flush_bindings(ctx);
    }

    if (a == ACT_ROOM_ADD) {
        show_room_modal(ctx);
        return flush_bindings(ctx);
    }

    if (a == ACT_ROOM_TYPE_LIVING_ROOM || a == ACT_ROOM_TYPE_KITCHEN ||
            a == ACT_ROOM_TYPE_STUDY || a == ACT_ROOM_TYPE_BEDROOM) {
        if (a == ACT_ROOM_TYPE_LIVING_ROOM) {
            selected_room_type_index_ = 0;
        } else if (a == ACT_ROOM_TYPE_KITCHEN) {
            selected_room_type_index_ = 1;
        } else if (a == ACT_ROOM_TYPE_STUDY) {
            selected_room_type_index_ = 2;
        } else {
            selected_room_type_index_ = 3;
        }
        room_draft_name_ = ROOM_TYPE_KEYS[selected_room_type_index_];
        room_draft_error_key_.clear();
        apply_room_type_options(ctx);
        const std::string error_path = STAGE_PATH + std::string("/room_modal/room_modal_box/room_modal_error");
        set_binding(ctx, error_path, KEY_TEXT, "");
        set_binding(ctx, error_path, KEY_HIDDEN, "true");
        return flush_bindings(ctx);
    }

    if (a == ACT_ROOM_MODAL_TOGGLE_DEVICE) {
        if (selected_room_option_index_ < room_draft_device_selected_.size()) {
            room_draft_device_selected_[selected_room_option_index_] =
                !room_draft_device_selected_[selected_room_option_index_];
            apply_room_device_option(ctx, selected_room_option_index_);
            room_draft_error_key_.clear();
            const std::string error_path = STAGE_PATH + std::string("/room_modal/room_modal_box/room_modal_error");
            set_binding(ctx, error_path, KEY_TEXT, "");
            set_binding(ctx, error_path, KEY_HIDDEN, "true");
        }
        return flush_bindings(ctx);
    }

    if (a == ACT_ROOM_MODAL_BACK || a == ACT_ROOM_MODAL_CANCEL) {
        hide_room_modal(ctx);
        return flush_bindings(ctx);
    }

    if (a == ACT_ROOM_MODAL_CONFIRM) {
        const auto room_name = trim_copy(room_draft_name_);
        const bool has_selected_devices = std::any_of(
        room_draft_device_selected_.begin(), room_draft_device_selected_.end(), [](bool selected) {
            return selected;
        }
                                          );
        const bool room_exists = std::any_of(
        rooms_.begin(), rooms_.end(), [&](const ControllerRoom & room) {
            return room.name_key == room_name || localized_room_name(room.name_key) == room_name;
        }
                                 );

        const std::string error_path = STAGE_PATH + std::string("/room_modal/room_modal_box/room_modal_error");
        if (room_name.empty()) {
            room_draft_error_key_ = "error.room_type_required";
            set_binding(ctx, error_path, KEY_TEXT, tr(room_draft_error_key_));
            set_binding(ctx, error_path, KEY_HIDDEN, "false");
            return flush_bindings(ctx);
        }
        if (room_exists) {
            room_draft_error_key_ = "error.room_exists";
            set_binding(ctx, error_path, KEY_TEXT, tr(room_draft_error_key_));
            set_binding(ctx, error_path, KEY_HIDDEN, "false");
            return flush_bindings(ctx);
        }
        if (!has_selected_devices) {
            room_draft_error_key_ = "error.room_select_device";
            set_binding(ctx, error_path, KEY_TEXT, tr(room_draft_error_key_));
            set_binding(ctx, error_path, KEY_HIDDEN, "false");
            return flush_bindings(ctx);
        }

        room_draft_error_key_.clear();
        hide_room_modal(ctx);
        if (backend_) {
            auto result = backend_->assign_room(room_name, selected_room_device_ids());
            if (!result) {
                BROOKESIA_LOGW("Assign room failed: %1%", result.error());
            }
            sync_backend_state(ctx, false);
        }
        return flush_bindings(ctx);
    }

    if (a == ACT_REMOVE_BASIC || a == ACT_REMOVE_LIGHT_DIMMABLE ||
            a == ACT_REMOVE_LIGHT_COLOR_TEMP || a == ACT_REMOVE_LIGHT_EXTENDED) {
        if (selected_device_index_ >= devices_.size()) {
            BROOKESIA_LOGW("Ignore Remove action without a selected device");
            return flush_bindings(ctx);
        }
        const auto &device = devices_[selected_device_index_];
        pending_remove_device_id_ = std::make_pair(device.node_id, device.endpoint_id);
        show_modal(ctx, "modal.remove_device.title", "modal.remove_device.message",
                   "modal.confirm_remove", "#FF6B6B");
        modal_action_ = "remove_device";
        return flush_bindings(ctx);
    }

    if (a == ACT_NETWORK_REFRESH) {
        setup_network_page(ctx);
        show_modal(ctx, "modal.network_refreshed.title", "modal.network_refreshed.message", "modal.close", "#6C63FF");
        modal_action_.clear();
        return flush_bindings(ctx);
    }

    if (a == ACT_SETTINGS_LANGUAGE) {
        const auto target = current_locale_ == LOCALE_ZH_CN ? LOCALE_EN : LOCALE_ZH_CN;
        auto language_result = ctx.gui().set_language(target, false);
        if (!language_result) {
            return std::unexpected(language_result.error());
        }
        auto refresh_result = refresh_locale_if_needed(ctx, true);
        if (!refresh_result) {
            return std::unexpected(refresh_result.error());
        }
        refresh_visible_content(ctx);
        setup_settings_page(ctx);
        return flush_bindings(ctx);
    }

    if (a == ACT_SETTINGS_BRIGHTNESS) {
        setup_settings_page(ctx);
        return flush_bindings(ctx);
    }

    if (a == ACT_SETTINGS_ABOUT) {
        show_modal(ctx, "modal.about.title", "modal.about.message", "modal.close", "#6C63FF");
        modal_action_.clear();
        return flush_bindings(ctx);
    }

    if (a == ACT_SETTINGS_CHECK_UPDATE) {
        show_modal(ctx, "modal.firmware_current.title", "modal.firmware_current.message", "modal.close", "#6C63FF");
        modal_action_.clear();
        return flush_bindings(ctx);
    }

    if (a == ACT_SETTINGS_RESET) {
        show_modal(
            ctx,
            "modal.factory_reset.title",
            "modal.factory_reset.message",
            "modal.confirm_erase",
            "#FF6B6B"
        );
        modal_action_ = "reset";
        return flush_bindings(ctx);
    }

    if (a == ACT_MODAL_CANCEL) {
        modal_action_.clear();
        pending_remove_device_id_.reset();
        hide_modal(ctx);
        return flush_bindings(ctx);
    }

    if (a == ACT_MODAL_CONFIRM) {
        const std::string confirmed_action = std::move(modal_action_);
        const auto remove_device_id = pending_remove_device_id_;
        modal_action_.clear();
        pending_remove_device_id_.reset();
        hide_modal(ctx);
        if (confirmed_action == "reset") {
            auto result = request_matter_factory_reset();
            if (!result) {
                BROOKESIA_LOGW("Matter factory reset failed: %1%", result.error());
                if (backend_) {
                    auto backend_result = backend_->reset();
                    if (!backend_result) {
                        BROOKESIA_LOGW("Reset backend fallback failed: %1%", backend_result.error());
                    }
                    sync_backend_state(ctx, false);
                }
            }
        } else if (confirmed_action == "remove_device") {
            if (!remove_device_id || !backend_) {
                BROOKESIA_LOGW("Remove Matter device request has no valid target/backend");
            } else {
                auto result = backend_->remove_device(remove_device_id->first, remove_device_id->second);
                if (!result) {
                    BROOKESIA_LOGW("Remove Matter device failed: %1%", result.error());
                    show_modal(ctx, "modal.remove_device_failed.title",
                               "modal.remove_device_failed.message", "modal.close", "#FF6B6B");
                } else {
                    sync_backend_state(ctx, false);
                }
            }
        }
        return flush_bindings(ctx);
    }

    if (*locale_result) {
        return flush_bindings(ctx);
    }
    return {};
}

std::expected<void, std::string> MatterControllerApp::on_timer(
    system::core::AppContext &ctx, system::core::TimerId, std::string_view name)
{
    boost::lock_guard<boost::recursive_mutex> lock(mutex_);
    auto locale_result = refresh_locale_if_needed(ctx, false);
    if (!locale_result) {
        return std::unexpected(locale_result.error());
    }
    if (*locale_result) {
        refresh_visible_content(ctx);
    }
    sync_backend_state(ctx, false);
    if (active_page_ == PAGE_PAIRING) {
        setup_pairing_page(ctx);
    }

    if (name == TIMER_CLOCK) {
        std::time_t now = std::time(nullptr);
        std::tm *local = std::localtime(&now);
        char t[8], d[32];
        std::snprintf(t, sizeof(t), "%02d:%02d", local->tm_hour, local->tm_min);
        std::snprintf(d, sizeof(d), "%d/%d/%d", local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
        set_binding(ctx, HEADER_TIME_PATH, KEY_TEXT, std::string(t));
        set_binding(ctx, HEADER_DATE_PATH, KEY_TEXT, std::string(d));
        return flush_bindings(ctx);
    }
    if (*locale_result) {
        return flush_bindings(ctx);
    }
    return {};
}

// --- Provider ---

system::core::AppManifest MatterControllerAppProvider::get_manifest() const
{
    return MatterControllerApp().get_manifest();
}

std::shared_ptr<system::core::IApp> MatterControllerAppProvider::create_app()
{
    return std::make_shared<MatterControllerApp>();
}

BROOKESIA_SYSTEM_CORE_APP_PROVIDER_REGISTER_WITH_SYMBOL(
    MatterControllerAppProvider,
    "brookesia.app.matter_controller",
    app_matter_controller_provider_symbol
);

} // namespace esp_brookesia::app::matter_controller
