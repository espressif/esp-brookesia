/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <ostream>

#include "brookesia/gui_interface/macro_configs.h"

namespace esp_brookesia::gui {

class BackendHandle {
public:
    using Value = uint64_t;

    constexpr BackendHandle() = default;
    explicit constexpr BackendHandle(Value value)
        : value_(value)
    {}

    constexpr bool is_valid() const
    {
        return value_ != 0;
    }

    constexpr Value value() const
    {
        return value_;
    }

    friend constexpr bool operator==(BackendHandle lhs, BackendHandle rhs)
    {
        return lhs.value_ == rhs.value_;
    }

    friend constexpr bool operator!=(BackendHandle lhs, BackendHandle rhs)
    {
        return !(lhs == rhs);
    }

    friend std::ostream &operator<<(std::ostream &os, BackendHandle handle)
    {
        return os << handle.value_;
    }

private:
    Value value_ = 0;
};

class DocumentId {
public:
    using Value = uint64_t;

    constexpr DocumentId() = default;
    explicit constexpr DocumentId(Value value)
        : value_(value)
    {}

    constexpr bool is_valid() const
    {
        return value_ != 0;
    }

    constexpr Value value() const
    {
        return value_;
    }

    friend constexpr bool operator==(DocumentId lhs, DocumentId rhs)
    {
        return lhs.value_ == rhs.value_;
    }

    friend constexpr bool operator!=(DocumentId lhs, DocumentId rhs)
    {
        return !(lhs == rhs);
    }

    friend std::ostream &operator<<(std::ostream &os, DocumentId id)
    {
        return os << id.value_;
    }

private:
    Value value_ = 0;
};

class TreeHandle {
public:
    using Value = uint64_t;

    constexpr TreeHandle() = default;
    explicit constexpr TreeHandle(Value value)
        : value_(value)
    {}

    constexpr bool is_valid() const
    {
        return value_ != 0;
    }

    constexpr Value value() const
    {
        return value_;
    }

    friend constexpr bool operator==(TreeHandle lhs, TreeHandle rhs)
    {
        return lhs.value_ == rhs.value_;
    }

    friend constexpr bool operator!=(TreeHandle lhs, TreeHandle rhs)
    {
        return !(lhs == rhs);
    }

    friend std::ostream &operator<<(std::ostream &os, TreeHandle handle)
    {
        return os << handle.value_;
    }

private:
    Value value_ = 0;
};

class TransientMountId {
public:
    using Value = uint64_t;

    constexpr TransientMountId() = default;
    explicit constexpr TransientMountId(Value value)
        : value_(value)
    {}

    constexpr bool is_valid() const
    {
        return value_ != 0;
    }

    constexpr Value value() const
    {
        return value_;
    }

    friend constexpr bool operator==(TransientMountId lhs, TransientMountId rhs)
    {
        return lhs.value_ == rhs.value_;
    }

    friend constexpr bool operator!=(TransientMountId lhs, TransientMountId rhs)
    {
        return !(lhs == rhs);
    }

    friend std::ostream &operator<<(std::ostream &os, TransientMountId id)
    {
        return os << id.value_;
    }

private:
    Value value_ = 0;
};

} // namespace esp_brookesia::gui
