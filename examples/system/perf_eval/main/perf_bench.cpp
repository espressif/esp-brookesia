/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "perf_bench.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "esp_log.h"

namespace brookesia::perf_eval::bench {

static const char *TAG = "perf_bench";

// Per-sample logging writes to the same UART the records travel on, so it is a
// first-class cost on the measured path, not an incidental debug aid. Steady
// state samples once per second and can afford it; high-rate scenarios such as
// scrolling cannot. Define to 0 when driving such a scenario.
#ifndef PERF_BENCH_LOG_SAMPLES
#define PERF_BENCH_LOG_SAMPLES 1
#endif

namespace {

// Large enough that append_double never truncates: sign, integer part, '.', and
// six decimals, plus slack.
constexpr int kNumberBuffer = 384;

} // namespace

void Emitter::flush() noexcept
{
    if (used_ > 0) {
        fwrite(buffer_, 1, used_, stdout);
        used_ = 0;
    }
}

void Emitter::reserve(std::size_t bytes) noexcept
{
    if (used_ + bytes > kBufferSize) {
        flush();
    }
}

void Emitter::append(const char *text) noexcept
{
    const std::size_t len = std::strlen(text);
    if (len >= kBufferSize) {
        // A caller-supplied string longer than the buffer must not be truncated
        // or overflow: write it straight out instead.
        flush();
        fwrite(text, 1, len, stdout);
        return;
    }
    reserve(len);
    std::memcpy(buffer_ + used_, text, len);
    used_ += len;
}

void Emitter::append(char c) noexcept
{
    reserve(1);
    buffer_[used_++] = c;
}

void Emitter::append_int(std::int32_t value) noexcept
{
    char tmp[16];
    // PRId32, not %d: int32_t is long int on riscv32 and %d would
    // misread the argument. -Werror=format makes that a build failure here.
    const int n = snprintf(tmp, sizeof(tmp), "%" PRId32, value);
    if (n > 0) {
        append(tmp);
    }
}

void Emitter::append_double(double value) noexcept
{
    char tmp[kNumberBuffer];
    // %f, not %g: matches the six decimals the previous std::to_string
    // implementation produced, so earlier captures stay comparable.
    const int n = snprintf(tmp, sizeof(tmp), "%f", value);
    if (n > 0) {
        append(tmp);
    }
}

void Emitter::append_escaped(const char *text) noexcept
{
    for (const char *p = text; *p != '\0'; ++p) {
        switch (*p) {
        case '"':
            append("\\\"");
            break;
        case '\\':
            append("\\\\");
            break;
        case '\n':
            append("\\n");
            break;
        default:
            append(*p);
            break;
        }
    }
}

void Emitter::metadata(const char *metadata_json)
{
    printf("[BENCH] {\"record\":\"metadata\",\"test_suite\":\"ESP-Brookesia-Perf\","
           "\"app_id\":\"com.brookesia.perf_eval\",\"runtime_type\":\"JSON_UI\","
           "\"metadata\":%s}\n",
           metadata_json);
    fflush(stdout);
}

void Emitter::sample(const char *scenario, std::int32_t sample_index, const MetricSet &metrics,
                     std::uint32_t factor) noexcept
{
    if (factor <= 1) {
        emit_record(scenario, sample_index, metrics);
        return;
    }
    if (avg_scenario_ != scenario) {
        avg_scenario_ = scenario;
        avg_count_ = 0;
        avg_emitted_ = 0;
    }
    if (avg_count_ == 0) {
        avg_metrics_ = metrics;
    } else if (!avg_metrics_.accumulate(metrics)) {
        avg_metrics_.scale(1.0 / static_cast<double>(avg_count_));
        emit_record(avg_scenario_, avg_emitted_++, avg_metrics_);
        avg_metrics_ = metrics;
        avg_count_ = 0;
    }
    ++avg_count_;
    if (avg_count_ == factor) {
        avg_metrics_.scale(1.0 / static_cast<double>(factor));
        emit_record(avg_scenario_, avg_emitted_++, avg_metrics_);
        avg_count_ = 0;
    }
}

void Emitter::emit_record(const char *scenario, std::int32_t sample_index, const MetricSet &metrics) noexcept
{
    append("[BENCH] {\"record\":\"sample\",\"scenario\":\"");
    append_escaped(scenario);
    append("\",\"sample_index\":");
    append_int(sample_index);
    append(",\"metrics\":{");
    for (std::size_t i = 0; i < metrics.size(); ++i) {
        if (i > 0) {
            append(',');
        }
        append('"');
        append_escaped(metrics.key_at(i));
        append("\":");
        append_double(metrics.value_at(i));
    }
    append("}}\n");
    flush();
#if PERF_BENCH_LOG_SAMPLES
    ESP_LOGI(TAG, "sample %s #%" PRId32, scenario, sample_index);
#endif
    sample_count_++;
}

} // namespace brookesia::perf_eval::bench
