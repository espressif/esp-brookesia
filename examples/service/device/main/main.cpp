/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <tuple>
#include <vector>
#include "boost/json.hpp"
#include "boost/thread.hpp"
#define BROOKESIA_LOG_TAG "Main"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper/device.hpp"
#include "brookesia/service_helper/nvs.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_adaptor.hpp"

using namespace esp_brookesia;
using DeviceHelper = service::helper::Device;
using NVSHelper = service::helper::NVS;
using DisplayBrightnessEventMonitor =
    DeviceHelper::EventMonitor<DeviceHelper::EventId::DisplayBacklightBrightnessChanged>;
using DisplayOnOffEventMonitor = DeviceHelper::EventMonitor<DeviceHelper::EventId::DisplayBacklightOnOffChanged>;
using AudioVolumeEventMonitor = DeviceHelper::EventMonitor<DeviceHelper::EventId::AudioPlayerVolumeChanged>;
using AudioMuteEventMonitor = DeviceHelper::EventMonitor<DeviceHelper::EventId::AudioPlayerMuteChanged>;
using PowerBatteryStateEventMonitor = DeviceHelper::EventMonitor<DeviceHelper::EventId::PowerBatteryStateChanged>;
using PowerBatteryChargeConfigEventMonitor =
    DeviceHelper::EventMonitor<DeviceHelper::EventId::PowerBatteryChargeConfigChanged>;

constexpr uint32_t EXAMPLE_TEST_ACTION_DELAY_MS = 2000;
constexpr uint32_t AUDIO_PCM_SAMPLE_RATE = 16000;
constexpr uint8_t AUDIO_PCM_BITS = 16;
constexpr uint8_t AUDIO_PCM_CHANNELS = 1;
constexpr size_t WAV_HEADER_SIZE = 44;
constexpr uint32_t DEVICE_WAIT_EVENT_TIMEOUT_MS = EXAMPLE_TEST_ACTION_DELAY_MS;
constexpr const char *AUDIO_PCM_FILE_PATH = "/spiffs/audio.pcm";

static bool get_capabilities(DeviceHelper::Capabilities &capabilities);
static bool get_board_info(DeviceHelper::BoardInfo &info);
static bool get_power_battery_info(DeviceHelper::PowerBatteryInfo &info);
static bool get_power_battery_state(DeviceHelper::PowerBatteryState &state);
static bool get_power_battery_charge_config(DeviceHelper::PowerBatteryChargeConfig &config);
static const std::vector<std::string> *get_capability_instances(
    const DeviceHelper::Capabilities &capabilities, std::string_view interface_name
);
static bool has_capability(const DeviceHelper::Capabilities &capabilities, std::string_view interface_name);
static void log_capability_status(
    const DeviceHelper::Capabilities &capabilities, std::string_view title, std::string_view interface_name
);
static bool fill_display_panel_color_bars();
static std::span<const uint8_t> get_audio_pcm_payload();

static bool demo_capabilities(DeviceHelper::Capabilities &capabilities);
static bool demo_board_info(const DeviceHelper::Capabilities &capabilities);
static bool demo_display_controls(const DeviceHelper::Capabilities &capabilities);
static bool demo_audio_controls(const DeviceHelper::Capabilities &capabilities);
static bool demo_storage_query(const DeviceHelper::Capabilities &capabilities);
static bool demo_power_battery(const DeviceHelper::Capabilities &capabilities);
static bool demo_reset_data(const DeviceHelper::Capabilities &capabilities);

extern "C" void app_main(void)
{
    BROOKESIA_LOGI("\n\n=== Device Service Example ===\n");

    hal::init_all_devices();

    auto &service_manager = service::ServiceManager::get_instance();
    BROOKESIA_CHECK_FALSE_EXIT(service_manager.start(), "Failed to start ServiceManager");

    lib_utils::FunctionGuard shutdown_guard([&service_manager]() {
        BROOKESIA_LOGI("Shutting down ServiceManager...");
        service_manager.stop();
        hal::deinit_all_devices();
    });

    BROOKESIA_CHECK_FALSE_EXIT(DeviceHelper::is_available(), "Device service is not available");

    auto nvs_binding = service_manager.bind(NVSHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(nvs_binding.is_valid(), "Failed to bind NVS service");

    auto device_binding = service_manager.bind(DeviceHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(device_binding.is_valid(), "Failed to bind Device service");

    DeviceHelper::Capabilities capabilities{};
    BROOKESIA_CHECK_FALSE_EXIT(demo_capabilities(capabilities), "Failed to demo capabilities");
    BROOKESIA_CHECK_FALSE_EXIT(demo_board_info(capabilities), "Failed to demo board info");
    BROOKESIA_CHECK_FALSE_EXIT(demo_display_controls(capabilities), "Failed to demo display controls");
    BROOKESIA_CHECK_FALSE_EXIT(demo_audio_controls(capabilities), "Failed to demo audio controls");
    BROOKESIA_CHECK_FALSE_EXIT(demo_storage_query(capabilities), "Failed to demo storage query");
    BROOKESIA_CHECK_FALSE_EXIT(demo_power_battery(capabilities), "Failed to demo power battery");
    BROOKESIA_CHECK_FALSE_EXIT(demo_reset_data(capabilities), "Failed to demo reset data");

    BROOKESIA_LOGI("\n\n=== Device Service Example Completed ===\n");
}

static bool get_capabilities(DeviceHelper::Capabilities &capabilities)
{
    auto result = DeviceHelper::call_function_sync<boost::json::object>(DeviceHelper::FunctionId::GetCapabilities);
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to get capabilities: %1%", result.error());

    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(result.value(), capabilities);
    BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse capabilities");

    return true;
}

static bool get_board_info(DeviceHelper::BoardInfo &info)
{
    auto result = DeviceHelper::call_function_sync<boost::json::object>(DeviceHelper::FunctionId::GetBoardInfo);
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to get board info: %1%", result.error());

    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(result.value(), info);
    BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse board info");

    return true;
}

static bool get_power_battery_info(DeviceHelper::PowerBatteryInfo &info)
{
    auto result = DeviceHelper::call_function_sync<boost::json::object>(DeviceHelper::FunctionId::GetPowerBatteryInfo);
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to get power battery info: %1%", result.error());

    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(result.value(), info);
    BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse power battery info");

    return true;
}

static bool get_power_battery_state(DeviceHelper::PowerBatteryState &state)
{
    auto result = DeviceHelper::call_function_sync<boost::json::object>(DeviceHelper::FunctionId::GetPowerBatteryState);
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to get power battery state: %1%", result.error());

    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(result.value(), state);
    BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse power battery state");

    return true;
}

static bool get_power_battery_charge_config(DeviceHelper::PowerBatteryChargeConfig &config)
{
    auto result = DeviceHelper::call_function_sync<boost::json::object>(
                      DeviceHelper::FunctionId::GetPowerBatteryChargeConfig
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to get power battery charge config: %1%", result.error());

    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(result.value(), config);
    BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse power battery charge config");

    return true;
}

static const std::vector<std::string> *get_capability_instances(
    const DeviceHelper::Capabilities &capabilities, std::string_view interface_name
)
{
    auto it = capabilities.find(std::string(interface_name));
    if (it == capabilities.end()) {
        return nullptr;
    }

    return &it->second;
}

static bool has_capability(const DeviceHelper::Capabilities &capabilities, std::string_view interface_name)
{
    return get_capability_instances(capabilities, interface_name) != nullptr;
}

static void log_capability_status(
    const DeviceHelper::Capabilities &capabilities, std::string_view title, std::string_view interface_name
)
{
    const auto instances = get_capability_instances(capabilities, interface_name);
    if (instances == nullptr) {
        BROOKESIA_LOGI("[Not supported] %1% (%2%)", title, interface_name);
        return;
    }

    BROOKESIA_LOGI("[Supported] %1% (%2%): %3%", title, interface_name, BROOKESIA_DESCRIBE_TO_STR(*instances));
}

static bool fill_display_panel_color_bars()
{
    auto [panel_name, panel_iface] = hal::get_first_interface<hal::DisplayPanelIface>();
    BROOKESIA_CHECK_NULL_RETURN(panel_iface, false, "Failed to get display panel interface");

    const auto &info = panel_iface->get_info();
    BROOKESIA_CHECK_FALSE_RETURN(info.is_valid(), false, "Display panel info is invalid");

    const auto pixel_bits = info.get_pixel_bits();
    BROOKESIA_CHECK_FALSE_RETURN(pixel_bits > 0, false, "Unsupported display panel pixel format");
    BROOKESIA_CHECK_FALSE_RETURN((pixel_bits % 8) == 0, false, "Unsupported display panel pixel bits: %1%", pixel_bits);

    const auto bytes_per_pixel = static_cast<size_t>(pixel_bits / 8);
    auto write_pixel_bit = [pixel_bits](uint8_t *pixel, uint8_t bit_index) {
        const uint32_t pixel_value = 1U << bit_index;

        if (pixel_bits == 16) {
            pixel[0] = static_cast<uint8_t>(pixel_value & 0xFF);
            pixel[1] = static_cast<uint8_t>((pixel_value >> 8) & 0xFF);
            return;
        }

        pixel[0] = static_cast<uint8_t>((pixel_value >> 16) & 0xFF);
        pixel[1] = static_cast<uint8_t>((pixel_value >> 8) & 0xFF);
        pixel[2] = static_cast<uint8_t>(pixel_value & 0xFF);
    };

    std::vector<uint8_t> color_bar_line(static_cast<size_t>(info.h_res) * bytes_per_pixel, 0);
    for (uint32_t x = 0; x < info.h_res; ++x) {
        const auto bit_index = static_cast<uint8_t>((static_cast<size_t>(x) * pixel_bits) / info.h_res);
        write_pixel_bit(&color_bar_line[static_cast<size_t>(x) * bytes_per_pixel], bit_index);
    }

    for (uint32_t y = 0; y < info.v_res; ++y) {
        BROOKESIA_CHECK_FALSE_RETURN(
            panel_iface->draw_bitmap(0, y, info.h_res, y + 1, color_bar_line.data()), false,
            "Failed to draw color-bar line at y=%1%", y
        );
        boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    }

    BROOKESIA_LOGI(
        "Filled display panel with %1% pixel-bit color bars: %2%x%3%, format=%4%",
        pixel_bits, info.h_res, info.v_res, BROOKESIA_DESCRIBE_TO_STR(info.pixel_format)
    );

    return true;
}

static std::span<const uint8_t> get_audio_pcm_payload()
{
    static std::vector<uint8_t> pcm_data;
    static bool is_loaded = false;

    if (!is_loaded) {
        auto *file = std::fopen(AUDIO_PCM_FILE_PATH, "rb");
        BROOKESIA_CHECK_NULL_RETURN(file, {}, "Failed to open audio PCM file: %1%", AUDIO_PCM_FILE_PATH);
        lib_utils::FunctionGuard close_file_guard([file]() {
            std::fclose(file);
        });

        BROOKESIA_CHECK_FALSE_RETURN(
            std::fseek(file, 0, SEEK_END) == 0, {}, "Failed to seek audio PCM file end: %1%", AUDIO_PCM_FILE_PATH
        );
        const auto file_size = std::ftell(file);
        BROOKESIA_CHECK_FALSE_RETURN(file_size > 0, {}, "Invalid audio PCM file size: %1%", file_size);
        BROOKESIA_CHECK_FALSE_RETURN(
            std::fseek(file, 0, SEEK_SET) == 0, {}, "Failed to seek audio PCM file start: %1%", AUDIO_PCM_FILE_PATH
        );

        pcm_data.resize(static_cast<size_t>(file_size));
        const auto expected_size = pcm_data.size();
        const auto read_size = std::fread(pcm_data.data(), 1, pcm_data.size(), file);
        if (read_size != expected_size) {
            pcm_data.clear();
            BROOKESIA_LOGE(
                "Failed to read audio PCM file completely: expected=%1%, actual=%2%", expected_size, read_size
            );
            return {};
        }

        is_loaded = true;
    }

    const auto *begin = pcm_data.data();
    const auto size = pcm_data.size();

    if ((size > WAV_HEADER_SIZE) && (std::memcmp(begin, "RIFF", 4) == 0) && (std::memcmp(begin + 8, "WAVE", 4) == 0)) {
        return std::span<const uint8_t>(begin + WAV_HEADER_SIZE, size - WAV_HEADER_SIZE);
    }

    return std::span<const uint8_t>(begin, size);
}

static bool demo_capabilities(DeviceHelper::Capabilities &capabilities)
{
    BROOKESIA_LOGI("\n\n--- Demo: Capabilities ---\n");

    BROOKESIA_CHECK_FALSE_RETURN(get_capabilities(capabilities), false, "Failed to get capabilities");

    BROOKESIA_LOGI("Raw HAL capabilities reported by the current board:");
    for (const auto &[interface_name, instance_names] : capabilities) {
        BROOKESIA_LOGI("Capability: %1% -> %2%", interface_name, BROOKESIA_DESCRIBE_TO_STR(instance_names));
    }

    BROOKESIA_LOGI("Device helper feature matrix:");
    log_capability_status(capabilities, "Board information query", hal::BoardInfoIface::NAME);
    log_capability_status(capabilities, "Display panel direct validation", hal::DisplayPanelIface::NAME);
    log_capability_status(capabilities, "Display backlight control", hal::DisplayBacklightIface::NAME);
    log_capability_status(capabilities, "Audio player control", hal::AudioCodecPlayerIface::NAME);
    log_capability_status(capabilities, "Storage file-system query", hal::StorageFsIface::NAME);
    log_capability_status(capabilities, "Power battery query/control", hal::PowerBatteryIface::NAME);

    BROOKESIA_LOGI("\n\n--- Demo: Capabilities Completed ---\n");

    return true;
}

static bool demo_board_info(const DeviceHelper::Capabilities &capabilities)
{
    BROOKESIA_LOGI("\n\n--- Demo: Board Info ---\n");

    if (!has_capability(capabilities, hal::BoardInfoIface::NAME)) {
        BROOKESIA_LOGW("Board info is not available on this board, skip");
        return true;
    }

    DeviceHelper::BoardInfo board_info{};
    BROOKESIA_CHECK_FALSE_RETURN(get_board_info(board_info), false, "Failed to get board info");
    BROOKESIA_LOGI("Board info: %1%", boost::json::serialize(BROOKESIA_DESCRIBE_TO_JSON(board_info)));

    BROOKESIA_LOGI("\n\n--- Demo: Board Info Completed ---\n");

    return true;
}

static bool demo_display_controls(const DeviceHelper::Capabilities &capabilities)
{
    BROOKESIA_LOGI("\n\n--- Demo: Display Controls ---\n");

    if (!has_capability(capabilities, hal::DisplayBacklightIface::NAME)) {
        BROOKESIA_LOGW("Display backlight is not available, skip");
        return true;
    }

    auto get_brightness_result = DeviceHelper::call_function_sync<double>(
                                     DeviceHelper::FunctionId::GetDisplayBacklightBrightness
                                 );
    BROOKESIA_CHECK_FALSE_RETURN(
        get_brightness_result, false, "Failed to get display backlight brightness: %1%", get_brightness_result.error()
    );

    auto get_on_off_result = DeviceHelper::call_function_sync<bool>(
                                 DeviceHelper::FunctionId::GetDisplayBacklightOnOff
                             );
    BROOKESIA_CHECK_FALSE_RETURN(
        get_on_off_result, false, "Failed to get display backlight on/off: %1%", get_on_off_result.error()
    );

    const auto current_brightness = get_brightness_result.value();
    const auto next_brightness = (current_brightness < 100.0) ? (current_brightness + 1.0) : 99.0;

    BROOKESIA_LOGI("Current brightness: %1%", current_brightness);
    BROOKESIA_LOGI("Current backlight on: %1%", get_on_off_result.value());

    if (!get_on_off_result.value()) {
        auto ensure_on_result = DeviceHelper::call_function_sync(DeviceHelper::FunctionId::SetDisplayBacklightOnOff, true);
        BROOKESIA_CHECK_FALSE_RETURN(
            ensure_on_result, false, "Failed to turn display backlight on before color-bar validation: %1%",
            ensure_on_result.error()
        );
        boost::this_thread::sleep_for(boost::chrono::milliseconds(EXAMPLE_TEST_ACTION_DELAY_MS));
    }

    DisplayBrightnessEventMonitor brightness_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(
        brightness_event_monitor.start(), false, "Failed to start display brightness event monitor"
    );
    DisplayOnOffEventMonitor on_off_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(on_off_event_monitor.start(), false, "Failed to start display on/off event monitor");

    if (has_capability(capabilities, hal::DisplayPanelIface::NAME)) {
        BROOKESIA_CHECK_FALSE_RETURN(
            fill_display_panel_color_bars(), false, "Failed to fill display panel with color bars"
        );
    } else {
        BROOKESIA_LOGW("Display panel is not available, skip color-bar validation");
    }

    auto set_brightness_result = DeviceHelper::call_function_sync(
                                     DeviceHelper::FunctionId::SetDisplayBacklightBrightness, next_brightness
                                 );
    BROOKESIA_CHECK_FALSE_RETURN(
        set_brightness_result, false, "Failed to set display backlight brightness: %1%", set_brightness_result.error()
    );
    auto got_brightness_event = brightness_event_monitor.wait_for(std::vector<service::EventItem> {
        next_brightness
    }, DEVICE_WAIT_EVENT_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_RETURN(got_brightness_event, false, "Failed to wait for brightness changed event");
    auto last_brightness_event = brightness_event_monitor.get_last<double>();
    if (last_brightness_event.has_value()) {
        BROOKESIA_LOGI("[Monitor] Brightness changed event: %1%", std::get<0>(last_brightness_event.value()));
    }

    boost::this_thread::sleep_for(boost::chrono::milliseconds(EXAMPLE_TEST_ACTION_DELAY_MS));

    get_brightness_result = DeviceHelper::call_function_sync<double>(DeviceHelper::FunctionId::GetDisplayBacklightBrightness);
    BROOKESIA_CHECK_FALSE_RETURN(
        get_brightness_result, false, "Failed to re-get display backlight brightness: %1%", get_brightness_result.error()
    );
    BROOKESIA_LOGI("Updated brightness: %1%", get_brightness_result.value());

    auto set_off_result = DeviceHelper::call_function_sync(DeviceHelper::FunctionId::SetDisplayBacklightOnOff, false);
    BROOKESIA_CHECK_FALSE_RETURN(
        set_off_result, false, "Failed to turn display backlight off: %1%", set_off_result.error()
    );
    auto got_off_event = on_off_event_monitor.wait_for(std::vector<service::EventItem> {
        false
    }, DEVICE_WAIT_EVENT_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_RETURN(got_off_event, false, "Failed to wait for backlight off event");

    boost::this_thread::sleep_for(boost::chrono::milliseconds(EXAMPLE_TEST_ACTION_DELAY_MS));

    auto set_on_result = DeviceHelper::call_function_sync(DeviceHelper::FunctionId::SetDisplayBacklightOnOff, true);
    BROOKESIA_CHECK_FALSE_RETURN(
        set_on_result, false, "Failed to turn display backlight on: %1%", set_on_result.error()
    );
    auto got_on_event = on_off_event_monitor.wait_for(std::vector<service::EventItem> {
        true
    }, DEVICE_WAIT_EVENT_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_RETURN(got_on_event, false, "Failed to wait for backlight on event");
    auto last_on_off_event = on_off_event_monitor.get_last<bool>();
    if (last_on_off_event.has_value()) {
        BROOKESIA_LOGI("[Monitor] Backlight on/off event: %1%", std::get<0>(last_on_off_event.value()));
    }

    boost::this_thread::sleep_for(boost::chrono::milliseconds(EXAMPLE_TEST_ACTION_DELAY_MS));

    get_on_off_result = DeviceHelper::call_function_sync<bool>(DeviceHelper::FunctionId::GetDisplayBacklightOnOff);
    BROOKESIA_CHECK_FALSE_RETURN(
        get_on_off_result, false, "Failed to re-get display backlight on/off: %1%", get_on_off_result.error()
    );
    BROOKESIA_LOGI("Backlight on after restore: %1%", get_on_off_result.value());

    BROOKESIA_LOGI("\n\n--- Demo: Display Controls Completed ---\n");

    return true;
}

static bool demo_audio_controls(const DeviceHelper::Capabilities &capabilities)
{
    BROOKESIA_LOGI("\n\n--- Demo: Audio Controls ---\n");

    if (!has_capability(capabilities, hal::AudioCodecPlayerIface::NAME)) {
        BROOKESIA_LOGW("Audio player is not available, skip");
        return true;
    }

    auto [player_name, player_iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
    BROOKESIA_CHECK_NULL_RETURN(player_iface, false, "Failed to get audio player interface");

    hal::AudioCodecPlayerIface::Config player_config{
        .bits = AUDIO_PCM_BITS,
        .channels = AUDIO_PCM_CHANNELS,
        .sample_rate = AUDIO_PCM_SAMPLE_RATE,
    };
    BROOKESIA_CHECK_FALSE_RETURN(player_iface->open(player_config), false, "Failed to open audio player");
    lib_utils::FunctionGuard close_player_guard([player_iface]() {
        player_iface->close();
    });

    auto get_volume_result = DeviceHelper::call_function_sync<double>(DeviceHelper::FunctionId::GetAudioPlayerVolume);
    BROOKESIA_CHECK_FALSE_RETURN(
        get_volume_result, false, "Failed to get audio player volume: %1%", get_volume_result.error()
    );

    auto get_mute_result = DeviceHelper::call_function_sync<bool>(DeviceHelper::FunctionId::GetAudioPlayerMute);
    BROOKESIA_CHECK_FALSE_RETURN(
        get_mute_result, false, "Failed to get audio player mute: %1%", get_mute_result.error()
    );

    const auto current_volume = get_volume_result.value();
    const auto next_volume = (current_volume < 100.0) ? (current_volume + 1.0) : 99.0;
    const auto pcm_payload = get_audio_pcm_payload();

    BROOKESIA_LOGI("Current volume: %1%", current_volume);
    BROOKESIA_LOGI("Current mute: %1%", get_mute_result.value());
    BROOKESIA_CHECK_FALSE_RETURN(!pcm_payload.empty(), false, "Embedded PCM payload is empty");

    if (get_mute_result.value()) {
        auto ensure_unmute_result = DeviceHelper::call_function_sync(DeviceHelper::FunctionId::SetAudioPlayerMute, false);
        BROOKESIA_CHECK_FALSE_RETURN(
            ensure_unmute_result, false, "Failed to unmute audio player before volume validation: %1%",
            ensure_unmute_result.error()
        );
        boost::this_thread::sleep_for(boost::chrono::milliseconds(EXAMPLE_TEST_ACTION_DELAY_MS));
    }

    AudioVolumeEventMonitor volume_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(volume_event_monitor.start(), false, "Failed to start audio volume event monitor");
    AudioMuteEventMonitor mute_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(mute_event_monitor.start(), false, "Failed to start audio mute event monitor");

    auto set_volume_result = DeviceHelper::call_function_sync(DeviceHelper::FunctionId::SetAudioPlayerVolume, next_volume);
    BROOKESIA_CHECK_FALSE_RETURN(
        set_volume_result, false, "Failed to set audio player volume: %1%", set_volume_result.error()
    );
    auto got_volume_event = volume_event_monitor.wait_for(std::vector<service::EventItem> {
        next_volume
    }, DEVICE_WAIT_EVENT_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_RETURN(got_volume_event, false, "Failed to wait for audio volume event");
    auto last_volume_event = volume_event_monitor.get_last<double>();
    if (last_volume_event.has_value()) {
        BROOKESIA_LOGI("[Monitor] Audio volume changed event: %1%", std::get<0>(last_volume_event.value()));
    }

    boost::this_thread::sleep_for(boost::chrono::milliseconds(EXAMPLE_TEST_ACTION_DELAY_MS));
    BROOKESIA_CHECK_FALSE_RETURN(
        player_iface->write_data(pcm_payload.data(), pcm_payload.size()), false, "Failed to write PCM payload"
    );

    get_volume_result = DeviceHelper::call_function_sync<double>(DeviceHelper::FunctionId::GetAudioPlayerVolume);
    BROOKESIA_CHECK_FALSE_RETURN(
        get_volume_result, false, "Failed to re-get audio player volume: %1%", get_volume_result.error()
    );
    BROOKESIA_LOGI("Updated volume: %1%", get_volume_result.value());

    auto set_mute_result = DeviceHelper::call_function_sync(DeviceHelper::FunctionId::SetAudioPlayerMute, true);
    BROOKESIA_CHECK_FALSE_RETURN(
        set_mute_result, false, "Failed to mute audio player: %1%", set_mute_result.error()
    );
    auto got_mute_event = mute_event_monitor.wait_for(std::vector<service::EventItem> {
        true
    }, DEVICE_WAIT_EVENT_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_RETURN(got_mute_event, false, "Failed to wait for audio mute event");

    boost::this_thread::sleep_for(boost::chrono::milliseconds(EXAMPLE_TEST_ACTION_DELAY_MS));
    BROOKESIA_CHECK_FALSE_RETURN(
        player_iface->write_data(pcm_payload.data(), pcm_payload.size()), false, "Failed to write muted PCM payload"
    );

    auto unset_mute_result = DeviceHelper::call_function_sync(DeviceHelper::FunctionId::SetAudioPlayerMute, false);
    BROOKESIA_CHECK_FALSE_RETURN(
        unset_mute_result, false, "Failed to unmute audio player: %1%", unset_mute_result.error()
    );
    auto got_unmute_event = mute_event_monitor.wait_for(std::vector<service::EventItem> {
        false
    }, DEVICE_WAIT_EVENT_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_RETURN(got_unmute_event, false, "Failed to wait for audio unmute event");
    auto last_mute_event = mute_event_monitor.get_last<bool>();
    if (last_mute_event.has_value()) {
        BROOKESIA_LOGI("[Monitor] Audio mute changed event: %1%", std::get<0>(last_mute_event.value()));
    }

    boost::this_thread::sleep_for(boost::chrono::milliseconds(EXAMPLE_TEST_ACTION_DELAY_MS));
    BROOKESIA_CHECK_FALSE_RETURN(
        player_iface->write_data(pcm_payload.data(), pcm_payload.size()), false, "Failed to write restored PCM payload"
    );

    get_mute_result = DeviceHelper::call_function_sync<bool>(DeviceHelper::FunctionId::GetAudioPlayerMute);
    BROOKESIA_CHECK_FALSE_RETURN(
        get_mute_result, false, "Failed to re-get audio player mute: %1%", get_mute_result.error()
    );
    BROOKESIA_LOGI("Mute after restore: %1%", get_mute_result.value());

    BROOKESIA_LOGI("\n\n--- Demo: Audio Controls Completed ---\n");

    return true;
}

static bool demo_storage_query(const DeviceHelper::Capabilities &capabilities)
{
    BROOKESIA_LOGI("\n\n--- Demo: Storage Query ---\n");

    if (!has_capability(capabilities, hal::StorageFsIface::NAME)) {
        BROOKESIA_LOGW("Storage file-system is not available, skip");
        return true;
    }

    auto result = DeviceHelper::call_function_sync<boost::json::array>(DeviceHelper::FunctionId::GetStorageFileSystems);
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to get storage file systems: %1%", result.error());

    BROOKESIA_LOGI(
        "Storage file systems: %1%", boost::json::serialize(boost::json::value(result.value()))
    );

    BROOKESIA_LOGI("\n\n--- Demo: Storage Query Completed ---\n");

    return true;
}

static bool demo_power_battery(const DeviceHelper::Capabilities &capabilities)
{
    BROOKESIA_LOGI("\n\n--- Demo: Power Battery ---\n");

    if (!has_capability(capabilities, hal::PowerBatteryIface::NAME)) {
        BROOKESIA_LOGW("Power battery is not available, skip");
        return true;
    }

    PowerBatteryStateEventMonitor state_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(
        state_event_monitor.start(), false, "Failed to start power battery state event monitor"
    );
    PowerBatteryChargeConfigEventMonitor charge_config_event_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(
        charge_config_event_monitor.start(), false, "Failed to start power battery charge config event monitor"
    );

    DeviceHelper::PowerBatteryInfo battery_info{};
    BROOKESIA_CHECK_FALSE_RETURN(get_power_battery_info(battery_info), false, "Failed to get power battery info");
    BROOKESIA_LOGI("Battery info: %1%", boost::json::serialize(BROOKESIA_DESCRIBE_TO_JSON(battery_info)));

    DeviceHelper::PowerBatteryState battery_state{};
    BROOKESIA_CHECK_FALSE_RETURN(get_power_battery_state(battery_state), false, "Failed to get power battery state");
    BROOKESIA_LOGI("Battery state: %1%", boost::json::serialize(BROOKESIA_DESCRIBE_TO_JSON(battery_state)));

    DeviceHelper::PowerBatteryChargeConfig charge_config{};
    const auto has_charge_config = battery_info.has_ability(hal::PowerBatteryIface::Ability::ChargeConfig);
    if (has_charge_config) {
        BROOKESIA_CHECK_FALSE_RETURN(
            get_power_battery_charge_config(charge_config), false, "Failed to get power battery charge config"
        );
        BROOKESIA_LOGI("Battery charge config: %1%", boost::json::serialize(BROOKESIA_DESCRIBE_TO_JSON(charge_config)));
    } else {
        BROOKESIA_LOGW("Power battery charge config is not supported by this board, skip charge config query");
    }

    if (battery_info.has_ability(hal::PowerBatteryIface::Ability::ChargerControl) && has_charge_config) {
        auto set_charging_result = DeviceHelper::call_function_sync(
                                       DeviceHelper::FunctionId::SetPowerBatteryChargingEnabled, charge_config.enabled
                                   );
        BROOKESIA_CHECK_FALSE_RETURN(
            set_charging_result, false, "Failed to refresh battery charging state: %1%", set_charging_result.error()
        );

        auto got_state_event = state_event_monitor.wait_for_any(DEVICE_WAIT_EVENT_TIMEOUT_MS);
        if (got_state_event) {
            auto last_state_event = state_event_monitor.get_last<boost::json::object>();
            if (last_state_event.has_value()) {
                BROOKESIA_LOGI(
                    "[Monitor] Battery state event: %1%", boost::json::serialize(std::get<0>(last_state_event.value()))
                );
            }
        } else {
            BROOKESIA_LOGW("No battery state event received after charging state refresh");
        }

        auto got_charge_config_event = charge_config_event_monitor.wait_for_any(DEVICE_WAIT_EVENT_TIMEOUT_MS);
        if (got_charge_config_event) {
            auto last_charge_config_event = charge_config_event_monitor.get_last<boost::json::object>();
            if (last_charge_config_event.has_value()) {
                BROOKESIA_LOGI(
                    "[Monitor] Battery charge config event: %1%",
                    boost::json::serialize(std::get<0>(last_charge_config_event.value()))
                );
            }
        } else {
            BROOKESIA_LOGW("No battery charge config event received after charging state refresh");
        }
    } else {
        BROOKESIA_LOGW(
            "Power battery charging control or readable charge config is not supported by this board, skip charging "
            "control demo"
        );
    }

    BROOKESIA_LOGI("\n\n--- Demo: Power Battery Completed ---\n");

    return true;
}

static bool demo_reset_data(const DeviceHelper::Capabilities &capabilities)
{
    BROOKESIA_LOGI("\n\n--- Demo: Reset Data ---\n");

    auto reset_result = DeviceHelper::call_function_sync(DeviceHelper::FunctionId::ResetData);
    BROOKESIA_CHECK_FALSE_RETURN(reset_result, false, "Failed to reset device data: %1%", reset_result.error());

    boost::this_thread::sleep_for(boost::chrono::milliseconds(EXAMPLE_TEST_ACTION_DELAY_MS));

    if (has_capability(capabilities, hal::DisplayBacklightIface::NAME)) {
        auto brightness_result = DeviceHelper::call_function_sync<double>(
                                     DeviceHelper::FunctionId::GetDisplayBacklightBrightness
                                 );
        BROOKESIA_CHECK_FALSE_RETURN(
            brightness_result, false, "Failed to get reset display brightness: %1%", brightness_result.error()
        );

        auto on_off_result = DeviceHelper::call_function_sync<bool>(
                                 DeviceHelper::FunctionId::GetDisplayBacklightOnOff
                             );
        BROOKESIA_CHECK_FALSE_RETURN(
            on_off_result, false, "Failed to get reset display on/off: %1%", on_off_result.error()
        );

        BROOKESIA_LOGI(
            "Reset display state: brightness=%1%, is_on=%2%", brightness_result.value(), on_off_result.value()
        );
    }

    if (has_capability(capabilities, hal::AudioCodecPlayerIface::NAME)) {
        auto volume_result = DeviceHelper::call_function_sync<double>(DeviceHelper::FunctionId::GetAudioPlayerVolume);
        BROOKESIA_CHECK_FALSE_RETURN(
            volume_result, false, "Failed to get reset audio volume: %1%", volume_result.error()
        );

        auto mute_result = DeviceHelper::call_function_sync<bool>(DeviceHelper::FunctionId::GetAudioPlayerMute);
        BROOKESIA_CHECK_FALSE_RETURN(
            mute_result, false, "Failed to get reset audio mute: %1%", mute_result.error()
        );

        BROOKESIA_LOGI("Reset audio state: volume=%1%, is_muted=%2%", volume_result.value(), mute_result.value());
    }

    BROOKESIA_LOGI("\n\n--- Demo: Reset Data Completed ---\n");

    return true;
}
