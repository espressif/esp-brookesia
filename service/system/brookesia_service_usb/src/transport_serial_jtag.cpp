/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"

#if defined(CONFIG_BROOKESIA_SERVICE_USB_TRANSPORT_SERIAL_JTAG)

#include "transport.hpp"

#include "driver/usb_serial_jtag.h"
#include "esp_private/periph_ctrl.h"
#include "freertos/FreeRTOS.h"
#include "hal/usb_serial_jtag_ll.h"

#include "brookesia/service_usb/macro_configs.h"

namespace esp_brookesia::service::usb_internal {

class SerialJtagTransport final : public Transport {
public:
    const char *name() const override
    {
        return "serial_jtag";
    }

    esp_err_t install() override
    {
        if (usb_serial_jtag_is_driver_installed()) {
            owns_driver_ = false;
            return ESP_OK;
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
        usb_serial_jtag_driver_config_t config = {
            .tx_buffer_size = BROOKESIA_SERVICE_USB_SERIAL_JTAG_TX_BUFFER_SIZE,
            .rx_buffer_size = BROOKESIA_SERVICE_USB_SERIAL_JTAG_RX_BUFFER_SIZE,
        };
#pragma GCC diagnostic pop

        // A CPU reset can leave peripheral interrupt enables behind. The driver handles only RX/TX;
        // mask all inherited enables before it installs its ISR, leaving raw status for SOF monitoring.
#if !SOC_RCC_IS_INDEPENDENT
        PERIPH_RCC_ATOMIC() {
            usb_serial_jtag_ll_enable_bus_clock(true);
        }
#else
        usb_serial_jtag_ll_enable_bus_clock(true);
#endif
        usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);

        const esp_err_t result = usb_serial_jtag_driver_install(&config);
        if (result != ESP_OK) {
            return result;
        }
        owns_driver_ = true;
        return ESP_OK;
    }

    void uninstall() override
    {
        if (owns_driver_) {
            (void)usb_serial_jtag_driver_uninstall();
            owns_driver_ = false;
        }
    }

    int read(uint8_t *buffer, size_t size) override
    {
        return usb_serial_jtag_read_bytes(buffer, size, 0);
    }

    int write(const uint8_t *buffer, size_t size, uint32_t timeout_ms) override
    {
        return usb_serial_jtag_write_bytes(buffer, size, pdMS_TO_TICKS(timeout_ms));
    }

    esp_err_t wait_tx_done(uint32_t timeout_ms) override
    {
        return usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(timeout_ms));
    }

    bool is_connected() const override
    {
        return usb_serial_jtag_is_connected();
    }

    bool owns_driver() const override
    {
        return owns_driver_;
    }

private:
    bool owns_driver_ = false;
};

std::unique_ptr<Transport> make_transport()
{
    return std::make_unique<SerialJtagTransport>();
}

} // namespace esp_brookesia::service::usb_internal

#endif
