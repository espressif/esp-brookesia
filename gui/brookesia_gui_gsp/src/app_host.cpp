/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstring>
#include <utility>

#if defined(BROOKESIA_GUI_GSP_DESKTOP)
#   include <cstdlib>
namespace {
void heap_caps_free(void *pointer)
{
    std::free(pointer);
}
#if defined(CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES) && CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES
constexpr int MALLOC_CAP_SPIRAM = 0;
constexpr int MALLOC_CAP_8BIT = 0;
void *heap_caps_aligned_alloc(size_t alignment, size_t size, int)
{
    return std::aligned_alloc(alignment, ((size + alignment - 1) / alignment) * alignment);
}
#endif
}
#else
#   include "esp_heap_caps.h"
#endif
#if defined(CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES) && CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES
#   include <cstdint>
#   include "zlib.h"
#endif

#if defined(BROOKESIA_GUI_GSP_HOST_TEST)
#   include "sdk_stubs.hpp"
#   include "host_log_stubs.hpp"
#else
#   include "private/utils.hpp"
#endif

#include "brookesia/gui_gsp/app_host.hpp"
#include "brookesia/gui_gsp/bundle.hpp"

namespace esp_brookesia::gui::gsp {

namespace {

AppHost *s_current = nullptr;
#if defined(CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES) && CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES
constexpr char COMPRESSED_MAGIC[] = "BGZ1";
constexpr size_t COMPRESSED_HEADER_SIZE = 16;
constexpr size_t MAX_EXPANDED_BUNDLE_BYTES = 8 * 1024 * 1024;
constexpr size_t BUNDLE_ALIGNMENT = 64;

struct HeapDeleter {
    void operator()(std::byte *bytes) const
    {
        heap_caps_free(bytes);
    }
};

struct ExpandedBundle {
    std::unique_ptr<std::byte, HeapDeleter> bytes;
    size_t size = 0;
};

bool has_compressed_magic(std::span<const std::byte> bytes)
{
    return bytes.size() >= 4 && std::memcmp(bytes.data(), COMPRESSED_MAGIC, 4) == 0;
}

uint32_t read_u32_le(const std::byte *bytes)
{
    return static_cast<uint32_t>(std::to_integer<uint8_t>(bytes[0])) |
           (static_cast<uint32_t>(std::to_integer<uint8_t>(bytes[1])) << 8) |
           (static_cast<uint32_t>(std::to_integer<uint8_t>(bytes[2])) << 16) |
           (static_cast<uint32_t>(std::to_integer<uint8_t>(bytes[3])) << 24);
}

std::expected<ExpandedBundle, std::string> expand_compiled_bundle(std::span<const std::byte> envelope)
{
    if (envelope.size() < COMPRESSED_HEADER_SIZE) {
        return std::unexpected("Compressed GSP bundle header is truncated");
    }
    const auto expanded_size = read_u32_le(envelope.data() + 4);
    const auto compressed_size = read_u32_le(envelope.data() + 8);
    const auto expected_crc = read_u32_le(envelope.data() + 12);
    if (expanded_size < 4 || expanded_size > MAX_EXPANDED_BUNDLE_BYTES ||
            compressed_size == 0 || compressed_size != envelope.size() - COMPRESSED_HEADER_SIZE) {
        return std::unexpected("Compressed GSP bundle has an invalid size");
    }

    ExpandedBundle result;
    result.size = expanded_size;
    result.bytes.reset(static_cast<std::byte *>(heap_caps_aligned_alloc(
                           BUNDLE_ALIGNMENT, result.size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!result.bytes) {
        return std::unexpected("Unable to allocate aligned PSRAM for expanded GSP bundle");
    }
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<std::byte *>(envelope.data() + COMPRESSED_HEADER_SIZE));
    stream.avail_in = compressed_size;
    stream.next_out = reinterpret_cast<Bytef *>(result.bytes.get());
    stream.avail_out = expanded_size;
    if (inflateInit(&stream) != Z_OK) {
        return std::unexpected("Unable to initialize GSP bundle decompression");
    }
    const int decode_result = inflate(&stream, Z_FINISH);
    const int finish_result = inflateEnd(&stream);
    if (decode_result != Z_STREAM_END || finish_result != Z_OK ||
            stream.total_in != compressed_size || stream.total_out != expanded_size ||
            std::memcmp(result.bytes.get(), "GSPB", 4) != 0) {
        return std::unexpected("Compressed GSP bundle is truncated or invalid");
    }
    const auto actual_crc = crc32(0, reinterpret_cast<const Bytef *>(result.bytes.get()), result.size);
    if (actual_crc != expected_crc) {
        return std::unexpected("Compressed GSP bundle CRC does not match the compiled original");
    }
    return result;
}
#endif

} // namespace

class AppHost::Impl {
public:
    Session session{};
    std::shared_ptr<const Bundle> bundle{};
    std::byte *owned_bytes = nullptr;
    void *event_user_ctx = nullptr;
};

AppHost::AppHost()
    : impl_(std::make_unique<Impl>())
{
}

AppHost::~AppHost()
{
    if (running()) {
        BROOKESIA_LOGE("Destroying a GSP app host that is still running");
        std::terminate();
    }
}

std::expected<void, std::string> AppHost::start(
    std::span<const std::byte> bytes, const DisplayLease &display, Session::EventCallback on_event, void *user_ctx
)
{
    if (running()) {
        return std::unexpected("GSP app host is already running");
    }

    auto loaded = Bundle::load(bytes);
    if (!loaded) {
        return std::unexpected(loaded.error());
    }

    impl_->bundle = std::move(loaded.value());
    impl_->event_user_ctx = user_ctx;
    if (!impl_->session.start(impl_->bundle, display, on_event, user_ctx)) {
        if (impl_->session.running()) {
            s_current = this;
            return std::unexpected("GSP session event registration failed and the session is still running");
        }
        impl_->bundle.reset();
        impl_->event_user_ctx = nullptr;
        return std::unexpected("Failed to start GSP session");
    }
    s_current = this;
    return {};
}

std::expected<void, std::string> AppHost::start(
    const esp_gsp_config_t &config, const DisplayLease &display, Session::EventCallback on_event, void *user_ctx
)
{
    if (running()) {
        return std::unexpected("GSP app host is already running");
    }
    if ((config.bundle == nullptr) || (config.bundle_size == 0)) {
        return std::unexpected("GSP compiler config has no bundle");
    }
    const auto bytes = std::span<const std::byte>(
                           reinterpret_cast<const std::byte *>(config.bundle), config.bundle_size);
#if defined(CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES) && CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES
    if (has_compressed_magic(bytes)) {
        auto expanded = expand_compiled_bundle(bytes);
        if (!expanded) {
            return std::unexpected(expanded.error());
        }
        // GSPC's generated directories are independent C tables. The bundle
        // bytes are borrowed until stop() succeeds, so retain the expansion.
        esp_gsp_config_t expanded_config = config;
        expanded_config.bundle = expanded->bytes.get();
        expanded_config.bundle_size = expanded->size;
        impl_->owned_bytes = expanded->bytes.release();
        impl_->event_user_ctx = user_ctx;
        if (!impl_->session.start(expanded_config, display, on_event, user_ctx)) {
            if (impl_->session.running()) {
                s_current = this;
                return std::unexpected("GSP session event registration failed and the session is still running");
            }
            heap_caps_free(impl_->owned_bytes);
            impl_->owned_bytes = nullptr;
            impl_->event_user_ctx = nullptr;
            return std::unexpected("Failed to start GSP session with expanded compiler bundle");
        }
        s_current = this;
        return {};
    }
#endif
    if (bytes.size() < 4 || std::memcmp(bytes.data(), "GSPB", 4) != 0) {
        return std::unexpected("GSP compiler config has an unknown bundle format");
    }
    auto loaded = Bundle::load(bytes);
    if (loaded) {
        impl_->bundle = std::move(loaded.value());
        impl_->event_user_ctx = user_ctx;
        if (!impl_->session.start(impl_->bundle, display, on_event, user_ctx)) {
            if (impl_->session.running()) {
                s_current = this;
                return std::unexpected("GSP session event registration failed and the session is still running");
            }
            impl_->bundle.reset();
            impl_->event_user_ctx = nullptr;
            return std::unexpected("Failed to start GSP session");
        }
        s_current = this;
        return {};
    }

#if defined(BROOKESIA_GUI_GSP_DESKTOP)
    return std::unexpected(loaded.error());
#else
    // Ordinary compiler bundles use the separately generated directory tables.
    // Retain the supplied config when self-describing metadata is absent.
    BROOKESIA_LOGW("GSP deployable open failed (%1%); using the compiler config in place", loaded.error());
    impl_->event_user_ctx = user_ctx;
    if (!impl_->session.start(config, display, on_event, user_ctx)) {
        if (impl_->session.running()) {
            s_current = this;
            return std::unexpected("GSP session event registration failed and the session is still running");
        }
        impl_->event_user_ctx = nullptr;
        return std::unexpected("Failed to start GSP session");
    }
    s_current = this;
    return {};
#endif
}

std::expected<void, std::string> AppHost::stop()
{
    if (!running()) {
        impl_->bundle.reset();
        if (impl_->owned_bytes != nullptr) {
            heap_caps_free(impl_->owned_bytes);
            impl_->owned_bytes = nullptr;
        }
        impl_->event_user_ctx = nullptr;
        return {};
    }
    if (!impl_->session.stop()) {
        return std::unexpected("Failed to stop GSP session");
    }
    impl_->bundle.reset();
    if (impl_->owned_bytes != nullptr) {
        heap_caps_free(impl_->owned_bytes);
        impl_->owned_bytes = nullptr;
    }
    impl_->event_user_ctx = nullptr;
    if (s_current == this) {
        s_current = nullptr;
    }
    return {};
}

bool AppHost::running() const
{
    return (impl_ != nullptr) && impl_->session.running();
}

esp_gsp_handle_t AppHost::handle() const
{
    return running() ? impl_->session.handle() : nullptr;
}

AppHost *AppHost::current()
{
    return s_current;
}

const std::shared_ptr<const Bundle> &AppHost::bundle() const
{
    return impl_->bundle;
}

void *AppHost::event_user_ctx() const
{
    return impl_->event_user_ctx;
}

} // namespace esp_brookesia::gui::gsp
