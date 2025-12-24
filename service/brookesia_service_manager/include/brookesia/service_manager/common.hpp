/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::service {

struct RawBuffer {
    RawBuffer() = default;

    /**
     * @brief Construct a new Raw Buffer object
     *
     * @param data_ptr Data pointer
     * @param data_size Data size in bytes
     */
    explicit RawBuffer(const void *data_ptr, size_t data_size):
        data_ptr(reinterpret_cast<const uint8_t *>(data_ptr)),
        data_size(data_size),
        is_const(true)
    {}

    /**
     * @brief Construct a new Raw Buffer object
     *
     * @param data_ptr Data pointer
     * @param data_size Data size in bytes
     */
    explicit RawBuffer(void *data_ptr, size_t data_size):
        data_ptr(reinterpret_cast<uint8_t *>(data_ptr)),
        data_size(data_size),
        is_const(false)
    {}

    template <typename T>
    const T *to_const_ptr() const
    {
        return reinterpret_cast<const T *>(data_ptr);
    }

    template <typename T>
    T *to_ptr()
    {
        if (is_const) {
            return nullptr;
        }
        return const_cast<T *>(reinterpret_cast<const T *>(data_ptr));
    }

    const uint8_t *data_ptr = nullptr; ///< Data pointer.
    size_t data_size = 0; ///< Data size in bytes. `0` means the data is stored in the `data_ptr`, and the receiver
    ///< should call `get_data_from_ptr<T>()` to get the data.
    bool is_const = true; ///< Whether the data is const. If true, receiver must not modify the data
};
BROOKESIA_DESCRIBE_STRUCT(RawBuffer, (), (data_ptr, data_size, is_const))

} // namespace esp_brookesia::service
