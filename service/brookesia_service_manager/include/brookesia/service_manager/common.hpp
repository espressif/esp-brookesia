/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <type_traits>
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::service {

/**
 * @brief Lightweight wrapper for raw binary data passed through service APIs.
 */
struct RawBuffer {
    RawBuffer() = default;

    /**
     * @brief Construct a new Raw Buffer object from a pointer
     *
     * @tparam T Pointer type (must be a pointer type)
     * @param[in] pointer Pointer to the raw data.
     * @param[in] size Size of the buffer in bytes. `0` means the value is stored directly in the pointer payload.
     *
     * The `is_const` flag is automatically set based on whether the pointee type is const.
     * `data_size` is set to 0, indicating that the data is stored in the pointer itself.
     * For example:
     * - `const int*` -> `is_const = true`
     * - `int*` -> `is_const = false`
     */
    template <typename T>
    requires std::is_pointer_v<T>
    explicit RawBuffer(T pointer, size_t size = 0):
        data_ptr(reinterpret_cast<const uint8_t *>(pointer)),
        data_size(size),
        is_const(std::is_const_v<std::remove_pointer_t<T>>)
    {}

    /**
     * @brief View the stored pointer as a const typed pointer.
     *
     * @tparam T Target pointee type.
     * @return Typed const pointer view of the stored buffer.
     */
    template <typename T = void>
    const T * to_const_ptr() const
    {
        return reinterpret_cast<const T *>(data_ptr);
    }

    /**
     * @brief View the stored pointer as a mutable typed pointer when allowed.
     *
     * @tparam T Target pointee type.
     * @return Mutable typed pointer view, or `nullptr` when the buffer is marked const.
     */
    template <typename T = void>
    T * to_ptr() const
    {
        if (is_const) {
            return nullptr;
        }
        return const_cast<T *>(reinterpret_cast<const T *>(data_ptr));
    }

    bool operator==(const RawBuffer &other) const
    {
        return (data_ptr == other.data_ptr) && (data_size == other.data_size) && (is_const == other.is_const);
    }

    bool operator!=(const RawBuffer &other) const
    {
        return !(*this == other);
    }

    const uint8_t *data_ptr = nullptr; ///< Data pointer.
    size_t data_size = 0; ///< Data size in bytes. `0` means the data is stored in the `data_ptr`, and the receiver
    ///< should call `get_data_from_ptr<T>()` to get the data.
    bool is_const = true; ///< Whether the data is const. If true, receiver must not modify the data
};
BROOKESIA_DESCRIBE_STRUCT(RawBuffer, (), (data_ptr, data_size, is_const))

} // namespace esp_brookesia::service
