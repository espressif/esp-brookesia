/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>

#include "esp_heap_caps.h"

#include "brookesia/gui_gsp/bundle.hpp"

using esp_brookesia::gui::gsp::Bundle;

struct esp_gsp_deployable_bundle {
    const std::byte *data;
    size_t size;
};

namespace {

bool fail_psram = false;
bool fail_internal = false;
bool metadata_open = false;
int psram_allocations = 0;
int internal_allocations = 0;
int frees = 0;
int closes = 0;

void reset()
{
    fail_psram = false;
    fail_internal = false;
    metadata_open = false;
    psram_allocations = 0;
    internal_allocations = 0;
    frees = 0;
    closes = 0;
}

std::span<const std::byte> as_bytes(const std::array<char, 4> &input)
{
    return std::as_bytes(std::span(input));
}

void test_success_and_ownership()
{
    reset();
    std::array<char, 4> input{'V', 'a', 'l', 'd'};
    auto result = Bundle::load(as_bytes(input));
    assert(result);
    assert(psram_allocations == 1 && internal_allocations == 0);
    assert(metadata_open);
    assert(result.value()->config().bundle != input.data());
    assert(result.value()->config().bundle_size == input.size());
    assert(result.value()->info().scene_count == 2);
    input[0] = 'X';
    assert(static_cast<const char *>(result.value()->config().bundle)[0] == 'V');
    assert(result.value()->action_key(1, 2).value() == 0x10002);
    auto missing = result.value()->action_key(1, UINT16_MAX);
    assert(!missing && missing.error().find("scene 1, action 65535") != std::string::npos);
    result.value().reset();
    assert(!metadata_open && closes == 1 && frees == 1);
}

void test_failure_cleanup()
{
    reset();
    assert(!Bundle::load({}));
    assert(psram_allocations == 0 && frees == 0);

    for (char marker : {
                'O', 'N', 'I', 'C'
            }) {
        reset();
        std::array<char, 4> input{marker, 'a', 'l', 'd'};
        auto result = Bundle::load(as_bytes(input));
        assert(!result);
        assert(frees == 1);
        assert(closes == ((marker == 'O' || marker == 'N') ? 0 : 1));
    }

    reset();
    std::array<char, 2> truncated{'V', 'a'};
    auto invalid = Bundle::load(as_bytes(truncated));
    assert(!invalid && invalid.error().find("Invalid GSP deployable") != std::string::npos);
    assert(frees == 1);
}

void test_memory_fallback()
{
    reset();
    fail_psram = true;
    std::array<char, 4> input{'V', 'a', 'l', 'd'};
    auto result = Bundle::load(as_bytes(input));
    assert(result && psram_allocations == 1 && internal_allocations == 1);
    result.value().reset();
    assert(frees == 1 && closes == 1);

    reset();
    fail_psram = true;
    fail_internal = true;
    auto no_memory = Bundle::load(as_bytes(input));
    assert(!no_memory && no_memory.error().find("allocate") != std::string::npos);
    assert(psram_allocations == 1 && internal_allocations == 1 && frees == 0);
}

} // namespace

extern "C" void *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps)
{
    assert(alignment == 64 && size > 0 && (caps & MALLOC_CAP_8BIT));
    if (caps & MALLOC_CAP_SPIRAM) {
        ++psram_allocations;
        if (fail_psram) {
            return nullptr;
        }
    } else {
        assert(caps & MALLOC_CAP_INTERNAL);
        ++internal_allocations;
        if (fail_internal) {
            return nullptr;
        }
    }
    void *ptr = nullptr;
    assert(posix_memalign(&ptr, alignment, size) == 0);
    return ptr;
}

extern "C" void heap_caps_free(void *ptr)
{
    assert(!metadata_open);
    ++frees;
    std::free(ptr);
}

extern "C" esp_gsp_err_t esp_gsp_deployable_bundle_open(
    const void *data, size_t size, bool verify_crc, esp_gsp_deployable_bundle_t **out_bundle
)
{
    assert(verify_crc && (reinterpret_cast<uintptr_t>(data) % 64 == 0));
    assert(out_bundle != nullptr);
    if (size < 4) {
        return ESP_GSP_ERR_INVALID_SIZE;
    }
    const char marker = static_cast<const char *>(data)[0];
    if (marker == 'O') {
        return ESP_GSP_FAIL;
    }
    if (marker == 'N') {
        return ESP_GSP_OK;
    }
    *out_bundle = new esp_gsp_deployable_bundle{static_cast<const std::byte *>(data), size};
    metadata_open = true;
    return ESP_GSP_OK;
}

extern "C" esp_gsp_err_t esp_gsp_deployable_bundle_get_info(
    const esp_gsp_deployable_bundle_t *bundle, esp_gsp_deployable_info_t *out_info
)
{
    if (bundle->data[0] == std::byte{'I'}) {
        return ESP_GSP_FAIL;
    }
    out_info->scene_count = 2;
    return ESP_GSP_OK;
}

extern "C" esp_gsp_err_t esp_gsp_deployable_bundle_make_config(
    const esp_gsp_deployable_bundle_t *bundle, esp_gsp_config_t *out_config
)
{
    if (bundle->data[0] == std::byte{'C'}) {
        return ESP_GSP_FAIL;
    }
    *out_config = ESP_GSP_CONFIG_INIT();
    out_config->bundle = bundle->data;
    out_config->bundle_size = bundle->size;
    return ESP_GSP_OK;
}

extern "C" esp_gsp_err_t esp_gsp_deployable_bundle_action_key(
    const esp_gsp_deployable_bundle_t *, uint16_t scene_id, uint16_t action_id, uint32_t *out_key
)
{
    if (action_id == UINT16_MAX) {
        return ESP_GSP_ERR_NOT_FOUND;
    }
    *out_key = (static_cast<uint32_t>(scene_id) << 16) | action_id;
    return ESP_GSP_OK;
}

extern "C" void esp_gsp_deployable_bundle_close(esp_gsp_deployable_bundle_t *bundle)
{
    assert(metadata_open && bundle != nullptr);
    metadata_open = false;
    ++closes;
    delete bundle;
}

int main()
{
    test_success_and_ownership();
    test_failure_cleanup();
    test_memory_fallback();
    std::cout << "Bundle host ownership tests passed\n";
}
