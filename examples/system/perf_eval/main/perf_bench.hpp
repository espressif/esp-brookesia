/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace brookesia::perf_eval::bench {

// Emits `[BENCH]` records for the host-side tools/brookesia_gui_perf harness.
//
// The host `aggregate` subcommand takes only `log`, `-o` and `--warmup`, so the
// 23 metadata fields required by `report.REQUIRED_METADATA_FIELDS` must travel
// with the stream as one `record=metadata` line. A single missing field makes
// the host refuse to emit a report.
//
// Instrumentation rule (v1.8 §3.1): timestamps and counters only. No locks, no
// allocation on the measured path, no behavioural change.

/**
 * Fixed-capacity metric set, reused across every sample.
 *
 * The measured path must not allocate: this app exists to report heap
 * residency, so growing a std::map per sample would perturb the very number it
 * reports. Keys are string literals owned by the caller, so only pointers are
 * stored, and one instance is reused for the whole run.
 */
class MetricSet {
public:
    // 24: composite_stress carries 16 metrics per sample (frame_time percentiles,
    // jank/hitch/frozen/overrun, area ratio, refresh period, four length-tail
    // buckets, fps, flush rate, sram), so 16 would silently drop the tail.
    static constexpr std::size_t kCapacity = 24;

    void reset() noexcept
    {
        count_ = 0;
    }

    /** Returns false and stores nothing once kCapacity is reached. */
    bool add(const char *key, double value) noexcept
    {
        if (count_ >= kCapacity) {
            return false;
        }
        keys_[count_] = key;
        values_[count_] = value;
        ++count_;
        return true;
    }

    std::size_t size() const noexcept
    {
        return count_;
    }
    const char *key_at(std::size_t index) const noexcept
    {
        return keys_[index];
    }
    double value_at(std::size_t index) const noexcept
    {
        return values_[index];
    }

    // Add another set index by index (keys line up within a scenario). False if shapes differ.
    bool accumulate(const MetricSet &other) noexcept
    {
        if (other.count_ != count_) {
            return false;
        }
        for (std::size_t i = 0; i < count_; ++i) {
            values_[i] += other.values_[i];
        }
        return true;
    }
    void scale(double factor) noexcept
    {
        for (std::size_t i = 0; i < count_; ++i) {
            values_[i] *= factor;
        }
    }

private:
    const char *keys_[kCapacity] {};
    double values_[kCapacity] {};
    std::size_t count_ = 0;
};

class Emitter {
public:
    // `sample_index` is per scenario and increases monotonically.
    //
    // Allocation-free: no std::string, no std::to_string, no container growth.
    // A line that would overflow the buffer is flushed first and then continued
    // across further writes, so an over-long line costs extra writes rather than
    // a truncated or corrupt record.
    // factor > 1 buffers that many consecutive raw samples for the scenario and emits
    // their element-wise mean: a longer effective window lowers the sample-to-sample cv
    // without changing the per-sample measurement. sample_index is then ignored.
    void sample(const char *scenario, std::int32_t sample_index, const MetricSet &metrics,
                std::uint32_t factor = 1) noexcept;

    // Emitted once, before any sample. Allocation here is irrelevant because it
    // is outside the measured path, so a caller-built std::string is fine; this
    // takes it as a C string purely to avoid a redundant copy.
    void metadata(const char *metadata_json);

    std::int32_t sample_count() const noexcept
    {
        return sample_count_;
    }

private:
    static constexpr std::size_t kBufferSize = 1024;

    void emit_record(const char *scenario, std::int32_t sample_index, const MetricSet &metrics) noexcept;
    void reserve(std::size_t bytes) noexcept;
    void append(const char *text) noexcept;
    void append(char c) noexcept;
    void append_int(std::int32_t value) noexcept;
    void append_double(double value) noexcept;
    void append_escaped(const char *text) noexcept;
    void flush() noexcept;

    char buffer_[kBufferSize] {};
    std::size_t used_ = 0;
    std::int32_t sample_count_ = 0;
    const char *avg_scenario_ = nullptr;
    std::uint32_t avg_factor_ = 1;
    std::uint32_t avg_count_ = 0;
    std::int32_t avg_emitted_ = 0;
    MetricSet avg_metrics_{};
};

} // namespace brookesia::perf_eval::bench
