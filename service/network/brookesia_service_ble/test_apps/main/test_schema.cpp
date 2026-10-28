/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "general.hpp"

#include <algorithm>
#include <utility>
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_helper/framework/manager.hpp"

using namespace esp_brookesia;
using namespace esp_brookesia::test;

namespace {

const service::FunctionSchema *find_function_schema(BleHelper::FunctionId id)
{
    const auto expected_name = BROOKESIA_DESCRIBE_TO_STR(id);
    const auto schemas = BleHelper::get_function_schemas();
    const auto it = std::ranges::find_if(schemas, [&](const auto & schema) {
        return schema.name == expected_name;
    });
    return (it == schemas.end()) ? nullptr : &*it;
}

const service::EventSchema *find_event_schema(BleHelper::EventId id)
{
    const auto expected_name = BROOKESIA_DESCRIBE_TO_STR(id);
    const auto schemas = BleHelper::get_event_schemas();
    const auto it = std::ranges::find_if(schemas, [&](const auto & schema) {
        return schema.name == expected_name;
    });
    return (it == schemas.end()) ? nullptr : &*it;
}

} // namespace

BROOKESIA_TEST_CASE(helper_schema_contract, "Test ServiceBle - helper schema contract", "[service][ble][schema]")
{
    TEST_ASSERT_EQUAL_STRING("Ble", BleHelper::get_name().data());
    TEST_ASSERT_EQUAL_size_t(8, BleHelper::get_function_schemas().size());
    TEST_ASSERT_EQUAL_size_t(7, BleHelper::get_event_schemas().size());

    const auto *notify = find_function_schema(BleHelper::FunctionId::Notify);
    TEST_ASSERT_NOT_NULL(notify);
    TEST_ASSERT_EQUAL_size_t(4, notify->parameters.size());
    TEST_ASSERT_EQUAL_STRING("ConnectionId", notify->parameters[0].name.c_str());
    TEST_ASSERT_EQUAL_STRING("ServiceUuid", notify->parameters[1].name.c_str());
    TEST_ASSERT_EQUAL_STRING("CharacteristicUuid", notify->parameters[2].name.c_str());
    TEST_ASSERT_EQUAL_STRING("Data", notify->parameters[3].name.c_str());
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(service::FunctionValueType::Array),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(notify->parameters[3].type)
    );

    const auto *written = find_event_schema(BleHelper::EventId::CharacteristicWritten);
    TEST_ASSERT_NOT_NULL(written);
    TEST_ASSERT_EQUAL_size_t(4, written->items.size());
    TEST_ASSERT_EQUAL_STRING("Data", written->items[3].name.c_str());
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(service::EventItemType::Array),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(written->items[3].type)
    );

    TEST_ASSERT_TRUE(service::ServiceRegistry::has_plugin(std::string(BleHelper::get_name())));
    TEST_ASSERT_TRUE(lib_utils::PluginRegistry<hal::Device>::has_plugin(FakeBleDevice::NAME));

    service::ServiceBinding binding;
    std::shared_ptr<FakeBlePeripheral> peripheral;
    TEST_ASSERT_TRUE(start_ble_service(binding, peripheral));
    lib_utils::FunctionGuard cleanup([&]() {
        stop_ble_service(binding);
    });

    const auto service_info = service::helper::Manager::get_service_info(
                                  std::string(BleHelper::get_name()), EVENT_TIMEOUT_MS
                              );
    TEST_ASSERT_TRUE_MESSAGE(
        service_info.has_value(), service_info ? "Manager returned BLE service info" : service_info.error().c_str()
    );
    TEST_ASSERT_EQUAL_STRING("Ble", service_info->name.c_str());
    TEST_ASSERT_EQUAL_STRING("0.8.0", service_info->version.c_str());

    const auto service_schema = service::helper::Manager::get_service_schema(
                                    std::string(BleHelper::get_name()), EVENT_TIMEOUT_MS
                                );
    TEST_ASSERT_TRUE_MESSAGE(
        service_schema.has_value(),
        service_schema ? "Manager returned BLE service schema" : service_schema.error().c_str()
    );
    TEST_ASSERT_EQUAL_STRING("Ble", service_schema->name.c_str());
    TEST_ASSERT_FALSE(service_schema->description.empty());
    TEST_ASSERT_EQUAL_STRING(
        "Manage BLE peripheral advertising, GATT connections, writes, and notifications.",
        service_schema->description.c_str()
    );

    std::vector<std::string> expected_function_names = {
        "SetPeripheralConfig",
        "GetPeripheralConfig",
        "TriggerAdvertisingStart",
        "TriggerAdvertisingStop",
        "GetState",
        "GetConnections",
        "Notify",
        "Disconnect",
    };
    auto function_names = service_schema->function_names;
    std::ranges::sort(expected_function_names);
    std::ranges::sort(function_names);
    TEST_ASSERT_TRUE(expected_function_names == function_names);

    std::vector<std::string> expected_event_names = {
        "GeneralStateChanged",
        "AdvertisingStateChanged",
        "ConnectionStateChanged",
        "MtuChanged",
        "SubscriptionChanged",
        "CharacteristicWritten",
        "ErrorHappened",
    };
    auto event_names = service_schema->event_names;
    std::ranges::sort(expected_event_names);
    std::ranges::sort(event_names);
    TEST_ASSERT_TRUE(expected_event_names == event_names);
}

BROOKESIA_TEST_CASE(
    config_describe_and_validation, "Test ServiceBle - described config and validation", "[service][ble][schema]"
)
{
    auto config = make_test_config();
    BleHelper::PeripheralConfig parsed;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(BROOKESIA_DESCRIBE_TO_JSON(config), parsed));
    TEST_ASSERT_TRUE(config == parsed);

    TEST_ASSERT_TRUE(hal::bluetooth::ble::is_valid_uuid("7A5A0001-9B7B-4D20-8F30-6D9F0E7F4A10"));
    TEST_ASSERT_EQUAL_STRING(
        SERVICE_UUID.data(), hal::bluetooth::ble::normalize_uuid("7A5A0001-9B7B-4D20-8F30-6D9F0E7F4A10").c_str()
    );
    TEST_ASSERT_FALSE(hal::bluetooth::ble::is_valid_uuid("7a5a0001"));

    std::string error_message;
    TEST_ASSERT_TRUE(hal::bluetooth::ble::validate_peripheral_config(config, &error_message));
    TEST_ASSERT_TRUE(error_message.empty());

    auto invalid = config;
    invalid.max_connections = 2;
    TEST_ASSERT_FALSE(hal::bluetooth::ble::validate_peripheral_config(invalid, &error_message));

    invalid = config;
    invalid.preferred_mtu = 22;
    TEST_ASSERT_FALSE(hal::bluetooth::ble::validate_peripheral_config(invalid, &error_message));
    invalid.preferred_mtu = 528;
    TEST_ASSERT_FALSE(hal::bluetooth::ble::validate_peripheral_config(invalid, &error_message));

    invalid = config;
    invalid.services.front().characteristics.front().write = false;
    invalid.services.front().characteristics.front().write_without_response = false;
    TEST_ASSERT_FALSE(hal::bluetooth::ble::validate_peripheral_config(invalid, &error_message));

    invalid = config;
    invalid.services.front().characteristics.push_back(invalid.services.front().characteristics.front());
    TEST_ASSERT_FALSE(hal::bluetooth::ble::validate_peripheral_config(invalid, &error_message));

    invalid = config;
    invalid.services.front().uuid.clear();
    TEST_ASSERT_FALSE(hal::bluetooth::ble::validate_peripheral_config(invalid, &error_message));

    invalid = config;
    invalid.services.front().characteristics.front().uuid.clear();
    TEST_ASSERT_FALSE(hal::bluetooth::ble::validate_peripheral_config(invalid, &error_message));

    invalid = config;
    invalid.advertised_service_uuids.push_back(invalid.advertised_service_uuids.front());
    TEST_ASSERT_FALSE(hal::bluetooth::ble::validate_peripheral_config(invalid, &error_message));

    auto same_characteristic_in_another_service = config;
    auto second_service = config.services.front();
    second_service.uuid = "7a5a0011-9b7b-4d20-8f30-6d9f0e7f4a10";
    same_characteristic_in_another_service.services.push_back(std::move(second_service));
    TEST_ASSERT_TRUE(
        hal::bluetooth::ble::validate_peripheral_config(same_characteristic_in_another_service, &error_message)
    );
}
