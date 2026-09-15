/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"

#if defined(CONFIG_BROOKESIA_SERVICE_USB_TRANSPORT_UART)

#include "transport.hpp"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

#include "brookesia/service_usb/macro_configs.h"

namespace esp_brookesia::service::usb_internal {

class UartTransport final : public Transport {
public:
    UartTransport()
        : port_(static_cast<uart_port_t>(BROOKESIA_SERVICE_USB_UART_PORT))
        , rx_buffer_size_(BROOKESIA_SERVICE_USB_UART_RX_BUFFER_SIZE)
        , tx_buffer_size_(BROOKESIA_SERVICE_USB_UART_TX_BUFFER_SIZE)
    {
    }

    const char *name() const override
    {
        return "uart";
    }

    esp_err_t install() override
    {
        if (static_cast<int>(port_) < 0 || port_ >= UART_NUM_MAX) {
            return ESP_ERR_INVALID_ARG;
        }

        if (uart_is_driver_installed(port_)) {
            owns_driver_ = false;
            return ESP_OK;
        }

        esp_err_t result = uart_driver_install(port_, rx_buffer_size_, tx_buffer_size_, 0, nullptr, 0);
        if (result != ESP_OK) {
            return result;
        }
        owns_driver_ = true;

        uart_config_t config = {};
        config.baud_rate = is_console_port() ? BROOKESIA_SERVICE_USB_CONSOLE_UART_BAUDRATE :
                           BROOKESIA_SERVICE_USB_UART_BAUDRATE;
        config.data_bits = UART_DATA_8_BITS;
        config.parity = UART_PARITY_DISABLE;
        config.stop_bits = UART_STOP_BITS_1;
        config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        config.rx_flow_ctrl_thresh = 122;
        config.source_clk = UART_SCLK_DEFAULT;

        result = uart_param_config(port_, &config);
        if (result != ESP_OK) {
            (void)uart_driver_delete(port_);
            owns_driver_ = false;
            return result;
        }
        return ESP_OK;
    }

    void uninstall() override
    {
        if (owns_driver_) {
            (void)uart_driver_delete(port_);
            owns_driver_ = false;
        }
    }

    int read(uint8_t *buffer, size_t size) override
    {
        return uart_read_bytes(port_, buffer, size, 0);
    }

    int write(const uint8_t *buffer, size_t size, uint32_t) override
    {
        return uart_write_bytes(port_, buffer, size);
    }

    esp_err_t wait_tx_done(uint32_t timeout_ms) override
    {
        return uart_wait_tx_done(port_, pdMS_TO_TICKS(timeout_ms));
    }

    bool is_connected() const override
    {
        return true;
    }

    bool owns_driver() const override
    {
        return owns_driver_;
    }

private:
    bool is_console_port() const
    {
        return port_ == static_cast<uart_port_t>(BROOKESIA_SERVICE_USB_CONSOLE_UART_NUM);
    }

    uart_port_t port_;
    size_t rx_buffer_size_;
    size_t tx_buffer_size_;
    bool owns_driver_ = false;
};

std::unique_ptr<Transport> make_transport()
{
    return std::make_unique<UartTransport>();
}

} // namespace esp_brookesia::service::usb_internal

#endif
