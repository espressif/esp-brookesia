/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed panel HAL interface (holds LCD board handles, not the device object).
 */
class LcdDisplayPanelImpl : public display::PanelIface {
public:
    static constexpr size_t MAX_EVENT_LISTENERS = 8;

    LcdDisplayPanelImpl();
    ~LcdDisplayPanelImpl() override;

    bool draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data) override;
    bool draw_bitmap_sync(
        uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data,
        uint32_t timeout_ms = display::PanelIface::DEFAULT_SYNC_DRAW_TIMEOUT_MS
    ) override;

    bool get_driver_specific(DriverSpecific &specific) override;
    bool dispatch_event_from_isr(EventType event);

    bool is_valid() const
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        return is_valid_internal();
    }

private:
    struct EventListener {
        bool is_used = false;
        uint32_t id = 0;
        EventCallback callback = nullptr;
        void *user_ctx = nullptr;
    };

    bool is_valid_internal() const
    {
        return handles_ != nullptr;
    }

    bool add_event_listener(EventCallback callback, void *user_ctx, uint32_t *listener_id);
    bool remove_event_listener(uint32_t listener_id);
    bool enable_event_dispatcher_locked();
    bool disable_event_dispatcher_locked();
    bool has_event_listener_locked() const;
    bool submit_draw_bitmap_locked(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data);

    static bool add_event_listener_trampoline(
        void *ctx, EventCallback callback, void *user_ctx, uint32_t *listener_id
    );
    static bool remove_event_listener_trampoline(void *ctx, uint32_t listener_id);

    mutable boost::mutex mutex_;
    void *handles_ = nullptr;
    SemaphoreHandle_t sync_done_ = nullptr;
    bool sync_waiting_ = false;
    bool event_dispatcher_enabled_ = false;
    uint64_t submitted_draw_seq_ = 0;
    uint64_t completed_draw_seq_ = 0;
    uint64_t sync_target_draw_seq_ = 0;
    uint32_t next_event_listener_id_ = 1;
    std::array<EventListener, MAX_EVENT_LISTENERS> event_listeners_{};
    mutable portMUX_TYPE event_lock_ = portMUX_INITIALIZER_UNLOCKED;
};

} // namespace esp_brookesia::hal
