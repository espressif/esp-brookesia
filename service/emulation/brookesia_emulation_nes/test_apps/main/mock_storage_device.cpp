/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/storage/key_value.hpp"
#include "brookesia/lib_utils/plugin.hpp"

namespace esp_brookesia::test_apps::emulation_nes {

class MockStorageDevice: public hal::Device {
public:
    static constexpr const char *DEVICE_NAME = "MockStorage";
    static constexpr const char *KEY_VALUE_IFACE_NAME = "MockStorage:KeyValue";

    static MockStorageDevice &get_instance()
    {
        static MockStorageDevice instance;
        return instance;
    }

private:
    class MockKeyValue: public hal::storage::KeyValueIface {
    public:
        bool init() override
        {
            std::lock_guard lock(mutex_);
            namespaces_.clear();
            initialized_ = true;
            last_error_.clear();
            return true;
        }

        void deinit() override
        {
            std::lock_guard lock(mutex_);
            namespaces_.clear();
            initialized_ = false;
            last_error_.clear();
        }

        bool list(const std::string &nspace, std::vector<EntryInfo> &entries) override
        {
            std::lock_guard lock(mutex_);
            if (!check_initialized_locked()) {
                return false;
            }
            entries.clear();
            const auto namespace_it = namespaces_.find(nspace);
            if (namespace_it == namespaces_.end()) {
                return true;
            }
            for (const auto &[key, value] : namespace_it->second) {
                entries.push_back(EntryInfo{
                    .nspace = nspace,
                    .key = key,
                    .type = get_value_type(value),
                });
            }
            last_error_.clear();
            return true;
        }

        bool set(const std::string &nspace, const KeyValueMap &key_value_map) override
        {
            std::lock_guard lock(mutex_);
            if (!check_initialized_locked()) {
                return false;
            }
            auto &values = namespaces_[nspace];
            for (const auto &[key, value] : key_value_map) {
                values[key] = value;
            }
            last_error_.clear();
            return true;
        }

        bool get(const std::string &nspace, const std::vector<std::string> &keys, KeyValueMap &key_value_map) override
        {
            std::lock_guard lock(mutex_);
            if (!check_initialized_locked()) {
                return false;
            }
            key_value_map.clear();
            const auto namespace_it = namespaces_.find(nspace);
            if (namespace_it == namespaces_.end()) {
                last_error_.clear();
                return true;
            }
            if (keys.empty()) {
                key_value_map = namespace_it->second;
                last_error_.clear();
                return true;
            }
            for (const auto &key : keys) {
                const auto value_it = namespace_it->second.find(key);
                if (value_it != namespace_it->second.end()) {
                    key_value_map.emplace(key, value_it->second);
                }
            }
            last_error_.clear();
            return true;
        }

        bool erase(const std::string &nspace, const std::vector<std::string> &keys) override
        {
            std::lock_guard lock(mutex_);
            if (!check_initialized_locked()) {
                return false;
            }
            if (keys.empty()) {
                namespaces_.erase(nspace);
                last_error_.clear();
                return true;
            }
            auto namespace_it = namespaces_.find(nspace);
            if (namespace_it != namespaces_.end()) {
                for (const auto &key : keys) {
                    namespace_it->second.erase(key);
                }
                if (namespace_it->second.empty()) {
                    namespaces_.erase(namespace_it);
                }
            }
            last_error_.clear();
            return true;
        }

    private:
        static ValueType get_value_type(const Value &value)
        {
            if (std::holds_alternative<bool>(value)) {
                return ValueType::Bool;
            }
            if (std::holds_alternative<int32_t>(value)) {
                return ValueType::Int;
            }
            if (std::holds_alternative<std::string>(value)) {
                return ValueType::String;
            }
            return ValueType::Max;
        }

        bool check_initialized_locked()
        {
            if (initialized_) {
                return true;
            }
            last_error_ = "Mock storage key-value is not initialized";
            return false;
        }

        bool initialized_ = false;
        std::mutex mutex_;
        std::map<std::string, KeyValueMap> namespaces_;
    };

    MockStorageDevice()
        : hal::Device(std::string(DEVICE_NAME))
    {
    }

    bool probe() override
    {
        return true;
    }

    std::vector<hal::InterfaceSpec> get_interface_specs() const override
    {
        return {{hal::storage::KeyValueIface::NAME, KEY_VALUE_IFACE_NAME}};
    }

    bool on_init() override
    {
        key_value_iface_ = std::make_shared<MockKeyValue>();
        interfaces_.emplace(KEY_VALUE_IFACE_NAME, key_value_iface_);
        return true;
    }

    void on_deinit() override
    {
        interfaces_.erase(KEY_VALUE_IFACE_NAME);
        key_value_iface_.reset();
    }

    std::shared_ptr<MockKeyValue> key_value_iface_;
};

BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    hal::Device, MockStorageDevice, MockStorageDevice::DEVICE_NAME, MockStorageDevice::get_instance(),
    mock_storage_device_symbol
);

} // namespace esp_brookesia::test_apps::emulation_nes
