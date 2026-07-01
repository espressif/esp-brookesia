/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_linux.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"

using namespace esp_brookesia;

namespace {

bool expect(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "[FAILED] " << message << std::endl;
    }

    return condition;
}

template<typename T>
hal::InterfaceHandle<T> get_iface(const char *expected_name, bool &ok)
{
    auto iface = hal::acquire_interface<T>(expected_name);
    ok &= expect(static_cast<bool>(iface), std::string(expected_name) + " is available");
    ok &= expect(iface.instance_name() == expected_name, std::string(expected_name) + " has expected instance name");

    return iface;
}

bool test_general()
{
    bool ok = true;
    auto iface = get_iface<hal::system::BoardInfoIface>(hal::SystemLinuxDevice::BOARD_INFO_IFACE_NAME, ok);
    if (!iface) {
        return false;
    }

    const auto &info = iface->get_info();
    ok &= expect(info.is_valid(), "board info is valid");
    ok &= expect(info.name == "ESP-Brookesia Linux", "board name matches");
    ok &= expect(!info.chip.empty(), "board chip is available");
    ok &= expect(!info.version.empty(), "board version is available");
    ok &= expect(!info.description.empty(), "board description is available");
    ok &= expect(info.manufacturer == "Espressif", "board manufacturer matches");

    return ok;
}

bool test_audio()
{
    bool ok = true;
    auto player = get_iface<hal::audio::CodecPlayerIface>(hal::AudioLinuxDevice::PLAYER_IFACE_NAME, ok);
    auto recorder = get_iface<hal::audio::CodecRecorderIface>(hal::AudioLinuxDevice::RECORDER_IFACE_NAME, ok);
    const auto playback_name = hal::AudioLinuxDevice::get_playback_iface_name();
    const auto encoder_name = hal::AudioLinuxDevice::get_encoder_iface_name(0);
    const auto decoder_name = hal::AudioLinuxDevice::get_decoder_iface_name(0);
    auto playback = get_iface<hal::audio::PlaybackIface>(playback_name.c_str(), ok);
    auto encoder = get_iface<hal::audio::EncoderIface>(encoder_name.c_str(), ok);
    auto decoder = get_iface<hal::audio::DecoderIface>(decoder_name.c_str(), ok);
    if (!player || !recorder || !playback || !encoder || !decoder) {
        return false;
    }

    ok &= expect(player->open(hal::audio::CodecPlayerIface::Config{
        .bits = 16,
        .channels = 1,
        .sample_rate = 16000,
    }), "audio player opens");
    ok &= expect(player->set_volume(42), "audio player sets volume");
    ok &= expect(player->is_pa_on_off_supported(), "audio player supports PA on/off");
    ok &= expect(player->set_pa_on_off(false), "audio player sets PA off");
    ok &= expect(!player->is_pa_on(), "audio player PA is off");

    std::array<uint8_t, 4> player_data = {0x11, 0x22, 0x33, 0x44};
    ok &= expect(player->write_data(player_data.data(), player_data.size()), "audio player writes payload");
    ok &= expect(!player->write_data(nullptr, player_data.size()), "audio player rejects null payload");
    player->close();

    const auto &recorder_info = recorder->get_info();
    ok &= expect(recorder_info.bits == 16, "audio recorder bits match");
    ok &= expect(recorder_info.channels == 2, "audio recorder channels match");
    ok &= expect(recorder_info.sample_rate == 16000, "audio recorder sample rate matches");
    ok &= expect(recorder_info.mic_layout == "LR", "audio recorder mic layout matches");
    ok &= expect(recorder->open(), "audio recorder opens");

    std::array<uint8_t, 8> recorder_data = {};
    ok &= expect(recorder->read_data(recorder_data.data(), recorder_data.size()), "audio recorder reads payload");
    ok &= expect(
    std::all_of(recorder_data.begin(), recorder_data.end(), [](uint8_t value) {
        return value == 0x5A;
    }),
    "audio recorder fills deterministic payload"
          );
    ok &= expect(!recorder->read_data(nullptr, recorder_data.size()), "audio recorder rejects null destination");
    recorder->close();

    hal::audio::PlayState last_playback_state = hal::audio::PlayState::Idle;
    ok &= expect(playback->open([&last_playback_state](hal::audio::PlayState state) {
        last_playback_state = state;
    }), "audio playback opens");
    ok &= expect(playback->is_opened(), "audio playback is opened");
    ok &= expect(playback->play("file:///tmp/brookesia.wav"), "audio playback plays URL");
    ok &= expect(last_playback_state == hal::audio::PlayState::Playing, "audio playback reports playing state");
    ok &= expect(playback->pause(), "audio playback pauses");
    ok &= expect(last_playback_state == hal::audio::PlayState::Paused, "audio playback reports paused state");
    ok &= expect(playback->resume(), "audio playback resumes");
    ok &= expect(playback->stop(), "audio playback stops");
    playback->close();
    ok &= expect(!playback->is_opened(), "audio playback closes");
    ok &= expect(encoder->get_afe_wake_words().empty(), "audio encoder stub returns no wake words");

    bool saw_disabled_afe_event = false;
    ok &= expect(encoder->start({
        .type = hal::audio::CodecFormat::PCM,
        .general = {
            .channels = 1,
            .sample_bits = 16,
            .sample_rate = 16000,
            .frame_duration = 20,
        },
        .fetch_interval_ms = 1,
        .fetch_data_size = 16,
    }, {
        .afe_event = [&saw_disabled_afe_event](hal::audio::AfeEvent event)
        {
            saw_disabled_afe_event = (event == hal::audio::AfeEvent::WakeStart);
        },
    }
                               ), "audio encoder starts with AFE disabled"
                );
    ok &= expect(!saw_disabled_afe_event, "audio encoder does not emit AFE event by default");
    encoder->stop();

    bool saw_afe_event = false;
    bool saw_recorder_data = false;
    ok &= expect(encoder->start({
        .type = hal::audio::CodecFormat::PCM,
        .general = {
            .channels = 1,
            .sample_bits = 16,
            .sample_rate = 16000,
            .frame_duration = 20,
        },
        .fetch_interval_ms = 1,
        .fetch_data_size = 16,
        .enable_afe = true,
    }, {
        .afe_event = [&saw_afe_event](hal::audio::AfeEvent event)
        {
            saw_afe_event = (event == hal::audio::AfeEvent::WakeStart);
        },
        .recorder_data = [&saw_recorder_data](const uint8_t *data, size_t size)
        {
            saw_recorder_data = (data != nullptr) && (size > 0);
        },
    }
                               ), "audio encoder starts"
                );
    std::array<uint8_t, 32> encoder_data = {};
    const int encoder_data_size = encoder->read_encoded_data(encoder_data.data(), encoder_data.size());
    ok &= expect(encoder->is_started(), "audio encoder is started");
    ok &= expect(saw_afe_event, "audio encoder emits deterministic AFE event");
    ok &= expect(saw_recorder_data, "audio encoder emits deterministic recorder data");
    ok &= expect(encoder_data_size > 0, "audio encoder reads deterministic encoder data");
    encoder->pause();
    ok &= expect(encoder->is_paused(), "audio encoder pauses");
    encoder->resume();
    ok &= expect(!encoder->is_paused(), "audio encoder resumes");
    encoder->stop();
    ok &= expect(!encoder->is_started(), "audio encoder stops");

    ok &= expect(decoder->start({
        .type = hal::audio::CodecFormat::PCM,
        .general = {
            .channels = 1,
            .sample_bits = 16,
            .sample_rate = 16000,
            .frame_duration = 20,
        },
    }), "audio decoder starts"
                );
    ok &= expect(decoder->is_started(), "audio decoder is started");
    ok &= expect(decoder->feed_data(player_data.data(), player_data.size()), "audio decoder feeds data");
    ok &= expect(!decoder->feed_data(nullptr, player_data.size()), "audio decoder rejects null data");
    decoder->stop();
    ok &= expect(!decoder->is_started(), "audio decoder stops");

    return ok;
}

bool test_video()
{
    bool ok = true;
    const auto encoder_name = hal::VideoLinuxDevice::get_encoder_iface_name(0);
    const auto decoder_name = hal::VideoLinuxDevice::get_decoder_iface_name(0);
    auto encoder = get_iface<hal::video::EncoderIface>(encoder_name.c_str(), ok);
    auto decoder = get_iface<hal::video::DecoderIface>(decoder_name.c_str(), ok);
    if (!encoder || !decoder) {
        return false;
    }

    std::string error_message;
    size_t encoder_frame_count = 0;
    const auto encoder_callback = [&encoder_frame_count](
                                      size_t sink_index, const hal::video::EncoderSinkInfo & sink_info,
                                      const uint8_t *data, size_t size
    ) {
        if ((sink_index == 0) && (sink_info.format == hal::video::EncoderSinkFormat::RGB565) &&
                (data != nullptr) && (size == (static_cast<size_t>(sink_info.width) * sink_info.height * 2))) {
            encoder_frame_count++;
        }
    };
    ok &= expect(encoder->open( {
        .sinks = {
            {
                .format = hal::video::EncoderSinkFormat::RGB565,
                .width = 64,
                .height = 48,
                .fps = 15,
            },
        },
        .enable_stream_mode = false,
        .source = hal::video::EncoderSourceConfig{
            .v4l2_buffer_count = 2,
        },
    },
    encoder_callback, &error_message
                              ),
    "video encoder opens"
                );
    ok &= expect(encoder->is_opened(), "video encoder is opened");
    ok &= expect(encoder->start(&error_message), "video encoder starts");
    ok &= expect(encoder->is_started(), "video encoder is started");
    ok &= expect(encoder->fetch_frame(0, encoder_callback, &error_message), "video encoder fetches frame 0");
    ok &= expect(encoder->fetch_frame(0, encoder_callback, &error_message), "video encoder fetches frame 1");
    ok &= expect(encoder->fetch_frame(0, encoder_callback, &error_message), "video encoder fetches frame 2");
    ok &= expect(encoder_frame_count == 3, "video encoder emits fetched frames");
    ok &= expect(!encoder->fetch_frame(1, encoder_callback, &error_message), "video encoder rejects bad sink index");
    ok &= expect(encoder->stop(&error_message), "video encoder stops");
    encoder->close();
    ok &= expect(!encoder->is_opened(), "video encoder closes");

    std::mutex stream_mutex;
    std::condition_variable stream_cv;
    size_t stream_frame_count = 0;
    const auto stream_callback = [&](size_t sink_index, const hal::video::EncoderSinkInfo & sink_info,
    const uint8_t *data, size_t size) {
        if ((sink_index == 0) && (sink_info.format == hal::video::EncoderSinkFormat::RGB565) &&
                (data != nullptr) && (size == (static_cast<size_t>(sink_info.width) * sink_info.height * 2))) {
            {
                std::lock_guard lock(stream_mutex);
                stream_frame_count++;
            }
            stream_cv.notify_all();
        }
    };
    ok &= expect(encoder->open({
        .sinks = {
            {
                .format = hal::video::EncoderSinkFormat::RGB565,
                .width = 64,
                .height = 48,
                .fps = 15,
            },
        },
        .enable_stream_mode = true,
        .source = hal::video::EncoderSourceConfig{
            .v4l2_buffer_count = 2,
        },
    }, stream_callback, &error_message), "video encoder opens stream mode");
    ok &= expect(encoder->start(&error_message), "video encoder starts stream mode");
    {
        std::unique_lock lock(stream_mutex);
        ok &= expect(stream_cv.wait_for(lock, std::chrono::seconds(3), [&stream_frame_count]() {
            return stream_frame_count >= 2;
        }), "video encoder stream emits frames");
    }
    ok &= expect(!encoder->fetch_frame(0, stream_callback, &error_message),
                 "video encoder rejects fetch in stream mode"
                );
    ok &= expect(encoder->stop(&error_message), "video encoder stops stream mode");
    size_t stopped_frame_count = 0;
    {
        std::lock_guard lock(stream_mutex);
        stopped_frame_count = stream_frame_count;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    {
        std::lock_guard lock(stream_mutex);
        ok &= expect(stream_frame_count == stopped_frame_count, "video encoder stream stops emitting frames");
    }
    encoder->close();

    bool saw_decoder_frame = false;
    ok &= expect(decoder->open( {
        .width = 320,
        .height = 240,
        .source_format = hal::video::DecoderSourceFormat::MJPEG,
        .sink_format = hal::video::DecoderSinkFormat::RGB565_LE,
        .enable_stream_mode = false,
        .enable_hw_acceleration = false,
    },
    [&saw_decoder_frame](uint16_t width, uint16_t height, const uint8_t *data, size_t size) {
        saw_decoder_frame = (width == 320) && (height == 240) && (data != nullptr) && (size > 0);
    },
    &error_message
                              ),
    "video decoder opens"
                );
    ok &= expect(decoder->is_opened(), "video decoder is opened");
    ok &= expect(decoder->start(&error_message), "video decoder starts");
    std::array<uint8_t, 8> encoded_frame = {};
    ok &= expect(decoder->feed_frame(encoded_frame.data(), encoded_frame.size(), &error_message),
                 "video decoder feeds frame"
                );
    ok &= expect(saw_decoder_frame, "video decoder emits deterministic frame");
    ok &= expect(!decoder->feed_frame(nullptr, encoded_frame.size(), &error_message),
                 "video decoder rejects null frame"
                );
    ok &= expect(decoder->stop(&error_message), "video decoder stops");
    decoder->close();
    ok &= expect(!decoder->is_opened(), "video decoder closes");

    return ok;
}

bool test_display()
{
    bool ok = true;
    auto panel = get_iface<hal::display::PanelIface>(hal::DisplayLinuxDevice::PANEL_IFACE_NAME, ok);
    auto touch = get_iface<hal::display::TouchIface>(hal::DisplayLinuxDevice::TOUCH_IFACE_NAME, ok);
    auto backlight = get_iface<hal::display::BacklightIface>(hal::DisplayLinuxDevice::BACKLIGHT_IFACE_NAME, ok);
    if (!panel || !touch || !backlight) {
        return false;
    }

    const auto &panel_info = panel->get_info();
    ok &= expect(panel_info.h_res == 320, "display panel horizontal resolution matches");
    ok &= expect(panel_info.v_res == 240, "display panel vertical resolution matches");
    ok &= expect(
              panel_info.pixel_format == hal::display::PanelIface::PixelFormat::RGB565,
              "display panel format matches"
          );

    std::array<uint8_t, 8> pixels = {};
    ok &= expect(panel->draw_bitmap(0, 0, 2, 2, pixels.data()), "display panel draws valid bitmap");
    ok &= expect(!panel->draw_bitmap(0, 0, 321, 2, pixels.data()), "display panel rejects out-of-bounds draw");
    ok &= expect(!panel->draw_bitmap(0, 0, 2, 2, nullptr), "display panel rejects null bitmap");

    hal::display::PanelIface::DriverSpecific panel_specific = {};
    ok &= expect(panel->get_driver_specific(panel_specific), "display panel returns driver-specific placeholder");
    ok &= expect(panel_specific.bus_type == hal::display::PanelIface::BusType::Generic, "display panel bus is generic");
    ok &= expect(panel_specific.io_handle == nullptr, "display panel IO handle is not real");
    ok &= expect(panel_specific.panel_handle == nullptr, "display panel handle is not real");

    std::vector<hal::display::TouchIface::Point> points;
    ok &= expect(touch->read_points(points), "display touch reads points");
    ok &= expect(points.size() == 2, "display touch returns deterministic points");
    ok &= expect(points[0].x == 12 && points[0].y == 34 && points[0].pressure == 56, "first touch point matches");
    ok &= expect(points[1].x == 78 && points[1].y == 90 && points[1].pressure == 12, "second touch point matches");
    ok &= expect(touch->register_interrupt_handler([](void *) {
        return false;
    }, nullptr), "display touch registers interrupt handler");

    hal::display::TouchIface::DriverSpecific touch_specific = {};
    ok &= expect(touch->get_driver_specific(touch_specific), "display touch returns driver-specific placeholder");
    ok &= expect(touch_specific.io_handle == nullptr, "display touch IO handle is not real");
    ok &= expect(touch_specific.touch_handle == nullptr, "display touch handle is not real");

    ok &= expect(backlight->set_brightness(42), "display backlight sets brightness");
    uint8_t brightness = 0;
    ok &= expect(backlight->get_brightness(brightness), "display backlight gets brightness");
    ok &= expect(brightness == 42, "display backlight brightness matches");
    ok &= expect(backlight->is_light_on_off_supported(), "display backlight supports on/off");
    ok &= expect(backlight->set_light_on_off(true), "display backlight turns on");
    ok &= expect(backlight->is_light_on(), "display backlight is on");

    return ok;
}

bool test_power()
{
    bool ok = true;
    auto battery = get_iface<hal::power::BatteryIface>(hal::PowerLinuxDevice::BATTERY_IFACE_NAME, ok);
    if (!battery) {
        return false;
    }

    const auto &info = battery->get_info();
    ok &= expect(info.has_ability(hal::power::BatteryIface::Ability::Voltage), "battery supports voltage");
    ok &= expect(info.has_ability(hal::power::BatteryIface::Ability::Percentage), "battery supports percentage");
    ok &= expect(info.has_ability(hal::power::BatteryIface::Ability::ChargerControl), "battery supports charger control");
    ok &= expect(info.has_ability(hal::power::BatteryIface::Ability::ChargeConfig), "battery supports charge config");

    hal::power::BatteryIface::State state = {};
    ok &= expect(battery->get_state(state), "battery gets state");
    ok &= expect(state.is_present, "battery is present");
    ok &= expect(state.power_source == hal::power::BatteryIface::PowerSource::External, "battery power source matches");
    ok &= expect(
              state.charge_state == hal::power::BatteryIface::ChargeState::ConstantCurrent,
              "battery charge state matches"
          );
    ok &= expect(state.voltage_mv == 3850, "battery voltage matches");
    ok &= expect(state.percentage == 67, "battery percentage matches");
    ok &= expect(state.vbus_voltage_mv == 5000, "battery VBUS voltage matches");

    hal::power::BatteryIface::ChargeConfig config = {};
    ok &= expect(battery->get_charge_config(config), "battery gets charge config");
    ok &= expect(config.enabled, "battery charging is enabled by default");
    ok &= expect(battery->set_charging_enabled(false), "battery disables charging");
    ok &= expect(battery->get_charge_config(config), "battery gets updated charge config");
    ok &= expect(!config.enabled, "battery charging is disabled");

    return ok;
}

bool test_storage()
{
    bool ok = true;
    auto kv = get_iface<hal::storage::KeyValueIface>(hal::StorageLinuxDevice::KEY_VALUE_IFACE_NAME, ok);
    auto fs = get_iface<hal::storage::FileSystemIface>(hal::StorageLinuxDevice::FILE_SYSTEM_IFACE_NAME, ok);
    if (!kv || !fs) {
        return false;
    }

    hal::storage::KeyValueIface::KeyValueMap values = {
        {"flag", true},
        {"count", static_cast<int32_t>(42)},
        {"name", std::string("linux")},
    };
    ok &= expect(kv->set("test", values), "storage KV sets values");

    std::vector<hal::storage::KeyValueIface::EntryInfo> entries;
    ok &= expect(kv->list("test", entries), "storage KV lists namespace");
    ok &= expect(entries.size() == 3, "storage KV entry count matches");

    hal::storage::KeyValueIface::KeyValueMap read_values;
    ok &= expect(kv->get("test", {"flag", "missing"}, read_values), "storage KV gets selected values");
    ok &= expect(read_values.size() == 1, "storage KV skips missing key");
    ok &= expect(std::get<bool>(read_values["flag"]), "storage KV bool value matches");

    ok &= expect(kv->get("test", {}, read_values), "storage KV gets all values");
    ok &= expect(read_values.size() == 3, "storage KV get-all count matches");
    ok &= expect(kv->erase("test", {"flag"}), "storage KV erases one key");
    ok &= expect(kv->get("test", {}, read_values), "storage KV gets values after erase");
    ok &= expect(read_values.size() == 2, "storage KV count after single erase matches");
    ok &= expect(kv->erase("test", {}), "storage KV erases namespace");
    ok &= expect(kv->get("test", {}, read_values), "storage KV gets erased namespace");
    ok &= expect(read_values.empty(), "storage KV namespace is empty");

    const auto &fs_infos = fs->get_all_info();
    ok &= expect(fs_infos.size() == 4, "storage FS entry count matches");
    ok &= expect(fs_infos[0].fs_type == hal::storage::FileSystemIface::FileSystemType::SPIFFS, "first FS type matches");
    ok &= expect(fs_infos[0].medium_type == hal::storage::FileSystemIface::MediumType::Flash, "first FS medium matches");
    ok &= expect(std::string(fs_infos[0].mount_point) == "/spiffs", "first FS mount point matches");
    ok &= expect(!fs_infos[0].supports_directories, "first FS directory support matches");
    ok &= expect(fs_infos[1].fs_type == hal::storage::FileSystemIface::FileSystemType::LittleFS, "second FS type matches");
    ok &= expect(fs_infos[1].medium_type == hal::storage::FileSystemIface::MediumType::Flash, "second FS medium matches");
    ok &= expect(std::string(fs_infos[1].mount_point) == "/littlefs", "second FS mount point matches");
    ok &= expect(fs_infos[1].supports_directories, "second FS directory support matches");
    ok &= expect(fs_infos[2].fs_type == hal::storage::FileSystemIface::FileSystemType::FATFS, "third FS type matches");
    ok &= expect(fs_infos[2].medium_type == hal::storage::FileSystemIface::MediumType::Flash, "third FS medium matches");
    ok &= expect(std::string(fs_infos[2].mount_point) == "/fatfs", "third FS mount point matches");
    ok &= expect(fs_infos[2].supports_directories, "third FS directory support matches");
    ok &= expect(fs_infos[3].fs_type == hal::storage::FileSystemIface::FileSystemType::FATFS, "fourth FS type matches");
    ok &= expect(fs_infos[3].medium_type == hal::storage::FileSystemIface::MediumType::SDCard, "fourth FS medium matches");
    ok &= expect(std::string(fs_infos[3].mount_point) == "/sdcard", "fourth FS mount point matches");
    ok &= expect(fs_infos[3].supports_directories, "fourth FS directory support matches");

    hal::storage::FileSystemIface::Capacity capacity = {};
    ok &= expect(fs->get_capacity("/spiffs", capacity), "storage FS gets SPIFFS capacity");
    ok &= expect(capacity.total_bytes > 0, "storage FS SPIFFS total capacity is available");
    ok &= expect(capacity.used_bytes <= capacity.total_bytes, "storage FS SPIFFS used capacity is valid");
    ok &= expect(fs->get_capacity("/littlefs", capacity), "storage FS gets LittleFS capacity");
    ok &= expect(capacity.total_bytes > 0, "storage FS LittleFS total capacity is available");
    ok &= expect(capacity.used_bytes <= capacity.total_bytes, "storage FS LittleFS used capacity is valid");
    ok &= expect(fs->get_capacity("/fatfs", capacity), "storage FS gets FATFS capacity");
    ok &= expect(capacity.total_bytes > 0, "storage FS FATFS total capacity is available");
    ok &= expect(capacity.used_bytes <= capacity.total_bytes, "storage FS FATFS used capacity is valid");
    ok &= expect(fs->get_capacity("/sdcard", capacity), "storage FS gets SDCard capacity");
    ok &= expect(capacity.total_bytes > 0, "storage FS SDCard total capacity is available");
    ok &= expect(capacity.used_bytes <= capacity.total_bytes, "storage FS SDCard used capacity is valid");
    ok &= expect(!fs->get_capacity("/missing", capacity), "storage FS rejects missing capacity mount point");

    return ok;
}

bool test_general_sntp()
{
    bool ok = true;
    auto sntp = get_iface<hal::network::SntpClientIface>(hal::NetworkLinuxDevice::SNTP_CLIENT_IFACE_NAME, ok);
    if (!sntp) {
        return false;
    }

    std::vector<std::string> servers = {"pool.ntp.org", "time.nist.gov"};
    ok &= expect(sntp->init({
        .servers = servers,
        .timezone = "UTC",
        .use_dhcp = true,
    }), "SNTP linux initializes");
    ok &= expect(sntp->is_initialized(), "SNTP linux is initialized");
    ok &= expect(sntp->get_timezone() == "UTC", "SNTP linux timezone matches");
    ok &= expect(sntp->get_servers().size() == servers.size(), "SNTP linux server count matches");
    ok &= expect(sntp->start(), "SNTP linux starts");
    ok &= expect(sntp->is_running(), "SNTP linux is running");
    ok &= expect(sntp->wait_time_sync(0), "SNTP linux sync wait succeeds");
    ok &= expect(sntp->is_time_synced(), "SNTP linux is synced");
    sntp->stop();
    ok &= expect(!sntp->is_running(), "SNTP linux stops");
    sntp->deinit();
    ok &= expect(!sntp->is_initialized(), "SNTP linux deinitializes");

    return ok;
}

bool test_network_connectivity()
{
    bool ok = true;
    auto connectivity = get_iface<hal::network::ConnectivityIface>(
                            hal::NetworkLinuxDevice::CONNECTIVITY_IFACE_NAME, ok
                        );
    if (!connectivity) {
        return false;
    }

    const auto status = connectivity->get_status();
    if (status.is_local_network_ready()) {
        ok &= expect(status.ip_info.has_value(), "Network connectivity exposes IP info when ready");
    }

    return ok;
}

bool test_wifi()
{
    bool ok = true;
    auto basic = get_iface<hal::wifi::BasicIface>(hal::WifiLinuxDevice::BASIC_IFACE_NAME, ok);
    auto station = get_iface<hal::wifi::StationIface>(hal::WifiLinuxDevice::STATION_IFACE_NAME, ok);
    auto connectivity = get_iface<hal::network::ConnectivityIface>(hal::WifiLinuxDevice::CONNECTIVITY_IFACE_NAME, ok);
    if (!basic || !station || !connectivity) {
        return false;
    }

    auto scheduler = std::make_shared<lib_utils::TaskScheduler>();
    ok &= expect(basic->configure({.task_scheduler = scheduler, .task_group = "wifi-linux"}, {}), "Wi-Fi basic configures");
    ok &= expect(basic->init(), "Wi-Fi basic initializes backend");
    ok &= expect(basic->start(), "Wi-Fi basic starts backend");
    ok &= expect(basic->do_action(hal::wifi::BasicAction::Init), "Wi-Fi basic init action succeeds");
    ok &= expect(basic->do_action(hal::wifi::BasicAction::Start), "Wi-Fi basic start action succeeds");
    ok &= expect(!connectivity->is_network_ready(), "Wi-Fi connectivity starts offline");

    ok &= expect(
              station->set_target_connect_ap_info(hal::wifi::ConnectApInfo("Brookesia-Linux-AP", "password")),
              "Wi-Fi station sets target AP"
          );
    ok &= expect(station->do_action(hal::wifi::StationAction::Connect), "Wi-Fi station connects");
    ok &= expect(connectivity->is_network_ready(), "Wi-Fi connectivity is locally ready after connect");
    ok &= expect(
              connectivity->get_status().state() == hal::network::ConnectivityState::LocalNetworkReady,
              "Wi-Fi connectivity state is local network ready"
          );
    ok &= expect(station->do_action(hal::wifi::StationAction::Disconnect), "Wi-Fi station disconnects");
    ok &= expect(!connectivity->is_network_ready(), "Wi-Fi connectivity is offline after disconnect");

    basic->deinit();

    return ok;
}

bool test_general_http_system_ota()
{
    bool ok = true;
    auto http = get_iface<hal::network::HttpClientIface>(hal::NetworkLinuxDevice::HTTP_CLIENT_IFACE_NAME, ok);
    auto system_ota = get_iface<hal::system::OtaUpdaterIface>(hal::SystemLinuxDevice::OTA_UPDATER_IFACE_NAME, ok);
    if (!http || !system_ota) {
        return false;
    }

    ok &= expect(static_cast<bool>(http->create_transaction()), "HTTP linux creates transactions");

    const auto identity = system_ota->get_target_identity("test");
    ok &= expect(identity.system == "test", "System OTA linux system identity matches");
    ok &= expect(identity.chip == "PC", "System OTA linux chip identity matches");
    ok &= expect(identity.board == "ESP-Brookesia Linux", "System OTA linux board identity matches");
    ok &= expect(!system_ota->get_running_firmware_version().empty(), "System OTA linux version is available");
    ok &= expect(system_ota->get_running_boot_partition_label() == "pc", "System OTA linux boot label matches");
    std::string update_partition;
    ok &= expect(system_ota->get_next_update_partition_label(update_partition), "System OTA linux update label exists");
    ok &= expect(update_partition == "pc", "System OTA linux update label matches");
    ok &= expect(system_ota->confirm_boot(), "System OTA linux confirms boot");

    return ok;
}

bool test_handle_lifetime()
{
    bool ok = true;

    auto first = hal::acquire_first_interface<hal::system::BoardInfoIface>();
    auto second = hal::acquire_interface<hal::system::BoardInfoIface>(hal::SystemLinuxDevice::BOARD_INFO_IFACE_NAME);
    ok &= expect(static_cast<bool>(first), "first board info handle is acquired");
    ok &= expect(static_cast<bool>(second), "second board info handle is acquired");

    first.reset();
    ok &= expect(second->get_info().is_valid(), "second board info handle remains usable");
    second.reset();

    auto reacquired = hal::acquire_first_interface<hal::system::BoardInfoIface>();
    ok &= expect(static_cast<bool>(reacquired), "board info can be reacquired after release");

    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    const auto device_infos = hal::get_device_infos();
    auto has_device = [&device_infos](std::string_view name) {
        return std::any_of(device_infos.begin(), device_infos.end(), [name](const hal::DeviceInfo & info) {
            return info.name == name;
        });
    };
    ok &= expect(has_device(hal::SystemLinuxDevice::DEVICE_NAME), "system linux device is available");
    ok &= expect(has_device(hal::NetworkLinuxDevice::DEVICE_NAME), "network linux device is available");
    ok &= expect(has_device(hal::AudioLinuxDevice::DEVICE_NAME), "audio linux device is available");
    ok &= expect(has_device(hal::VideoLinuxDevice::DEVICE_NAME), "video linux device is available");
    ok &= expect(has_device(hal::DisplayLinuxDevice::DEVICE_NAME), "display linux device is available");
    ok &= expect(has_device(hal::PowerLinuxDevice::DEVICE_NAME), "power linux device is available");
    ok &= expect(has_device(hal::StorageLinuxDevice::DEVICE_NAME), "storage linux device is available");
    ok &= expect(has_device(hal::WifiLinuxDevice::DEVICE_NAME), "Wi-Fi linux device is available");

    ok &= test_general();
    ok &= test_audio();
    ok &= test_video();
    ok &= test_display();
    ok &= test_power();
    ok &= test_storage();
    ok &= test_network_connectivity();
    ok &= test_wifi();
    ok &= test_general_sntp();
    ok &= test_general_http_system_ota();
    ok &= test_handle_lifetime();

    if (!ok) {
        return 1;
    }

    std::cout << "HAL linux smoke test passed" << std::endl;
    return 0;
}
