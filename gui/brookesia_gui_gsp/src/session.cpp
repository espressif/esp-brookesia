/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <chrono>
#include <exception>
#include <mutex>
#include <set>
#include <thread>
#include "boost/thread.hpp"

#if defined(BROOKESIA_GUI_GSP_HOST_TEST)
#   include "sdk_stubs.hpp"
#   include "host_log_stubs.hpp"
#   define BROOKESIA_THREAD_CONFIG_GUARD(...) ((void)0)
#else
#   include "brookesia/gui_gsp/macro_configs.h"
#   if !BROOKESIA_GUI_GSP_ENABLE_DEBUG_LOG
#       define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#   endif
#   include "private/utils.hpp"
#   include "brookesia/lib_utils/thread_config.hpp"
#   include "esp_gsp_esp_lcd.h"
#endif

#include "brookesia/gui_gsp/bundle.hpp"
#include "brookesia/gui_gsp/session.hpp"

namespace esp_brookesia::gui::gsp {

namespace {

constexpr uint32_t RENDER_TASK_STACK_SIZE = 49152;
constexpr int STOP_ATTEMPTS = 3;
constexpr size_t CLEANUP_THREAD_STACK_SIZE = 8 * 1024;
constexpr auto CLEANUP_RETRY_INTERVAL = std::chrono::milliseconds(250);
constexpr auto CLEANUP_MAX_RETRY_INTERVAL = std::chrono::seconds(4);

struct ActivePanels {
    std::mutex mutex;
    std::set<const void *> panels;
};

ActivePanels &active_panels()
{
    // A deferred session may outlive other static objects at process shutdown.
    static auto *panels = new ActivePanels;
    return *panels;
}

bool reserve_panel(const void *panel)
{
    auto &registry = active_panels();
    std::lock_guard lock(registry.mutex);
    return registry.panels.insert(panel).second;
}

void release_panel(const void *panel)
{
    auto &registry = active_panels();
    std::lock_guard lock(registry.mutex);
    registry.panels.erase(panel);
}

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
    boost::mutex mutex;
    boost::condition_variable condition;
    bool abandoned = false;
    bool finished = false;
    bool stopping = false;
    const void *panel = nullptr;
    esp_gsp_handle_t handle = nullptr;
    EventBridge bridge{};
    std::shared_ptr<const Bundle> bundle{};
    std::shared_ptr<void> display_owner{};
    std::shared_ptr<void> callback_owner{};
    std::shared_ptr<const void> bundle_owner{};

    struct Resources {
        std::shared_ptr<const Bundle> bundle;
        std::shared_ptr<void> display;
        std::shared_ptr<void> callback;
        std::shared_ptr<const void> config;
    };

    Resources take_resources()
    {
        bridge = {};
        return {std::move(bundle), std::move(display_owner), std::move(callback_owner), std::move(bundle_owner)};
    }

    static void cleanup(std::shared_ptr<Impl> state)
    {
        boost::unique_lock lock(state->mutex);
        state->condition.wait(lock, [&]() {
            return state->abandoned || state->finished;
        });
        auto retry_interval = CLEANUP_RETRY_INTERVAL;
        while (!state->finished) {
            state->condition.wait(lock, [&]() {
                return !state->stopping || state->finished;
            });
            if (state->finished) {
                break;
            }
            const auto handle = state->handle;
            state->stopping = true;
            lock.unlock();
            const auto result = esp_gsp_stop(handle);
            lock.lock();
            state->stopping = false;
            if (result == ESP_GSP_OK) {
                state->handle = nullptr;
                release_panel(state->panel);
                auto resources = state->take_resources();
                state->finished = true;
                state->condition.notify_all();
                lock.unlock();
                return;
            }
            BROOKESIA_LOGW("Deferred GSP stop returned %1%; retrying", static_cast<int>(result));
            lock.unlock();
            std::this_thread::sleep_for(retry_interval);
            retry_interval = std::min(retry_interval * 2,
                                      std::chrono::duration_cast<std::chrono::milliseconds>(CLEANUP_MAX_RETRY_INTERVAL));
            lock.lock();
        }
    }
};

Session::Session()
    : impl_(std::make_shared<Impl>())
{
}

Session::~Session()
{
    if (impl_) {
        boost::lock_guard lock(impl_->mutex);
        if (impl_->handle != nullptr) {
            impl_->abandoned = true;
        } else {
            impl_->finished = true;
        }
        impl_->condition.notify_all();
    }
}

bool Session::start(
    const esp_gsp_config_t &bundle, const DisplayLease &display, EventCallback on_event, void *user_ctx,
    std::shared_ptr<void> callback_owner, std::shared_ptr<const void> bundle_owner
)
{
    return start_impl(bundle, {}, display, on_event, user_ctx, std::move(callback_owner), std::move(bundle_owner));
}

bool Session::start(
    std::shared_ptr<const Bundle> bundle, const DisplayLease &display, EventCallback on_event, void *user_ctx,
    std::shared_ptr<void> callback_owner
)
{
    BROOKESIA_CHECK_NULL_RETURN(bundle, false, "GSP bundle is null");
    const auto &config = bundle->config();
    return start_impl(config, std::move(bundle), display, on_event, user_ctx, std::move(callback_owner), {});
}

bool Session::start_impl(
    const esp_gsp_config_t &bundle, std::shared_ptr<const Bundle> owned_bundle, const DisplayLease &display,
    EventCallback on_event, void *user_ctx, std::shared_ptr<void> callback_owner,
    std::shared_ptr<const void> bundle_owner
)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_CHECK_FALSE_RETURN(!running(), false, "GSP session is already running");
    BROOKESIA_CHECK_NULL_RETURN(display.panel, false, "Display panel is null");
    BROOKESIA_CHECK_NULL_RETURN(display.touch, false, "Display touch is null");
    BROOKESIA_CHECK_FALSE_RETURN(display.lifetime != nullptr, false, "GSP display lease has no lifetime owner");
    BROOKESIA_CHECK_FALSE_RETURN(user_ctx == nullptr || callback_owner != nullptr, false,
                                 "GSP callback context has no lifetime owner");
    BROOKESIA_CHECK_FALSE_RETURN(owned_bundle != nullptr || bundle_owner != nullptr, false,
                                 "GSP configuration data has no lifetime owner");

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

    auto state = std::make_shared<Impl>();
    state->bundle = std::move(owned_bundle);
    state->display_owner = display.lifetime;
    state->callback_owner = std::move(callback_owner);
    state->bundle_owner = std::move(bundle_owner);
    state->panel = panel_specific.panel_handle;
    BROOKESIA_CHECK_FALSE_RETURN(reserve_panel(state->panel), false, "GSP panel is already in use");
    try {
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_size = CLEANUP_THREAD_STACK_SIZE,
            .stack_in_ext = false,
        });
        boost::thread(&Impl::cleanup, state).detach();
    } catch (const std::exception &error) {
        release_panel(state->panel);
        BROOKESIA_LOGE("Failed to start GSP cleanup task: %1%", error.what());
        return false;
    }
    impl_ = state;
    esp_gsp_config_t config = bundle;
    esp_gsp_handle_t handle = nullptr;
    const esp_err_t start_result = esp_gsp_esp_lcd_start(&config, &lcd, &handle);
    if (start_result != ESP_OK || handle == nullptr) {
        Impl::Resources resources;
        {
            boost::lock_guard lock(state->mutex);
            resources = state->take_resources();
            release_panel(state->panel);
            state->finished = true;
            state->condition.notify_all();
        }
        BROOKESIA_LOGE("Failed to start GSP: %1%", static_cast<int>(start_result));
        return false;
    }

    state->handle = handle;
    state->bridge.on_event = on_event;
    state->bridge.user_ctx = user_ctx;
    if (on_event != nullptr) {
        const esp_gsp_err_t event_result = esp_gsp_on_event(handle, forward_event, &state->bridge);
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
    BROOKESIA_LOG_TRACE_GUARD();
    auto state = impl_;
    esp_gsp_handle_t handle = nullptr;
    {
        boost::lock_guard lock(state->mutex);
        if (state->handle == nullptr) {
            return true;
        }
        if (state->stopping) {
            return false;
        }
        state->stopping = true;
        handle = state->handle;
    }

    esp_gsp_err_t result = ESP_GSP_FAIL;
    for (int attempt = 0; attempt < STOP_ATTEMPTS; ++attempt) {
        result = esp_gsp_stop(handle);
        if (result == ESP_GSP_OK) {
            Impl::Resources resources;
            {
                boost::lock_guard lock(state->mutex);
                state->handle = nullptr;
                release_panel(state->panel);
                resources = state->take_resources();
                state->stopping = false;
                state->finished = true;
                state->condition.notify_all();
            }
            BROOKESIA_LOGI("GSP session stopped");
            return true;
        }
        BROOKESIA_LOGW("GSP stop attempt %1% returned %2%", attempt + 1, static_cast<int>(result));
        if (result != ESP_GSP_ERR_TIMEOUT) {
            break;
        }
    }

    {
        boost::lock_guard lock(state->mutex);
        state->stopping = false;
        state->condition.notify_all();
    }

    BROOKESIA_LOGE("GSP session remains owned after stop failure: result(%1%)", static_cast<int>(result));
    return false;
}

bool Session::running() const
{
    if (!impl_) {
        return false;
    }
    boost::lock_guard lock(impl_->mutex);
    return impl_->handle != nullptr;
}

esp_gsp_handle_t Session::handle() const
{
    if (!impl_) {
        return nullptr;
    }
    boost::lock_guard lock(impl_->mutex);
    return impl_->handle;
}

} // namespace esp_brookesia::gui::gsp
