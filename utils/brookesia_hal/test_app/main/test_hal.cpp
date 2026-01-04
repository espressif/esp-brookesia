/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "unity.h"
#include "brookesia/hal.hpp"
#include "esp_err.h"

#include <array>
#include <cstdint>
#include <string_view>

using namespace esp_brookesia::hal;

namespace {

constexpr uint64_t kFnvEmpty = 0xcbf29ce484222325ULL;
constexpr uint64_t kFnvStringA = 0xaf63dc4c8601ec8cULL;
constexpr uint64_t kFnvFoobar = 0x85944171f73967e8ULL;

class TestLedDevice : public DeviceImpl<TestLedDevice>, public ILED_Indicator {
public:
    TestLedDevice(): DeviceImpl<TestLedDevice>("test_led_device") {}

    void startBlink(BlinkType type) override
    {
        last_started = type;
        ++start_count;
    }

    void stopBlink(BlinkType type) override
    {
        last_stopped = type;
        ++stop_count;
    }

    void *query_interface(uint64_t id) override
    {
        return build_table<ILED_Indicator>(id);
    }

    BlinkType last_started = BLINK_MAX;
    BlinkType last_stopped = BLINK_MAX;
    uint32_t start_count = 0;
    uint32_t stop_count = 0;
    uint32_t register_wifi_event_calls = 0;
};

class MultiInterfaceDevice : public DeviceImpl<MultiInterfaceDevice>, public IIMUSensor, public ITouchSensor {
public:
    explicit MultiInterfaceDevice(void *button)
        : DeviceImpl<MultiInterfaceDevice>("multi_iface_device"), button_handle(button)
    {
    }

    void *query_interface(uint64_t id) override
    {
        return build_table<IIMUSensor, ITouchSensor>(id);
    }

    bool read_accel(std::array<float, 3> &mps2, uint64_t &ts_us) override
    {
        ++accel_read_count;
        mps2 = accel_data;
        ts_us = timestamp_us;
        return true;
    }

    void *get_button_handle() override
    {
        return button_handle;
    }

    std::array<float, 3> accel_data{0.1F, 0.2F, 9.81F};
    uint64_t timestamp_us = 123456;
    int accel_read_count = 0;

private:
    void *button_handle;
};

struct GestureEmitter : public IIMUSensor {
    void emit(GestureType type)
    {
        gesture_signal(type);
    }
};

} // namespace

TEST_CASE("fnv1a64 hash matches known vectors", "[hal][auto][hash]")
{
    TEST_ASSERT_EQUAL(kFnvEmpty, fnv1a_64(std::string_view("")));
    TEST_ASSERT_EQUAL(kFnvStringA, fnv1a_64(std::string_view("a")));
    TEST_ASSERT_EQUAL(kFnvFoobar, fnv1a_64(std::string_view("foobar")));
}

TEST_CASE("IID derives from interface names and stays unique", "[hal][auto][iid]")
{
    TEST_ASSERT_EQUAL(fnv1a_64(IIMUSensor::interface_name), IID<IIMUSensor>());
    TEST_ASSERT_EQUAL(fnv1a_64(ITouchSensor::interface_name), IID<ITouchSensor>());
    TEST_ASSERT_NOT_EQUAL(IID<IIMUSensor>(), IID<ILED_Indicator>());
    TEST_ASSERT_NOT_EQUAL(IID<ITouchSensor>(), IID<IFileSystem>());
}

TEST_CASE("DeviceImpl exposes supported interfaces only", "[hal][auto][device]")
{
    TestLedDevice device;
    auto *led_iface = device.query<ILED_Indicator>();
    TEST_ASSERT_NOT_NULL(led_iface);

    led_iface->startBlink(ILED_Indicator::BLINK_WIFI_CONNECTED);
    led_iface->startBlink(ILED_Indicator::BLINK_WIFI_DISCONNECTED);
    led_iface->stopBlink(ILED_Indicator::BLINK_WIFI_CONNECTED);

    TEST_ASSERT_EQUAL(2, device.start_count);
    TEST_ASSERT_EQUAL(1, device.stop_count);
    TEST_ASSERT_EQUAL(ILED_Indicator::BLINK_WIFI_DISCONNECTED, device.last_started);
    TEST_ASSERT_EQUAL(ILED_Indicator::BLINK_WIFI_CONNECTED, device.last_stopped);
    TEST_ASSERT_NULL(device.query<IIMUSensor>());
}

TEST_CASE("Multi interface device shares sensor data and touch handle", "[hal][auto][device][sensor]")
{
    constexpr void *kHandle = nullptr;
    MultiInterfaceDevice device(kHandle);

    auto *imu_iface = device.query<IIMUSensor>();
    TEST_ASSERT_NOT_NULL(imu_iface);

    std::array<float, 3> accel{0.0F, 0.0F, 0.0F};
    uint64_t ts = 0;
    TEST_ASSERT_TRUE(imu_iface->read_accel(accel, ts));
    TEST_ASSERT_EQUAL(123456ULL, ts);
    TEST_ASSERT_EQUAL_FLOAT(device.accel_data[0], accel[0]);
    TEST_ASSERT_EQUAL_FLOAT(device.accel_data[1], accel[1]);
    TEST_ASSERT_EQUAL_FLOAT(device.accel_data[2], accel[2]);
    TEST_ASSERT_EQUAL(1, device.accel_read_count);

    auto *touch_iface = device.query<ITouchSensor>();
    TEST_ASSERT_NOT_NULL(touch_iface);
    TEST_ASSERT_EQUAL_PTR(kHandle, touch_iface->get_button_handle());
}

TEST_CASE("IIMUSensor gesture signal fanout", "[hal][auto][sensor]")
{
    GestureEmitter emitter;
    int any_motion_count = 0;
    int circle_count = 0;

    auto conn1 = emitter.gesture_signal.connect([&](IIMUSensor::GestureType type) {
        if (type == IIMUSensor::ANY_MOTION) {
            ++any_motion_count;
        }
    });
    auto conn2 = emitter.gesture_signal.connect([&](IIMUSensor::GestureType type) {
        if (type == IIMUSensor::CIRCLE_CLOCKWISE) {
            ++circle_count;
        }
    });

    emitter.emit(IIMUSensor::ANY_MOTION);
    emitter.emit(IIMUSensor::CIRCLE_CLOCKWISE);
    emitter.emit(IIMUSensor::CIRCLE_CLOCKWISE);

    TEST_ASSERT_EQUAL(1, any_motion_count);
    TEST_ASSERT_EQUAL(2, circle_count);

    conn1.disconnect();
    conn2.disconnect();
}

TEST_CASE("Default IMU methods report unsupported data gracefully", "[hal][auto][sensor][edge]")
{
    struct DefaultIMU : public IIMUSensor {};
    DefaultIMU imu;
    std::array<float, 3> accel{1.0F, 2.0F, 3.0F};
    uint64_t ts = 99;

    TEST_ASSERT_FALSE(imu.read_accel(accel, ts));
    TEST_ASSERT_FALSE(imu.read_gyro(accel, ts));
    TEST_ASSERT_FALSE(imu.read_mag(accel, ts));
    TEST_ASSERT_FALSE(imu.read_temperature(accel[0], ts));

    TEST_ASSERT_EQUAL_FLOAT(1.0F, accel[0]);
    TEST_ASSERT_EQUAL_FLOAT(2.0F, accel[1]);
    TEST_ASSERT_EQUAL_FLOAT(3.0F, accel[2]);
    TEST_ASSERT_EQUAL(99, ts);
}
