/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/video/processor.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_video.hpp"

namespace {

using namespace esp_brookesia;
namespace dataflow = service::dataflow;
using Video = service::helper::Video;
using Encoder = service::helper::VideoEncoder<0>;

bool mock_enabled = false;
bool activation_fails = false;
size_t close_count = 0;
size_t present_count = 0;
uint32_t presented_timeout_ms = 0;
std::vector<uint8_t> captured_pixels;
std::vector<uint8_t> presented_pixels;
hal::video::EncoderConfig capture_config;
dataflow::VisualPixelFormat output_format = dataflow::VisualPixelFormat::RGB565;
dataflow::VisualByteOrder output_order = dataflow::VisualByteOrder::Swap16;

class MockEncoder : public hal::video::EncoderIface {
public:
    bool open(const hal::video::EncoderConfig &config, FrameCallback, std::string *) override
    {
        if (opened_) {
            return false;
        }
        capture_config = config;
        opened_ = true;
        return true;
    }

    void close() override
    {
        ++close_count;
        opened_ = false;
        started_ = false;
    }

    bool start(std::string *) override
    {
        started_ = opened_;
        return started_;
    }

    bool stop(std::string *) override
    {
        started_ = false;
        return true;
    }

    bool fetch_frame(size_t sink, FrameCallback callback, std::string *) override
    {
        callback(sink, capture_config.sinks.at(sink), captured_pixels.data(), captured_pixels.size());
        return true;
    }

    bool is_opened() const override
    {
        return opened_;
    }
    bool is_started() const override
    {
        return started_;
    }

private:
    bool opened_ = false;
    bool started_ = false;
};

class MockEncoderDevice : public hal::Device {
public:
    MockEncoderDevice() : Device("VideoDisplayTest") {}

private:
    bool probe() override
    {
        return mock_enabled;
    }
    bool deinit_on_zero_references() const override
    {
        return false;
    }
    std::vector<hal::InterfaceSpec> get_interface_specs() const override
    {
        return {{hal::video::EncoderIface::NAME, hal::video::EncoderIface::get_default_instance_name(0)}};
    }
    bool on_init() override
    {
        interfaces_.emplace(hal::video::EncoderIface::get_default_instance_name(0), std::make_shared<MockEncoder>());
        return true;
    }
    void on_deinit() override
    {
        interfaces_.clear();
    }
};

class MockDisplayService : public service::ServiceBase {
public:
    MockDisplayService() : ServiceBase({.name = "VideoDisplayTest", .description = "Mock preview output", .version = "0.0.0"}) {}
};

dataflow::VisualOutputInfo make_output()
{
    return {
        .output = {.provider_id = "VideoDisplayTest", .name = "panel", .width = 480, .height = 480},
        .pixel_format = output_format,
        .byte_order = output_order,
    };
}

class MockVisualOperation : public dataflow::VisualOperation {
public:
    std::vector<dataflow::VisualOutputInfo> get_outputs() const override
    {
        return {make_output()};
    }
    std::vector<dataflow::SourceInfo> get_sources() const override
    {
        return {};
    }
    std::vector<std::string> get_source_roles() const override
    {
        return {};
    }
    std::expected<void, std::string> request_output(std::string_view) override
    {
        return {};
    }
    std::expected<void, std::string> release_output(std::string_view) override
    {
        return {};
    }
    std::expected<void, std::string> set_active_source(std::string_view) override
    {
        if (activation_fails) {
            return std::unexpected("Injected activation failure");
        }
        return {};
    }
    std::expected<void, std::string> set_active_source_role(std::string_view, std::string_view) override
    {
        return {};
    }
    std::expected<std::string, std::string> get_active_source(std::string_view) const override
    {
        return "preview";
    }
    std::expected<std::string, std::string> get_active_source_role(std::string_view) const override
    {
        return "preview";
    }
    dataflow::VisualPresentResult present_frame_sync(
        std::string_view, const dataflow::VisualFrameInfo &, std::span<const uint8_t> data, uint32_t timeout_ms
    ) override
    {
        ++present_count;
        presented_timeout_ms = timeout_ms;
        presented_pixels.assign(data.begin(), data.end());
        return dataflow::VisualPresentResult::Presented;
    }
    dataflow::VisualPresentResult present_buffer_frame_sync(
        std::string_view, const dataflow::VisualFrameInfo &, BufferWriter
    ) override
    {
        return dataflow::VisualPresentResult::Error;
    }
    dataflow::VisualAsyncSubmitResult present_frame_async(
        std::string_view, const dataflow::VisualFrameInfo &, std::span<const uint8_t>, CompletionCallback, uint32_t
    ) override
    {
        return {};
    }
    std::expected<dataflow::VisualBufferView, std::string> map_output_buffer(std::string_view) const override
    {
        return std::unexpected("Not a buffer output");
    }
    lib_utils::connection connect_source_state_changed(SourceStateCallback) override
    {
        return {};
    }
    lib_utils::connection connect_active_source_changed(ActiveSourceCallback) override
    {
        return {};
    }

protected:
    void on_close() override {}
};

class MockVisualProvider : public dataflow::DataFlowProvider {
public:
    dataflow::ProviderInfo get_provider_info() const override
    {
        return {.id = "VideoDisplayTest", .service_name = "VideoDisplayTest", .models = {dataflow::Model::Visual}};
    }
    std::vector<dataflow::OutputInfo> list_outputs(dataflow::Model) const override
    {
        return {make_output().output};
    }
    std::expected<std::shared_ptr<dataflow::VisualOperation>, std::string> open_visual_operation(
        const dataflow::VisualOperationConfig &
    ) override
    {
        return std::make_shared<MockVisualOperation>();
    }
};

class DisplayFixture {
public:
    DisplayFixture()
    {
        mock_enabled = true;
        activation_fails = false;
        close_count = 0;
        present_count = 0;
        captured_pixels = {0x12, 0x34, 0xAB, 0xCD};
        presented_pixels.clear();
        output_format = dataflow::VisualPixelFormat::RGB565;
        output_order = dataflow::VisualByteOrder::Swap16;
        auto &manager = service::ServiceManager::get_instance();
        TEST_ASSERT_TRUE_MESSAGE(manager.start(), "Failed to start ServiceManager");
        binding_ = manager.bind(Encoder::get_name().data());
        TEST_ASSERT_TRUE_MESSAGE(binding_.is_valid(), "Failed to bind encoder");
        auto registration = manager.get_dataflow_registry().register_provider(std::make_shared<MockVisualProvider>());
        TEST_ASSERT_TRUE_MESSAGE(registration.has_value(), "Failed to register display provider");
        registration_.emplace(std::move(registration.value()));
    }

    ~DisplayFixture()
    {
        (void)Encoder::call_function_sync<void>(Video::EncoderFunctionId::Close);
        registration_.reset();
        binding_.release();
        service::ServiceManager::get_instance().deinit();
        hal::detail::cleanup_all_devices();
        mock_enabled = false;
    }

    std::expected<void, std::string> open()
    {
        Video::EncoderConfig config;
        config.sinks = {{.format = Video::EncoderSinkFormat::Max, .width = 2, .height = 1, .fps = 30}};
        config.display = Video::EncoderDisplayConfig{.draw_timeout_ms = 17, .publish_sink_event = false};
        return Encoder::call_function_sync<void>(
                   Video::EncoderFunctionId::Open, BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
               );
    }

    void fetch()
    {
        TEST_ASSERT_TRUE(Encoder::call_function_sync<void>(Video::EncoderFunctionId::Start).has_value());
        TEST_ASSERT_TRUE(Encoder::call_function_sync<void>(Video::EncoderFunctionId::FetchFrame, 0.0).has_value());
    }

private:
    service::ServiceBinding binding_;
    std::optional<dataflow::ProviderRegistration> registration_;
};

BROOKESIA_PLUGIN_REGISTER(hal::Device, MockEncoderDevice, "VideoDisplayTest");
BROOKESIA_PLUGIN_REGISTER(service::ServiceBase, MockDisplayService, "VideoDisplayTest");

} // namespace

BROOKESIA_TEST_CASE(
    test_encoder_display_activation_failure_closes_hal,
    "Display activation failure rolls back an opened encoder even when the HAL instance is retained",
    "[service][video][display]"
)
{
    DisplayFixture fixture;
    activation_fails = true;
    TEST_ASSERT_FALSE(fixture.open().has_value());
    TEST_ASSERT_EQUAL_size_t(1, close_count);
    activation_fails = false;
    TEST_ASSERT_TRUE_MESSAGE(fixture.open().has_value(), "Failed to open encoder");
}

BROOKESIA_TEST_CASE(
    test_encoder_display_swap_preserves_source_and_requested_timing,
    "RGB565 preview converts a private copy without changing input pixels, fps or draw timeout",
    "[service][video][display]"
)
{
    DisplayFixture fixture;
    TEST_ASSERT_TRUE_MESSAGE(fixture.open().has_value(), "Failed to open encoder");
    const auto original_pixels = captured_pixels;
    fixture.fetch();
    TEST_ASSERT_TRUE(captured_pixels == original_pixels);
    TEST_ASSERT_TRUE((presented_pixels == std::vector<uint8_t> {0x34, 0x12, 0xCD, 0xAB}));
    TEST_ASSERT_EQUAL_UINT32(30, capture_config.sinks[0].fps);
    TEST_ASSERT_EQUAL_UINT32(17, presented_timeout_ms);
}

BROOKESIA_TEST_CASE(
    test_encoder_display_rejects_short_and_oversized_frames,
    "Preview rejects frames whose byte count differs from the configured pixel format",
    "[service][video][display]"
)
{
    DisplayFixture fixture;
    TEST_ASSERT_TRUE_MESSAGE(fixture.open().has_value(), "Failed to open encoder");
    captured_pixels.resize(3);
    fixture.fetch();
    captured_pixels.resize(6);
    fixture.fetch();
    TEST_ASSERT_EQUAL_size_t(0, present_count);
}

BROOKESIA_TEST_CASE(
    test_encoder_display_native_rgb888,
    "Native RGB888 preview accepts three bytes per pixel without conversion",
    "[service][video][display]"
)
{
    DisplayFixture fixture;
    output_format = dataflow::VisualPixelFormat::RGB888;
    output_order = dataflow::VisualByteOrder::Native;
    captured_pixels = {1, 2, 3, 4, 5, 6};
    TEST_ASSERT_TRUE_MESSAGE(fixture.open().has_value(), "Failed to open encoder");
    fixture.fetch();
    TEST_ASSERT_TRUE(presented_pixels == captured_pixels);
    TEST_ASSERT_EQUAL_size_t(1, present_count);
}
