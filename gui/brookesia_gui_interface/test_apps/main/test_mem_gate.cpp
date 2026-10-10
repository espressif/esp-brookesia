/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "brookesia/gui_interface.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"

#if !defined(ESP_PLATFORM)

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <sstream>
#include <system_error>
#include <vector>

using namespace esp_brookesia::gui;

namespace {

constexpr std::uint64_t kBlockMagic = 0xBADC0FFEE0DDF00DULL;
constexpr std::size_t kBlockHeaderSize = 16;

struct BlockHeader {
    std::uint64_t size;
    std::uint64_t magic;
};

static_assert(sizeof(BlockHeader) == kBlockHeaderSize, "BlockHeader must be 16 bytes");

std::atomic<std::size_t> g_current_bytes{0};
std::atomic<std::size_t> g_peak_bytes{0};
std::atomic<std::size_t> g_baseline_bytes{0};
std::atomic<std::size_t> g_alloc_count{0};
std::atomic<std::size_t> g_header_mismatches{0};
std::atomic<bool> g_armed{false};

void *tracked_alloc(std::size_t size)
{
    if (size > SIZE_MAX - kBlockHeaderSize) {
        return nullptr;
    }
    void *raw = std::malloc(size + kBlockHeaderSize);
    if (raw == nullptr) {
        return nullptr;
    }
    auto *header = static_cast<BlockHeader *>(raw);
    header->size = static_cast<std::uint64_t>(size);
    header->magic = kBlockMagic;
    std::size_t current = g_current_bytes.fetch_add(size, std::memory_order_relaxed) + size;
    if (g_armed.load(std::memory_order_relaxed)) {
        g_alloc_count.fetch_add(1, std::memory_order_relaxed);
        std::size_t peak = g_peak_bytes.load(std::memory_order_relaxed);
        while (current > peak && !g_peak_bytes.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {
        }
    }
    return static_cast<std::byte *>(raw) + kBlockHeaderSize;
}

void tracked_free(void *ptr) noexcept
{
    if (ptr == nullptr) {
        return;
    }
    auto *raw = static_cast<std::byte *>(ptr) - kBlockHeaderSize;
    auto *header = reinterpret_cast<BlockHeader *>(raw);
    if (header->magic != kBlockMagic) {
        g_header_mismatches.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    std::size_t size = static_cast<std::size_t>(header->size);
    std::size_t current = g_current_bytes.load(std::memory_order_relaxed);
    if (size > current) {
        g_current_bytes.store(0, std::memory_order_relaxed);
    } else {
        g_current_bytes.fetch_sub(size, std::memory_order_relaxed);
    }
    header->magic = 0;
    std::free(raw);
}

}  // namespace

// Replaceable allocation functions must live at global namespace scope with
// external linkage so that dynamic libraries (boost::json, libstdc++)
// interpose onto them.
void *operator new (std::size_t size)
{
    void *ptr = tracked_alloc(size);
    if (ptr == nullptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void *operator new[](std::size_t size)
{
    void *ptr = tracked_alloc(size);
    if (ptr == nullptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void *operator new (std::size_t size, const std::nothrow_t &) noexcept
{
    return tracked_alloc(size);
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    return tracked_alloc(size);
}

void operator delete (void *ptr) noexcept
{
    tracked_free(ptr);
}

void operator delete[](void *ptr) noexcept
{
    tracked_free(ptr);
}

void operator delete (void *ptr, std::size_t) noexcept
{
    tracked_free(ptr);
}

void operator delete[](void *ptr, std::size_t) noexcept
{
    tracked_free(ptr);
}

void operator delete (void *ptr, const std::nothrow_t &) noexcept
{
    tracked_free(ptr);
}

void operator delete[](void *ptr, const std::nothrow_t &) noexcept
{
    tracked_free(ptr);
}

namespace {

constexpr std::string_view kMiniScreenAsset = R"({
    "type": "viewScreen",
    "id": "mem_gate_style_constants_screen",
    "children": [
        {
            "type": "label",
            "id": "title",
            "labelProps": { "text": "mem gate" },
            "style": { "fontSize": "20sp", "textColor": "#111827" }
        }
    ]
})";

struct MemGateSnapshot {
    std::int64_t retained_bytes;
    std::int64_t peak_bytes;
    std::size_t alloc_count;
    std::size_t header_mismatches;
};

void mem_gate_arm()
{
    std::size_t current = g_current_bytes.load(std::memory_order_relaxed);
    g_baseline_bytes.store(current, std::memory_order_relaxed);
    g_peak_bytes.store(current, std::memory_order_relaxed);
    g_alloc_count.store(0, std::memory_order_relaxed);
    g_header_mismatches.store(0, std::memory_order_relaxed);
    g_armed.store(true, std::memory_order_release);
}

MemGateSnapshot mem_gate_disarm()
{
    g_armed.store(false, std::memory_order_release);
    MemGateSnapshot snapshot{};
    std::int64_t current = static_cast<std::int64_t>(g_current_bytes.load(std::memory_order_relaxed));
    std::int64_t baseline = static_cast<std::int64_t>(g_baseline_bytes.load(std::memory_order_relaxed));
    snapshot.retained_bytes = current - baseline;
    snapshot.peak_bytes = static_cast<std::int64_t>(g_peak_bytes.load(std::memory_order_relaxed)) - baseline;
    snapshot.alloc_count = g_alloc_count.load(std::memory_order_relaxed);
    snapshot.header_mismatches = g_header_mismatches.load(std::memory_order_relaxed);
    return snapshot;
}

std::filesystem::path find_shell_resource_dir(std::string &error)
{
    error.clear();
    const std::filesystem::path source_path(__FILE__);
    if (source_path.has_parent_path()) {
        std::filesystem::path candidate = source_path;
        for (int i = 0; i < 5; ++i) {
            candidate = candidate.parent_path();
            if (candidate.empty()) {
                break;
            }
        }
        if (!candidate.empty()) {
            const std::filesystem::path shell =
                candidate / "system" / "brookesia_system_super" / "resource" / "shell";
            std::error_code dir_error;
            if (std::filesystem::is_directory(shell, dir_error)) {
                return shell;
            }
        }
    }
    std::error_code path_error;
    std::filesystem::path walk = std::filesystem::current_path(path_error);
    if (path_error) {
        error = "cannot resolve current_path: " + path_error.message();
        return {};
    }
    for (int i = 0; i < 12 && !walk.empty(); ++i) {
        const std::filesystem::path shell =
            walk / "system" / "brookesia_system_super" / "resource" / "shell";
        std::error_code dir_error;
        if (std::filesystem::is_directory(shell, dir_error)) {
            return shell;
        }
        const std::filesystem::path parent = walk.parent_path();
        if (parent == walk) {
            break;
        }
        walk = parent;
    }
    error = "shell resource directory not found";
    return {};
}

bool read_asset_file(const std::filesystem::path &path, std::string &out, std::string &error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "cannot open " + path.string();
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    out = buffer.str();
    if (input.bad()) {
        error = "read failed " + path.string();
        return false;
    }
    return true;
}

std::string compose_fixture_json(const std::filesystem::path &shell_dir,
                                 const std::vector<std::string> &asset_files,
                                 std::string_view extra_asset, std::string &error)
{
    std::string json = R"({"version":"0.1.1","assets":[)";
    bool first = true;
    for (const std::string &relative_path : asset_files) {
        std::string content;
        if (!read_asset_file(shell_dir / relative_path, content, error)) {
            return {};
        }
        if (!first) {
            json += ',';
        }
        first = false;
        json += content;
    }
    if (!extra_asset.empty()) {
        if (!first) {
            json += ',';
        }
        json += extra_asset;
    }
    json += "]}";
    return json;
}

struct MemGateLimits {
    double retained_ratio;
    double peak_ratio;
    std::size_t max_alloc_count;
};

void run_mem_gate(const char *fixture_name, const std::vector<std::string> &asset_files,
                  std::string_view extra_asset, const MemGateLimits &limits)
{
    std::string error;
    const std::filesystem::path shell_dir = find_shell_resource_dir(error);
    TEST_ASSERT_TRUE_MESSAGE(error.empty(), error.c_str());
    const std::string composed = compose_fixture_json(shell_dir, asset_files, extra_asset, error);
    TEST_ASSERT_TRUE_MESSAGE(error.empty(), error.c_str());

    const std::size_t raw_json_bytes = composed.size();
    Environment environment;
    mem_gate_arm();
    auto document = parse_document(composed, "test", environment);
    const MemGateSnapshot snapshot = mem_gate_disarm();

    const double retained_ratio =
        raw_json_bytes > 0 ? static_cast<double>(snapshot.retained_bytes) / static_cast<double>(raw_json_bytes) : 0.0;
    const double peak_ratio =
        raw_json_bytes > 0 ? static_cast<double>(snapshot.peak_bytes) / static_cast<double>(raw_json_bytes) : 0.0;

    std::printf("[mem_gate] fixture=%s raw=%zu retained=%lld peak=%lld allocs=%zu R=%.3f K=%.3f mismatches=%zu\n",
                fixture_name, raw_json_bytes, static_cast<long long>(snapshot.retained_bytes),
                static_cast<long long>(snapshot.peak_bytes), snapshot.alloc_count, retained_ratio, peak_ratio,
                snapshot.header_mismatches);
    std::fflush(stdout);

    const std::string parse_message =
        std::string("parse_document failed: ") + (document.has_value() ? std::string() : document.error());
    TEST_ASSERT_TRUE_MESSAGE(document.has_value(), parse_message.c_str());

    TEST_ASSERT_EQUAL_size_t(1, document->screens.size());
    TEST_ASSERT_EQUAL_size_t(0, snapshot.header_mismatches);

    std::string ratio_message =
        "fixture=" + std::string(fixture_name) + " retained R=" + std::to_string(retained_ratio) + " (bytes=" +
        std::to_string(snapshot.retained_bytes) + "/" + std::to_string(raw_json_bytes) + ") exceeds limit " +
        std::to_string(limits.retained_ratio);
    TEST_ASSERT_TRUE_MESSAGE(static_cast<double>(snapshot.retained_bytes) <=
                             limits.retained_ratio * static_cast<double>(raw_json_bytes),
                             ratio_message.c_str());

    ratio_message =
        "fixture=" + std::string(fixture_name) + " peak K=" + std::to_string(peak_ratio) + " (bytes=" +
        std::to_string(snapshot.peak_bytes) + "/" + std::to_string(raw_json_bytes) + ") exceeds limit " +
        std::to_string(limits.peak_ratio);
    TEST_ASSERT_TRUE_MESSAGE(static_cast<double>(snapshot.peak_bytes) <=
                             limits.peak_ratio * static_cast<double>(raw_json_bytes),
                             ratio_message.c_str());

    const std::string alloc_message = "fixture=" + std::string(fixture_name) +
                                      " allocs=" + std::to_string(snapshot.alloc_count) + " exceeds limit " +
                                      std::to_string(limits.max_alloc_count);
    TEST_ASSERT_TRUE_MESSAGE(snapshot.alloc_count <= limits.max_alloc_count, alloc_message.c_str());
}

}  // namespace

BROOKESIA_TEST_CASE(test_gui_interface_mem_gate_style_constants_fixture,
                    "mem gate style/constants fixture", "[gui][interface][mem_gate]")
{
    // Limits = first-run measured x1.25 (host, boost 1.87): R=2.573 K=8.363 allocs=6677 (2026-09-30).
    run_mem_gate("style_constants",
    {"constants/default.json", "constants/portrait.json", "constants/small.json", "styles/shell.json"},
    kMiniScreenAsset, MemGateLimits{3.25, 10.5, 8400});
}

BROOKESIA_TEST_CASE(test_gui_interface_mem_gate_deep_tree_fixture, "mem gate deep tree fixture",
                    "[gui][interface][mem_gate]")
{
    // Limits = first-run measured x1.25 (host, boost 1.87): R=3.704 K=10.933 allocs=10057 (2026-09-30).
    run_mem_gate("deep_tree", {
        "constants/default.json", "styles/shell.json", "templates/press_scale.json",
        "screens/keyboard_input.json"
    },
    "", MemGateLimits{4.75, 13.75, 12600});
}

#endif  // !defined(ESP_PLATFORM)
