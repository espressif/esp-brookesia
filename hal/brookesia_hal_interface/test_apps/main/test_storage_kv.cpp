/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <type_traits>
#include <variant>
#include "unity.h"
#include "common.hpp"
#include "test_storage_kv.hpp"

using namespace esp_brookesia;

namespace esp_brookesia {

bool TestStorageKvIface::init()
{
    is_initialized_ = true;
    return true;
}

void TestStorageKvIface::deinit()
{
    namespaces_.clear();
    is_initialized_ = false;
}

bool TestStorageKvIface::list(const std::string &nspace, std::vector<EntryInfo> &entries)
{
    entries.clear();
    if (!is_initialized_) {
        last_error_ = "Not initialized";
        return false;
    }

    auto namespace_it = namespaces_.find(nspace);
    if (namespace_it == namespaces_.end()) {
        return true;
    }

    for (const auto &[key, value] : namespace_it->second) {
        auto type = std::visit([](auto &&stored_value) {
            using T = std::decay_t<decltype(stored_value)>;
            if constexpr (std::is_same_v<T, bool>) {
                return ValueType::Bool;
            } else if constexpr (std::is_same_v<T, int32_t>) {
                return ValueType::Int;
            } else {
                return ValueType::String;
            }
        }, value);
        entries.push_back(EntryInfo{
            .nspace = nspace,
            .key = key,
            .type = type,
        });
    }

    return true;
}

bool TestStorageKvIface::set(const std::string &nspace, const KeyValueMap &key_value_map)
{
    if (!is_initialized_) {
        last_error_ = "Not initialized";
        return false;
    }

    auto &storage = namespaces_[nspace];
    for (const auto &[key, value] : key_value_map) {
        storage[key] = value;
    }

    return true;
}

bool TestStorageKvIface::get(
    const std::string &nspace, const std::vector<std::string> &keys, KeyValueMap &key_value_map
)
{
    key_value_map.clear();
    if (!is_initialized_) {
        last_error_ = "Not initialized";
        return false;
    }

    auto namespace_it = namespaces_.find(nspace);
    if (namespace_it == namespaces_.end()) {
        return true;
    }

    if (keys.empty()) {
        key_value_map = namespace_it->second;
        return true;
    }

    for (const auto &key : keys) {
        auto value_it = namespace_it->second.find(key);
        if (value_it != namespace_it->second.end()) {
            key_value_map.emplace(key, value_it->second);
        }
    }

    return true;
}

bool TestStorageKvIface::erase(const std::string &nspace, const std::vector<std::string> &keys)
{
    if (!is_initialized_) {
        last_error_ = "Not initialized";
        return false;
    }

    auto namespace_it = namespaces_.find(nspace);
    if (namespace_it == namespaces_.end()) {
        return true;
    }

    if (keys.empty()) {
        namespaces_.erase(namespace_it);
        return true;
    }

    for (const auto &key : keys) {
        namespace_it->second.erase(key);
    }
    if (namespace_it->second.empty()) {
        namespaces_.erase(namespace_it);
    }

    return true;
}

bool TestStorageKvDevice::probe()
{
    return true;
}

std::vector<hal::InterfaceSpec> TestStorageKvDevice::get_interface_specs() const
{
    return {{hal::storage::KeyValueIface::NAME, TestStorageKvIface::NAME}};
}

bool TestStorageKvDevice::on_init()
{
    auto interface = std::make_shared<TestStorageKvIface>();
    TEST_ASSERT_TRUE(interface->init());

    interfaces_.emplace(TestStorageKvIface::NAME, interface);

    return true;
}

void TestStorageKvDevice::on_deinit()
{
    interfaces_.clear();
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestStorageKvDevice, std::string(TestStorageKvDevice::NAME));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Test Cases //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("storage::KeyValueIface: acquire and perform key-value operations", "[hal][interface]")
{
    auto handle = hal::acquire_first_interface<hal::storage::KeyValueIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(handle));
    TEST_ASSERT_EQUAL_STRING(TestStorageKvIface::NAME, std::string(handle.instance_name()).c_str());

    auto iface = handle.get();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_TRUE(iface->init());

    hal::storage::KeyValueIface::KeyValueMap values = {
        {"enabled", true},
        {"count", int32_t(7)},
        {"name", std::string("storage")},
    };
    TEST_ASSERT_TRUE(iface->set("test", values));

    std::vector<hal::storage::KeyValueIface::EntryInfo> entries;
    TEST_ASSERT_TRUE(iface->list("test", entries));
    TEST_ASSERT_EQUAL(3, entries.size());

    hal::storage::KeyValueIface::KeyValueMap read_values;
    TEST_ASSERT_TRUE(iface->get("test", {"enabled", "count", "missing"}, read_values));
    TEST_ASSERT_EQUAL(2, read_values.size());
    TEST_ASSERT_TRUE(std::get<bool>(read_values.at("enabled")));
    TEST_ASSERT_EQUAL(7, std::get<int32_t>(read_values.at("count")));

    TEST_ASSERT_TRUE(iface->erase("test", {}));
    iface->deinit();
}

TEST_CASE("storage::KeyValueIface: acquire by instance and list handles", "[hal][interface]")
{
    auto handle = hal::acquire_interface<hal::storage::KeyValueIface>(TestStorageKvIface::NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(handle));

    auto handles = hal::acquire_interfaces<hal::storage::KeyValueIface>();
    TEST_ASSERT_GREATER_OR_EQUAL(1, handles.size());
    TEST_ASSERT_TRUE(hal::has_interface<hal::storage::KeyValueIface>());
}
