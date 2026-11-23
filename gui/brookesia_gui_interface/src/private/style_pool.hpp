/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <memory>
#include <mutex>

#include "private/sparse_document.hpp"

namespace esp_brookesia::gui {

/**
 * @brief Fixed-capacity residency pool for interned SparseResolvedStyle objects.
 *
 * A linked document interns one immutable SparseResolvedStyle per unique style.
 * Left on the heap those are dozens of small allocations that fragment the heap.
 * The pool reserves one contiguous slab and hands slots out through the shared_ptr
 * deleter, so reference-count semantics are unchanged: a style that outlives its
 * tree still returns its slot, and an exhausted pool falls back to the heap.
 */
class SparseResolvedStylePool {
public:
    explicit SparseResolvedStylePool(size_t slot_count);
    ~SparseResolvedStylePool() = default;

    SparseResolvedStylePool(const SparseResolvedStylePool &) = delete;
    SparseResolvedStylePool &operator=(const SparseResolvedStylePool &) = delete;

    SparseResolvedStyle *create(SparseResolvedStyle &&style);
    void destroy(SparseResolvedStyle *style);

    size_t capacity() const
    {
        return capacity_;
    }

    size_t in_use() const;

private:
    struct Slot {
        alignas(SparseResolvedStyle) unsigned char storage[sizeof(SparseResolvedStyle)];
        bool used = false;
    };

    std::unique_ptr<Slot[]> slots_;
    size_t capacity_ = 0;
    mutable std::mutex mutex_;
};

} // namespace esp_brookesia::gui
