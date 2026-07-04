/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_manager/common.hpp"

#include <utility>

namespace esp_brookesia::service {
namespace {

thread_local CallContext current_call_context;

} // namespace

CallContext get_current_call_context()
{
    return current_call_context;
}

std::optional<std::string> get_current_call_context_value(std::string_view key)
{
    auto it = current_call_context.find(std::string(key));
    if (it == current_call_context.end()) {
        return std::nullopt;
    }
    return it->second;
}

ScopedCallContext::ScopedCallContext(CallContext context)
    : previous_context_(std::move(current_call_context))
    , active_(true)
{
    current_call_context = std::move(context);
}

ScopedCallContext::ScopedCallContext(ScopedCallContext &&other) noexcept
    : previous_context_(std::move(other.previous_context_))
    , active_(other.active_)
{
    other.active_ = false;
}

ScopedCallContext &ScopedCallContext::operator=(ScopedCallContext &&other) noexcept
{
    if (this == &other) {
        return *this;
    }
    if (active_) {
        current_call_context = std::move(previous_context_);
    }
    previous_context_ = std::move(other.previous_context_);
    active_ = other.active_;
    other.active_ = false;
    return *this;
}

ScopedCallContext::~ScopedCallContext()
{
    if (!active_) {
        return;
    }
    current_call_context = std::move(previous_context_);
}

} // namespace esp_brookesia::service
