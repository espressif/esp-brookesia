/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <array>
#include <memory>
#include <set>
#include <string_view>

#include "unity.h"

#include "test_fixtures.hpp"

using namespace esp_brookesia::hal;

TEST_CASE("HAL fnv1a64 hash matches known vectors", "[hal][hash][fnv1a]")
{
    TEST_ASSERT_TRUE(0xcbf29ce484222325ULL == cal_fnv1a_64(std::string_view("")));
    TEST_ASSERT_TRUE(0xaf63dc4c8601ec8cULL == cal_fnv1a_64(std::string_view("a")));
    TEST_ASSERT_TRUE(0x85944171f73967e8ULL == cal_fnv1a_64(std::string_view("foobar")));
}

TEST_CASE("HAL get_device_interface_fnv1a derives from interface names and stays unique", "[hal][hash][interfaces]")
{
    // Keep this list aligned with currently exported HAL interfaces.
    const std::array ids = {
        get_device_interface_fnv1a<AudioPlayerIface>(),
        get_device_interface_fnv1a<AudioRecorderIface>(),
        get_device_interface_fnv1a<DisplayPanelIface>(),
        get_device_interface_fnv1a<DisplayTouchIface>(),
        get_device_interface_fnv1a<StatusLedIface>(),
        get_device_interface_fnv1a<StorageFsIface>(),
    };
    const std::set<uint64_t> unique_ids(ids.begin(), ids.end());

    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(ids.size()), static_cast<uint32_t>(unique_ids.size()));
    TEST_ASSERT_TRUE(cal_fnv1a_64(AudioPlayerIface::interface_name) == get_device_interface_fnv1a<AudioPlayerIface>());
    TEST_ASSERT_TRUE(cal_fnv1a_64(DisplayPanelIface::interface_name) == get_device_interface_fnv1a<DisplayPanelIface>());
    TEST_ASSERT_TRUE(cal_fnv1a_64(StorageFsIface::interface_name) == get_device_interface_fnv1a<StorageFsIface>());
}
