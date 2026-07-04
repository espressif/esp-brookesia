/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/display/backlight.hpp"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"

namespace esp_brookesia::test_apps::service_display {

struct MockDrawRecord {
    uint32_t panel_index = 0;
    uint32_t x1 = 0;
    uint32_t y1 = 0;
    uint32_t x2 = 0;
    uint32_t y2 = 0;
    const uint8_t *data_ptr = nullptr;
    std::vector<uint8_t> data;
};

class MockDisplayDevice: public hal::Device {
public:
    static constexpr const char *DEVICE_NAME = "MockDisplay";
    static constexpr const char *PANEL0_IFACE_NAME = "MockDisplay:Panel0";
    static constexpr const char *PANEL1_IFACE_NAME = "MockDisplay:Panel1";
    static constexpr const char *BACKLIGHT0_IFACE_NAME = "MockDisplay:Backlight0";
    static constexpr const char *BACKLIGHT1_IFACE_NAME = "MockDisplay:Backlight1";
    static constexpr const char *TOUCH0_IFACE_NAME = "MockDisplay:Touch0";
    static constexpr const char *TOUCH1_IFACE_NAME = "MockDisplay:Touch1";
    static constexpr const char *GROUP0_ID = "mock_display_0";
    static constexpr const char *GROUP1_ID = "mock_display_1";

    static MockDisplayDevice &get_instance()
    {
        static MockDisplayDevice instance;
        return instance;
    }

    static void reset_draw_records();
    static std::vector<MockDrawRecord> get_draw_records();
    static void set_sync_draw_blocked(bool blocked);
    static bool wait_for_sync_draw_enter_count(size_t expected_count, uint32_t timeout_ms);
    static void release_sync_draws(size_t count);
    static uint8_t get_backlight_brightness(uint32_t backlight_index);
    static bool get_backlight_on(uint32_t backlight_index);
    static void set_touch_points(uint32_t touch_index, std::vector<hal::display::TouchIface::Point> points);
    static void trigger_touch_interrupt(uint32_t touch_index);
    static bool wait_for_touch_read_count(uint32_t touch_index, size_t expected_count, uint32_t timeout_ms);

private:
    class MockPanel: public hal::display::PanelIface {
    public:
        explicit MockPanel(uint32_t panel_index)
            : hal::display::PanelIface(Info{
            .h_res = 240,
            .v_res = 240,
            .pixel_format = PixelFormat::RGB565,
            .group_id = (panel_index == 0) ? GROUP0_ID : GROUP1_ID,
        })
        , panel_index_(panel_index)
        {
        }

        bool draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data) override;
        bool draw_bitmap_sync(
            uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data, uint32_t timeout_ms
        ) override;

    private:
        uint32_t panel_index_ = 0;
    };

    class MockTouch: public hal::display::TouchIface {
    public:
        MockTouch(uint32_t touch_index, OperationMode operation_mode)
            : hal::display::TouchIface(Info{
            .x_max = 240,
            .y_max = 240,
            .max_points = 5,
            .operation_mode = operation_mode,
            .group_id = (touch_index == 0) ? GROUP0_ID : GROUP1_ID,
        })
        , touch_index_(touch_index)
        {
        }

        bool read_points(std::vector<Point> &points) override;
        bool register_interrupt_handler(InterruptHandler handler, void *ctx) override;
        void trigger_interrupt();

    private:
        uint32_t touch_index_ = 0;
        InterruptHandler interrupt_handler_ = nullptr;
        void *interrupt_handler_ctx_ = nullptr;
    };

    class MockBacklight: public hal::display::BacklightIface {
    public:
        explicit MockBacklight(uint32_t backlight_index)
            : hal::display::BacklightIface(Info{
            .group_id = (backlight_index == 0) ? GROUP0_ID : GROUP1_ID,
        })
        , backlight_index_(backlight_index)
        {
        }

        bool set_brightness(uint8_t percent) override;
        bool get_brightness(uint8_t &percent) override;
        bool is_light_on_off_supported() override;
        bool set_light_on_off(bool on) override;
        bool is_light_on() const override;

    private:
        uint32_t backlight_index_ = 0;
    };

    MockDisplayDevice()
        : hal::Device(std::string(DEVICE_NAME))
    {
    }
    ~MockDisplayDevice() = default;

    bool probe() override;
    std::vector<hal::InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<MockPanel> panel0_;
    std::shared_ptr<MockPanel> panel1_;
    std::shared_ptr<MockBacklight> backlight0_;
    std::shared_ptr<MockBacklight> backlight1_;
    std::shared_ptr<MockTouch> touch0_;
    std::shared_ptr<MockTouch> touch1_;

    static std::mutex draw_records_mutex_;
    static std::vector<MockDrawRecord> draw_records_;
    static std::mutex sync_draw_mutex_;
    static std::condition_variable sync_draw_cv_;
    static bool sync_draw_blocked_;
    static size_t sync_draw_enter_count_;
    static size_t sync_draw_release_count_;
    static std::mutex touch_mutex_;
    static std::condition_variable touch_cv_;
    static std::vector<hal::display::TouchIface::Point> touch_points_[2];
    static size_t touch_read_count_[2];
    static std::mutex backlight_mutex_;
    static uint8_t backlight_brightness_[2];
    static bool backlight_on_[2];
};

} // namespace esp_brookesia::test_apps::service_display
