/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <exception>

#if defined(BROOKESIA_GUI_GSP_HOST_TEST)
#   include "sdk_stubs.hpp"
#   include "host_log_stubs.hpp"
#else
#   include "brookesia/gui_gsp/macro_configs.h"
#   if !BROOKESIA_GUI_GSP_ENABLE_DEBUG_LOG
#       define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#   endif
#   include "private/utils.hpp"
#   include "esp_gsp_esp_lcd.h"
#endif

#include "brookesia/gui_gsp/bundle.hpp"
#include "brookesia/gui_gsp/session.hpp"

namespace esp_brookesia::gui::gsp {

namespace {

constexpr uint32_t RENDER_TASK_STACK_SIZE = 49152;
constexpr int STOP_ATTEMPTS = 3;

esp_display_present_panel_t panel_type_from_bus(hal::display::PanelIface::BusType bus_type, bool &swap_bytes)
{
    swap_bytes = false;
    switch (bus_type) {
    case hal::display::PanelIface::BusType::MIPI:
        return ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI;
    case hal::display::PanelIface::BusType::RGB:
        return ESP_DISPLAY_PRESENT_PANEL_RGB;
    case hal::display::PanelIface::BusType::Generic:
        // SPI and QSPI panels take big-endian RGB565 on the wire.
        swap_bytes = true;
        return ESP_DISPLAY_PRESENT_PANEL_IO;
    default:
        return ESP_DISPLAY_PRESENT_PANEL_AUTO;
    }
}

bool pixel_format_from_panel(
    hal::display::PanelIface::PixelFormat format, esp_display_present_pixel_format_t &out_format
)
{
    switch (format) {
    case hal::display::PanelIface::PixelFormat::RGB565:
        out_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565;
        return true;
    case hal::display::PanelIface::PixelFormat::RGB888:
        out_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB888;
        return true;
    default:
        return false;
    }
}

struct EventBridge {
    Session::EventCallback on_event = nullptr;
    void *user_ctx = nullptr;
};

void forward_event(esp_gsp_handle_t gsp, const esp_gsp_event_t *event, void *user_ctx)
{
    auto *bridge = static_cast<EventBridge *>(user_ctx);
    if ((bridge == nullptr) || (bridge->on_event == nullptr) || (event == nullptr)) {
        return;
    }
    bridge->on_event(gsp, event, bridge->user_ctx);
}

} // namespace

class Session::Impl {
public:
    esp_gsp_handle_t handle = nullptr;
    EventBridge bridge{};
    std::shared_ptr<const Bundle> bundle{};
};

Session::Session()
    : impl_(std::make_unique<Impl>())
{
}

Session::~Session()
{
    if (!stop()) {
        BROOKESIA_LOGE("Cannot destroy a GSP session while the UI still owns callback and display resources");
        std::terminate();
    }
}

bool Session::start(
    const esp_gsp_config_t &bundle, const DisplayLease &display, EventCallback on_event, void *user_ctx
)
{
    return start_impl(bundle, {}, display, on_event, user_ctx);
}

bool Session::start(
    std::shared_ptr<const Bundle> bundle, const DisplayLease &display, EventCallback on_event, void *user_ctx
)
{
    BROOKESIA_CHECK_NULL_RETURN(bundle, false, "GSP bundle is null");
    const auto &config = bundle->config();
    return start_impl(config, std::move(bundle), display, on_event, user_ctx);
}

bool Session::start_impl(
    const esp_gsp_config_t &bundle, std::shared_ptr<const Bundle> owned_bundle, const DisplayLease &display,
    EventCallback on_event, void *user_ctx
)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_CHECK_FALSE_RETURN(!running(), false, "GSP session is already running");
    BROOKESIA_CHECK_NULL_RETURN(display.panel, false, "Display panel is null");
    BROOKESIA_CHECK_NULL_RETURN(display.touch, false, "Display touch is null");

    hal::display::PanelIface::DriverSpecific panel_specific{};
    BROOKESIA_CHECK_FALSE_RETURN(
        display.panel->get_driver_specific(panel_specific), false, "Failed to read panel driver data"
    );
    BROOKESIA_CHECK_NULL_RETURN(panel_specific.panel_handle, false, "Panel handle is null");

    // Leave the HAL dispatcher idle. Enabling it would register LCD callbacks, and the
    // presenter needs to be the only owner of those callbacks for this session.
    bool swap_bytes = false;
    const auto panel_type = panel_type_from_bus(panel_specific.bus_type, swap_bytes);
    BROOKESIA_CHECK_FALSE_RETURN(
        panel_type != ESP_DISPLAY_PRESENT_PANEL_AUTO, false, "Unsupported panel bus"
    );

    esp_display_present_pixel_format_t pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565;
    BROOKESIA_CHECK_FALSE_RETURN(
        pixel_format_from_panel(display.panel->get_info().pixel_format, pixel_format), false,
        "Unsupported panel pixel format"
    );

    hal::display::TouchIface::DriverSpecific touch_specific{};
    BROOKESIA_CHECK_FALSE_RETURN(
        display.touch->get_driver_specific(touch_specific), false, "Failed to read touch driver data"
    );
    BROOKESIA_CHECK_NULL_RETURN(touch_specific.touch_handle, false, "Touch handle is null");

    const auto &panel_info = display.panel->get_info();
    const auto &touch_info = display.touch->get_info();
    BROOKESIA_LOGI(
        "Start GSP on %1%x%2% panel, bus %3%, touch mode %4%", panel_info.h_res, panel_info.v_res,
        static_cast<int>(panel_specific.bus_type), static_cast<int>(touch_info.operation_mode)
    );

    esp_gsp_esp_lcd_config_t lcd = ESP_GSP_ESP_LCD_CONFIG_INIT();
    lcd.display.hw.panel = static_cast<esp_lcd_panel_handle_t>(panel_specific.panel_handle);
    lcd.display.hw.io = static_cast<esp_lcd_panel_io_handle_t>(panel_specific.io_handle);
    lcd.display.hw.panel_type = panel_type;
    lcd.display.hw.input_pixel_format = pixel_format;
    lcd.display.hw.rotation = ESP_DISPLAY_PRESENT_ROTATE_0;
    lcd.display.hw.swap_bytes = swap_bytes;
    lcd.display.hw.te_enabled = false;
    lcd.display.hw.te_sync = {};
    lcd.display.hw.te_sync.gpio_num = -1;
    // Zero asks the presenter to borrow the framebuffers already owned by the panel driver.
    lcd.display.fb.mode = ESP_DISPLAY_PRESENT_MODE_AUTO;
    lcd.display.fb.frame_buffer_count = 0;
    lcd.touch = static_cast<esp_lcd_touch_handle_t>(touch_specific.touch_handle);
    lcd.touch_input_mode = (touch_info.operation_mode == hal::display::TouchIface::OperationMode::Interrupt)
                           ? ESP_GSP_TOUCH_INPUT_INTERRUPT
                           : ESP_GSP_TOUCH_INPUT_POLLING;
    lcd.task_stack_size = RENDER_TASK_STACK_SIZE;
    lcd.task_stack_size_freetype = RENDER_TASK_STACK_SIZE;
    lcd.task_stack_psram = true;

    esp_gsp_config_t config = bundle;
    esp_gsp_handle_t handle = nullptr;
    const esp_err_t start_result = esp_gsp_esp_lcd_start(&config, &lcd, &handle);
    BROOKESIA_CHECK_ESP_ERR_RETURN(start_result, false, "Failed to start GSP");
    BROOKESIA_CHECK_NULL_RETURN(handle, false, "GSP start returned a null handle");

    impl_->handle = handle;
    impl_->bundle = std::move(owned_bundle);
    impl_->bridge.on_event = on_event;
    impl_->bridge.user_ctx = user_ctx;
    if (on_event != nullptr) {
        const esp_gsp_err_t event_result = esp_gsp_on_event(handle, forward_event, &impl_->bridge);
        if (event_result != ESP_GSP_OK) {
            BROOKESIA_LOGE("Failed to register GSP event callback: %1%", static_cast<int>(event_result));
            stop();
            return false;
        }
    }

    BROOKESIA_LOGI("GSP session started");
    return true;
}

bool Session::stop()
{
    if (!running()) {
        return true;
    }

    BROOKESIA_LOG_TRACE_GUARD();

    esp_gsp_handle_t handle = impl_->handle;

    esp_gsp_err_t result = ESP_GSP_FAIL;
    for (int attempt = 0; attempt < STOP_ATTEMPTS; ++attempt) {
        result = esp_gsp_stop(handle);
        if (result == ESP_GSP_OK) {
            impl_->handle = nullptr;
            // A successful stop drains callbacks. Only now may their context be released.
            impl_->bridge = {};
            impl_->bundle.reset();
            BROOKESIA_LOGI("GSP session stopped");
            return true;
        }
        BROOKESIA_LOGW("GSP stop attempt %1% returned %2%", attempt + 1, static_cast<int>(result));
        if (result != ESP_GSP_ERR_TIMEOUT) {
            break;
        }
    }

    BROOKESIA_LOGE("GSP session remains owned after stop failure: result(%1%)", static_cast<int>(result));
    return false;
}

bool Session::running() const
{
    return (impl_ != nullptr) && (impl_->handle != nullptr);
}

esp_gsp_handle_t Session::handle() const
{
    return running() ? impl_->handle : nullptr;
}

} // namespace esp_brookesia::gui::gsp
