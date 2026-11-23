/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/style_pool.hpp"

#include <new>

namespace esp_brookesia::gui {

SparseResolvedStylePool::SparseResolvedStylePool(size_t slot_count)
    : slots_(slot_count > 0 ? std::make_unique<Slot[]>(slot_count) : nullptr)
    , capacity_(slot_count)
{
}

SparseResolvedStyle *SparseResolvedStylePool::create(SparseResolvedStyle &&style)
{
    std::lock_guard lock(mutex_);
    for (size_t index = 0; index < capacity_; ++index) {
        if (slots_[index].used) {
            continue;
        }
        slots_[index].used = true;
        return ::new (static_cast<void *>(slots_[index].storage)) SparseResolvedStyle(std::move(style));
    }
    return nullptr;
}

void SparseResolvedStylePool::destroy(SparseResolvedStyle *style)
{
    if (style == nullptr) {
        return;
    }
    auto *slot = reinterpret_cast<Slot *>(reinterpret_cast<unsigned char *>(style) - offsetof(Slot, storage));
    std::lock_guard lock(mutex_);
    style->~SparseResolvedStyle();
    slot->used = false;
}

size_t SparseResolvedStylePool::in_use() const
{
    std::lock_guard lock(mutex_);
    size_t used = 0;
    for (size_t index = 0; index < capacity_; ++index) {
        used += slots_[index].used ? 1 : 0;
    }
    return used;
}

} // namespace esp_brookesia::gui
