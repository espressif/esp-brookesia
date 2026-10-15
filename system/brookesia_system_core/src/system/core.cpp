/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string>

#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/system/impl.hpp"

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "boost/thread/lock_guard.hpp"
#include "boost/thread/lock_types.hpp"
#include "boost/thread/mutex.hpp"

#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include "super_shell_gsp.h"
#include "brookesia/gui_gsp/app_host.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_helper/network/sntp.hpp"
#include "esp_gsp.h"
#endif

namespace esp_brookesia::system::core {

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP

extern "C" {
    extern const uint8_t settings_dynamic_font_start[] __attribute__((weak));
    extern const uint8_t settings_dynamic_font_end[] __attribute__((weak));
}

esp_gsp_config_t super_shell_dark_config();

namespace {

constexpr const char *GSP_SHELL_APP_ID = "brookesia.system.super.shell";
constexpr const char *GSP_CLOCK_TIMER_NAME = "gsp.shell.status.clock";
constexpr size_t GSP_LAUNCHER_SLOT_COUNT = 6;
constexpr size_t MAX_KEYBOARD_TEXT_BYTES = 64;

enum class GspKeyboardLayout : uint8_t {
    Lower,
    Upper,
    Symbols,
    Number,
};

constexpr std::array<unsigned, 36> KEYBOARD_KEY_ACTIONS = {
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_0_0,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_0_1,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_0_2,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_0_3,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_0_4,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_0_5,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_0_6,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_0_7,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_0_8,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_0_9,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_1_0,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_1_1,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_1_2,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_1_3,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_1_4,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_1_5,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_1_6,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_1_7,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_1_8,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_1_9,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_2_0,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_2_1,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_2_2,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_2_3,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_2_4,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_2_5,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_2_6,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_2_7,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_2_8,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_3_0,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_3_1,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_3_2,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_3_3,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_3_4,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_3_5,
    GSP_SUPER_KEYBOARD_ACT_ID_KEY_3_6,
};

constexpr std::array<uint32_t, 36> KEYBOARD_KEY_OBJECTS = {
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_0,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_1,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_2,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_3,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_4,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_5,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_6,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_7,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_8,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_9,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_0,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_1,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_2,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_3,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_4,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_5,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_6,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_7,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_8,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_9,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_0,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_1,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_2,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_3,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_4,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_5,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_6,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_7,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_8,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_0,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_1,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_2,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_3,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_4,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_5,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_6,
};

constexpr std::array<uint32_t, 36> KEYBOARD_KEY_VISIBILITY_OBJECTS = {
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_0_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_1_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_2_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_3_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_4_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_5_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_6_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_7_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_8_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_0_9_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_0_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_1_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_2_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_3_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_4_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_5_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_6_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_7_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_8_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_1_9_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_0_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_1_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_2_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_3_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_4_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_5_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_6_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_7_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_2_8_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_0_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_1_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_2_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_3_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_4_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_5_GROUP,
    GSP_SUPER_KEYBOARD_OBJ_KEY_KEY_3_6_GROUP,
};

constexpr std::array<std::string_view, 4> LOWER_ROWS = {
    "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm",
};
constexpr std::array<std::string_view, 4> SYMBOL_ROWS = {
    "!@#$%^&*()", "-_=+[]{}\\|", ";:'\",.<>?", "~`/+=_-",
};

std::optional<size_t> find_keyboard_key(unsigned id)
{
    const auto found = std::find(KEYBOARD_KEY_ACTIONS.begin(), KEYBOARD_KEY_ACTIONS.end(), id);
    if (found == KEYBOARD_KEY_ACTIONS.end()) {
        return std::nullopt;
    }
    return static_cast<size_t>(found - KEYBOARD_KEY_ACTIONS.begin());
}

char keyboard_key_character(size_t index, bool upper, bool symbols)
{
    const auto &rows = symbols ? SYMBOL_ROWS : LOWER_ROWS;
    for (const auto row : rows) {
        if (index < row.size()) {
            char value = row[index];
            if (upper && value >= 'a' && value <= 'z') {
                value = static_cast<char>(value - 'a' + 'A');
            }
            return value;
        }
        index -= row.size();
    }
    return '\0';
}

bool is_printable_ascii(std::string_view text)
{
    return std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return ch >= 0x20 && ch <= 0x7e;
    });
}

bool is_supported_keyboard_mode(std::string_view mode)
{
    return mode == "text" || mode == "upper" || mode == "number" || mode == "special";
}

std::expected<KeyboardRequestOptions, std::string> normalize_keyboard_options(KeyboardRequestOptions options)
{
    if (!is_supported_keyboard_mode(options.mode)) {
        return std::unexpected("Keyboard mode must be one of: text, upper, number, special");
    }
    if (options.allowed_modes.empty()) {
        options.allowed_modes = {"text", "upper", "number", "special"};
    }
    for (const auto &mode : options.allowed_modes) {
        if (!is_supported_keyboard_mode(mode)) {
            return std::unexpected("Keyboard allowed mode must be one of: text, upper, number, special");
        }
        if (std::count(options.allowed_modes.begin(), options.allowed_modes.end(), mode) > 1) {
            return std::unexpected("Keyboard allowed modes must not contain duplicates");
        }
    }
    if (std::find(options.allowed_modes.begin(), options.allowed_modes.end(), options.mode) ==
            options.allowed_modes.end()) {
        options.mode = options.allowed_modes.front();
    }
    return options;
}

std::string gsp_app_state_text(AppState state, bool chinese)
{
    if (!chinese) {
        return BROOKESIA_DESCRIBE_TO_STR(state);
    }
    switch (state) {
    case AppState::Installed: return "已安装";
    case AppState::Starting: return "启动中";
    case AppState::Running: return "运行中";
    case AppState::Paused: return "已暂停";
    case AppState::Stopping: return "停止中";
    case AppState::Stopped: return "已停止";
    case AppState::Error: return "错误";
    }
    return "未知";
}

std::string make_gsp_clock_text()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#if defined(_WIN32)
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif
    char buffer[8] = {};
    if (std::strftime(buffer, sizeof(buffer), "%H:%M", &local_time) == 0) {
        return "--:--";
    }
    return buffer;
}

std::string dialog_badge_text(MessageDialogIcon icon, bool chinese)
{
    switch (icon) {
    case MessageDialogIcon::Information:
        return chinese ? "提示" : "Information";
    case MessageDialogIcon::Question:
        return chinese ? "确认" : "Confirmation";
    case MessageDialogIcon::Warning:
        return chinese ? "警告" : "Warning";
    case MessageDialogIcon::Critical:
        return chinese ? "错误" : "Error";
    case MessageDialogIcon::None:
    default:
        return {};
    }
}

std::expected<void, std::string> apply_launcher_static_labels(esp_gsp_handle_t handle, bool chinese)
{
    if (handle == nullptr) {
        return std::unexpected("Launcher GSP handle is null");
    }
    if (esp_gsp_component_set_text(handle, GSP_SUPER_LAUNCHER_OBJ_KEY_TITLE, "ESP-Brookesia") != ESP_GSP_OK ||
            esp_gsp_component_set_text(
                handle,
                GSP_SUPER_LAUNCHER_OBJ_KEY_SUBTITLE,
                chinese ? "系统主页" : "System Home"
            ) != ESP_GSP_OK ||
            esp_gsp_component_set_text(
                handle,
                GSP_SUPER_LAUNCHER_OBJ_KEY_OPEN_SETTINGS,
                chinese ? "设置" : "Settings"
            ) != ESP_GSP_OK ||
            esp_gsp_component_set_text(
                handle,
                GSP_SUPER_LAUNCHER_OBJ_KEY_INSTALLED_CAPTION,
                chinese ? "全部应用" : "Applications"
            ) != ESP_GSP_OK ||
            esp_gsp_component_set_text(
                handle,
                GSP_SUPER_LAUNCHER_OBJ_KEY_PREV_PAGE,
                chinese ? "上一页" : "Previous"
            ) != ESP_GSP_OK ||
            esp_gsp_component_set_text(
                handle,
                GSP_SUPER_LAUNCHER_OBJ_KEY_NEXT_PAGE,
                chinese ? "下一页" : "Next"
            ) != ESP_GSP_OK) {
        return std::unexpected("Failed to apply GSP launcher static labels");
    }
    return {};
}

std::expected<void, std::string> apply_keyboard_static_labels(esp_gsp_handle_t handle, bool chinese)
{
    if (handle == nullptr) {
        return std::unexpected("Keyboard GSP handle is null");
    }
    if (esp_gsp_component_set_text(
                handle, GSP_SUPER_KEYBOARD_OBJ_KEY_DELETE, chinese ? "删除" : "Delete"
            ) != ESP_GSP_OK ||
            esp_gsp_component_set_text(
                handle, GSP_SUPER_KEYBOARD_OBJ_KEY_SPACE, chinese ? "空格" : "Space"
            ) != ESP_GSP_OK ||
            esp_gsp_component_set_text(
                handle, GSP_SUPER_KEYBOARD_OBJ_KEY_CANCEL, chinese ? "取消" : "Cancel"
            ) != ESP_GSP_OK ||
            esp_gsp_component_set_text(
                handle, GSP_SUPER_KEYBOARD_OBJ_KEY_CONFIRM, chinese ? "完成" : "Done"
            ) != ESP_GSP_OK) {
        return std::unexpected("Failed to apply GSP keyboard static labels");
    }
    return {};
}

std::expected<void, std::string> render_keyboard_preview(
    esp_gsp_handle_t handle,
    std::string_view text,
    std::string_view placeholder,
    size_t max_length,
    bool password
)
{
    const std::string preview = text.empty() ?
                                std::string(placeholder) :
                                (password ? std::string(text.size(), '*') : std::string(text));
    if (esp_gsp_component_set_text(handle, GSP_SUPER_KEYBOARD_OBJ_KEY_MASKED_PASSWORD, preview.c_str()) !=
            ESP_GSP_OK) {
        return std::unexpected("Failed to update GSP keyboard text preview");
    }
    const auto count = std::to_string(text.size()) + " / " + std::to_string(max_length);
    if (esp_gsp_component_set_text(handle, GSP_SUPER_KEYBOARD_OBJ_KEY_PASSWORD_COUNT, count.c_str()) !=
            ESP_GSP_OK) {
        return std::unexpected("Failed to update GSP keyboard length indicator");
    }
    return {};
}

std::expected<void, std::string> render_keyboard_keys(
    esp_gsp_handle_t handle,
    GspKeyboardLayout layout,
    bool allow_lower,
    bool allow_upper,
    bool allow_symbols,
    bool allow_number,
    bool chinese
)
{
    const bool number_only = !allow_lower && !allow_upper && !allow_symbols && allow_number;
    const bool upper = layout == GspKeyboardLayout::Upper;
    const bool symbols = layout == GspKeyboardLayout::Symbols;
    for (size_t index = 0; index < KEYBOARD_KEY_OBJECTS.size(); ++index) {
        const char value = keyboard_key_character(index, upper, symbols);
        if (value == '\0') {
            return std::unexpected("GSP keyboard key map is invalid");
        }
        const bool key_enabled = !number_only || (index < 10);
        const char label[] = {key_enabled ? value : ' ', '\0'};
        if (esp_gsp_component_set_text(handle, KEYBOARD_KEY_OBJECTS[index], label) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(handle, KEYBOARD_KEY_OBJECTS[index], key_enabled) != ESP_GSP_OK ||
                esp_gsp_component_set_visible(
                    handle, KEYBOARD_KEY_VISIBILITY_OBJECTS[index], key_enabled
                ) != ESP_GSP_OK) {
            return std::unexpected("Failed to update GSP keyboard keys");
        }
    }
    const char *shift_label = upper ? (chinese ? "小写" : "Lower") :
                              symbols ? (chinese ? "字母" : "Letters") :
                              (chinese ? "大写" : "Shift");
    const char *symbols_label = symbols ? (chinese ? "字母" : "Letters") : (chinese ? "符号" : "Symbols");
    const bool shift_enabled = !number_only && allow_lower && allow_upper && !symbols;
    const bool symbols_enabled = !number_only && allow_symbols && (allow_lower || allow_upper);
    if (esp_gsp_component_set_text(handle, GSP_SUPER_KEYBOARD_OBJ_KEY_SHIFT, shift_label) != ESP_GSP_OK ||
            esp_gsp_component_set_text(handle, GSP_SUPER_KEYBOARD_OBJ_KEY_SYMBOLS, symbols_label) != ESP_GSP_OK ||
            esp_gsp_component_set_enabled(handle, GSP_SUPER_KEYBOARD_OBJ_KEY_SHIFT, shift_enabled) != ESP_GSP_OK ||
            esp_gsp_component_set_enabled(handle, GSP_SUPER_KEYBOARD_OBJ_KEY_SYMBOLS, symbols_enabled) != ESP_GSP_OK ||
            esp_gsp_component_set_enabled(handle, GSP_SUPER_KEYBOARD_OBJ_KEY_SPACE, !number_only) != ESP_GSP_OK ||
            esp_gsp_component_set_visible(handle, GSP_SUPER_KEYBOARD_OBJ_KEY_SHIFT_GROUP, !number_only) != ESP_GSP_OK ||
            esp_gsp_component_set_visible(
                handle, GSP_SUPER_KEYBOARD_OBJ_KEY_SYMBOLS_GROUP, !number_only
            ) != ESP_GSP_OK ||
            esp_gsp_component_set_visible(handle, GSP_SUPER_KEYBOARD_OBJ_KEY_SPACE_GROUP, !number_only) != ESP_GSP_OK) {
        return std::unexpected("Failed to update GSP keyboard mode controls");
    }
    return {};
}

std::expected<void, std::string> render_dialog_scene(
    esp_gsp_handle_t handle,
    const MessageDialogOptions &options,
    bool chinese
)
{
    if (esp_gsp_goto_scene(handle, GSP_SUPER_SHELL_SCENE_SUPER_DIALOG, ESP_GSP_NO_TRANSITION) != ESP_GSP_OK ||
            esp_gsp_flush(handle, 500) != ESP_GSP_OK) {
        return std::unexpected("Failed to show GSP message dialog scene");
    }
    const auto badge = dialog_badge_text(options.icon, chinese);
    const bool has_badge = !badge.empty();
    const bool has_info = !options.informative_text.empty();
    const std::string title_text = options.text.empty() ? (chinese ? "提示" : "Notice") : options.text;
    const std::string info_text = has_info ? options.informative_text : std::string(" ");
    if (esp_gsp_component_set_text(
                handle, GSP_SUPER_DIALOG_OBJ_KEY_DIALOG_BADGE, has_badge ? badge.c_str() : " "
            ) != ESP_GSP_OK ||
            esp_gsp_component_set_visible(handle, GSP_SUPER_DIALOG_OBJ_KEY_DIALOG_BADGE, has_badge) != ESP_GSP_OK ||
            esp_gsp_component_set_text(
                handle, GSP_SUPER_DIALOG_OBJ_KEY_DIALOG_TEXT, title_text.c_str()
            ) != ESP_GSP_OK ||
            esp_gsp_component_set_text(
                handle, GSP_SUPER_DIALOG_OBJ_KEY_DIALOG_INFO, info_text.c_str()
            ) != ESP_GSP_OK ||
            esp_gsp_component_set_visible(handle, GSP_SUPER_DIALOG_OBJ_KEY_DIALOG_INFO, has_info) != ESP_GSP_OK) {
        return std::unexpected("Failed to update GSP message dialog labels");
    }

    static constexpr std::array<uint32_t, 3> BUTTON_KEYS = {
        GSP_SUPER_DIALOG_OBJ_KEY_BUTTON_0,
        GSP_SUPER_DIALOG_OBJ_KEY_BUTTON_1,
        GSP_SUPER_DIALOG_OBJ_KEY_BUTTON_2,
    };
    static constexpr std::array<uint32_t, 3> BUTTON_VISIBILITY_KEYS = {
        GSP_SUPER_DIALOG_OBJ_KEY_BUTTON_0_GROUP,
        GSP_SUPER_DIALOG_OBJ_KEY_BUTTON_1_GROUP,
        GSP_SUPER_DIALOG_OBJ_KEY_BUTTON_2_GROUP,
    };
    for (size_t i = 0; i < BUTTON_KEYS.size(); ++i) {
        const bool visible = i < options.buttons.size();
        const std::string label = visible ? options.buttons[i].text : std::string(" ");
        if (esp_gsp_component_set_text(handle, BUTTON_KEYS[i], label.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_visible(handle, BUTTON_VISIBILITY_KEYS[i], visible) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(handle, BUTTON_KEYS[i], visible) != ESP_GSP_OK) {
            return std::unexpected("Failed to update GSP message dialog buttons");
        }
    }
    return {};
}

} // namespace

class GspSystemShellApp final: public IApp {
public:
    struct ModalCallbackGate {
        boost::mutex mutex;
        GspSystemShellApp *shell = nullptr;
        std::atomic<uint64_t> epoch{0};
    };

    explicit GspSystemShellApp(System &owner)
        : owner_(owner)
    {}

    ~GspSystemShellApp() override
    {
        disconnect_sntp_events();
        detach_modal_callbacks();
    }

    AppManifest get_manifest() const override
    {
        return {
            .id = GSP_SHELL_APP_ID,
            .name = "Shell",
            .localized_names = {
                {"en", "Shell"},
                {"zh_CN", "桌面"},
            },
            .version = "0.1.0",
            .kind = AppKind::Native,
            .visible = false,
            .icon_id = "super",
            .supported_systems = {},
            .icon_path = "",
            .runtime_type = runtime::BackendType::Unknown,
            .app_path = "",
            .entry = "",
            .resource_dir = "",
            .arguments = {},
        };
    }

    AppGuiDescriptor get_gui_descriptor() const override
    {
        return {
            .root_kind = GuiRootKind::GspBundle,
            .root = {},
            .resources = {},
            .screen_flows = {},
        };
    }

    std::span<const std::byte> gui_bundle() const override
    {
        const auto config = *static_cast<const esp_gsp_config_t *>(gui_runtime_config());
        return {reinterpret_cast<const std::byte *>(config.bundle), config.bundle_size};
    }

    const void *gui_runtime_config() const override
    {
        static esp_gsp_config_t config;
        const bool dark = owner_.get_environment().theme_id == "dark";
        config = dark ? super_shell_dark_config() : gsp_super_shell_config();
        const uint8_t *font_start = +settings_dynamic_font_start;
        const uint8_t *font_end = +settings_dynamic_font_end;
        if ((font_start != nullptr) && (font_end != nullptr) && (font_end > font_start)) {
            config.ttf = font_start;
            config.ttf_size = static_cast<size_t>(font_end - font_start);
        }
        return &config;
    }

    std::expected<void, std::string> on_start(AppContext &context) override
    {
        context_ = &context;
        auto *host = gui::gsp::AppHost::current();
        if (!host || !host->handle()) {
            context_ = nullptr;
            return std::unexpected("Shell GSP session is unavailable");
        }
        const auto handle = host->handle();
        (void)esp_gsp_set_swipe_enabled(handle, false);
        active_scene_ = GSP_SUPER_SHELL_SCENE_SUPER_LAUNCHER;
        auto labels = apply_launcher_static_labels(handle, is_chinese());
        if (!labels) {
            context_ = nullptr;
            return labels;
        }
        (void)esp_gsp_component_set_text(
            handle, GSP_SUPER_LAUNCHER_OBJ_KEY_STATUS_CLOCK, make_gsp_clock_text().c_str()
        );
        schedule_clock_timer();
        subscribe_sntp_events();
        auto refreshed = refresh_launcher();
        if (!refreshed) {
            stop_clock_timer();
            disconnect_sntp_events();
            context_ = nullptr;
            return refreshed;
        }
        return {};
    }

    std::expected<void, std::string> on_stop(AppContext &) override
    {
        stop_clock_timer();
        disconnect_sntp_events();
        context_ = nullptr;
        return {};
    }

    std::expected<void, std::string> on_pause(AppContext &) override
    {
        return {};
    }

    std::expected<void, std::string> on_resume(AppContext &) override
    {
        return refresh_launcher();
    }

    std::expected<void, std::string> on_timer(
        AppContext &,
        TimerId timer_id,
        std::string_view name
    ) override
    {
        if (name == GSP_CLOCK_TIMER_NAME && timer_id == clock_timer_id_) {
            clock_timer_id_ = INVALID_TIMER_ID;
            if (context_ != nullptr && active_scene_ == GSP_SUPER_SHELL_SCENE_SUPER_LAUNCHER) {
                if (auto *host = gui::gsp::AppHost::current(); host && host->handle()) {
                    (void)esp_gsp_component_set_text(
                        host->handle(),
                        GSP_SUPER_LAUNCHER_OBJ_KEY_STATUS_CLOCK,
                        make_gsp_clock_text().c_str()
                    );
                }
            }
            schedule_clock_timer();
        }
        return {};
    }

    std::expected<void, std::string> on_action(
        AppContext &,
        std::string_view action
    ) override
    {
        if (action.starts_with("scene:")) {
            return {};
        }
        unsigned scene = 0;
        unsigned id = 0;
        unsigned arg = 0;
        const std::string text(action);
        if (std::sscanf(text.c_str(), "%u:%u:%u", &scene, &id, &arg) != 3) {
            return std::unexpected("Unrecognized GSP shell action");
        }
        (void)arg;

        if (scene == GSP_SUPER_SHELL_SCENE_SUPER_DIALOG) {
            if (dialog_request_id_ == INVALID_MESSAGE_DIALOG_REQUEST_ID) {
                return {};
            }
            int32_t button_index = -1;
            if (id == GSP_SUPER_DIALOG_ACT_ID_BUTTON_0) {
                button_index = 0;
            } else if (id == GSP_SUPER_DIALOG_ACT_ID_BUTTON_1) {
                button_index = 1;
            } else if (id == GSP_SUPER_DIALOG_ACT_ID_BUTTON_2) {
                button_index = 2;
            }
            if (button_index < 0 || static_cast<size_t>(button_index) >= dialog_options_.buttons.size()) {
                return {};
            }
            if (dialog_timeout_task_id_ != 0) {
                owner_.cancel_system_app_delayed_task(dialog_timeout_task_id_);
                dialog_timeout_task_id_ = 0;
            }
            ++dialog_timeout_generation_;
            const auto app_id = dialog_owner_;
            const auto request_id = dialog_request_id_;
            dialog_owner_ = INVALID_APP_ID;
            dialog_request_id_ = INVALID_MESSAGE_DIALOG_REQUEST_ID;
            dialog_options_ = {};
            if (!owner_.has_queued_message_dialogs()) {
                handle_message_dialog_idle();
            }
            auto completed = owner_.complete_app_message_dialog(
                                 app_id, request_id, button_index, MessageDialogCloseReason::Button
                             );
            if (!completed) {
                BROOKESIA_LOGW("Failed to complete GSP message dialog: %1%", completed.error());
            }
            return {};
        }

        if (scene == GSP_SUPER_SHELL_SCENE_SUPER_KEYBOARD) {
            if (keyboard_request_id_ == INVALID_KEYBOARD_REQUEST_ID) {
                return {};
            }
            const auto finish_keyboard = [this](bool confirmed) {
                const auto app_id = keyboard_owner_;
                const auto request_id = keyboard_request_id_;
                std::string confirmed_text = confirmed ? keyboard_text_ : std::string();
                hide_keyboard(app_id, request_id);
                auto completed = owner_.complete_app_keyboard(
                                     app_id, request_id, confirmed, std::move(confirmed_text)
                                 );
                if (!completed) {
                    BROOKESIA_LOGW("Failed to complete GSP keyboard request: %1%", completed.error());
                }
            };
            if (id == GSP_SUPER_KEYBOARD_ACT_ID_CANCEL || id == GSP_SUPER_KEYBOARD_ACT_ID_CONFIRM) {
                finish_keyboard(id == GSP_SUPER_KEYBOARD_ACT_ID_CONFIRM);
                return {};
            }
            esp_gsp_handle_t handle = active_handle();
            if (handle == nullptr) {
                finish_keyboard(false);
                return std::unexpected("GSP keyboard session is unavailable");
            }
            const bool chinese = is_chinese();
            if (id == GSP_SUPER_KEYBOARD_ACT_ID_DELETE) {
                keyboard_fresh_numeric_ = false;
                if (!keyboard_text_.empty()) {
                    keyboard_text_.pop_back();
                    (void)render_keyboard_preview(
                        handle, keyboard_text_, keyboard_placeholder_, keyboard_max_length_, keyboard_password_
                    );
                }
                return {};
            }
            if (id == GSP_SUPER_KEYBOARD_ACT_ID_SPACE) {
                const bool number_only = !allow_lower_ && !allow_upper_ && !allow_symbols_ && allow_number_;
                if (!number_only && keyboard_text_.size() < keyboard_max_length_) {
                    keyboard_fresh_numeric_ = false;
                    keyboard_text_.push_back(' ');
                    (void)render_keyboard_preview(
                        handle, keyboard_text_, keyboard_placeholder_, keyboard_max_length_, keyboard_password_
                    );
                }
                return {};
            }
            if (id == GSP_SUPER_KEYBOARD_ACT_ID_SHIFT) {
                if (allow_lower_ && allow_upper_ && keyboard_layout_ != GspKeyboardLayout::Symbols) {
                    keyboard_layout_ = (keyboard_layout_ == GspKeyboardLayout::Upper) ?
                                       GspKeyboardLayout::Lower :
                                       GspKeyboardLayout::Upper;
                    (void)render_keyboard_keys(
                        handle, keyboard_layout_, allow_lower_, allow_upper_, allow_symbols_, allow_number_, chinese
                    );
                }
                return {};
            }
            if (id == GSP_SUPER_KEYBOARD_ACT_ID_SYMBOLS) {
                if (allow_symbols_ && (allow_lower_ || allow_upper_)) {
                    keyboard_layout_ = (keyboard_layout_ == GspKeyboardLayout::Symbols) ?
                                       (allow_lower_ ? GspKeyboardLayout::Lower : GspKeyboardLayout::Upper) :
                                       GspKeyboardLayout::Symbols;
                    (void)render_keyboard_keys(
                        handle, keyboard_layout_, allow_lower_, allow_upper_, allow_symbols_, allow_number_, chinese
                    );
                }
                return {};
            }
            const auto key_index = find_keyboard_key(id);
            if (!key_index) {
                return {};
            }
            const bool number_only = !allow_lower_ && !allow_upper_ && !allow_symbols_ && allow_number_;
            if (number_only && *key_index >= 10) {
                return {};
            }
            if (keyboard_fresh_numeric_) {
                keyboard_text_.clear();
                keyboard_fresh_numeric_ = false;
            }
            if (keyboard_text_.size() >= keyboard_max_length_) {
                return {};
            }
            const char ch = keyboard_key_character(
                                *key_index,
                                keyboard_layout_ == GspKeyboardLayout::Upper,
                                keyboard_layout_ == GspKeyboardLayout::Symbols
                            );
            if (ch == '\0') {
                return {};
            }
            keyboard_text_.push_back(ch);
            (void)render_keyboard_preview(
                handle, keyboard_text_, keyboard_placeholder_, keyboard_max_length_, keyboard_password_
            );
            return {};
        }

        if (scene != GSP_SUPER_SHELL_SCENE_SUPER_LAUNCHER) {
            return {};
        }
        if (id == GSP_SUPER_LAUNCHER_ACT_ID_PREV_PAGE) {
            if (launcher_page_ > 0) {
                --launcher_page_;
                return refresh_launcher();
            }
            return {};
        }
        if (id == GSP_SUPER_LAUNCHER_ACT_ID_NEXT_PAGE) {
            ++launcher_page_;
            return refresh_launcher();
        }

        AppId target_app_id = INVALID_APP_ID;
        if (id == GSP_SUPER_LAUNCHER_ACT_ID_OPEN_SETTINGS) {
            target_app_id = settings_app_id_;
        } else if (id == GSP_SUPER_LAUNCHER_ACT_ID_OPEN_APP_0) {
            target_app_id = slot_apps_[0];
        } else if (id == GSP_SUPER_LAUNCHER_ACT_ID_OPEN_APP_1) {
            target_app_id = slot_apps_[1];
        } else if (id == GSP_SUPER_LAUNCHER_ACT_ID_OPEN_APP_2) {
            target_app_id = slot_apps_[2];
        } else if (id == GSP_SUPER_LAUNCHER_ACT_ID_OPEN_APP_3) {
            target_app_id = slot_apps_[3];
        } else if (id == GSP_SUPER_LAUNCHER_ACT_ID_OPEN_APP_4) {
            target_app_id = slot_apps_[4];
        } else if (id == GSP_SUPER_LAUNCHER_ACT_ID_OPEN_APP_5) {
            target_app_id = slot_apps_[5];
        }
        if (target_app_id == INVALID_APP_ID) {
            return {};
        }
        const auto started_at = std::chrono::steady_clock::now();
        auto started = owner_.start_app(target_app_id);
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - started_at
                                ).count();
        if (!started) {
            BROOKESIA_LOGE(
                "App open failed elapsed_ms(%1%) id(%2%): %3%",
                elapsed_ms, target_app_id, started.error()
            );
            return started;
        }
        BROOKESIA_LOGI("App open elapsed_ms(%1%) id(%2%)", elapsed_ms, target_app_id);
        return {};
    }

    std::expected<void, std::string> close_modals_for_app_switch()
    {
        auto stopped = stop_modal_session();
        if (!stopped) {
            return stopped;
        }
        closing_modals_ = true;
        lib_utils::FunctionGuard restore_closing([this]() {
            closing_modals_ = false;
        });
        // Close queued requests first so closing the active one cannot display another modal.
        auto request_ids = owner_.impl_->queued_message_dialog_requests_;
        if (owner_.impl_->active_message_dialog_request_id_) {
            request_ids.push_back(*owner_.impl_->active_message_dialog_request_id_);
        }
        for (const auto request_id : request_ids) {
            auto closed = owner_.close_message_dialog(
                              INVALID_APP_ID, request_id, -1, MessageDialogCloseReason::Closed, false, true
                          );
            if (!closed) {
                return closed;
            }
        }
        if (keyboard_request_id_ != INVALID_KEYBOARD_REQUEST_ID) {
            auto closed = owner_.hide_app_keyboard(keyboard_owner_, keyboard_request_id_);
            if (!closed) {
                return closed;
            }
        }
        suspended_app_id_ = INVALID_APP_ID;
        return {};
    }

    bool modal_session_running() const
    {
        return modal_host_ != nullptr && modal_host_->running();
    }

    void detach_callbacks()
    {
        disconnect_sntp_events();
        detach_modal_callbacks();
    }

    std::expected<void, std::string> refresh_launcher()
    {
        if (context_ == nullptr || active_scene_ != GSP_SUPER_SHELL_SCENE_SUPER_LAUNCHER) {
            return {};
        }
        auto *host = gui::gsp::AppHost::current();
        if (!host || !host->handle()) {
            return std::unexpected("Launcher GSP session is unavailable");
        }
        const auto handle = host->handle();
        const bool chinese = is_chinese();
        const auto language = chinese ? std::string_view("zh_CN") : std::string_view("en");

        auto labels = apply_launcher_static_labels(handle, chinese);
        if (!labels) {
            return labels;
        }
        (void)esp_gsp_component_set_text(
            handle, GSP_SUPER_LAUNCHER_OBJ_KEY_STATUS_CLOCK, make_gsp_clock_text().c_str()
        );

        slot_apps_.fill(INVALID_APP_ID);
        settings_app_id_ = INVALID_APP_ID;
        std::vector<AppInfo> visible_apps;
        for (const auto &app : owner_.list_apps()) {
            if (!app.manifest.visible || app.manifest.id == GSP_SHELL_APP_ID) {
                continue;
            }
            if (app.manifest.id == "brookesia.general.settings") {
                settings_app_id_ = app.app_id;
            }
            visible_apps.push_back(app);
        }
        std::stable_sort(
            visible_apps.begin(),
            visible_apps.end(),
        [](const AppInfo & lhs, const AppInfo & rhs) {
            const bool lhs_native = lhs.manifest.kind == AppKind::Native;
            const bool rhs_native = rhs.manifest.kind == AppKind::Native;
            if (lhs_native != rhs_native) {
                return lhs_native;
            }
            return lhs.app_id < rhs.app_id;
        }
        );

        const size_t total = visible_apps.size();
        const size_t pages = std::max<size_t>(1, (total + GSP_LAUNCHER_SLOT_COUNT - 1) / GSP_LAUNCHER_SLOT_COUNT);
        launcher_page_ = std::min(launcher_page_, pages - 1);

        static constexpr std::array<uint32_t, GSP_LAUNCHER_SLOT_COUNT> SLOT_KEYS = {
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_SLOT_0,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_SLOT_1,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_SLOT_2,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_SLOT_3,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_SLOT_4,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_SLOT_5,
        };
        static constexpr std::array<uint32_t, GSP_LAUNCHER_SLOT_COUNT> SLOT_VISIBILITY_KEYS = {
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_SLOT_0_GROUP,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_SLOT_1_GROUP,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_SLOT_2_GROUP,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_SLOT_3_GROUP,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_SLOT_4_GROUP,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_SLOT_5_GROUP,
        };
        static constexpr std::array<uint32_t, GSP_LAUNCHER_SLOT_COUNT> META_KEYS = {
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_META_0,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_META_1,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_META_2,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_META_3,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_META_4,
            GSP_SUPER_LAUNCHER_OBJ_KEY_APP_META_5,
        };

        for (size_t slot = 0; slot < GSP_LAUNCHER_SLOT_COUNT; ++slot) {
            const size_t index = launcher_page_ * GSP_LAUNCHER_SLOT_COUNT + slot;
            const bool present = index < total;
            if (present) {
                const auto &app = visible_apps[index];
                slot_apps_[slot] = app.app_id;
                const auto name = resolve_app_display_name(app.manifest, language);
                std::string meta = (app.manifest.kind == AppKind::Native) ?
                                   (chinese ? "原生应用" : "Native") :
                                   std::string(BROOKESIA_DESCRIBE_TO_STR(app.manifest.runtime_type));
                if (!app.manifest.version.empty()) {
                    meta += " | v" + app.manifest.version;
                }
                meta += " | " + std::string(gsp_app_state_text(app.state, chinese));
                if (auto error = owner_.app_support_error(app.app_id); error) {
                    meta += chinese ? " | 不可用" : " | Unavailable";
                }
                (void)esp_gsp_component_set_text(handle, SLOT_KEYS[slot], name.c_str());
                (void)esp_gsp_component_set_text(handle, META_KEYS[slot], meta.c_str());
            } else {
                (void)esp_gsp_component_set_text(handle, SLOT_KEYS[slot], "");
                (void)esp_gsp_component_set_text(handle, META_KEYS[slot], " ");
            }
            if (esp_gsp_component_set_visible(handle, SLOT_VISIBILITY_KEYS[slot], present) != ESP_GSP_OK ||
                    esp_gsp_component_set_enabled(handle, SLOT_KEYS[slot], present) != ESP_GSP_OK) {
                return std::unexpected("Failed to update GSP launcher slot visibility");
            }
        }

        const bool has_settings = settings_app_id_ != INVALID_APP_ID;
        (void)esp_gsp_component_set_enabled(handle, GSP_SUPER_LAUNCHER_OBJ_KEY_OPEN_SETTINGS, has_settings);
        (void)esp_gsp_component_set_enabled(
            handle, GSP_SUPER_LAUNCHER_OBJ_KEY_PREV_PAGE, launcher_page_ > 0
        );
        (void)esp_gsp_component_set_enabled(
            handle, GSP_SUPER_LAUNCHER_OBJ_KEY_NEXT_PAGE, launcher_page_ + 1 < pages
        );
        const std::string page_info = std::to_string(total) + (chinese ? " 个应用   " : " apps   ") +
                                      std::to_string(launcher_page_ + 1) + " / " + std::to_string(pages);
        if (esp_gsp_component_set_text(handle, GSP_SUPER_LAUNCHER_OBJ_KEY_PAGE_INFO, page_info.c_str()) != ESP_GSP_OK) {
            return std::unexpected("Failed to update GSP launcher pagination label");
        }
        return {};
    }

    std::expected<void, std::string> show_keyboard(
        AppId app_id,
        KeyboardRequestId request_id,
        const KeyboardRequestOptions &options
    )
    {
        if (closing_modals_) {
            return std::unexpected("GSP modals are closing");
        }
        if (keyboard_request_id_ != INVALID_KEYBOARD_REQUEST_ID) {
            return std::unexpected("GSP keyboard is already active");
        }
        if (app_id == INVALID_APP_ID || request_id == INVALID_KEYBOARD_REQUEST_ID) {
            return std::unexpected("GSP keyboard request is not valid");
        }
        auto normalized = normalize_keyboard_options(options);
        if (!normalized) {
            return std::unexpected(normalized.error());
        }
        if (normalized->max_length < 0 ||
                normalized->max_length > static_cast<int32_t>(MAX_KEYBOARD_TEXT_BYTES)) {
            return std::unexpected("GSP keyboard max_length must be between 0 and 64 bytes");
        }
        const size_t max_length = normalized->max_length == 0 ?
                                  MAX_KEYBOARD_TEXT_BYTES :
                                  static_cast<size_t>(normalized->max_length);
        if (normalized->initial_text.size() > max_length || !is_printable_ascii(normalized->initial_text)) {
            return std::unexpected("GSP keyboard initial text must be printable ASCII within max_length");
        }

        auto handle_result = ensure_modal_or_shell_handle();
        if (!handle_result) {
            return std::unexpected(handle_result.error());
        }
        esp_gsp_handle_t handle = *handle_result;

        const bool chinese = is_chinese();
        keyboard_owner_ = app_id;
        keyboard_request_id_ = request_id;
        keyboard_title_ = normalized->title.empty() ?
                          (normalized->password ? (chinese ? "输入密码" : "Password") :
                           (chinese ? "输入内容" : "Keyboard")) :
                          normalized->title;
        keyboard_text_ = normalized->initial_text;
        keyboard_placeholder_ = normalized->placeholder.empty() ?
                                (normalized->password ? (chinese ? "请输入密码" : "Enter password") :
                                 (chinese ? "请输入内容" : "Enter text")) :
                                normalized->placeholder;
        keyboard_max_length_ = max_length;
        keyboard_password_ = normalized->password;
        allow_lower_ = std::find(normalized->allowed_modes.begin(), normalized->allowed_modes.end(), "text") !=
                       normalized->allowed_modes.end();
        allow_upper_ = std::find(normalized->allowed_modes.begin(), normalized->allowed_modes.end(), "upper") !=
                       normalized->allowed_modes.end();
        allow_symbols_ = std::find(normalized->allowed_modes.begin(), normalized->allowed_modes.end(), "special") !=
                         normalized->allowed_modes.end();
        allow_number_ = std::find(normalized->allowed_modes.begin(), normalized->allowed_modes.end(), "number") !=
                        normalized->allowed_modes.end();
        keyboard_layout_ = normalized->mode == "upper" ? GspKeyboardLayout::Upper :
                           normalized->mode == "special" ? GspKeyboardLayout::Symbols :
                           normalized->mode == "number" ? GspKeyboardLayout::Number :
                           GspKeyboardLayout::Lower;
        keyboard_fresh_numeric_ = !normalized->password && normalized->mode == "number" &&
                                  !normalized->initial_text.empty();

        if (esp_gsp_goto_scene(handle, GSP_SUPER_SHELL_SCENE_SUPER_KEYBOARD, ESP_GSP_NO_TRANSITION) != ESP_GSP_OK ||
                esp_gsp_flush(handle, 500) != ESP_GSP_OK) {
            hide_keyboard(app_id, request_id);
            return std::unexpected("Failed to show GSP keyboard scene");
        }
        active_scene_ = GSP_SUPER_SHELL_SCENE_SUPER_KEYBOARD;
        if (auto labels = apply_keyboard_static_labels(handle, chinese); !labels) {
            hide_keyboard(app_id, request_id);
            return labels;
        }
        if (esp_gsp_component_set_text(
                    handle, GSP_SUPER_KEYBOARD_OBJ_KEY_TITLE, keyboard_title_.c_str()
                ) != ESP_GSP_OK) {
            hide_keyboard(app_id, request_id);
            return std::unexpected("Failed to set GSP keyboard title");
        }
        if (auto preview = render_keyboard_preview(
                               handle, keyboard_text_, keyboard_placeholder_, keyboard_max_length_, keyboard_password_
                           ); !preview) {
            hide_keyboard(app_id, request_id);
            return preview;
        }
        if (auto keys = render_keyboard_keys(
                            handle, keyboard_layout_, allow_lower_, allow_upper_, allow_symbols_, allow_number_, chinese
                        ); !keys) {
            hide_keyboard(app_id, request_id);
            return keys;
        }
        return {};
    }

    void hide_keyboard(AppId app_id, KeyboardRequestId request_id)
    {
        if (keyboard_request_id_ != request_id ||
                (app_id != INVALID_APP_ID && keyboard_owner_ != app_id)) {
            return;
        }
        std::fill(keyboard_text_.begin(), keyboard_text_.end(), '\0');
        keyboard_text_.clear();
        keyboard_placeholder_.clear();
        keyboard_title_.clear();
        keyboard_owner_ = INVALID_APP_ID;
        keyboard_request_id_ = INVALID_KEYBOARD_REQUEST_ID;
        keyboard_fresh_numeric_ = false;
        handle_message_dialog_idle();
    }

    std::expected<void, std::string> show_message_dialog(
        AppId app_id,
        MessageDialogRequestId request_id,
        const MessageDialogOptions &options
    )
    {
        if (closing_modals_) {
            return std::unexpected("GSP modals are closing");
        }
        if (dialog_request_id_ != INVALID_MESSAGE_DIALOG_REQUEST_ID) {
            return std::unexpected("GSP message dialog is already active");
        }
        auto handle_result = ensure_modal_or_shell_handle();
        if (!handle_result) {
            return std::unexpected(handle_result.error());
        }
        esp_gsp_handle_t handle = *handle_result;

        dialog_owner_ = app_id;
        dialog_request_id_ = request_id;
        dialog_options_ = options;
        active_scene_ = GSP_SUPER_SHELL_SCENE_SUPER_DIALOG;

        auto rendered = render_dialog_scene(handle, options, is_chinese());
        if (!rendered) {
            dialog_owner_ = INVALID_APP_ID;
            dialog_request_id_ = INVALID_MESSAGE_DIALOG_REQUEST_ID;
            dialog_options_ = {};
            handle_message_dialog_idle();
            return rendered;
        }

        if (dialog_timeout_task_id_ != 0) {
            owner_.cancel_system_app_delayed_task(dialog_timeout_task_id_);
            dialog_timeout_task_id_ = 0;
        }
        const uint64_t generation = ++dialog_timeout_generation_;
        if (options.auto_close_ms > 0) {
            auto scheduled = owner_.post_system_app_delayed_task(
                                 options.auto_close_ms,
            [this, generation, request_id]() {
                if (dialog_timeout_generation_ != generation || dialog_request_id_ != request_id) {
                    return;
                }
                dialog_timeout_task_id_ = 0;
                const auto owner_id = dialog_owner_;
                dialog_owner_ = INVALID_APP_ID;
                dialog_request_id_ = INVALID_MESSAGE_DIALOG_REQUEST_ID;
                dialog_options_ = {};
                if (!owner_.has_queued_message_dialogs()) {
                    handle_message_dialog_idle();
                }
                (void)owner_.complete_app_message_dialog(
                    owner_id, request_id, -1, MessageDialogCloseReason::Timeout
                );
            }
                             );
            if (scheduled) {
                dialog_timeout_task_id_ = *scheduled;
            }
        }
        return {};
    }

    std::expected<void, std::string> update_message_dialog(
        AppId app_id,
        MessageDialogRequestId request_id,
        const MessageDialogOptions &options
    )
    {
        if (dialog_request_id_ != request_id || dialog_owner_ != app_id) {
            return std::unexpected("GSP message dialog is not active");
        }
        esp_gsp_handle_t handle = active_handle();
        if (handle == nullptr) {
            return std::unexpected("GSP message dialog session is unavailable");
        }
        dialog_options_ = options;
        auto rendered = render_dialog_scene(handle, options, is_chinese());
        if (!rendered) {
            return rendered;
        }
        if (dialog_timeout_task_id_ != 0) {
            owner_.cancel_system_app_delayed_task(dialog_timeout_task_id_);
            dialog_timeout_task_id_ = 0;
        }
        const uint64_t generation = ++dialog_timeout_generation_;
        if (options.auto_close_ms > 0) {
            auto scheduled = owner_.post_system_app_delayed_task(
                                 options.auto_close_ms,
            [this, generation, request_id]() {
                if (dialog_timeout_generation_ != generation || dialog_request_id_ != request_id) {
                    return;
                }
                dialog_timeout_task_id_ = 0;
                const auto owner_id = dialog_owner_;
                dialog_owner_ = INVALID_APP_ID;
                dialog_request_id_ = INVALID_MESSAGE_DIALOG_REQUEST_ID;
                dialog_options_ = {};
                if (!owner_.has_queued_message_dialogs()) {
                    handle_message_dialog_idle();
                }
                (void)owner_.complete_app_message_dialog(
                    owner_id, request_id, -1, MessageDialogCloseReason::Timeout
                );
            }
                             );
            if (scheduled) {
                dialog_timeout_task_id_ = *scheduled;
            }
        }
        return {};
    }

    void hide_message_dialog(AppId app_id, MessageDialogRequestId request_id)
    {
        if (dialog_request_id_ != request_id ||
                (app_id != INVALID_APP_ID && dialog_owner_ != app_id)) {
            return;
        }
        if (dialog_timeout_task_id_ != 0) {
            owner_.cancel_system_app_delayed_task(dialog_timeout_task_id_);
            dialog_timeout_task_id_ = 0;
        }
        ++dialog_timeout_generation_;
        dialog_owner_ = INVALID_APP_ID;
        dialog_request_id_ = INVALID_MESSAGE_DIALOG_REQUEST_ID;
        dialog_options_ = {};
        if (!owner_.has_queued_message_dialogs()) {
            handle_message_dialog_idle();
        }
    }

    void handle_message_dialog_idle()
    {
        if (closing_modals_ || dialog_request_id_ != INVALID_MESSAGE_DIALOG_REQUEST_ID) {
            return;
        }
        if (keyboard_request_id_ != INVALID_KEYBOARD_REQUEST_ID) {
            esp_gsp_handle_t handle = active_handle();
            if (handle != nullptr) {
                const bool chinese = is_chinese();
                (void)esp_gsp_goto_scene(handle, GSP_SUPER_SHELL_SCENE_SUPER_KEYBOARD, ESP_GSP_NO_TRANSITION);
                (void)esp_gsp_flush(handle, 500);
                active_scene_ = GSP_SUPER_SHELL_SCENE_SUPER_KEYBOARD;
                (void)apply_keyboard_static_labels(handle, chinese);
                (void)esp_gsp_component_set_text(
                    handle, GSP_SUPER_KEYBOARD_OBJ_KEY_TITLE, keyboard_title_.c_str()
                );
                (void)render_keyboard_preview(
                    handle, keyboard_text_, keyboard_placeholder_, keyboard_max_length_, keyboard_password_
                );
                (void)render_keyboard_keys(
                    handle, keyboard_layout_, allow_lower_, allow_upper_, allow_symbols_, allow_number_, chinese
                );
            }
            return;
        }

        auto stopped = stop_modal_session();
        if (!stopped) {
            BROOKESIA_LOGE("Failed to stop GSP modal; keeping its session and devices: %1%", stopped.error());
            return;
        }
        if (suspended_app_id_ != INVALID_APP_ID) {
            const auto app_to_resume = std::exchange(suspended_app_id_, INVALID_APP_ID);
            auto resumed = owner_.resume_gsp_session_after_modal(app_to_resume);
            if (!resumed) {
                BROOKESIA_LOGW("Failed to resume GSP app after modal closed: %1%", resumed.error());
            }
            return;
        }
        if (context_ != nullptr) {
            if (auto *host = gui::gsp::AppHost::current(); host && host->handle()) {
                const auto handle = host->handle();
                (void)esp_gsp_goto_scene(handle, GSP_SUPER_SHELL_SCENE_SUPER_LAUNCHER, ESP_GSP_NO_TRANSITION);
                (void)esp_gsp_flush(handle, 500);
                active_scene_ = GSP_SUPER_SHELL_SCENE_SUPER_LAUNCHER;
                (void)refresh_launcher();
            }
        }
    }

private:
    bool is_chinese() const
    {
        const auto lang = owner_.get_environment().language;
        return lang == "zh_CN" || lang == "zh";
    }

    esp_gsp_handle_t active_handle() const
    {
        if (modal_host_ && modal_host_->running()) {
            return modal_host_->handle();
        }
        if (context_ != nullptr) {
            if (auto *host = gui::gsp::AppHost::current(); host != nullptr) {
                return host->handle();
            }
        }
        return nullptr;
    }

    std::expected<esp_gsp_handle_t, std::string> ensure_modal_or_shell_handle()
    {
        if (auto handle = active_handle(); handle != nullptr) {
            return handle;
        }
        auto *panel = static_cast<hal::display::PanelIface *>(owner_.gsp_panel());
        auto *touch = static_cast<hal::display::TouchIface *>(owner_.gsp_touch());
        if (!owner_.gsp_desktop_display() && (panel == nullptr || touch == nullptr)) {
            return std::unexpected("GSP display devices are not available for system modal");
        }
        auto suspended = owner_.suspend_gsp_session_for_modal();
        if (!suspended) {
            return std::unexpected(suspended.error());
        }
        suspended_app_id_ = *suspended;
        modal_host_ = std::make_unique<gui::gsp::AppHost>();
        {
            boost::lock_guard lock(modal_gate_->mutex);
            modal_gate_->shell = this;
            modal_gate_->epoch.fetch_add(1, std::memory_order_release);
        }
        const auto *config = static_cast<const esp_gsp_config_t *>(gui_runtime_config());
        const gui::gsp::DisplayLease lease{
            .panel = panel, .touch = touch, .desktop = owner_.gsp_desktop_display()
        };
        auto *gate_ptr = modal_gate_.get();
        const auto on_modal_event = [](esp_gsp_handle_t, const esp_gsp_event_t *event, void *user_ctx) {
            auto *gate = static_cast<ModalCallbackGate *>(user_ctx);
            if (gate == nullptr || event == nullptr || event->type != ESP_GSP_EVENT_CALL) {
                return;
            }
            boost::lock_guard lock(gate->mutex);
            if (gate->shell == nullptr) {
                return;
            }
            const uint64_t epoch = gate->epoch.load(std::memory_order_acquire);
            char action_buf[64];
            std::snprintf(
                action_buf, sizeof(action_buf), "%u:%u:%u",
                static_cast<unsigned>(event->scene_id),
                static_cast<unsigned>(event->action_id),
                static_cast<unsigned>(event->arg)
            );
            std::string action_str(action_buf);
            const auto shared_gate = gate->shell->modal_gate_;
            auto dispatch = [shared_gate, epoch, action_str = std::move(action_str)]() {
                boost::unique_lock task_lock(shared_gate->mutex);
                auto *shell = shared_gate->shell;
                if (shell == nullptr || shared_gate->epoch.load(std::memory_order_acquire) != epoch) {
                    return;
                }
                // Release the gate before actions that stop the modal and invalidate this gate.
                task_lock.unlock();
                if (shell->context_ != nullptr) {
                    (void)shell->on_action(*shell->context_, action_str);
                } else {
                    AppContext dummy_context(shell->owner_, INVALID_APP_ID);
                    (void)shell->on_action(dummy_context, action_str);
                }
            };
            (void)gate->shell->owner_.post_system_app_task(std::move(dispatch));
        };
        auto started = modal_host_->start(*config, lease, on_modal_event, gate_ptr);
        if (!started) {
            if (modal_host_->running()) {
                return std::unexpected(started.error());
            }
            detach_modal_callbacks();
            modal_host_.reset();
            const auto to_resume = std::exchange(suspended_app_id_, INVALID_APP_ID);
            (void)owner_.resume_gsp_session_after_modal(to_resume);
            return std::unexpected(started.error());
        }
        const auto handle = modal_host_->handle();
        (void)esp_gsp_set_swipe_enabled(handle, false);
        return handle;
    }

    void schedule_clock_timer()
    {
        if (context_ == nullptr || clock_timer_id_ != INVALID_TIMER_ID) {
            return;
        }
        if (auto timer = context_->timer().start_delayed(GSP_CLOCK_TIMER_NAME, 1000); timer) {
            clock_timer_id_ = *timer;
        }
    }

    void stop_clock_timer()
    {
        if (context_ != nullptr && clock_timer_id_ != INVALID_TIMER_ID) {
            (void)context_->timer().stop(clock_timer_id_);
        }
        clock_timer_id_ = INVALID_TIMER_ID;
    }

    void detach_modal_callbacks()
    {
        boost::lock_guard lock(modal_gate_->mutex);
        modal_gate_->shell = nullptr;
        modal_gate_->epoch.fetch_add(1, std::memory_order_release);
    }

    std::expected<void, std::string> stop_modal_session()
    {
        detach_modal_callbacks();
        if (modal_host_) {
            auto stopped = modal_host_->stop();
            if (!stopped) {
                boost::lock_guard lock(modal_gate_->mutex);
                modal_gate_->shell = this;
                return stopped;
            }
            modal_host_.reset();
        }
        return {};
    }

    void disconnect_sntp_events()
    {
        {
            boost::lock_guard lock(sntp_gate_->mutex);
            sntp_gate_->shell = nullptr;
            sntp_gate_->epoch.fetch_add(1, std::memory_order_release);
        }
        sntp_event_connection_.disconnect();
    }

    void subscribe_sntp_events()
    {
        disconnect_sntp_events();
        if (!service::helper::SNTP::is_available()) {
            return;
        }
        const auto gate = sntp_gate_;
        {
            boost::lock_guard lock(gate->mutex);
            gate->shell = this;
        }
        const uint64_t subscription_epoch = gate->epoch.load(std::memory_order_acquire);
        const auto callback = [gate, subscription_epoch](const std::string &, const service::EventItemMap &) {
            boost::lock_guard lock(gate->mutex);
            if (gate->shell == nullptr || gate->epoch.load(std::memory_order_acquire) != subscription_epoch) {
                return;
            }
            const uint64_t epoch = gate->epoch.load(std::memory_order_acquire);
            auto refresh_clock = [gate, epoch]() {
                boost::lock_guard task_lock(gate->mutex);
                auto *shell = gate->shell;
                if (shell == nullptr || gate->epoch.load(std::memory_order_acquire) != epoch ||
                        shell->context_ == nullptr) {
                    return;
                }
                shell->stop_clock_timer();
                if (auto timer = shell->context_->timer().start_delayed(GSP_CLOCK_TIMER_NAME, 10); timer) {
                    shell->clock_timer_id_ = *timer;
                }
            };
            auto posted = gate->shell->owner_.post_system_app_task(std::move(refresh_clock));
            if (!posted) {
                BROOKESIA_LOGW("Failed to post SNTP clock refresh: %1%", posted.error());
            }
        };
        sntp_event_connection_ = service::helper::SNTP::subscribe_event(
                                     service::helper::SNTP::EventId::TimezoneChanged,
                                     callback
                                 );
    }

    System &owner_;
    AppContext *context_ = nullptr;
    TimerId clock_timer_id_ = INVALID_TIMER_ID;
    service::EventRegistry::SignalConnection sntp_event_connection_;
    std::shared_ptr<ModalCallbackGate> sntp_gate_ = std::make_shared<ModalCallbackGate>();
    bool closing_modals_ = false;

    std::shared_ptr<ModalCallbackGate> modal_gate_ = std::make_shared<ModalCallbackGate>();
    std::unique_ptr<gui::gsp::AppHost> modal_host_;
    AppId suspended_app_id_ = INVALID_APP_ID;
    size_t launcher_page_ = 0;
    std::array<AppId, GSP_LAUNCHER_SLOT_COUNT> slot_apps_{};
    AppId settings_app_id_ = INVALID_APP_ID;
    uint16_t active_scene_ = GSP_SUPER_SHELL_SCENE_SUPER_LAUNCHER;

    AppId keyboard_owner_ = INVALID_APP_ID;
    KeyboardRequestId keyboard_request_id_ = INVALID_KEYBOARD_REQUEST_ID;
    std::string keyboard_title_;
    std::string keyboard_text_;
    std::string keyboard_placeholder_;
    size_t keyboard_max_length_ = MAX_KEYBOARD_TEXT_BYTES;
    bool keyboard_password_ = false;
    bool keyboard_fresh_numeric_ = false;
    GspKeyboardLayout keyboard_layout_ = GspKeyboardLayout::Lower;
    bool allow_lower_ = true;
    bool allow_upper_ = true;
    bool allow_symbols_ = true;
    bool allow_number_ = true;

    AppId dialog_owner_ = INVALID_APP_ID;
    MessageDialogRequestId dialog_request_id_ = INVALID_MESSAGE_DIALOG_REQUEST_ID;
    MessageDialogOptions dialog_options_{};
    uint64_t dialog_timeout_task_id_ = 0;
    uint64_t dialog_timeout_generation_ = 0;
};

GspSystemShellApp *as_gsp_shell(const std::shared_ptr<IApp> &app)
{
    return static_cast<GspSystemShellApp *>(app.get());
}

void start_gsp_sntp_if_needed(service::ServiceBinding &binding)
{
    using SNTPHelper = service::helper::SNTP;
    if (!SNTPHelper::is_available()) {
        return;
    }
    if (!binding.is_valid()) {
        binding = service::ServiceManager::get_instance().bind(SNTPHelper::get_name().data());
    }
    if (!binding.is_valid()) {
        return;
    }
    if (auto state = SNTPHelper::call_function_sync<std::string>(SNTPHelper::FunctionId::GetState);
            state && (*state == BROOKESIA_DESCRIBE_TO_STR(SNTPHelper::State::Stopped))) {
        (void)SNTPHelper::call_function_sync<void>(SNTPHelper::FunctionId::Start);
    }
}

#endif

namespace {

std::string make_component_version(int major, int minor, int patch)
{
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

} // namespace

System::System()
    : system_gui_access_(*this)
    , impl_(std::make_unique<Impl>(*this))
{}

bool System::Impl::gsp_shell_session_running() const
{
#if CONFIG_BROOKESIA_GUI_BACKEND_GSP
    auto *shell = as_gsp_shell(gsp_shell_app_);
    return shell != nullptr && shell->modal_session_running();
#else
    return false;
#endif
}

std::expected<void, std::string> System::Impl::stop_gsp_shell_session()
{
#if CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto *shell = as_gsp_shell(gsp_shell_app_); shell != nullptr) {
        return shell->close_modals_for_app_switch();
    }
#endif
    return {};
}

void System::Impl::detach_gsp_shell_callbacks()
{
#if CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto *shell = as_gsp_shell(gsp_shell_app_); shell != nullptr) {
        shell->detach_callbacks();
    }
#endif
}

System::~System()
{
    deinit();
    if ((impl_ != nullptr) && impl_->any_gsp_session_running()) {
        BROOKESIA_LOGE(
            "GSP session is still running; display and touch devices stay owned"
        );
        impl_->abandon_system_references();
        impl_.release();
    }
}

SystemInfo System::on_get_system_info() const
{
    return {
        .name = BROOKESIA_SYSTEM_CORE_SYSTEM_NAME,
        .version = make_component_version(
            BROOKESIA_SYSTEM_CORE_VER_MAJOR,
            BROOKESIA_SYSTEM_CORE_VER_MINOR,
            BROOKESIA_SYSTEM_CORE_VER_PATCH
        ),
    };
}

std::expected<void, std::string> System::on_prepare_startup_overlay()
{
    return {};
}

std::expected<void, std::string> System::on_init()
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (impl_->config_.gsp_desktop_display || (impl_->gsp_panel_ != nullptr && impl_->gsp_touch_ != nullptr)) {
        begin_gui_preferences_restore();
        const auto saved_theme = get_stored_gui_theme_id().value_or("light");
        const auto saved_language = get_stored_gui_language().value_or("en");
        const auto theme = saved_theme == "dark" ? "dark" : "light";
        const auto language = (saved_language == "zh_CN" || saved_language == "zh") ? "zh_CN" : "en";
        auto result = system_gui().set_theme(theme, false);
        if (result) {
            result = system_gui().set_language(language, false);
        }
        mark_gui_preferences_restored();
        if (!result) {
            return result;
        }
        if (impl_->gsp_shell_app_id_ == INVALID_APP_ID) {
            impl_->gsp_shell_app_ = std::make_shared<GspSystemShellApp>(*this);
            auto installed = install_app(impl_->gsp_shell_app_);
            if (!installed) {
                impl_->gsp_shell_app_.reset();
                return std::unexpected(installed.error());
            }
            impl_->gsp_shell_app_id_ = *installed;
        }
    }
#endif
    return {};
}

std::expected<void, std::string> System::on_start()
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    impl_->gsp_stopping_ = false;
    if (impl_->gsp_shell_app_id_ != INVALID_APP_ID) {
        start_gsp_sntp_if_needed(impl_->gsp_sntp_binding_);
        return start_app(impl_->gsp_shell_app_id_);
    }
#endif
    return {};
}

void System::on_stop()
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    impl_->gsp_stopping_ = true;
    impl_->gsp_sntp_binding_.release();
#endif
}

void System::on_deinit()
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto *shell = as_gsp_shell(impl_->gsp_shell_app_); shell != nullptr) {
        auto closed = shell->close_modals_for_app_switch();
        if (!closed) {
            BROOKESIA_LOGE("Failed to close GSP modals during deinit: %1%", closed.error());
        }
    }
#endif
}

std::expected<void, std::string> System::on_app_installed(const AppInfo &)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto *shell = as_gsp_shell(impl_->gsp_shell_app_); shell != nullptr) {
        return shell->refresh_launcher();
    }
#endif
    return {};
}

std::expected<void, std::string> System::on_app_uninstalled(const AppInfo &)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto *shell = as_gsp_shell(impl_->gsp_shell_app_); shell != nullptr) {
        return shell->refresh_launcher();
    }
#endif
    return {};
}

std::expected<void, std::string> System::on_prepare_app_start(AppId app_id)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (impl_->gsp_shell_app_id_ != INVALID_APP_ID) {
        if (auto *shell = as_gsp_shell(impl_->gsp_shell_app_); shell != nullptr) {
            auto closed = shell->close_modals_for_app_switch();
            if (!closed) {
                return closed;
            }
        }
        const bool prev_launching = impl_->gsp_launching_app_;
        impl_->gsp_launching_app_ = true;
        lib_utils::FunctionGuard restore_launching([this, prev_launching]() {
            impl_->gsp_launching_app_ = prev_launching;
        });
        for (const auto &app : list_apps()) {
            if ((app.app_id != app_id) &&
                    (app.state == AppState::Running || app.state == AppState::Paused)) {
                auto stopped = stop_app(app.app_id);
                if (!stopped) {
                    return stopped;
                }
            }
        }
    }
#else
    (void)app_id;
#endif
    return {};
}

std::expected<void, std::string> System::on_app_started(const AppInfo &app)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    // Desktop bridge sessions publish user events only; deliver the initial scene once the JS app is running.
    if (impl_->config_.gsp_desktop_display && app.manifest.kind == AppKind::Runtime) {
        auto found = impl_->get_record(app.app_id);
        if (found && (*found)->gsp_host && (*found)->gsp_host->running()) {
            const auto &record = **found;
            esp_gsp_event_t event{};
            event.type = ESP_GSP_EVENT_SCENE_CHANGED;
            event.scene_id = record.gsp_scene_known ? record.gsp_current_scene : 0;
            const auto epoch = record.gsp_epoch ? record.gsp_epoch->load(std::memory_order_acquire) : 0;
            impl_->dispatch_gsp_event(app.app_id, &event, epoch);
        }
    }
#else
    (void)app;
#endif
    return {};
}

void System::on_app_start_failed(const AppInfo &app, std::string_view)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (impl_->gsp_stopping_ || impl_->gsp_launching_app_ ||
            impl_->gsp_shell_app_id_ == INVALID_APP_ID || app.app_id == impl_->gsp_shell_app_id_) {
        return;
    }
    (void)start_app(impl_->gsp_shell_app_id_);
#else
    (void)app;
#endif
}

void System::on_app_stopped(const AppInfo &app)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (impl_->gsp_stopping_ || impl_->gsp_launching_app_ ||
            impl_->gsp_shell_app_id_ == INVALID_APP_ID || app.app_id == impl_->gsp_shell_app_id_) {
        return;
    }
    (void)start_app(impl_->gsp_shell_app_id_);
#else
    (void)app;
#endif
}

void System::on_app_stop_failed(const AppInfo &app, std::string_view)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (impl_->gsp_stopping_ || impl_->gsp_launching_app_ ||
            impl_->gsp_shell_app_id_ == INVALID_APP_ID || app.app_id == impl_->gsp_shell_app_id_) {
        return;
    }
    (void)start_app(impl_->gsp_shell_app_id_);
#else
    (void)app;
#endif
}

std::expected<void, std::string> System::on_theme_changed(std::string_view)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto *shell = as_gsp_shell(impl_->gsp_shell_app_); shell != nullptr) {
        return shell->refresh_launcher();
    }
#endif
    return {};
}

std::expected<void, std::string> System::on_language_changed(std::string_view)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto *shell = as_gsp_shell(impl_->gsp_shell_app_); shell != nullptr) {
        return shell->refresh_launcher();
    }
#endif
    return {};
}

std::expected<void, std::string> System::on_show_app_loading(AppId)
{
    return {};
}

void System::on_hide_app_loading(AppId)
{}

std::expected<void, std::string> System::on_show_app_keyboard(
    AppId app_id,
    KeyboardRequestId request_id,
    const KeyboardRequestOptions &options
)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto *shell = as_gsp_shell(impl_->gsp_shell_app_); shell != nullptr) {
        return shell->show_keyboard(app_id, request_id, options);
    }
#else
    (void)app_id;
    (void)request_id;
    (void)options;
#endif
    return std::unexpected("Keyboard input is not supported by this system");
}

void System::on_hide_app_keyboard(AppId app_id, KeyboardRequestId request_id)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto *shell = as_gsp_shell(impl_->gsp_shell_app_); shell != nullptr) {
        shell->hide_keyboard(app_id, request_id);
    }
#else
    (void)app_id;
    (void)request_id;
#endif
}

std::expected<void, std::string> System::on_show_message_dialog(
    AppId app_id,
    MessageDialogRequestId request_id,
    const MessageDialogOptions &options
)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto *shell = as_gsp_shell(impl_->gsp_shell_app_); shell != nullptr) {
        return shell->show_message_dialog(app_id, request_id, options);
    }
#else
    (void)app_id;
    (void)request_id;
    (void)options;
#endif
    return std::unexpected("Message dialog is not supported by this system");
}

std::expected<void, std::string> System::on_update_message_dialog(
    AppId app_id,
    MessageDialogRequestId request_id,
    const MessageDialogOptions &options
)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto *shell = as_gsp_shell(impl_->gsp_shell_app_); shell != nullptr) {
        return shell->update_message_dialog(app_id, request_id, options);
    }
#else
    (void)app_id;
    (void)request_id;
    (void)options;
#endif
    return std::unexpected("Message dialog is not supported by this system");
}

void System::on_hide_message_dialog(AppId app_id, MessageDialogRequestId request_id)
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto *shell = as_gsp_shell(impl_->gsp_shell_app_); shell != nullptr) {
        shell->hide_message_dialog(app_id, request_id);
    }
#else
    (void)app_id;
    (void)request_id;
#endif
}

void System::on_message_dialog_idle()
{
#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
    if (auto *shell = as_gsp_shell(impl_->gsp_shell_app_); shell != nullptr) {
        shell->handle_message_dialog_idle();
    }
#endif
}

SystemGuiAccess &System::system_gui()
{
    return system_gui_access_;
}

void *System::gsp_panel() const
{
    return impl_ != nullptr ? impl_->gsp_panel_ : nullptr;
}

void *System::gsp_touch() const
{
    return impl_ != nullptr ? impl_->gsp_touch_ : nullptr;
}

std::shared_ptr<gui::gsp::DesktopDisplay> System::gsp_desktop_display() const
{
    return impl_ != nullptr ? impl_->config_.gsp_desktop_display : nullptr;
}

std::expected<void, std::string> System::post_system_app_task(std::function<void()> task)
{
    if (!impl_ || !task) {
        return std::unexpected("System task scheduler is unavailable");
    }
    return impl_->post_task(SYSTEM_APP_TASK_GROUP, std::move(task));
}

std::expected<uint64_t, std::string> System::post_system_app_delayed_task(
    int32_t delay_ms,
    std::function<void()> task
)
{
    if (!impl_ || !impl_->task_scheduler_ || !impl_->task_scheduler_->is_running() || !task) {
        return std::unexpected("System task scheduler is unavailable");
    }
    lib_utils::TaskScheduler::TaskId task_id = 0;
    if (!impl_->task_scheduler_->post_delayed(
                std::move(task),
                std::max<int32_t>(1, delay_ms),
                &task_id,
                SYSTEM_APP_TASK_GROUP
            )) {
        return std::unexpected("Failed to schedule delayed system app task");
    }
    return static_cast<uint64_t>(task_id);
}

void System::cancel_system_app_delayed_task(uint64_t task_id)
{
    if (impl_ && impl_->task_scheduler_ && task_id != 0) {
        impl_->task_scheduler_->cancel(static_cast<lib_utils::TaskScheduler::TaskId>(task_id));
    }
}

bool System::has_queued_message_dialogs() const
{
    return impl_ != nullptr && !impl_->queued_message_dialog_requests_.empty();
}

} // namespace esp_brookesia::system::core
