/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_PANEL_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "lcd_panel_impl.hpp"
#include "brookesia/hal_adaptor/display/device.hpp"

#if BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_PANEL_IMPL
#include <cstring>

#include "boost/thread.hpp"
#include "esp_attr.h"
#include "esp_board_manager_includes.h"
#include "esp_idf_version.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "soc/soc_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "brookesia/lib_utils/thread_config.hpp"

#if CONFIG_SOC_LCD_RGB_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif
#if CONFIG_SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_mipi_dsi.h"
#endif

namespace esp_brookesia::hal {

namespace {
constexpr const char *LCD_PANEL_INIT_THREAD_NAME = "HalLcdInit";
constexpr size_t LCD_PANEL_INIT_THREAD_STACK_SIZE = 8192;

esp_lcd_panel_handle_t get_panel_handle(void *handles)
{
    return reinterpret_cast<dev_display_lcd_handles_t *>(handles)->panel_handle;
}

esp_lcd_panel_io_handle_t get_io_handle(void *handles)
{
    return reinterpret_cast<dev_display_lcd_handles_t *>(handles)->io_handle;
}

display::PanelIface::BusType get_bus_type_from_config(const dev_display_lcd_config_t *config)
{
    if (config == nullptr) {
        return display::PanelIface::BusType::Max;
    }
    if (strcmp(config->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_SPI) == 0) {
        return display::PanelIface::BusType::Generic;
    }
    if (strcmp(config->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_DSI) == 0) {
        return display::PanelIface::BusType::MIPI;
    }
    if (strcmp(config->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_RGB) == 0) {
        return display::PanelIface::BusType::RGB;
    }

    return display::PanelIface::BusType::Max;
}

#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT || CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_SUPPORT
uint8_t normalize_frame_buffer_count(uint8_t frame_buffer_count)
{
    return (frame_buffer_count == 0) ? 1 : frame_buffer_count;
}
#endif

uint8_t get_frame_buffer_count_from_config(const dev_display_lcd_config_t *config)
{
    if (config == nullptr) {
        return 0;
    }

    switch (get_bus_type_from_config(config)) {
    case display::PanelIface::BusType::MIPI:
#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
        return normalize_frame_buffer_count(static_cast<uint8_t>(config->sub_cfg.dsi.dpi_config.num_fbs));
#else
        return 0;
#endif
    case display::PanelIface::BusType::RGB:
#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_SUPPORT
        return normalize_frame_buffer_count(static_cast<uint8_t>(config->sub_cfg.rgb.panel_config.num_fbs));
#else
        return 0;
#endif
    default:
        return 0;
    }
}

display::PanelIface::BusType get_bus_type()
{
    if (!esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD)) {
        BROOKESIA_LOGW("LCD panel device not found");
        return display::PanelIface::BusType::Max;
    }

    dev_display_lcd_config_t *config = nullptr;
    auto ret = esp_board_manager_get_device_config(
                   ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, reinterpret_cast<void **>(&config)
               );
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, display::PanelIface::BusType::Max, "Failed to get LCD config");
    BROOKESIA_CHECK_NULL_RETURN(config, display::PanelIface::BusType::Max, "Failed to get LCD config");

    return get_bus_type_from_config(config);
}

display::PanelIface::Info generate_info()
{
    if (!esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD)) {
        BROOKESIA_LOGW("LCD panel device not found");
        return {};
    }

    dev_display_lcd_config_t *config = nullptr;
    auto ret = esp_board_manager_get_device_config(
                   ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, reinterpret_cast<void **>(&config)
               );
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, {}, "Failed to get LCD config");
    BROOKESIA_CHECK_NULL_RETURN(config, {}, "Failed to get LCD config");

    display::PanelIface::Info info{};
    info.h_res = config->lcd_width;
    info.v_res = config->lcd_height;
    info.group_id = DisplayDevice::LCD_GROUP_ID;
    auto pixel_bits = config->bits_per_pixel;
    switch (pixel_bits) {
    case 16:
        info.pixel_format = display::PanelIface::PixelFormat::RGB565;
        break;
    case 24:
        info.pixel_format = display::PanelIface::PixelFormat::RGB888;
        break;
    default:
        BROOKESIA_CHECK_FALSE_RETURN(false, {}, "Unsupported pixel bits(%1%)", pixel_bits);
    }

    return info;
}

bool IRAM_ATTR on_io_color_trans_done(
    esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *event_data, void *user_ctx
)
{
    (void)panel_io;
    (void)event_data;

    auto *impl = static_cast<LcdDisplayPanelImpl *>(user_ctx);
    if (impl == nullptr) {
        return false;
    }

    return impl->dispatch_event_from_isr(display::PanelIface::EventType::ColorTransDone);
}

#if CONFIG_SOC_LCD_RGB_SUPPORTED
bool IRAM_ATTR on_rgb_color_trans_done(
    esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data, void *user_ctx
)
{
    (void)panel;
    (void)event_data;

    auto *impl = static_cast<LcdDisplayPanelImpl *>(user_ctx);
    if (impl == nullptr) {
        return false;
    }

    return impl->dispatch_event_from_isr(display::PanelIface::EventType::ColorTransDone);
}

bool IRAM_ATTR on_rgb_frame_done(
    esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data, void *user_ctx
)
{
    (void)panel;
    (void)event_data;

    auto *impl = static_cast<LcdDisplayPanelImpl *>(user_ctx);
    if (impl == nullptr) {
        return false;
    }

    return impl->dispatch_event_from_isr(display::PanelIface::EventType::FrameDone);
}
#endif

#if CONFIG_SOC_MIPI_DSI_SUPPORTED
bool IRAM_ATTR on_mipi_color_trans_done(
    esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *event_data, void *user_ctx
)
{
    (void)panel;
    (void)event_data;

    auto *impl = static_cast<LcdDisplayPanelImpl *>(user_ctx);
    if (impl == nullptr) {
        return false;
    }

    return impl->dispatch_event_from_isr(display::PanelIface::EventType::ColorTransDone);
}

bool IRAM_ATTR on_mipi_frame_done(
    esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *event_data, void *user_ctx
)
{
    (void)panel;
    (void)event_data;

    auto *impl = static_cast<LcdDisplayPanelImpl *>(user_ctx);
    if (impl == nullptr) {
        return false;
    }

    return impl->dispatch_event_from_isr(display::PanelIface::EventType::FrameDone);
}
#endif
} // namespace

LcdDisplayPanelImpl::LcdDisplayPanelImpl()
    : display::PanelIface(generate_info())
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Info: %1%", get_info());

    BROOKESIA_CHECK_FALSE_EXIT(get_info().is_valid(), "Invalid panel information");

    if (!esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD)) {
        BROOKESIA_LOGW("LCD panel device not found, skip");
        return;
    }

    esp_err_t ret = ESP_OK;
    auto init_func = [&]() {
        BROOKESIA_LOG_TRACE_GUARD();
        ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    };

    const auto bus_type = get_bus_type();
    if ((bus_type == display::PanelIface::BusType::Generic) &&
            (BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_PANEL_INIT_THREAD_CORE_ID >= 0)) {
        BROOKESIA_THREAD_CONFIG_GUARD({
            .name = LCD_PANEL_INIT_THREAD_NAME,
            .core_id = BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_PANEL_INIT_THREAD_CORE_ID,
            .stack_size = LCD_PANEL_INIT_THREAD_STACK_SIZE,
            .stack_in_ext = false,
        });
        BROOKESIA_CHECK_EXCEPTION_EXIT(
            boost::thread(init_func).join(), "Failed to init LCD panel in dedicated thread"
        );
    } else {
        if (BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_PANEL_INIT_THREAD_CORE_ID >= 0) {
            BROOKESIA_LOGD(
                "Ignore LCD panel init thread core_id(%1%) because bus type is %2%",
                BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_PANEL_INIT_THREAD_CORE_ID, bus_type
            );
        }
        init_func();
    }

    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to init LCD panel");

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, &handles_);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to get handles");

    sync_done_ = xSemaphoreCreateBinary();
    BROOKESIA_CHECK_NULL_EXIT(sync_done_, "Failed to create draw sync semaphore");
}

LcdDisplayPanelImpl::~LcdDisplayPanelImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        disable_event_dispatcher_locked();
    }

    if (sync_done_ != nullptr) {
        vSemaphoreDelete(sync_done_);
        sync_done_ = nullptr;
    }

    if (esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD)) {
        auto ret = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinit LCD"); });
    }
}

bool LcdDisplayPanelImpl::draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: x1(%1%), y1(%2%), x2(%3%), y2(%4%), data(%5%)", x1, y1, x2, y2, data);

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "LCD is not initialized");

    return submit_draw_bitmap_locked(x1, y1, x2, y2, data);
}

bool LcdDisplayPanelImpl::submit_draw_bitmap_locked(
    uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data
)
{
    uint64_t draw_seq = 0;
    portENTER_CRITICAL(&event_lock_);
    submitted_draw_seq_++;
    draw_seq = submitted_draw_seq_;
    portEXIT_CRITICAL(&event_lock_);

    auto ret = esp_lcd_panel_draw_bitmap(get_panel_handle(handles_), x1, y1, x2, y2, data);
    if (ret != ESP_OK) {
        portENTER_CRITICAL(&event_lock_);
        if (submitted_draw_seq_ == draw_seq) {
            submitted_draw_seq_ = draw_seq - 1;
        }
        portEXIT_CRITICAL(&event_lock_);
        BROOKESIA_LOGE("Failed to draw bitmap");
        return false;
    }

    return true;
}

bool LcdDisplayPanelImpl::draw_bitmap_sync(
    uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data, uint32_t timeout_ms
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (timeout_ms == 0) {
        return draw_bitmap(x1, y1, x2, y2, data);
    }

    BROOKESIA_CHECK_NULL_RETURN(sync_done_, draw_bitmap(x1, y1, x2, y2, data), "Draw sync semaphore is not available");

    bool success = false;
    {
        boost::lock_guard<boost::mutex> lock(mutex_);

        if (!is_valid_internal()) {
            BROOKESIA_LOGE("LCD is not initialized");
        } else if (!event_dispatcher_enabled_ && !enable_event_dispatcher_locked()) {
            BROOKESIA_LOGE("Failed to enable LCD event dispatcher for sync draw");
        } else {
            // Clear stale tokens; draw sequence matching below guards against tokens that arrive after this drain.
            while (xSemaphoreTake(sync_done_, 0) == pdTRUE) {
            }

            portENTER_CRITICAL(&event_lock_);
            sync_waiting_ = true;
            sync_target_draw_seq_ = submitted_draw_seq_ + 1;
            portEXIT_CRITICAL(&event_lock_);

            if (submit_draw_bitmap_locked(x1, y1, x2, y2, data)) {
                success = xSemaphoreTake(sync_done_, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
                if (!success) {
                    BROOKESIA_LOGE("Timed out waiting for LCD draw completion");
                }
            }

            portENTER_CRITICAL(&event_lock_);
            sync_waiting_ = false;
            sync_target_draw_seq_ = 0;
            portEXIT_CRITICAL(&event_lock_);
        }
    }

    return success;
}

bool LcdDisplayPanelImpl::dispatch_event_from_isr(EventType event)
{
    EventListener listeners[MAX_EVENT_LISTENERS] = {};
    size_t listener_count = 0;
    BaseType_t need_yield = pdFALSE;

    portENTER_CRITICAL_ISR(&event_lock_);
    if (event == EventType::ColorTransDone) {
        completed_draw_seq_++;
        if (
            sync_waiting_ && (sync_done_ != nullptr) && (sync_target_draw_seq_ != 0) &&
            (completed_draw_seq_ >= sync_target_draw_seq_)
        ) {
            xSemaphoreGiveFromISR(sync_done_, &need_yield);
        }
    }
    for (const auto &listener : event_listeners_) {
        if (listener.is_used && (listener.callback != nullptr)) {
            listeners[listener_count] = listener;
            listener_count++;
        }
    }
    portEXIT_CRITICAL_ISR(&event_lock_);

    const auto event_id = static_cast<uint8_t>(event);
    bool listener_need_yield = false;
    for (size_t i = 0; i < listener_count; i++) {
        listener_need_yield = listeners[i].callback(event_id, true, listeners[i].user_ctx) || listener_need_yield;
    }

    return (need_yield == pdTRUE) || listener_need_yield;
}

bool LcdDisplayPanelImpl::add_event_listener(EventCallback callback, void *user_ctx, uint32_t *listener_id)
{
    BROOKESIA_CHECK_NULL_RETURN(callback, false, "Panel event listener callback is null");
    BROOKESIA_CHECK_NULL_RETURN(listener_id, false, "Panel event listener id pointer is null");

    boost::lock_guard<boost::mutex> lock(mutex_);
    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "LCD is not initialized");

    EventListener *free_slot = nullptr;
    portENTER_CRITICAL(&event_lock_);
    for (auto &listener : event_listeners_) {
        if (!listener.is_used) {
            free_slot = &listener;
            break;
        }
    }
    if (free_slot != nullptr) {
        const auto id = next_event_listener_id_++;
        if (next_event_listener_id_ == 0) {
            next_event_listener_id_ = 1;
        }
        *free_slot = {
            .is_used = true,
            .id = id,
            .callback = callback,
            .user_ctx = user_ctx,
        };
        *listener_id = id;
    }
    portEXIT_CRITICAL(&event_lock_);

    BROOKESIA_CHECK_FALSE_RETURN(free_slot != nullptr, false, "Panel event listener table is full");

    if (!enable_event_dispatcher_locked()) {
        portENTER_CRITICAL(&event_lock_);
        for (auto &listener : event_listeners_) {
            if (listener.is_used && (listener.id == *listener_id)) {
                listener = {};
                break;
            }
        }
        portEXIT_CRITICAL(&event_lock_);
        *listener_id = 0;
        return false;
    }

    return true;
}

bool LcdDisplayPanelImpl::remove_event_listener(uint32_t listener_id)
{
    boost::lock_guard<boost::mutex> lock(mutex_);

    bool removed = false;
    portENTER_CRITICAL(&event_lock_);
    for (auto &listener : event_listeners_) {
        if (listener.is_used && (listener.id == listener_id)) {
            listener = {};
            removed = true;
            break;
        }
    }
    portEXIT_CRITICAL(&event_lock_);

    BROOKESIA_CHECK_FALSE_RETURN(removed, false, "Panel event listener not found: %1%", listener_id);

    if (!has_event_listener_locked()) {
        disable_event_dispatcher_locked();
    }

    return true;
}

bool LcdDisplayPanelImpl::enable_event_dispatcher_locked()
{
    if (event_dispatcher_enabled_) {
        return true;
    }

    const auto bus_type = get_bus_type();
    esp_err_t ret = ESP_FAIL;
    switch (bus_type) {
    case display::PanelIface::BusType::Generic: {
        auto io_handle = get_io_handle(handles_);
        BROOKESIA_CHECK_NULL_RETURN(io_handle, false, "LCD panel IO handle is not available");

        esp_lcd_panel_io_callbacks_t callbacks = {};
        callbacks.on_color_trans_done = on_io_color_trans_done;
        ret = esp_lcd_panel_io_register_event_callbacks(io_handle, &callbacks, this);
        break;
    }
    case display::PanelIface::BusType::RGB: {
#if CONFIG_SOC_LCD_RGB_SUPPORTED
        esp_lcd_rgb_panel_event_callbacks_t callbacks = {};
        callbacks.on_color_trans_done = on_rgb_color_trans_done;
        callbacks.on_frame_buf_complete = on_rgb_frame_done;
        ret = esp_lcd_rgb_panel_register_event_callbacks(get_panel_handle(handles_), &callbacks, this);
#else
        BROOKESIA_LOGE("RGB LCD callbacks are not supported on this target");
#endif
        break;
    }
    case display::PanelIface::BusType::MIPI: {
#if CONFIG_SOC_MIPI_DSI_SUPPORTED
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        esp_lcd_dpi_panel_event_callbacks_t callbacks = {};
        callbacks.on_color_trans_done = on_mipi_color_trans_done;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 2)
        callbacks.on_frame_buf_complete = on_mipi_frame_done;
#else
        callbacks.on_refresh_done = on_mipi_frame_done;
#endif
        ret = esp_lcd_dpi_panel_register_event_callbacks(get_panel_handle(handles_), &callbacks, this);
#else
        BROOKESIA_LOGE("MIPI DPI callbacks are not supported on this target");
#endif
#pragma GCC diagnostic pop
        break;
    }
    default:
        BROOKESIA_LOGE("Unsupported LCD bus type: %1%", bus_type);
        break;
    }

    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to enable LCD event dispatcher");
    event_dispatcher_enabled_ = true;
    return true;
}

bool LcdDisplayPanelImpl::disable_event_dispatcher_locked()
{
    if (!event_dispatcher_enabled_ || !is_valid_internal()) {
        event_dispatcher_enabled_ = false;
        return true;
    }

    const auto bus_type = get_bus_type();
    esp_err_t ret = ESP_FAIL;
    switch (bus_type) {
    case display::PanelIface::BusType::Generic: {
        auto io_handle = get_io_handle(handles_);
        if (io_handle == nullptr) {
            ret = ESP_OK;
            break;
        }
        const esp_lcd_panel_io_callbacks_t callbacks = {};
        ret = esp_lcd_panel_io_register_event_callbacks(io_handle, &callbacks, nullptr);
        break;
    }
    case display::PanelIface::BusType::RGB: {
#if CONFIG_SOC_LCD_RGB_SUPPORTED
        const esp_lcd_rgb_panel_event_callbacks_t callbacks = {};
        ret = esp_lcd_rgb_panel_register_event_callbacks(get_panel_handle(handles_), &callbacks, nullptr);
#else
        ret = ESP_OK;
#endif
        break;
    }
    case display::PanelIface::BusType::MIPI: {
#if CONFIG_SOC_MIPI_DSI_SUPPORTED
        const esp_lcd_dpi_panel_event_callbacks_t callbacks = {};
        ret = esp_lcd_dpi_panel_register_event_callbacks(get_panel_handle(handles_), &callbacks, nullptr);
#else
        ret = ESP_OK;
#endif
        break;
    }
    default:
        ret = ESP_OK;
        break;
    }

    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to disable LCD event dispatcher");
    event_dispatcher_enabled_ = false;
    return true;
}

bool LcdDisplayPanelImpl::has_event_listener_locked() const
{
    bool has_listener = false;
    portENTER_CRITICAL(&event_lock_);
    for (const auto &listener : event_listeners_) {
        if (listener.is_used) {
            has_listener = true;
            break;
        }
    }
    portEXIT_CRITICAL(&event_lock_);

    return has_listener;
}

bool LcdDisplayPanelImpl::add_event_listener_trampoline(
    void *ctx, EventCallback callback, void *user_ctx, uint32_t *listener_id
)
{
    auto *impl = static_cast<LcdDisplayPanelImpl *>(ctx);
    BROOKESIA_CHECK_NULL_RETURN(impl, false, "Panel event dispatcher context is null");

    return impl->add_event_listener(callback, user_ctx, listener_id);
}

bool LcdDisplayPanelImpl::remove_event_listener_trampoline(void *ctx, uint32_t listener_id)
{
    auto *impl = static_cast<LcdDisplayPanelImpl *>(ctx);
    BROOKESIA_CHECK_NULL_RETURN(impl, false, "Panel event dispatcher context is null");

    return impl->remove_event_listener(listener_id);
}

bool LcdDisplayPanelImpl::get_driver_specific(DriverSpecific &specific)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD)) {
        BROOKESIA_LOGW("LCD panel device not found");
        return false;
    }

    dev_display_lcd_config_t *config = nullptr;
    auto ret = esp_board_manager_get_device_config(
                   ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, reinterpret_cast<void **>(&config)
               );
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get LCD config");
    BROOKESIA_CHECK_NULL_RETURN(config, false, "Failed to get LCD config");

    specific.io_handle = get_io_handle(handles_);
    specific.panel_handle = get_panel_handle(handles_);
    specific.bus_type = get_bus_type_from_config(config);
    specific.frame_buffer_count = get_frame_buffer_count_from_config(config);
    if (specific.bus_type == display::PanelIface::BusType::Max) {
        BROOKESIA_LOGW("Unsupported sub type: %1%", config->sub_type);
    }
    specific.draw_x_align_bytes = BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_PANEL_DRAW_X_ALIGN_BYTES;
    specific.draw_y_align_bytes = BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_PANEL_DRAW_Y_ALIGN_BYTES;
    specific.event_dispatcher = {
        .ctx = this,
        .add_listener = add_event_listener_trampoline,
        .remove_listener = remove_event_listener_trampoline,
    };

    return true;
}

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_PANEL_IMPL
