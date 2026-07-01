/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "mock_display_device.hpp"

#include <chrono>
#include <utility>

#include "brookesia/lib_utils/plugin.hpp"

namespace esp_brookesia::test_apps::service_display {

std::mutex MockDisplayDevice::draw_records_mutex_;
std::vector<MockDrawRecord> MockDisplayDevice::draw_records_;
std::mutex MockDisplayDevice::sync_draw_mutex_;
std::condition_variable MockDisplayDevice::sync_draw_cv_;
bool MockDisplayDevice::sync_draw_blocked_ = false;
size_t MockDisplayDevice::sync_draw_enter_count_ = 0;
size_t MockDisplayDevice::sync_draw_release_count_ = 0;
std::mutex MockDisplayDevice::touch_mutex_;
std::condition_variable MockDisplayDevice::touch_cv_;
std::vector<hal::display::TouchIface::Point> MockDisplayDevice::touch_points_[2];
size_t MockDisplayDevice::touch_read_count_[2] = {0, 0};
std::mutex MockDisplayDevice::backlight_mutex_;
uint8_t MockDisplayDevice::backlight_brightness_[2] = {0, 0};
bool MockDisplayDevice::backlight_on_[2] = {false, false};

void MockDisplayDevice::reset_draw_records()
{
    {
        std::lock_guard lock(draw_records_mutex_);
        draw_records_.clear();
    }
    {
        std::lock_guard lock(sync_draw_mutex_);
        sync_draw_blocked_ = false;
        sync_draw_enter_count_ = 0;
        sync_draw_release_count_ = 0;
    }
    {
        std::lock_guard lock(touch_mutex_);
        touch_points_[0].clear();
        touch_points_[1].clear();
        touch_read_count_[0] = 0;
        touch_read_count_[1] = 0;
    }
    {
        std::lock_guard lock(backlight_mutex_);
        backlight_brightness_[0] = 0;
        backlight_brightness_[1] = 0;
        backlight_on_[0] = false;
        backlight_on_[1] = false;
    }
    sync_draw_cv_.notify_all();
    touch_cv_.notify_all();
}

std::vector<MockDrawRecord> MockDisplayDevice::get_draw_records()
{
    std::lock_guard lock(draw_records_mutex_);
    return draw_records_;
}

void MockDisplayDevice::set_sync_draw_blocked(bool blocked)
{
    {
        std::lock_guard lock(sync_draw_mutex_);
        sync_draw_blocked_ = blocked;
    }
    sync_draw_cv_.notify_all();
}

bool MockDisplayDevice::wait_for_sync_draw_enter_count(size_t expected_count, uint32_t timeout_ms)
{
    std::unique_lock lock(sync_draw_mutex_);
    return sync_draw_cv_.wait_for(
    lock, std::chrono::milliseconds(timeout_ms), [expected_count]() {
        return sync_draw_enter_count_ >= expected_count;
    }
           );
}

void MockDisplayDevice::release_sync_draws(size_t count)
{
    {
        std::lock_guard lock(sync_draw_mutex_);
        sync_draw_release_count_ += count;
    }
    sync_draw_cv_.notify_all();
}

uint8_t MockDisplayDevice::get_backlight_brightness(uint32_t backlight_index)
{
    if (backlight_index >= 2) {
        return 0;
    }
    std::lock_guard lock(backlight_mutex_);
    return backlight_brightness_[backlight_index];
}

bool MockDisplayDevice::get_backlight_on(uint32_t backlight_index)
{
    if (backlight_index >= 2) {
        return false;
    }
    std::lock_guard lock(backlight_mutex_);
    return backlight_on_[backlight_index];
}

void MockDisplayDevice::set_touch_points(uint32_t touch_index, std::vector<hal::display::TouchIface::Point> points)
{
    if (touch_index >= 2) {
        return;
    }
    {
        std::lock_guard lock(touch_mutex_);
        touch_points_[touch_index] = std::move(points);
    }
    touch_cv_.notify_all();
}

void MockDisplayDevice::trigger_touch_interrupt(uint32_t touch_index)
{
    if (touch_index == 0 && get_instance().touch0_) {
        get_instance().touch0_->trigger_interrupt();
    } else if (touch_index == 1 && get_instance().touch1_) {
        get_instance().touch1_->trigger_interrupt();
    }
}

bool MockDisplayDevice::wait_for_touch_read_count(uint32_t touch_index, size_t expected_count, uint32_t timeout_ms)
{
    if (touch_index >= 2) {
        return false;
    }
    std::unique_lock lock(touch_mutex_);
    return touch_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [touch_index, expected_count]() {
        return touch_read_count_[touch_index] >= expected_count;
    });
}

bool MockDisplayDevice::MockPanel::draw_bitmap(
    uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data
)
{
    const size_t width = x2 - x1;
    const size_t height = y2 - y1;
    const size_t data_size = width * height * 2;

    MockDrawRecord record = {
        .panel_index = panel_index_,
        .x1 = x1,
        .y1 = y1,
        .x2 = x2,
        .y2 = y2,
        .data_ptr = data,
        .data = {},
    };
    record.data.assign(data, data + data_size);

    std::lock_guard lock(MockDisplayDevice::draw_records_mutex_);
    MockDisplayDevice::draw_records_.push_back(std::move(record));
    return true;
}

bool MockDisplayDevice::MockPanel::draw_bitmap_sync(
    uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data, uint32_t timeout_ms
)
{
    {
        std::unique_lock lock(MockDisplayDevice::sync_draw_mutex_);
        ++MockDisplayDevice::sync_draw_enter_count_;
        MockDisplayDevice::sync_draw_cv_.notify_all();
        if (MockDisplayDevice::sync_draw_blocked_ && (timeout_ms != 0)) {
            const auto timeout = std::chrono::milliseconds(timeout_ms);
            const bool released = MockDisplayDevice::sync_draw_cv_.wait_for(lock, timeout, []() {
                return !MockDisplayDevice::sync_draw_blocked_ ||
                       (MockDisplayDevice::sync_draw_release_count_ > 0);
            });
            if (!released) {
                return false;
            }
            if (MockDisplayDevice::sync_draw_release_count_ > 0) {
                --MockDisplayDevice::sync_draw_release_count_;
            }
        }
    }

    return draw_bitmap(x1, y1, x2, y2, data);
}

bool MockDisplayDevice::MockTouch::read_points(std::vector<Point> &points)
{
    {
        std::lock_guard lock(MockDisplayDevice::touch_mutex_);
        points = MockDisplayDevice::touch_points_[touch_index_];
        ++MockDisplayDevice::touch_read_count_[touch_index_];
    }
    MockDisplayDevice::touch_cv_.notify_all();
    return true;
}

bool MockDisplayDevice::MockTouch::register_interrupt_handler(InterruptHandler handler, void *ctx)
{
    interrupt_handler_ = handler;
    interrupt_handler_ctx_ = ctx;
    return true;
}

void MockDisplayDevice::MockTouch::trigger_interrupt()
{
    if (interrupt_handler_) {
        (void)interrupt_handler_(interrupt_handler_ctx_);
    }
}

bool MockDisplayDevice::MockBacklight::set_brightness(uint8_t percent)
{
    if (backlight_index_ >= 2) {
        return false;
    }
    std::lock_guard lock(MockDisplayDevice::backlight_mutex_);
    MockDisplayDevice::backlight_brightness_[backlight_index_] = percent;
    return true;
}

bool MockDisplayDevice::MockBacklight::get_brightness(uint8_t &percent)
{
    if (backlight_index_ >= 2) {
        return false;
    }
    std::lock_guard lock(MockDisplayDevice::backlight_mutex_);
    percent = MockDisplayDevice::backlight_brightness_[backlight_index_];
    return true;
}

bool MockDisplayDevice::MockBacklight::is_light_on_off_supported()
{
    return true;
}

bool MockDisplayDevice::MockBacklight::set_light_on_off(bool on)
{
    if (backlight_index_ >= 2) {
        return false;
    }
    std::lock_guard lock(MockDisplayDevice::backlight_mutex_);
    MockDisplayDevice::backlight_on_[backlight_index_] = on;
    return true;
}

bool MockDisplayDevice::MockBacklight::is_light_on() const
{
    if (backlight_index_ >= 2) {
        return false;
    }
    std::lock_guard lock(MockDisplayDevice::backlight_mutex_);
    return MockDisplayDevice::backlight_on_[backlight_index_];
}

bool MockDisplayDevice::probe()
{
    return true;
}

std::vector<hal::InterfaceSpec> MockDisplayDevice::get_interface_specs() const
{
    return {
        {hal::display::PanelIface::NAME, PANEL0_IFACE_NAME},
        {hal::display::PanelIface::NAME, PANEL1_IFACE_NAME},
        {hal::display::BacklightIface::NAME, BACKLIGHT0_IFACE_NAME},
        {hal::display::BacklightIface::NAME, BACKLIGHT1_IFACE_NAME},
        {hal::display::TouchIface::NAME, TOUCH0_IFACE_NAME},
        {hal::display::TouchIface::NAME, TOUCH1_IFACE_NAME},
    };
}

bool MockDisplayDevice::on_init()
{
    panel0_ = std::make_shared<MockPanel>(0);
    panel1_ = std::make_shared<MockPanel>(1);
    backlight0_ = std::make_shared<MockBacklight>(0);
    backlight1_ = std::make_shared<MockBacklight>(1);
    touch0_ = std::make_shared<MockTouch>(0, hal::display::TouchIface::OperationMode::Interrupt);
    touch1_ = std::make_shared<MockTouch>(1, hal::display::TouchIface::OperationMode::Polling);
    interfaces_.emplace(PANEL0_IFACE_NAME, panel0_);
    interfaces_.emplace(PANEL1_IFACE_NAME, panel1_);
    interfaces_.emplace(BACKLIGHT0_IFACE_NAME, backlight0_);
    interfaces_.emplace(BACKLIGHT1_IFACE_NAME, backlight1_);
    interfaces_.emplace(TOUCH0_IFACE_NAME, touch0_);
    interfaces_.emplace(TOUCH1_IFACE_NAME, touch1_);
    return true;
}

void MockDisplayDevice::on_deinit()
{
    interfaces_.erase(TOUCH1_IFACE_NAME);
    interfaces_.erase(TOUCH0_IFACE_NAME);
    interfaces_.erase(BACKLIGHT1_IFACE_NAME);
    interfaces_.erase(BACKLIGHT0_IFACE_NAME);
    interfaces_.erase(PANEL1_IFACE_NAME);
    interfaces_.erase(PANEL0_IFACE_NAME);
    touch1_.reset();
    touch0_.reset();
    backlight1_.reset();
    backlight0_.reset();
    panel1_.reset();
    panel0_.reset();
}

BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    hal::Device, MockDisplayDevice, MockDisplayDevice::DEVICE_NAME, MockDisplayDevice::get_instance(),
    mock_display_device_symbol
);

} // namespace esp_brookesia::test_apps::service_display
