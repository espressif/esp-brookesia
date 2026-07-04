/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace esp_brookesia::gui {

using SubscriptionId = uint64_t;

class ScopedConnection {
public:
    ScopedConnection() = default;
    explicit ScopedConnection(std::shared_ptr<std::function<void()>> disconnect_handler)
        : disconnect_handler_(std::move(disconnect_handler))
    {}
    explicit ScopedConnection(std::function<void()> disconnect_handler)
        : disconnect_handler_(std::make_shared<std::function<void()>>(std::move(disconnect_handler)))
    {}
    ScopedConnection(const ScopedConnection &) = delete;
    ScopedConnection &operator=(const ScopedConnection &) = delete;

    ScopedConnection(ScopedConnection &&other) noexcept
        : disconnect_handler_(std::move(other.disconnect_handler_))
    {}

    ScopedConnection &operator=(ScopedConnection &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        disconnect();
        disconnect_handler_ = std::move(other.disconnect_handler_);
        return *this;
    }

    ~ScopedConnection()
    {
        disconnect();
    }

    bool connected() const
    {
        return disconnect_handler_ != nullptr && static_cast<bool>(*disconnect_handler_);
    }

    void disconnect()
    {
        if (disconnect_handler_ == nullptr || !*disconnect_handler_) {
            return;
        }
        auto handler = std::move(*disconnect_handler_);
        handler();
    }

private:
    std::shared_ptr<std::function<void()>> disconnect_handler_;
};

} // namespace esp_brookesia::gui
