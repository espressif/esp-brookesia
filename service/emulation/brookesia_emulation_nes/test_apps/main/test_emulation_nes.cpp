/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "brookesia/lib_utils.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/service_display.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/emulation_nes.hpp"

using namespace esp_brookesia;

namespace {

using DisplayService = service::Display;
using DisplayHelper = service::helper::Display;
using NesHelper = service::helper::Nes;

constexpr uint32_t TEST_OUTPUT_WIDTH = 64;
constexpr uint32_t TEST_OUTPUT_HEIGHT = 64;
constexpr size_t TEST_OUTPUT_STRIDE = TEST_OUTPUT_WIDTH * 2;
constexpr uint32_t EVENT_WAIT_MS = 100;

auto &service_manager = service::ServiceManager::get_instance();
service::ServiceBinding display_binding;
service::ServiceBinding nes_binding;
std::vector<uint8_t> display_buffer;

struct FrameCapture {
    std::atomic<int> count = 0;
    std::atomic<uint32_t> x = 0;
    std::atomic<uint32_t> y = 0;
    std::atomic<uint32_t> width = 0;
    std::atomic<uint32_t> height = 0;

    void update(const DisplayService::FrameInfo &frame)
    {
        x = frame.x;
        y = frame.y;
        width = frame.width;
        height = frame.height;
        ++count;
    }
};

#ifndef BROOKESIA_EMULATION_NES_TEST_ROM_PATH
#define BROOKESIA_EMULATION_NES_TEST_ROM_PATH ""
#endif

std::filesystem::path make_test_dir()
{
    auto dir = std::filesystem::temp_directory_path() / "brookesia_emulation_nes_test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    TEST_ASSERT_FALSE_MESSAGE(static_cast<bool>(ec), ec.message().c_str());
    return dir;
}

void write_test_rom(const std::filesystem::path &path)
{
    FILE *file = std::fopen(path.c_str(), "wb");
    TEST_ASSERT_NOT_NULL(file);
    std::vector<uint8_t> data;
    data.resize(16 + (16 * 1024) + (8 * 1024), 0);
    data[0] = 'N';
    data[1] = 'E';
    data[2] = 'S';
    data[3] = 0x1A;
    data[4] = 1;
    data[5] = 1;
    constexpr size_t PRG_OFFSET = 16;
    data[PRG_OFFSET + 0] = 0x4C;
    data[PRG_OFFSET + 1] = 0x00;
    data[PRG_OFFSET + 2] = 0x80;
    data[PRG_OFFSET + 0x3FFC] = 0x00;
    data[PRG_OFFSET + 0x3FFD] = 0x80;
    TEST_ASSERT_EQUAL(data.size(), std::fwrite(data.data(), 1, data.size(), file));
    std::fclose(file);
}

std::pair<std::filesystem::path, bool> make_loadable_rom(const std::filesystem::path &test_dir)
{
    if constexpr (std::string_view(BROOKESIA_EMULATION_NES_TEST_ROM_PATH).size() > 0) {
        auto demo_rom = std::filesystem::path(BROOKESIA_EMULATION_NES_TEST_ROM_PATH);
        if (std::filesystem::exists(demo_rom)) {
            return {demo_rom, true};
        }
    }
    auto rom_path = test_dir / "demo.nes";
    write_test_rom(rom_path);
    return {rom_path, false};
}

bool startup()
{
    display_binding.release();
    nes_binding.release();
    display_buffer.assign(TEST_OUTPUT_HEIGHT * TEST_OUTPUT_STRIDE, 0);
    if (!service_manager.start()) {
        return false;
    }
    display_binding = service_manager.bind(DisplayHelper::get_name().data());
    nes_binding = service_manager.bind(NesHelper::get_name().data());
    if (!display_binding.is_valid() || !nes_binding.is_valid()) {
        return false;
    }

    DisplayService::BufferOutputConfig output{
        .name = "NesTestOutput",
        .width = TEST_OUTPUT_WIDTH,
        .height = TEST_OUTPUT_HEIGHT,
        .pixel_format = DisplayService::PixelFormat::RGB565,
        .buffer = service::RawBuffer(display_buffer.data(), display_buffer.size()),
        .stride_bytes = TEST_OUTPUT_STRIDE,
    };
    auto output_result = DisplayService::get_instance().register_output(std::move(output));
    return output_result.has_value();
}

void shutdown()
{
    auto unregister_result = DisplayService::get_instance().unregister_output("NesTestOutput");
    if (!unregister_result) {
        std::printf("Failed to unregister NES test output: %s\n", unregister_result.error().c_str());
    }
    nes_binding.release();
    display_binding.release();
    service_manager.deinit();
}

} // namespace

BROOKESIA_TEST_CASE(
    test_nes_load_start_pause_save,
    "Test ServiceNES - load start pause save",
    "[service][nes]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to start NES test services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const auto test_dir = make_test_dir();
    const auto save_path = test_dir / "demo_nes.save";
    const auto [rom_path, expect_visible_frame] = make_loadable_rom(test_dir);

    NesHelper::Config config{
        .rom_path = rom_path.string(),
        .save_path = save_path.string(),
        .display_output_name = "NesTestOutput",
        .display_source_name = "NES",
        .video_area = NesHelper::VideoArea{},
        .video_mode = NesHelper::VideoMode::Fit,
        .audio_mode = NesHelper::AudioMode::Disabled,
        .auto_activate_display = true,
    };
    auto load_result = NesHelper::call_function_sync(
                           NesHelper::FunctionId::Load, BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                       );
    TEST_ASSERT_TRUE_MESSAGE(load_result, load_result ? "" : load_result.error().c_str());

    FrameCapture frame_capture;
    auto frame_connection = DisplayService::get_instance().connect_frame_presented(
                                "NesTestOutput",
    [&frame_capture](const std::string &, const DisplayService::FrameInfo & frame) {
        frame_capture.update(frame);
    }
                            );
    auto start_result = NesHelper::call_function_sync(NesHelper::FunctionId::Start);
    TEST_ASSERT_TRUE_MESSAGE(start_result, start_result ? "" : start_result.error().c_str());
    lib_utils::test_adapter::delay_ms(EVENT_WAIT_MS);
    (void)expect_visible_frame;
    TEST_ASSERT_TRUE_MESSAGE(frame_capture.count.load() > 0, "NES runtime did not present a frame");
    TEST_ASSERT_EQUAL_UINT32(0, frame_capture.x.load());
    TEST_ASSERT_EQUAL_UINT32(4, frame_capture.y.load());
    TEST_ASSERT_EQUAL_UINT32(64, frame_capture.width.load());
    TEST_ASSERT_EQUAL_UINT32(56, frame_capture.height.load());
    frame_connection.disconnect();

    NesHelper::GamepadState gamepad{
        .right = true,
        .a = true,
    };
    auto gamepad_result = NesHelper::call_function_sync(
                              NesHelper::FunctionId::SetGamepadState, BROOKESIA_DESCRIBE_TO_JSON(gamepad).as_object()
                          );
    TEST_ASSERT_TRUE_MESSAGE(gamepad_result, gamepad_result ? "" : gamepad_result.error().c_str());

    auto pause_result = NesHelper::call_function_sync(NesHelper::FunctionId::Pause);
    TEST_ASSERT_TRUE_MESSAGE(pause_result, pause_result ? "" : pause_result.error().c_str());
    auto state_result = NesHelper::call_function_sync<std::string>(NesHelper::FunctionId::GetState);
    TEST_ASSERT_TRUE_MESSAGE(state_result, state_result ? "" : state_result.error().c_str());
    TEST_ASSERT_EQUAL_STRING("Paused", state_result->c_str());

    auto save_result = NesHelper::call_function_sync(NesHelper::FunctionId::Save);
    TEST_ASSERT_TRUE_MESSAGE(save_result, save_result ? "" : save_result.error().c_str());
    TEST_ASSERT_TRUE(std::filesystem::exists(save_path));

    auto stop_result = NesHelper::call_function_sync(NesHelper::FunctionId::Stop);
    TEST_ASSERT_TRUE_MESSAGE(stop_result, stop_result ? "" : stop_result.error().c_str());
}

BROOKESIA_TEST_CASE(
    test_nes_explicit_video_area,
    "Test ServiceNES - explicit video area",
    "[service][nes]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to start NES test services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const auto [rom_path, expect_visible_frame] = make_loadable_rom(make_test_dir());
    NesHelper::Config config{
        .rom_path = rom_path.string(),
        .save_path = {},
        .display_output_name = "NesTestOutput",
        .display_source_name = "NES",
        .video_area = NesHelper::VideoArea{
            .x = 8,
            .y = 4,
            .width = 40,
            .height = 40,
        },
        .video_mode = NesHelper::VideoMode::Fit,
        .audio_mode = NesHelper::AudioMode::Disabled,
        .auto_activate_display = true,
    };
    auto load_result = NesHelper::call_function_sync(
                           NesHelper::FunctionId::Load, BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                       );
    TEST_ASSERT_TRUE_MESSAGE(load_result, load_result ? "" : load_result.error().c_str());

    FrameCapture frame_capture;
    auto frame_connection = DisplayService::get_instance().connect_frame_presented(
                                "NesTestOutput",
    [&frame_capture](const std::string &, const DisplayService::FrameInfo & frame) {
        frame_capture.update(frame);
    }
                            );
    auto start_result = NesHelper::call_function_sync(NesHelper::FunctionId::Start);
    TEST_ASSERT_TRUE_MESSAGE(start_result, start_result ? "" : start_result.error().c_str());
    lib_utils::test_adapter::delay_ms(EVENT_WAIT_MS);
    (void)expect_visible_frame;
    TEST_ASSERT_TRUE_MESSAGE(frame_capture.count.load() > 0, "NES runtime did not present a frame");
    TEST_ASSERT_EQUAL_UINT32(8, frame_capture.x.load());
    TEST_ASSERT_EQUAL_UINT32(6, frame_capture.y.load());
    TEST_ASSERT_EQUAL_UINT32(40, frame_capture.width.load());
    TEST_ASSERT_EQUAL_UINT32(35, frame_capture.height.load());
    frame_connection.disconnect();

    auto stop_result = NesHelper::call_function_sync(NesHelper::FunctionId::Stop);
    TEST_ASSERT_TRUE_MESSAGE(stop_result, stop_result ? "" : stop_result.error().c_str());
}

BROOKESIA_TEST_CASE(
    test_nes_rejects_invalid_video_area,
    "Test ServiceNES - rejects invalid video area",
    "[service][nes]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to start NES test services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    const auto [rom_path, expect_visible_frame] = make_loadable_rom(make_test_dir());
    (void)expect_visible_frame;

    NesHelper::Config native_config{
        .rom_path = rom_path.string(),
        .save_path = {},
        .display_output_name = "NesTestOutput",
        .video_area = NesHelper::VideoArea{},
        .video_mode = NesHelper::VideoMode::Native,
        .audio_mode = NesHelper::AudioMode::Disabled,
    };
    auto load_result = NesHelper::call_function_sync(
                           NesHelper::FunctionId::Load, BROOKESIA_DESCRIBE_TO_JSON(native_config).as_object()
                       );
    TEST_ASSERT_FALSE(load_result);
    TEST_ASSERT_TRUE_MESSAGE(
        load_result.error().find("native frame") != std::string::npos, load_result.error().c_str()
    );

    NesHelper::Config out_of_bounds_config{
        .rom_path = rom_path.string(),
        .save_path = {},
        .display_output_name = "NesTestOutput",
        .video_area = NesHelper::VideoArea{
            .x = TEST_OUTPUT_WIDTH,
            .y = 0,
        },
        .video_mode = NesHelper::VideoMode::Fit,
        .audio_mode = NesHelper::AudioMode::Disabled,
    };
    load_result = NesHelper::call_function_sync(
                      NesHelper::FunctionId::Load, BROOKESIA_DESCRIBE_TO_JSON(out_of_bounds_config).as_object()
                  );
    TEST_ASSERT_FALSE(load_result);
    TEST_ASSERT_TRUE_MESSAGE(
        load_result.error().find("video area") != std::string::npos, load_result.error().c_str()
    );
}

BROOKESIA_TEST_CASE(
    test_nes_load_rejects_missing_rom,
    "Test ServiceNES - load rejects missing ROM",
    "[service][nes]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to start NES test services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    NesHelper::Config config{
        .rom_path = (make_test_dir() / "missing.nes").string(),
        .save_path = {},
        .display_output_name = "NesTestOutput",
        .video_area = NesHelper::VideoArea{},
        .audio_mode = NesHelper::AudioMode::Disabled,
    };
    auto load_result = NesHelper::call_function_sync(
                           NesHelper::FunctionId::Load, BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                       );
    TEST_ASSERT_FALSE(load_result);
}
