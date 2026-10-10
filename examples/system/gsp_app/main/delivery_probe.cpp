/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define BROOKESIA_LOG_TAG "GspProbe"
#include "brookesia/lib_utils/log.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "boost/json.hpp"
#include "boost/thread.hpp"
#include "esp_gsp.h"
#include "esp_gsp_debug.h"
#include "esp_lcd_mipi_dsi.h"
#include "brookesia/gui_gsp/app_host.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/service_manager/service/manager.hpp"
#include "delivery_probe.hpp"

namespace {

std::atomic<const void *> last_presented{nullptr};

} // namespace

// Linked with --wrap so the probe knows which framebuffer was handed to the panel last.
extern "C" esp_err_t __real_esp_lcd_panel_draw_bitmap(
    esp_lcd_panel_handle_t panel, int x_start, int y_start, int x_end, int y_end, const void *color_data
);

extern "C" esp_err_t __wrap_esp_lcd_panel_draw_bitmap(
    esp_lcd_panel_handle_t panel, int x_start, int y_start, int x_end, int y_end, const void *color_data
)
{
    last_presented.store(color_data, std::memory_order_relaxed);
    return __real_esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, color_data);
}

namespace esp_brookesia::gsp_app {
namespace {

using service::FunctionParameterMap;
using service::FunctionResult;
using service::FunctionSchema;
using service::FunctionValueType;

constexpr int PANEL_WIDTH = 1024;
constexpr int PANEL_HEIGHT = 600;
constexpr size_t FRAME_BUFFER_COUNT = 3;

esp_gsp_handle_t current_handle()
{
    auto *host = gui::gsp::AppHost::current();
    return host ? host->handle() : nullptr;
}

uint32_t component_key(std::string_view name)
{
    uint32_t key = 2166136261U;
    for (const auto character : name) {
        key = (key ^ static_cast<uint8_t>(character)) * 16777619U;
    }
    return key;
}

int number_param(const FunctionParameterMap &params, const char *name)
{
    auto it = params.find(name);
    return (it == params.end()) ? 0 : static_cast<int>(std::get<double>(it->second));
}

FunctionResult make_result(bool success, std::string error = {}, std::optional<service::FunctionValue> data = {})
{
    return {
        .success = success,
        .error_message = std::move(error),
        .data = std::move(data),
    };
}

bool touch(esp_gsp_handle_t handle, int x, int y, bool pressed)
{
    return esp_gsp_inject_touch(handle, static_cast<int16_t>(x), static_cast<int16_t>(y), pressed) == ESP_GSP_OK;
}

void print_frames(hal::display::PanelIface &panel)
{
    if (auto handle = current_handle()) {
        (void)esp_gsp_flush(handle, 1000);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    std::array<void *, FRAME_BUFFER_COUNT> buffers{};
    hal::display::PanelIface::DriverSpecific driver;
    if (!panel.get_driver_specific(driver) || (driver.frame_buffer_count != buffers.size()) ||
            (esp_lcd_dpi_panel_get_frame_buffer(static_cast<esp_lcd_panel_handle_t>(driver.panel_handle),
                    buffers.size(), &buffers[0], &buffers[1], &buffers[2]) != ESP_OK)) {
        std::puts("GSP_FRAME_UNAVAILABLE");
        return;
    }
    constexpr size_t FRAME_BYTES = PANEL_WIDTH * PANEL_HEIGHT * sizeof(uint16_t);
    const auto *presented = static_cast<const uint8_t *>(last_presented.load(std::memory_order_relaxed));
    int presented_index = -1;
    for (size_t frame = 0; frame < buffers.size(); ++frame) {
        const auto *start = static_cast<const uint8_t *>(buffers[frame]);
        if ((presented >= start) && (presented < start + FRAME_BYTES)) {
            presented_index = static_cast<int>(frame);
        }
    }
    std::printf("GSP_FRAME_PRESENTED %d\n", presented_index);
    // Every buffer is printed; GSP_FRAME_PRESENTED names the one last handed to the panel.
    for (size_t frame = 0; frame < buffers.size(); ++frame) {
        const auto *pixels = static_cast<const uint16_t *>(buffers[frame]);
        std::printf("GSP_FRAME_BEGIN %zu %d %d\n", frame, PANEL_WIDTH / 2, PANEL_HEIGHT / 2);
        std::string line;
        line.reserve(9 + PANEL_WIDTH * 2);
        for (int y = 0; y < PANEL_HEIGHT; y += 2) {
            line = "GSP_PIX ";
            for (int x = 0; x < PANEL_WIDTH; x += 2) {
                std::array<char, 5> hex{};
                std::snprintf(hex.data(), hex.size(), "%04x", pixels[y * PANEL_WIDTH + x]);
                line += hex.data();
            }
            std::puts(line.c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        std::puts("GSP_FRAME_END");
    }
    std::fflush(stdout);
}

class DeliveryProbeService final: public service::ServiceBase {
public:
    explicit DeliveryProbeService(hal::display::PanelIface &panel)
        : ServiceBase(Attributes{
        .name = "GspProbe",
        .description = "Touch, component and frame diagnostics for USB-driven host checks.",
        .version = "0.1.0",
    })
    , panel_(panel)
    {}

    std::vector<FunctionSchema> get_function_schemas() override
    {
        return {
            {
                .name = "Tap",
                .description = "Press and release at a screen point.",
                .parameters = {{.name = "X", .type = FunctionValueType::Number},
                    {.name = "Y", .type = FunctionValueType::Number}
                },
            },
            {
                .name = "Drag",
                .description = "Press, move in steps, then release.",
                .parameters = {{.name = "X", .type = FunctionValueType::Number},
                    {.name = "Y", .type = FunctionValueType::Number},
                    {.name = "ToX", .type = FunctionValueType::Number},
                    {.name = "ToY", .type = FunctionValueType::Number}
                },
            },
            {
                .name = "Component",
                .description = "Report whether a named component exists and its integer value.",
                .parameters = {{.name = "Name", .type = FunctionValueType::String}},
                .return_value = service::FunctionReturnSchema{.type = FunctionValueType::Object},
            },
            {
                .name = "Frame",
                .description = "Print the framebuffers to the console after DelayMs.",
                .parameters = {{.name = "DelayMs", .type = FunctionValueType::Number, .default_value = 1500.0}},
            },
        };
    }

protected:
    FunctionHandlerMap get_function_handlers() override
    {
        auto tap = [](FunctionParameterMap && params) {
            const auto handle = current_handle();
            const int x = number_param(params, "X");
            const int y = number_param(params, "Y");
            if (!handle || !touch(handle, x, y, true)) {
                return make_result(false, "No GSP session accepted the touch");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            return make_result(touch(handle, x, y, false), "Touch release failed");
        };
        auto drag = [](FunctionParameterMap && params) {
            constexpr int STEPS = 8;
            const auto handle = current_handle();
            const int x = number_param(params, "X");
            const int y = number_param(params, "Y");
            const int to_x = number_param(params, "ToX");
            const int to_y = number_param(params, "ToY");
            if (!handle || !touch(handle, x, y, true)) {
                return make_result(false, "No GSP session accepted the touch");
            }
            for (int step = 1; step <= STEPS; ++step) {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                if (!touch(handle, x + (to_x - x) * step / STEPS, y + (to_y - y) * step / STEPS, true)) {
                    return make_result(false, "Touch move failed");
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            return make_result(touch(handle, to_x, to_y, false), "Touch release failed");
        };
        auto component = [](FunctionParameterMap && params) {
            const auto name_it = params.find("Name");
            const auto &name = std::get<std::string>(name_it->second);
            const auto handle = current_handle();
            const auto key = component_key(name);
            esp_gsp_component_info_t info{};
            boost::json::object result{{"exists", false}};
            if (handle && (esp_gsp_component_get_info(handle, key, &info) == ESP_GSP_OK)) {
                result["exists"] = true;
                int32_t value = 0;
                if (esp_gsp_component_get_value(handle, key, &value) == ESP_GSP_OK) {
                    result["value"] = value;
                }
            }
            return make_result(true, {}, std::move(result));
        };
        auto frame = [this](FunctionParameterMap && params) {
            const int delay_ms = number_param(params, "DelayMs");
            auto &panel = panel_;
            auto print_later = [&panel, delay_ms]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                print_frames(panel);
            };
            BROOKESIA_THREAD_CONFIG_GUARD({
                .name = "gsp_probe_frame",
                .stack_size = 6 * 1024,
                .stack_in_ext = false,
            });
            boost::thread(print_later).detach();
            return make_result(true);
        };
        return {
            {"Tap", std::move(tap)},
            {"Drag", std::move(drag)},
            {"Component", std::move(component)},
            {"Frame", std::move(frame)},
        };
    }

private:
    hal::display::PanelIface &panel_;
};

} // namespace

bool start_delivery_probe(hal::display::PanelIface &panel)
{
    auto &manager = service::ServiceManager::get_instance();
    auto probe = std::make_shared<DeliveryProbeService>(panel);
    if (!manager.add_service(probe)) {
        BROOKESIA_LOGE("Failed to add the GspProbe service");
        return false;
    }
    static service::ServiceBinding binding = manager.bind("GspProbe");
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to start the GspProbe service");
        return false;
    }
    BROOKESIA_LOGW("GspProbe is enabled: USB callers can inject touches and read the screen");
    return true;
}

} // namespace esp_brookesia::gsp_app
