/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "brookesia/lib_utils/memory_profiler.hpp"
#include "brookesia/system_core/macro_configs.h"
#include "private/utils.hpp"

namespace esp_brookesia::system::core::heap_trace {

// Full HeapTrace (includes high-frequency log_detail on every RPC/action).
#if defined(ESP_PLATFORM) && BROOKESIA_SYSTEM_HEAP_TRACE_APP_START
inline constexpr bool enabled = true;
inline constexpr bool detail_enabled = true;
// Sparse MemSnap: only heap_trace::log() at install/start/stop stages (PROFILE_LOG).
// log_detail stays off so SubscribeAction / SetBindings do not flood the serial.
#elif defined(ESP_PLATFORM) && BROOKESIA_SYSTEM_CORE_ENABLE_PROFILE_LOG
inline constexpr bool enabled = true;
inline constexpr bool detail_enabled = false;
#else
inline constexpr bool enabled = false;
inline constexpr bool detail_enabled = false;
#endif

struct Snapshot {
    size_t internal_free = 0;
    size_t internal_largest = 0;
    size_t psram_free = 0;
    size_t psram_largest = 0;
    size_t cap8_free = 0;
    size_t cap8_largest = 0;
    bool valid = false;
};

#if defined(ESP_PLATFORM) && (BROOKESIA_SYSTEM_HEAP_TRACE_APP_START || BROOKESIA_SYSTEM_CORE_ENABLE_PROFILE_LOG)
inline Snapshot capture()
{
    auto raw_snapshot = esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
    Snapshot snapshot;
    snapshot.internal_free = raw_snapshot.internal_free;
    snapshot.internal_largest = raw_snapshot.internal_largest;
    snapshot.psram_free = raw_snapshot.external_free;
    snapshot.psram_largest = raw_snapshot.external_largest;
    snapshot.cap8_free = raw_snapshot.cap8_free;
    snapshot.cap8_largest = raw_snapshot.cap8_largest;
    snapshot.valid = raw_snapshot.valid;
    return snapshot;
}

inline int64_t delta(size_t current, size_t baseline)
{
    return esp_brookesia::lib_utils::MemoryProfiler::heap_delta(current, baseline);
}

inline const char *log_tag()
{
#if BROOKESIA_SYSTEM_HEAP_TRACE_APP_START
    return "HeapTrace";
#else
    return "MemSnap";
#endif
}

inline void log(
    std::string_view scope,
    std::string_view stage,
    std::string_view app_id,
    const Snapshot &snapshot,
    const Snapshot *baseline = nullptr
)
{
    if (baseline != nullptr && baseline->valid) {
        BROOKESIA_LOGI(
            "[%1%][%2%] app(%3%) stage(%4%) internal_free(%5%, delta=%6%) "
            "internal_largest(%7%, delta=%8%) psram_free(%9%, delta=%10%) "
            "psram_largest(%11%, delta=%12%) cap8_free(%13%, delta=%14%) cap8_largest(%15%, delta=%16%)",
            log_tag(), scope, app_id, stage,
            snapshot.internal_free, delta(snapshot.internal_free, baseline->internal_free),
            snapshot.internal_largest, delta(snapshot.internal_largest, baseline->internal_largest),
            snapshot.psram_free, delta(snapshot.psram_free, baseline->psram_free),
            snapshot.psram_largest, delta(snapshot.psram_largest, baseline->psram_largest),
            snapshot.cap8_free, delta(snapshot.cap8_free, baseline->cap8_free),
            snapshot.cap8_largest, delta(snapshot.cap8_largest, baseline->cap8_largest)
        );
        return;
    }

    BROOKESIA_LOGI(
        "[%1%][%2%] app(%3%) stage(%4%) internal_free(%5%) internal_largest(%6%) "
        "psram_free(%7%) psram_largest(%8%) cap8_free(%9%) cap8_largest(%10%)",
        log_tag(), scope, app_id, stage,
        snapshot.internal_free, snapshot.internal_largest,
        snapshot.psram_free, snapshot.psram_largest,
        snapshot.cap8_free, snapshot.cap8_largest
    );
}

inline void log_detail(
    std::string_view scope,
    std::string_view stage,
    std::string_view app_id,
    std::string_view details,
    const Snapshot &snapshot,
    const Snapshot *baseline = nullptr
)
{
    if (!detail_enabled) {
        return;
    }
    if (baseline != nullptr && baseline->valid) {
        BROOKESIA_LOGI(
            "[HeapTrace][%1%] app(%2%) stage(%3%) detail(%4%) internal_free(%5%, delta=%6%) "
            "internal_largest(%7%, delta=%8%) psram_free(%9%, delta=%10%) "
            "psram_largest(%11%, delta=%12%) cap8_free(%13%, delta=%14%) cap8_largest(%15%, delta=%16%)",
            scope, app_id, stage, details,
            snapshot.internal_free, delta(snapshot.internal_free, baseline->internal_free),
            snapshot.internal_largest, delta(snapshot.internal_largest, baseline->internal_largest),
            snapshot.psram_free, delta(snapshot.psram_free, baseline->psram_free),
            snapshot.psram_largest, delta(snapshot.psram_largest, baseline->psram_largest),
            snapshot.cap8_free, delta(snapshot.cap8_free, baseline->cap8_free),
            snapshot.cap8_largest, delta(snapshot.cap8_largest, baseline->cap8_largest)
        );
        return;
    }

    BROOKESIA_LOGI(
        "[HeapTrace][%1%] app(%2%) stage(%3%) detail(%4%) internal_free(%5%) internal_largest(%6%) "
        "psram_free(%7%) psram_largest(%8%) cap8_free(%9%) cap8_largest(%10%)",
        scope, app_id, stage, details,
        snapshot.internal_free, snapshot.internal_largest,
        snapshot.psram_free, snapshot.psram_largest,
        snapshot.cap8_free, snapshot.cap8_largest
    );
}
#else
inline Snapshot capture()
{
    return {};
}

inline void log(
    std::string_view,
    std::string_view,
    std::string_view,
    const Snapshot &,
    const Snapshot * = nullptr
)
{}

inline void log_detail(
    std::string_view,
    std::string_view,
    std::string_view,
    std::string_view,
    const Snapshot &,
    const Snapshot * = nullptr
)
{}
#endif

} // namespace esp_brookesia::system::core::heap_trace
