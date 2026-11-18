/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>

#define MALLOC_CAP_SPIRAM (1U << 0)
#define MALLOC_CAP_INTERNAL (1U << 1)
#define MALLOC_CAP_8BIT (1U << 2)

extern "C" void *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps);
extern "C" void heap_caps_free(void *ptr);
