/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#if CONFIG_BT_CONTROLLER_DISABLED
#   include "esp_hosted.h"
#   include "esp_hosted_misc.h"
#endif
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_BLE_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_adaptor/bluetooth/ble/device.hpp"
#include "brookesia/hal_interface/interfaces/bluetooth/ble/peripheral.hpp"
#include "backend.hpp"

namespace esp_brookesia::hal {

namespace {

class BlePeripheralAdaptorIface: public bluetooth::ble::PeripheralIface {
public:
    explicit BlePeripheralAdaptorIface(std::shared_ptr<BleEspBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(const bluetooth::ble::PeripheralConfig &config, Callbacks callbacks) override
    {
        return backend_->configure(config, std::move(callbacks));
    }

    bool clear_callbacks() override
    {
        return backend_->clear_callbacks();
    }

    bool init() override
    {
        return backend_->init();
    }

    bool deinit() override
    {
        return backend_->deinit();
    }

    bool start() override
    {
        return backend_->start();
    }

    bool stop() override
    {
        return backend_->stop();
    }

    bool start_advertising() override
    {
        return backend_->start_advertising();
    }

    bool stop_advertising() override
    {
        return backend_->stop_advertising();
    }

    std::vector<bluetooth::ble::ConnectionInfo> get_connections() const override
    {
        return backend_->get_connections();
    }

    bool is_subscribed(uint16_t connection_id, const bluetooth::ble::CharacteristicId &characteristic) const override
    {
        return backend_->is_subscribed(connection_id, characteristic);
    }

    bool notify(
        uint16_t connection_id, const bluetooth::ble::CharacteristicId &characteristic, const bluetooth::ble::ByteArray &data
    ) override
    {
        return backend_->notify(connection_id, characteristic, data);
    }

    bool disconnect(uint16_t connection_id) override
    {
        return backend_->disconnect(connection_id);
    }

private:
    std::shared_ptr<BleEspBackend> backend_;
};

} // namespace

bool BleDevice::probe()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if CONFIG_SOC_BLE_SUPPORTED && CONFIG_BT_CONTROLLER_ENABLED
    return true;
#elif CONFIG_BT_CONTROLLER_DISABLED && CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE && CONFIG_ESP_HOSTED_NIMBLE_HCI_VHCI
    // ESP-Hosted 2.x exposes no passive BLE capability query. Probe once with the
    // least invasive controller RPC pair and cache the result: repeated HAL discovery
    // must not deinitialize a controller already owned by an acquired BLE interface.
    static const bool hosted_ble_available = []() {
        const auto connect_result = esp_hosted_connect_to_slave();
        if (connect_result != ESP_OK) {
            BROOKESIA_LOGW(
                "ESP-Hosted transport or co-processor is unavailable during BLE probe: %1%", connect_result
            );
            return false;
        }

        const auto init_result = esp_hosted_bt_controller_init();
        if (init_result != ESP_OK) {
            BROOKESIA_LOGW("ESP-Hosted co-processor does not report BLE controller capability: %1%", init_result);
            return false;
        }

        // mem_release=false keeps the controller reusable. This RPC does not tear down
        // the shared SDIO transport, so an already active Remote Wi-Fi path is preserved.
        const auto deinit_result = esp_hosted_bt_controller_deinit(false);
        if (deinit_result != ESP_OK) {
            BROOKESIA_LOGW("Failed to roll back ESP-Hosted BLE capability probe: %1%", deinit_result);
            return false;
        }
        return true;
    }();
    return hosted_ble_available;
#else
    return false;
#endif
}

std::vector<InterfaceSpec> BleDevice::get_interface_specs() const
{
    return {{bluetooth::ble::PeripheralIface::NAME, PERIPHERAL_IFACE_NAME}};
}

bool BleDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        backend_ = std::make_shared<BleEspBackend>(), false, "Failed to create BLE ESP backend"
    );
    interfaces_.emplace(PERIPHERAL_IFACE_NAME, std::make_shared<BlePeripheralAdaptorIface>(backend_));
    return true;
}

void BleDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(PERIPHERAL_IFACE_NAME);
    backend_.reset();
}

#if BROOKESIA_HAL_ADAPTOR_ENABLE_BLE_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, BleDevice, BleDevice::DEVICE_NAME, BleDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_BLE_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
