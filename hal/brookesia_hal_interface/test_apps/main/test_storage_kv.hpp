/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <map>
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/storage/key_value.hpp"

namespace esp_brookesia {

class TestStorageKvIface: public hal::storage::KeyValueIface {
public:
    static constexpr const char *NAME = "TestStorageKv:StorageKv";

    bool init() override;
    void deinit() override;
    bool list(const std::string &nspace, std::vector<EntryInfo> &entries) override;
    bool set(const std::string &nspace, const KeyValueMap &key_value_map) override;
    bool get(const std::string &nspace, const std::vector<std::string> &keys, KeyValueMap &key_value_map) override;
    bool erase(const std::string &nspace, const std::vector<std::string> &keys) override;

private:
    std::map<std::string, KeyValueMap> namespaces_;
    bool is_initialized_ = false;
};

class TestStorageKvDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestStorageKv";

    TestStorageKvDevice()
        : hal::Device(std::string(NAME))
    {
    }

    bool probe() override;
    std::vector<hal::InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia
