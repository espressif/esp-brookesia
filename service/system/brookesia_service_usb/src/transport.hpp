/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "esp_err.h"

namespace esp_brookesia::service::usb_internal {

class Transport {
public:
    virtual ~Transport() = default;

    virtual const char *name() const = 0;

    virtual esp_err_t install() = 0;
    virtual void uninstall() = 0;

    virtual int read(uint8_t *buffer, size_t size) = 0;
    virtual int write(const uint8_t *buffer, size_t size, uint32_t timeout_ms) = 0;
    virtual esp_err_t wait_tx_done(uint32_t timeout_ms) = 0;

    virtual bool is_connected() const = 0;
    virtual bool owns_driver() const = 0;
};

std::unique_ptr<Transport> make_transport();

} // namespace esp_brookesia::service::usb_internal
