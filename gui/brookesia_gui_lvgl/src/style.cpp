/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/types.hpp"
#include "brookesia/gui_lvgl/macro_configs.h"
#if !BROOKESIA_GUI_LVGL_STYLE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#if LV_USE_FREETYPE
#   if __has_include("lvgl/font/lv_freetype.h")
#       include "lvgl/font/lv_freetype.h"
#   elif __has_include("font/lv_freetype.h")
#       include "font/lv_freetype.h"
#   elif __has_include("src/libs/freetype/lv_freetype.h")
#       include "src/libs/freetype/lv_freetype.h"
#   elif __has_include("lvgl/src/libs/freetype/lv_freetype.h")
#       include "lvgl/src/libs/freetype/lv_freetype.h"
#   else
#       error "LVGL FreeType header not found"
#   endif
#   if defined(LV_USE_FS_MEMFS) && LV_USE_FS_MEMFS && defined(LV_FREETYPE_USE_LVGL_PORT) && LV_FREETYPE_USE_LVGL_PORT
#       define BROOKESIA_GUI_LVGL_USE_FREETYPE_MEMFS_PORT (1)
#   else
#       define BROOKESIA_GUI_LVGL_USE_FREETYPE_MEMFS_PORT (0)
#   endif
#   if defined(ESP_PLATFORM) && (!defined(CONFIG_SPIRAM_XIP_FROM_PSRAM) || !CONFIG_SPIRAM_XIP_FROM_PSRAM) && \
        !BROOKESIA_GUI_LVGL_USE_FREETYPE_MEMFS_PORT
#       error "Enable LV_USE_FS_MEMFS and LV_FREETYPE_USE_LVGL_PORT when CONFIG_SPIRAM_XIP_FROM_PSRAM is disabled"
#   endif
#endif

#if LV_USE_IMGFONT
#   if __has_include("lvgl/font/lv_imgfont.h")
#       include "lvgl/font/lv_imgfont.h"
#       define BROOKESIA_GUI_LVGL_HAS_IMGFONT (1)
#   elif __has_include("font/lv_imgfont.h")
#       include "font/lv_imgfont.h"
#       define BROOKESIA_GUI_LVGL_HAS_IMGFONT (1)
#   elif __has_include("src/font/imgfont/lv_imgfont.h")
#       include "src/font/imgfont/lv_imgfont.h"
#       define BROOKESIA_GUI_LVGL_HAS_IMGFONT (1)
#   elif __has_include("lvgl/src/font/imgfont/lv_imgfont.h")
#       include "lvgl/src/font/imgfont/lv_imgfont.h"
#       define BROOKESIA_GUI_LVGL_HAS_IMGFONT (1)
#   else
#       define BROOKESIA_GUI_LVGL_HAS_IMGFONT (0)
#   endif
#else
#   define BROOKESIA_GUI_LVGL_HAS_IMGFONT (0)
#endif

#include "brookesia/service_helper/system/storage.hpp"

namespace esp_brookesia::gui::lvgl {

namespace {

using StorageHelper = service::helper::Storage;

constexpr lv_opa_t DEBUG_OUTLINE_OPACITY = LV_OPA_COVER;
constexpr int32_t DEBUG_OUTLINE_WIDTH = 2;
constexpr int32_t DEBUG_OUTLINE_PAD = 2;

lv_color_t get_debug_outline_color(uint32_t depth)
{
    static constexpr std::array<uint32_t, 4> palette = {
        0xDC2626, // red
        0x2563EB, // blue
        0x16A34A, // green
        0xEA580C, // orange
    };

    return lv_color_hex(palette[depth % palette.size()]);
}

#if LV_USE_FREETYPE && !BROOKESIA_GUI_LVGL_HAS_ESP_FONT_BACKEND
bool file_exists(std::string_view path)
{
    auto info = StorageHelper::fs_stat(std::string(path));
    return info && info->type == StorageHelper::FileType::File;
}
#endif

#if LV_USE_FREETYPE && BROOKESIA_GUI_LVGL_USE_FREETYPE_MEMFS_PORT
std::expected<std::shared_ptr<FontCacheEntry::FontSource>, std::string> load_font_source_from_storage(
    BackendImpl &impl, std::string_view font_path)
{
    const std::string font_path_string(font_path);
    auto cached_it = impl.font_source_cache.find(font_path_string);
    if (cached_it != impl.font_source_cache.end()) {
        if (auto cached_source = cached_it->second.lock(); cached_source != nullptr) {
            return cached_source;
        }
        impl.font_source_cache.erase(cached_it);
    }

    auto info = StorageHelper::fs_stat(font_path_string);
    if (!info) {
        return std::unexpected("Failed to stat font file: " + font_path_string + ", error: " + info.error());
    }
    if (info->type != StorageHelper::FileType::File) {
        return std::unexpected("Font path is not a file: " + font_path_string);
    }
    if (info->size == 0) {
        return std::unexpected("Font file is empty: " + font_path_string);
    }
    if (info->size > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
            info->size > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
        return std::unexpected("Font file is too large: " + font_path_string);
    }

    auto source = std::make_shared<FontCacheEntry::FontSource>();
    source->data.resize(static_cast<size_t>(info->size));
    auto read_result = StorageHelper::fs_read(
                           font_path_string,
                           service::RawBuffer(source->data.data(), source->data.size())
                       );
    if (!read_result) {
        return std::unexpected("Failed to read font file: " + font_path_string + ", error: " + read_result.error());
    }
    if (read_result.value() != source->data.size()) {
        return std::unexpected("Font file read size mismatch: " + font_path_string);
    }

    lv_fs_make_path_from_buffer(
        &source->memfs_path,
        static_cast<char>(LV_FS_MEMFS_LETTER),
        source->data.data(),
        static_cast<uint32_t>(source->data.size()),
        nullptr
    );
    // Different FreeType sizes for the same path can share the immutable font bytes.
    impl.font_source_cache[font_path_string] = source;
    return source;
}
#endif

void log_font_warning_once(std::string_view message)
{
    static std::unordered_set<std::string> logged_messages;

    const auto [_, inserted] = logged_messages.emplace(message);
    if (inserted) {
        BROOKESIA_LOGW("%1%", message);
    }
}

void log_font_info_once(std::string_view message)
{
    static std::unordered_set<std::string> logged_messages;

    const auto [_, inserted] = logged_messages.emplace(message);
    if (inserted) {
        BROOKESIA_LOGD("%1%", message);
    }
}

const lv_font_t *get_builtin_font(int32_t font_size)
{
    (void)font_size;
    const lv_font_t *closest_smaller_font = nullptr;

#if CONFIG_LV_FONT_MONTSERRAT_8
    if (font_size == 8) {
        return &lv_font_montserrat_8;
    } else if (font_size > 8) {
        closest_smaller_font = &lv_font_montserrat_8;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_10
    if (font_size == 10) {
        return &lv_font_montserrat_10;
    } else if (font_size > 10) {
        closest_smaller_font = &lv_font_montserrat_10;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_12
    if (font_size == 12) {
        return &lv_font_montserrat_12;
    } else if (font_size > 12) {
        closest_smaller_font = &lv_font_montserrat_12;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_14
    if (font_size == 14) {
        return &lv_font_montserrat_14;
    } else if (font_size > 14) {
        closest_smaller_font = &lv_font_montserrat_14;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_16
    if (font_size == 16) {
        return &lv_font_montserrat_16;
    } else if (font_size > 16) {
        closest_smaller_font = &lv_font_montserrat_16;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_18
    if (font_size == 18) {
        return &lv_font_montserrat_18;
    } else if (font_size > 18) {
        closest_smaller_font = &lv_font_montserrat_18;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_20
    if (font_size == 20) {
        return &lv_font_montserrat_20;
    } else if (font_size > 20) {
        closest_smaller_font = &lv_font_montserrat_20;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_22
    if (font_size == 22) {
        return &lv_font_montserrat_22;
    } else if (font_size > 22) {
        closest_smaller_font = &lv_font_montserrat_22;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_24
    if (font_size == 24) {
        return &lv_font_montserrat_24;
    } else if (font_size > 24) {
        closest_smaller_font = &lv_font_montserrat_24;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_26
    if (font_size == 26) {
        return &lv_font_montserrat_26;
    } else if (font_size > 26) {
        closest_smaller_font = &lv_font_montserrat_26;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_28
    if (font_size == 28) {
        return &lv_font_montserrat_28;
    } else if (font_size > 28) {
        closest_smaller_font = &lv_font_montserrat_28;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_30
    if (font_size == 30) {
        return &lv_font_montserrat_30;
    } else if (font_size > 30) {
        closest_smaller_font = &lv_font_montserrat_30;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_32
    if (font_size == 32) {
        return &lv_font_montserrat_32;
    } else if (font_size > 32) {
        closest_smaller_font = &lv_font_montserrat_32;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_34
    if (font_size == 34) {
        return &lv_font_montserrat_34;
    } else if (font_size > 34) {
        closest_smaller_font = &lv_font_montserrat_34;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_36
    if (font_size == 36) {
        return &lv_font_montserrat_36;
    } else if (font_size > 36) {
        closest_smaller_font = &lv_font_montserrat_36;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_38
    if (font_size == 38) {
        return &lv_font_montserrat_38;
    } else if (font_size > 38) {
        closest_smaller_font = &lv_font_montserrat_38;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_40
    if (font_size == 40) {
        return &lv_font_montserrat_40;
    } else if (font_size > 40) {
        closest_smaller_font = &lv_font_montserrat_40;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_42
    if (font_size == 42) {
        return &lv_font_montserrat_42;
    } else if (font_size > 42) {
        closest_smaller_font = &lv_font_montserrat_42;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_44
    if (font_size == 44) {
        return &lv_font_montserrat_44;
    } else if (font_size > 44) {
        closest_smaller_font = &lv_font_montserrat_44;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_46
    if (font_size == 46) {
        return &lv_font_montserrat_46;
    } else if (font_size > 46) {
        closest_smaller_font = &lv_font_montserrat_46;
    }
#endif
#if CONFIG_LV_FONT_MONTSERRAT_48
    if (font_size == 48) {
        return &lv_font_montserrat_48;
    } else if (font_size > 48) {
        closest_smaller_font = &lv_font_montserrat_48;
    }
#endif

    if (closest_smaller_font != nullptr) {
        return closest_smaller_font;
    }

#if defined(LV_FONT_DEFAULT)
    return LV_FONT_DEFAULT;
#else
    return nullptr;
#endif
}

bool node_type_uses_text_font(NodeType type)
{
    switch (type) {
    case NodeType::Label:
    case NodeType::TextInput:
    case NodeType::Checkbox:
    case NodeType::Dropdown:
    case NodeType::Table:
    case NodeType::Keyboard:
        return true;
    default:
        return false;
    }
}

const NativeFontVariant *select_native_font_variant(const ResolvedFontSpec &font_spec, int32_t font_size)
{
    const NativeFontVariant *selected_variant = nullptr;
    int32_t selected_delta = 0;
    for (const auto &variant : font_spec.native_fonts) {
        if (variant.native_src == 0 || variant.native_size <= 0) {
            continue;
        }
        const auto delta = std::abs(variant.native_size - font_size);
        if (selected_variant == nullptr || delta < selected_delta ||
                (delta == selected_delta && variant.native_size < selected_variant->native_size)) {
            selected_variant = &variant;
            selected_delta = delta;
        }
        if (delta == 0) {
            break;
        }
    }
    return selected_variant;
}

std::vector<ImageFontSize> normalized_image_font_sizes(const ResolvedFontSpec &font_spec)
{
    if (!font_spec.image_font_sizes.empty()) {
        return font_spec.image_font_sizes;
    }
    if (font_spec.image_font_height > 0 && !font_spec.image_font_glyphs.empty()) {
        return {
            ImageFontSize {
                .height = font_spec.image_font_height,
                .glyphs = font_spec.image_font_glyphs,
            },
        };
    }
    return {};
}

std::optional<ImageFontSize> select_image_font_size(const ResolvedFontSpec &font_spec, int32_t requested_size)
{
    const auto sizes = normalized_image_font_sizes(font_spec);
    if (sizes.empty()) {
        return std::nullopt;
    }
    const auto target_size = requested_size > 0 ? requested_size : sizes.front().height;
    const ImageFontSize *selected_size = nullptr;
    int32_t selected_delta = 0;
    for (const auto &size : sizes) {
        if (size.height <= 0 || size.glyphs.empty()) {
            continue;
        }
        const auto delta = std::abs(size.height - target_size);
        if (selected_size == nullptr || delta < selected_delta ||
                (delta == selected_delta && size.height > selected_size->height)) {
            selected_size = &size;
            selected_delta = delta;
        }
        if (delta == 0) {
            break;
        }
    }
    if (selected_size == nullptr) {
        return std::nullopt;
    }
    return *selected_size;
}

std::string make_font_cache_key(const ResolvedFontSpec &font_spec, int32_t font_size, int32_t image_font_size = 0)
{
    std::ostringstream oss;
    oss << font_spec.kind << "|" << font_size << "|" << font_spec.primary_src;
    for (const auto &fallback_src : font_spec.fallback_srcs) {
        oss << "|" << fallback_src;
    }
    if (font_spec.kind == "imageFont") {
        const auto selected_size = select_image_font_size(font_spec, image_font_size);
        if (!selected_size.has_value()) {
            return oss.str();
        }
        oss << "|image_size=" << image_font_size << "|height=" << selected_size->height;
        for (const auto &glyph : selected_size->glyphs) {
            oss << "|glyph=" << glyph.codepoint << ":" << glyph.src;
        }
    }
    return oss.str();
}

#if LV_USE_FREETYPE
FontCacheEntry *create_font_cache_entry(BackendImpl &impl, const ResolvedFontSpec &font_spec, int32_t font_size)
{
#if !BROOKESIA_GUI_LVGL_USE_FREETYPE_MEMFS_PORT
    (void)impl;
#endif

    if (font_spec.primary_src.empty()) {
        return nullptr;
    }

    FontCacheEntry entry;
    entry.cache_key = make_font_cache_key(font_spec, font_size);

    using FontType = lv_font_t *;
    struct CreatedFont {
        FontType font = nullptr;
        void *handle = nullptr;
        std::shared_ptr<FontCacheEntry::FontSource> source;
    };
    auto create_font = [&](std::string_view font_path) -> CreatedFont {
#if BROOKESIA_GUI_LVGL_USE_FREETYPE_MEMFS_PORT
        auto font_source = load_font_source_from_storage(impl, font_path);
        if (!font_source)
        {
            log_font_warning_once(
                "Failed to load FreeType font source from storage: path='" + std::string(font_path) +
                "', error=" + font_source.error()
            );
            return {};
        }

        lv_font_info_t font_info;
        lv_freetype_init_font_info(&font_info);
        // LVGL FreeType copies memfs sources as lv_fs_path_ex_t, so keep the full object alive in FontCacheEntry.
        font_info.name = reinterpret_cast<const char *>(&(*font_source)->memfs_path);
        font_info.size = static_cast<uint32_t>(font_size);
        font_info.render_mode = LV_FREETYPE_FONT_RENDER_MODE_BITMAP;
        font_info.style = LV_FREETYPE_FONT_STYLE_NORMAL;

        auto *font = lv_freetype_font_create_with_info(&font_info);
        if (font == nullptr)
        {
            log_font_warning_once(
                "Failed to create memory FreeType font from '" + std::string(font_path) +
                "', fallback to built-in Montserrat"
            );
            return {};
        }

        log_font_info_once(
            "Created memory FreeType font: path='" + std::string(font_path) +
            "', size=" + std::to_string(font_size)
        );
        return {
            .font = font,
            .handle = nullptr,
            .source = *font_source,
        };
#else
#if BROOKESIA_GUI_LVGL_HAS_ESP_FONT_BACKEND
        esp_lv_adapter_ft_font_handle_t font_handle = nullptr;
        const std::string font_path_string(font_path);
        auto file_info = StorageHelper::fs_stat(font_path_string);
        if (file_info && file_info->type == StorageHelper::FileType::File)
        {
            log_font_info_once(
                "ESP font file stat ok: path='" + font_path_string +
                "', size=" + std::to_string(static_cast<unsigned long long>(file_info->size))
            );
        } else
        {
            log_font_warning_once(
                "ESP font file stat failed: path='" + font_path_string +
                "', reason='" + (file_info ? std::string("not a file") : file_info.error()) + "'"
            );
        }

        if (file_info && file_info->size > 0)
        {
            uint8_t probe_byte = 0;
            auto read_result = StorageHelper::fs_read(font_path_string, service::RawBuffer(&probe_byte, 1));
            if (read_result && read_result.value() == 1) {
                log_font_info_once("ESP font file read probe ok: path='" + font_path_string + "'");
            } else {
                log_font_warning_once(
                    "ESP font file read probe failed: path='" + font_path_string +
                    "', reason='" + (read_result ? std::string("short read") : read_result.error()) + "'"
                );
            }
        }

        const esp_lv_adapter_ft_font_config_t font_config = ESP_LV_ADAPTER_FT_FONT_FILE_CONFIG(
                    font_path_string.c_str(),
                    static_cast<uint16_t>(font_size),
                    ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL
                );
        if (esp_lv_adapter_ft_font_init(&font_config, &font_handle) != ESP_OK || font_handle == nullptr)
        {
            log_font_warning_once(
                "Failed to create ESP FreeType font from '" + std::string(font_path) +
                "', fallback to built-in Montserrat"
            );
            return {};
        }

        auto *font = const_cast<lv_font_t *>(esp_lv_adapter_ft_font_get(font_handle));
        if (font == nullptr)
        {
            esp_lv_adapter_ft_font_deinit(font_handle);
            log_font_warning_once(
                "Failed to get ESP LVGL font from '" + std::string(font_path) +
                "', fallback to built-in Montserrat"
            );
            return {};
        }

        log_font_info_once(
            "Created ESP FreeType font: path='" + font_path_string + "', size=" + std::to_string(font_size)
        );
        return {
            .font = font,
            .handle = font_handle,
            .source = nullptr,
        };
#else
        if (!file_exists(font_path))
        {
            log_font_warning_once(
                "Font file not found: '" + std::string(font_path) + "', fallback to built-in Montserrat"
            );
            return {};
        }

        auto *font = lv_freetype_font_create(
                         std::string(font_path).c_str(),
                         LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                         static_cast<uint32_t>(font_size),
                         LV_FREETYPE_FONT_STYLE_NORMAL
                     );
        if (font == nullptr)
        {
            log_font_warning_once(
                "Failed to create FreeType font from '" + std::string(font_path) +
                "', fallback to built-in Montserrat"
            );
        } else
        {
            log_font_info_once(
                "Created PC FreeType font: path='" + std::string(font_path) + "', size=" + std::to_string(font_size)
            );
        }
        return {
            .font = font,
            .handle = nullptr,
            .source = nullptr,
        };
#endif
#endif
    };

    struct CandidateFont {
        std::string source_font_id;
        std::string src;
        FontType font = nullptr;
        void *handle = nullptr;
        std::shared_ptr<FontCacheEntry::FontSource> source;
        bool is_primary = false;
    };

    std::vector<CandidateFont> resolved_fonts;
    resolved_fonts.reserve(1 + font_spec.fallback_srcs.size());

    auto primary = create_font(font_spec.primary_src);
    if (primary.font != nullptr) {
        resolved_fonts.push_back({
            .source_font_id = font_spec.font_id,
            .src = font_spec.primary_src,
            .font = primary.font,
            .handle = primary.handle,
            .source = primary.source,
            .is_primary = true,
        });
        log_font_info_once(
            "Primary font resolved: font_id='" + font_spec.font_id + "', src='" + font_spec.primary_src +
            "', size=" + std::to_string(font_size)
        );
    }

    for (const auto &fallback_src : font_spec.fallback_srcs) {
        auto fallback = create_font(fallback_src);
        if (fallback.font == nullptr) {
            continue;
        }
        resolved_fonts.push_back({
            .source_font_id = "<fallback>",
            .src = fallback_src,
            .font = fallback.font,
            .handle = fallback.handle,
            .source = fallback.source,
            .is_primary = false,
        });
        log_font_info_once(
            "Fallback font resolved: font_id='" + font_spec.font_id + "', src='" + fallback_src +
            "', size=" + std::to_string(font_size)
        );
    }

    if (resolved_fonts.empty()) {
        return nullptr;
    }

    if (!resolved_fonts.front().is_primary) {
        log_font_warning_once(
            "Primary font unavailable for font_id='" + font_spec.font_id +
            "', using fallback chain root src='" + resolved_fonts.front().src + "' as root"
        );
    }

    entry.chain.push_back(resolved_fonts.front().font);
    entry.platform_font_handles.push_back(resolved_fonts.front().handle);
    entry.font_sources.push_back(resolved_fonts.front().source);
    entry.font_kinds.push_back(FontCacheEntry::FontKind::FreeType);
    for (size_t i = 1; i < resolved_fonts.size(); ++i) {
        entry.chain.back()->fallback = resolved_fonts[i].font;
        entry.chain.push_back(resolved_fonts[i].font);
        entry.platform_font_handles.push_back(resolved_fonts[i].handle);
        entry.font_sources.push_back(resolved_fonts[i].source);
        entry.font_kinds.push_back(FontCacheEntry::FontKind::FreeType);
    }

    return new FontCacheEntry(std::move(entry));
}
#endif

#if BROOKESIA_GUI_LVGL_HAS_IMGFONT
const void *image_font_get_source_cb(
    const lv_font_t *font,
    uint32_t unicode,
    uint32_t unicode_next,
    int32_t *offset_y,
    void *user_data)
{
    (void)font;
    (void)unicode_next;
    if (offset_y != nullptr) {
        *offset_y = 0;
    }

    const auto *context = static_cast<const FontCacheEntry::ImageFontContext *>(user_data);
    if (context == nullptr) {
        return nullptr;
    }

    for (const auto &glyph_source : context->glyph_sources) {
        if (glyph_source.codepoint == unicode && glyph_source.source != nullptr) {
            return &glyph_source.source->descriptor;
        }
    }
    return nullptr;
}

FontCacheEntry *create_image_font_cache_entry(
    BackendImpl &impl,
    const ResolvedFontSpec &font_spec,
    int32_t font_size,
    int32_t image_font_size)
{
    const auto selected_size = select_image_font_size(font_spec, image_font_size);
    if (!selected_size.has_value()) {
        return nullptr;
    }

    FontCacheEntry entry;
    entry.cache_key = make_font_cache_key(font_spec, font_size, image_font_size);

    auto context = std::make_unique<FontCacheEntry::ImageFontContext>();
    context->glyph_sources.reserve(selected_size->glyphs.size());
    for (const auto &glyph : selected_size->glyphs) {
        auto source = load_image_source(glyph.src);
        if (!source) {
            log_font_warning_once(
                "Failed to load image font glyph source: font_id='" + font_spec.font_id +
                "', src='" + glyph.src + "', error=" + source.error()
            );
            return nullptr;
        }
        context->glyph_sources.push_back(FontCacheEntry::ImageFontGlyphSource{
            .codepoint = glyph.codepoint,
            .source = *source,
        });
    }

    auto *image_font = lv_imgfont_create(
                           static_cast<uint16_t>(selected_size->height),
                           image_font_get_source_cb,
                           context.get()
                       );
    if (image_font == nullptr) {
        log_font_warning_once(
            "Failed to create imageFont: font_id='" + font_spec.font_id + "', height=" +
            std::to_string(selected_size->height)
        );
        return nullptr;
    }

    image_font->fallback = const_cast<lv_font_t *>(get_builtin_font(font_size));

    entry.chain.push_back(image_font);
    entry.platform_font_handles.push_back(nullptr);
    entry.font_kinds.push_back(FontCacheEntry::FontKind::ImageFont);
    entry.image_font_contexts.push_back(std::move(context));

#if LV_USE_FREETYPE
    if (!font_spec.fallback_srcs.empty()) {
        ResolvedFontSpec fallback_spec;
        fallback_spec.font_id = font_spec.font_id + ".fallback";
        fallback_spec.kind = "file";
        fallback_spec.primary_src = font_spec.fallback_srcs.front();
        fallback_spec.fallback_srcs.assign(font_spec.fallback_srcs.begin() + 1, font_spec.fallback_srcs.end());

        std::unique_ptr<FontCacheEntry> fallback_entry(create_font_cache_entry(impl, fallback_spec, font_size));
        if (fallback_entry != nullptr && !fallback_entry->chain.empty()) {
            image_font->fallback = fallback_entry->chain.front();
            entry.chain.insert(entry.chain.end(), fallback_entry->chain.begin(), fallback_entry->chain.end());
            entry.platform_font_handles.insert(
                entry.platform_font_handles.end(),
                fallback_entry->platform_font_handles.begin(),
                fallback_entry->platform_font_handles.end()
            );
            entry.font_sources.insert(
                entry.font_sources.end(),
                fallback_entry->font_sources.begin(),
                fallback_entry->font_sources.end()
            );
            entry.font_kinds.insert(
                entry.font_kinds.end(),
                fallback_entry->font_kinds.begin(),
                fallback_entry->font_kinds.end()
            );
            fallback_entry->chain.clear();
            fallback_entry->platform_font_handles.clear();
            fallback_entry->font_sources.clear();
            fallback_entry->font_kinds.clear();
        }
    }
#endif

    return new FontCacheEntry(std::move(entry));
}
#endif

} // namespace

std::optional<uint32_t> parse_color(std::string_view color)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: color(%1%)", color);

    if (color.empty()) {
        return std::nullopt;
    }

    if (color.size() != 7 || color.front() != '#') {
        return std::nullopt;
    }

    const std::string hex(color.substr(1));
    char *end = nullptr;
    const auto value = std::strtoul(hex.c_str(), &end, 16);
    if (end == nullptr || *end != '\0') {
        return std::nullopt;
    }
    return static_cast<uint32_t>(value);
}

std::optional<uint32_t> style_part_selector(std::string_view part)
{
    if (part == "main") {
        return LV_PART_MAIN;
    }
    if (part == "indicator") {
        return LV_PART_INDICATOR;
    }
    if (part == "knob") {
        return LV_PART_KNOB;
    }
    return std::nullopt;
}

std::optional<uint32_t> style_state_selector(std::string_view state, uint32_t part = LV_PART_MAIN)
{
    const auto make_selector = [part](lv_state_t lv_state) {
        return part | static_cast<uint32_t>(lv_state);
    };
    if (state == "pressed") {
        return make_selector(LV_STATE_PRESSED);
    }
    if (state == "checked") {
        return make_selector(LV_STATE_CHECKED);
    }
    if (state == "focused") {
        return make_selector(LV_STATE_FOCUSED);
    }
    if (state == "focusKey") {
        return make_selector(LV_STATE_FOCUS_KEY);
    }
    if (state == "edited") {
        return make_selector(LV_STATE_EDITED);
    }
    if (state == "hovered") {
        return make_selector(LV_STATE_HOVERED);
    }
    if (state == "scrolled") {
        return make_selector(LV_STATE_SCROLLED);
    }
    if (state == "disabled") {
        return make_selector(LV_STATE_DISABLED);
    }
    if (state == "user1") {
        return make_selector(LV_STATE_USER_1);
    }
    if (state == "user2") {
        return make_selector(LV_STATE_USER_2);
    }
    if (state == "user3") {
        return make_selector(LV_STATE_USER_3);
    }
    if (state == "user4") {
        return make_selector(LV_STATE_USER_4);
    }
    return std::nullopt;
}

void release_font(BackendImpl &impl, Record &record)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: record(%1%)", record);

    if (record.font_cache_key.empty()) {
        return;
    }

    auto cache_it = impl.font_cache.find(record.font_cache_key);
    if (cache_it == impl.font_cache.end()) {
        record.font_cache_key.clear();
        return;
    }

    if (cache_it->second.ref_count > 0) {
        --cache_it->second.ref_count;
    }

    if (cache_it->second.ref_count == 0) {
        for (size_t index = cache_it->second.chain.size(); index > 0; --index) {
            const auto font_index = index - 1;
            const auto font_kind = font_index < cache_it->second.font_kinds.size() ?
                                   cache_it->second.font_kinds[font_index] :
                                   FontCacheEntry::FontKind::FreeType;
            auto *font = cache_it->second.chain[font_index];
            if (font == nullptr) {
                continue;
            }
            if (font_kind == FontCacheEntry::FontKind::ImageFont) {
#if BROOKESIA_GUI_LVGL_HAS_IMGFONT
                lv_imgfont_destroy(font);
#endif
                continue;
            }
#if LV_USE_FREETYPE
#if BROOKESIA_GUI_LVGL_HAS_ESP_FONT_BACKEND
            auto *font_handle = font_index < cache_it->second.platform_font_handles.size() ?
                                static_cast<esp_lv_adapter_ft_font_handle_t>(
                                    cache_it->second.platform_font_handles[font_index]
                                ) :
                                nullptr;
            if (font_handle != nullptr) {
                esp_lv_adapter_ft_font_deinit(font_handle);
            } else {
                lv_freetype_font_delete(font);
            }
#else
            lv_freetype_font_delete(font);
#endif
#endif
        }
        impl.font_cache.erase(cache_it);
    }
    record.font_cache_key.clear();
}

const lv_font_t *get_font(BackendImpl &impl, Record &record, const ResolvedStyle &style)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: record(%1%), style(%2%)", record, style);

    release_font(impl, record);

    const auto font_size = style.style.font_size.value_or(16);
    const auto image_font_size = style.style.image_font_size.value_or(0);
    if (!style.resolved_font.native_fonts.empty()) {
        const auto *native_variant = select_native_font_variant(style.resolved_font, font_size);
        if (native_variant != nullptr) {
            auto *font = reinterpret_cast<const lv_font_t *>(native_variant->native_src);
            if (native_variant->native_size != font_size) {
                log_font_warning_once(
                    "Native font size mismatch: node='" + record.absolute_path + "', font_id='" +
                    style.resolved_font.font_id + "', requested_size=" + std::to_string(font_size) +
                    ", selected_size=" + std::to_string(native_variant->native_size)
                );
            }
            log_font_info_once(
                "Using native LVGL font: node='" + record.absolute_path + "', font_id='" +
                style.resolved_font.font_id + "', primary_src='" + style.resolved_font.primary_src +
                "', native_size=" + std::to_string(native_variant->native_size)
            );
            return font;
        }
    }

    if (style.resolved_font.kind == "imageFont") {
#if BROOKESIA_GUI_LVGL_HAS_IMGFONT
        const auto cache_key = make_font_cache_key(style.resolved_font, font_size, image_font_size);
        auto cache_it = impl.font_cache.find(cache_key);
        if (cache_it != impl.font_cache.end()) {
            ++cache_it->second.ref_count;
            record.font_cache_key = cache_key;
            log_font_info_once(
                "Reusing imageFont cache: node='" + record.absolute_path + "', font_id='" +
                style.resolved_font.font_id + "', size=" + std::to_string(font_size)
            );
            return cache_it->second.chain.front();
        }

        std::unique_ptr<FontCacheEntry> entry(
            create_image_font_cache_entry(impl, style.resolved_font, font_size, image_font_size)
        );
        if (entry == nullptr || entry->chain.empty()) {
            log_font_warning_once(
                "Failed to resolve imageFont for node '" + record.absolute_path + "', font_id='" +
                style.resolved_font.font_id + "', fallback to built-in Montserrat"
            );
            return get_builtin_font(font_size);
        }

        entry->ref_count = 1;
        record.font_cache_key = cache_key;
        auto *font = entry->chain.front();
        impl.font_cache.emplace(cache_key, std::move(*entry));
        log_font_info_once(
            "Created imageFont cache: node='" + record.absolute_path + "', font_id='" +
            style.resolved_font.font_id + "', glyph_count=" +
            std::to_string(style.resolved_font.image_font_glyphs.size()) +
            ", requested_image_size=" + std::to_string(image_font_size)
        );
        return font;
#else
        log_font_warning_once(
            "Font asset '" + style.resolved_font.font_id +
            "' requires LV_USE_IMGFONT and lv_imgfont header, fallback to built-in Montserrat"
        );
        return get_builtin_font(font_size);
#endif
    }

    if (style.resolved_font.primary_src.empty()) {
        log_font_info_once(
            "Using built-in LVGL font: node='" + record.absolute_path + "', requested_font_id='" +
            style.style.font.value_or("") + "', size=" + std::to_string(font_size)
        );
        return get_builtin_font(font_size);
    }

#if LV_USE_FREETYPE
    const auto cache_key = make_font_cache_key(style.resolved_font, font_size);
    auto cache_it = impl.font_cache.find(cache_key);
    if (cache_it != impl.font_cache.end()) {
        ++cache_it->second.ref_count;
        record.font_cache_key = cache_key;
        log_font_info_once(
            "Reusing font cache: node='" + record.absolute_path + "', font_id='" + style.resolved_font.font_id +
            "', primary_src='" + style.resolved_font.primary_src + "', size=" + std::to_string(font_size)
        );
        return cache_it->second.chain.front();
    }
    if (impl.failed_font_cache.contains(cache_key)) {
        log_font_warning_once(
            "Skip known unavailable FreeType font: font_id='" + style.resolved_font.font_id +
            "', primary_src='" + style.resolved_font.primary_src + "', size=" + std::to_string(font_size) +
            ", fallback to built-in Montserrat"
        );
        return get_builtin_font(font_size);
    }

    std::unique_ptr<FontCacheEntry> entry(create_font_cache_entry(impl, style.resolved_font, font_size));
    if (entry == nullptr || entry->chain.empty()) {
        impl.failed_font_cache.insert(cache_key);
        log_font_warning_once(
            "Failed to resolve FreeType font for node '" + record.absolute_path + "', font_id='" +
            style.resolved_font.font_id + "', fallback to built-in Montserrat"
        );
        return get_builtin_font(font_size);
    }
    entry->ref_count = 1;
    record.font_cache_key = cache_key;
    auto *font = entry->chain.front();
    impl.font_cache.emplace(cache_key, std::move(*entry));
    log_font_info_once(
        "Created font cache: node='" + record.absolute_path + "', font_id='" + style.resolved_font.font_id +
        "', primary_src='" + style.resolved_font.primary_src + "', size=" + std::to_string(font_size) +
        ", fallback_count=" + std::to_string(style.resolved_font.fallback_srcs.size())
    );
    return font;
#else
    log_font_warning_once(
        "Font asset '" + style.resolved_font.font_id + "' requires FreeType support, fallback to built-in Montserrat"
    );
    return get_builtin_font(font_size);
#endif
}

static std::optional<lv_grad_dir_t> parse_gradient_direction(std::string_view direction);

static void apply_style_colors(Record &record, const ResolvedStyle &style)
{
    lv_style_set_bg_opa(&record.style, LV_OPA_TRANSP);
    auto bg_color = parse_color(style.style.bg_color.value_or(""));
    if (bg_color.has_value()) {
        lv_style_set_bg_color(&record.style, lv_color_hex(*bg_color));
        lv_style_set_bg_opa(&record.style, LV_OPA_COVER);
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_BG_COLOR);
    }

    auto bg_gradient_color = parse_color(style.style.bg_gradient_color.value_or(""));
    if (bg_gradient_color.has_value()) {
        lv_style_set_bg_grad_color(&record.style, lv_color_hex(*bg_gradient_color));
        lv_style_set_bg_grad_opa(
            &record.style,
            static_cast<lv_opa_t>(style.style.bg_gradient_opacity.value_or(255))
        );
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_BG_GRAD_COLOR);
        lv_style_remove_prop(&record.style, LV_STYLE_BG_GRAD_OPA);
    }
    if (style.style.bg_gradient_direction.has_value()) {
        auto direction = parse_gradient_direction(*style.style.bg_gradient_direction);
        if (direction.has_value()) {
            lv_style_set_bg_grad_dir(&record.style, *direction);
        }
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_BG_GRAD_DIR);
    }
    if (style.style.bg_main_stop.has_value()) {
        lv_style_set_bg_main_stop(&record.style, *style.style.bg_main_stop);
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_BG_MAIN_STOP);
    }
    if (style.style.bg_gradient_stop.has_value()) {
        lv_style_set_bg_grad_stop(&record.style, *style.style.bg_gradient_stop);
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_BG_GRAD_STOP);
    }

    auto text_color = parse_color(style.style.text_color.value_or(""));
    if (text_color.has_value()) {
        lv_style_set_text_color(&record.style, lv_color_hex(*text_color));
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_TEXT_COLOR);
    }

    auto border_color = parse_color(style.style.border_color.value_or(""));
    if (border_color.has_value()) {
        lv_style_set_border_color(&record.style, lv_color_hex(*border_color));
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_BORDER_COLOR);
    }

    auto line_color = parse_color(style.style.line_color.value_or(""));
    if (line_color.has_value()) {
        lv_style_set_line_color(&record.style, lv_color_hex(*line_color));
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_LINE_COLOR);
    }

    auto arc_color = parse_color(style.style.arc_color.value_or(""));
    if (arc_color.has_value()) {
        lv_style_set_arc_color(&record.style, lv_color_hex(*arc_color));
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_ARC_COLOR);
    }

    auto shadow_color = parse_color(style.style.shadow_color.value_or(""));
    if (shadow_color.has_value()) {
        lv_style_set_shadow_color(&record.style, lv_color_hex(*shadow_color));
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_SHADOW_COLOR);
    }
}

static void apply_style_border(Record &record, const ResolvedStyle &style)
{
    lv_style_set_border_width(&record.style, style.style.border_width.value_or(0));
}

static void apply_style_radius(Record &record, const ResolvedStyle &style)
{
    lv_style_set_radius(&record.style, style.style.radius.value_or(0));
    lv_style_set_clip_corner(&record.style, style.style.clip_corner.value_or(false));
}

static void apply_style_padding(Record &record, const ResolvedStyle &style)
{
    const auto padding_default = style.style.padding.value_or(0);
    lv_style_set_pad_left(&record.style, style.style.padding_left.value_or(padding_default));
    lv_style_set_pad_right(&record.style, style.style.padding_right.value_or(padding_default));
    lv_style_set_pad_top(&record.style, style.style.padding_top.value_or(padding_default));
    lv_style_set_pad_bottom(&record.style, style.style.padding_bottom.value_or(padding_default));
}

static void apply_style_margin(Record &record, const ResolvedStyle &style)
{
    const auto margin_default = style.style.margin.value_or(0);
    lv_style_set_margin_left(&record.style, style.style.margin_left.value_or(margin_default));
    lv_style_set_margin_right(&record.style, style.style.margin_right.value_or(margin_default));
    lv_style_set_margin_top(&record.style, style.style.margin_top.value_or(margin_default));
    lv_style_set_margin_bottom(&record.style, style.style.margin_bottom.value_or(margin_default));
}

static void apply_style_shadow(Record &record, const ResolvedStyle &style)
{
    lv_style_set_shadow_width(&record.style, style.style.shadow_width.value_or(0));
    lv_style_set_shadow_offset_x(&record.style, style.style.shadow_offset_x.value_or(0));
    lv_style_set_shadow_offset_y(&record.style, style.style.shadow_offset_y.value_or(0));
}

static void apply_style_opacity(Record &record, const ResolvedStyle &style)
{
    lv_style_set_opa(&record.style, static_cast<lv_opa_t>(style.style.opacity.value_or(255)));
}

static void apply_style_line(Record &record, const ResolvedStyle &style)
{
    lv_style_set_line_width(&record.style, style.style.line_width.value_or(0));
    if (style.style.arc_width.has_value()) {
        lv_style_set_arc_width(&record.style, *style.style.arc_width);
    }
    if (style.style.arc_opacity.has_value()) {
        lv_style_set_arc_opa(&record.style, static_cast<lv_opa_t>(*style.style.arc_opacity));
    }
    if (style.style.arc_rounded.has_value()) {
        lv_style_set_arc_rounded(&record.style, *style.style.arc_rounded);
    }
}

static void apply_style_image_opacity(Record &record, const ResolvedStyle &style)
{
    lv_style_set_image_opa(&record.style, static_cast<lv_opa_t>(style.style.image_opacity.value_or(255)));
}

static void apply_style_image_recolor(Record &record, const ResolvedStyle &style)
{
    const auto recolor = parse_color(style.style.image_recolor.value_or(""));
    if (recolor.has_value()) {
        lv_style_set_image_recolor(&record.style, lv_color_hex(*recolor));
        lv_style_set_image_recolor_opa(
            &record.style,
            static_cast<lv_opa_t>(style.style.image_recolor_opacity.value_or(255))
        );
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_IMAGE_RECOLOR);
        lv_style_remove_prop(&record.style, LV_STYLE_IMAGE_RECOLOR_OPA);
    }
}

static void apply_style_font(BackendImpl &impl, Record &record, const ResolvedStyle &style)
{
    if (style.style.text_align.has_value()) {
        switch (*style.style.text_align) {
        case TextAlign::Auto:
            lv_style_set_text_align(&record.style, LV_TEXT_ALIGN_AUTO);
            break;
        case TextAlign::Left:
            lv_style_set_text_align(&record.style, LV_TEXT_ALIGN_LEFT);
            break;
        case TextAlign::Center:
            lv_style_set_text_align(&record.style, LV_TEXT_ALIGN_CENTER);
            break;
        case TextAlign::Right:
            lv_style_set_text_align(&record.style, LV_TEXT_ALIGN_RIGHT);
            break;
        case TextAlign::Max:
            break;
        }
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_TEXT_ALIGN);
    }

    if (node_type_uses_text_font(record.type) || style.style.font_size.has_value() ||
            style.style.image_font_size.has_value() ||
            (style.style.font.has_value() && !style.style.font->empty()) ||
            !style.resolved_font.primary_src.empty() || style.resolved_font.kind == "imageFont") {
        auto *font = get_font(impl, record, style);
        lv_style_set_text_font(&record.style, font);
        BROOKESIA_LOGD(
            "Applied text font: node='%1%', requested_font_id='%2%', resolved_font_id='%3%', primary_src='%4%', "
            "font_size=%5%, cache_key='%6%', lv_font=%7%",
            record.absolute_path,
            style.style.font.value_or(""),
            style.resolved_font.font_id,
            style.resolved_font.primary_src,
            style.style.font_size.value_or(16),
            record.font_cache_key,
            static_cast<const void *>(font)
        );
    } else {
        release_font(impl, record);
    }
}

static void apply_optional_color(lv_style_t &lv_style, const std::optional<std::string> &color, lv_style_prop_t prop)
{
    if (!color.has_value()) {
        return;
    }
    auto parsed = parse_color(*color);
    if (!parsed.has_value()) {
        lv_style_remove_prop(&lv_style, prop);
        return;
    }
    const auto lv_color = lv_color_hex(*parsed);
    switch (prop) {
    case LV_STYLE_BG_COLOR:
        lv_style_set_bg_color(&lv_style, lv_color);
        lv_style_set_bg_opa(&lv_style, LV_OPA_COVER);
        break;
    case LV_STYLE_BG_GRAD_COLOR:
        lv_style_set_bg_grad_color(&lv_style, lv_color);
        break;
    case LV_STYLE_TEXT_COLOR:
        lv_style_set_text_color(&lv_style, lv_color);
        break;
    case LV_STYLE_BORDER_COLOR:
        lv_style_set_border_color(&lv_style, lv_color);
        break;
    case LV_STYLE_LINE_COLOR:
        lv_style_set_line_color(&lv_style, lv_color);
        break;
    case LV_STYLE_ARC_COLOR:
        lv_style_set_arc_color(&lv_style, lv_color);
        break;
    case LV_STYLE_SHADOW_COLOR:
        lv_style_set_shadow_color(&lv_style, lv_color);
        break;
    default:
        break;
    }
}

static std::optional<lv_grad_dir_t> parse_gradient_direction(std::string_view direction)
{
    if (direction == "none") {
        return LV_GRAD_DIR_NONE;
    }
    if (direction == "horizontal") {
        return LV_GRAD_DIR_HOR;
    }
    if (direction == "vertical") {
        return LV_GRAD_DIR_VER;
    }
    return std::nullopt;
}

static void apply_state_style_fields(lv_style_t &lv_style, const Style &style)
{
    apply_optional_color(lv_style, style.bg_color, LV_STYLE_BG_COLOR);
    apply_optional_color(lv_style, style.bg_gradient_color, LV_STYLE_BG_GRAD_COLOR);
    apply_optional_color(lv_style, style.text_color, LV_STYLE_TEXT_COLOR);
    apply_optional_color(lv_style, style.border_color, LV_STYLE_BORDER_COLOR);
    apply_optional_color(lv_style, style.line_color, LV_STYLE_LINE_COLOR);
    apply_optional_color(lv_style, style.arc_color, LV_STYLE_ARC_COLOR);
    apply_optional_color(lv_style, style.shadow_color, LV_STYLE_SHADOW_COLOR);

    if (style.bg_gradient_direction.has_value()) {
        auto direction = parse_gradient_direction(*style.bg_gradient_direction);
        if (direction.has_value()) {
            lv_style_set_bg_grad_dir(&lv_style, *direction);
        }
    }
    if (style.bg_main_stop.has_value()) {
        lv_style_set_bg_main_stop(&lv_style, *style.bg_main_stop);
    }
    if (style.bg_gradient_stop.has_value()) {
        lv_style_set_bg_grad_stop(&lv_style, *style.bg_gradient_stop);
    }
    if (style.bg_gradient_opacity.has_value()) {
        lv_style_set_bg_grad_opa(&lv_style, static_cast<lv_opa_t>(*style.bg_gradient_opacity));
    }

    if (style.border_width.has_value()) {
        lv_style_set_border_width(&lv_style, *style.border_width);
    }
    if (style.radius.has_value()) {
        lv_style_set_radius(&lv_style, *style.radius);
    }
    if (style.padding.has_value()) {
        lv_style_set_pad_all(&lv_style, *style.padding);
    }
    if (style.padding_left.has_value()) {
        lv_style_set_pad_left(&lv_style, *style.padding_left);
    }
    if (style.padding_right.has_value()) {
        lv_style_set_pad_right(&lv_style, *style.padding_right);
    }
    if (style.padding_top.has_value()) {
        lv_style_set_pad_top(&lv_style, *style.padding_top);
    }
    if (style.padding_bottom.has_value()) {
        lv_style_set_pad_bottom(&lv_style, *style.padding_bottom);
    }
    if (style.margin.has_value()) {
        lv_style_set_margin_all(&lv_style, *style.margin);
    }
    if (style.margin_left.has_value()) {
        lv_style_set_margin_left(&lv_style, *style.margin_left);
    }
    if (style.margin_right.has_value()) {
        lv_style_set_margin_right(&lv_style, *style.margin_right);
    }
    if (style.margin_top.has_value()) {
        lv_style_set_margin_top(&lv_style, *style.margin_top);
    }
    if (style.margin_bottom.has_value()) {
        lv_style_set_margin_bottom(&lv_style, *style.margin_bottom);
    }
    if (style.shadow_width.has_value()) {
        lv_style_set_shadow_width(&lv_style, *style.shadow_width);
    }
    if (style.shadow_offset_x.has_value()) {
        lv_style_set_shadow_offset_x(&lv_style, *style.shadow_offset_x);
    }
    if (style.shadow_offset_y.has_value()) {
        lv_style_set_shadow_offset_y(&lv_style, *style.shadow_offset_y);
    }
    if (style.opacity.has_value()) {
        lv_style_set_opa(&lv_style, static_cast<lv_opa_t>(*style.opacity));
    }
    if (style.line_width.has_value()) {
        lv_style_set_line_width(&lv_style, *style.line_width);
    }
    if (style.image_opacity.has_value()) {
        lv_style_set_image_opa(&lv_style, static_cast<lv_opa_t>(*style.image_opacity));
    }
    if (style.image_recolor.has_value()) {
        auto recolor = parse_color(*style.image_recolor);
        if (recolor.has_value()) {
            lv_style_set_image_recolor(&lv_style, lv_color_hex(*recolor));
            lv_style_set_image_recolor_opa(
                &lv_style,
                static_cast<lv_opa_t>(style.image_recolor_opacity.value_or(255))
            );
        }
    }
    if (style.text_align.has_value()) {
        switch (*style.text_align) {
        case TextAlign::Auto:
            lv_style_set_text_align(&lv_style, LV_TEXT_ALIGN_AUTO);
            break;
        case TextAlign::Left:
            lv_style_set_text_align(&lv_style, LV_TEXT_ALIGN_LEFT);
            break;
        case TextAlign::Center:
            lv_style_set_text_align(&lv_style, LV_TEXT_ALIGN_CENTER);
            break;
        case TextAlign::Right:
            lv_style_set_text_align(&lv_style, LV_TEXT_ALIGN_RIGHT);
            break;
        case TextAlign::Max:
            break;
        }
    }
    if (style.arc_width.has_value()) {
        lv_style_set_arc_width(&lv_style, *style.arc_width);
    }
    if (style.arc_opacity.has_value()) {
        lv_style_set_arc_opa(&lv_style, static_cast<lv_opa_t>(*style.arc_opacity));
    }
    if (style.arc_rounded.has_value()) {
        lv_style_set_arc_rounded(&lv_style, *style.arc_rounded);
    }
    if (style.clip_corner.has_value()) {
        lv_style_set_clip_corner(&lv_style, *style.clip_corner);
    }
}

static void reset_state_styles(Record &record)
{
    for (auto &[unused_state, state_style] : record.state_styles) {
        (void)unused_state;
        if (record.object != nullptr) {
            lv_obj_remove_style(record.object, &state_style.style, state_style.selector);
        }
        lv_style_reset(&state_style.style);
    }
    record.state_styles.clear();
}

static void reset_part_styles(Record &record)
{
    for (auto &[unused_part, part_style] : record.part_styles) {
        (void)unused_part;
        if (record.object != nullptr) {
            lv_obj_remove_style(record.object, &part_style.style, part_style.selector);
        }
        lv_style_reset(&part_style.style);
        for (auto &[unused_state, state_style] : part_style.state_styles) {
            (void)unused_state;
            if (record.object != nullptr) {
                lv_obj_remove_style(record.object, &state_style.style, state_style.selector);
            }
            lv_style_reset(&state_style.style);
        }
    }
    record.part_styles.clear();
    record.arc_gradients.clear();
}

static void apply_state_styles(Record &record, const ResolvedStyle &style)
{
    reset_state_styles(record);
    for (const auto &[state_name, state_style] : style.state_styles) {
        auto selector = style_state_selector(state_name);
        if (!selector.has_value()) {
            BROOKESIA_LOGW("Skipping unsupported style state '%1%' for node '%2%'", state_name, record.absolute_path);
            continue;
        }
        auto &entry = record.state_styles[state_name];
        entry.selector = *selector;
        lv_style_init(&entry.style);
        apply_state_style_fields(entry.style, state_style);
        lv_obj_add_style(record.object, &entry.style, entry.selector);
    }
}

static void apply_part_styles(Record &record, const ResolvedStyle &style)
{
    reset_part_styles(record);
    for (const auto &[part_name, part_style] : style.part_styles) {
        auto part_selector = style_part_selector(part_name);
        if (!part_selector.has_value() || *part_selector == LV_PART_MAIN) {
            BROOKESIA_LOGW("Skipping unsupported style part '%1%' for node '%2%'", part_name, record.absolute_path);
            continue;
        }

        auto &entry = record.part_styles[part_name];
        entry.selector = *part_selector;
        lv_style_init(&entry.style);
        apply_state_style_fields(entry.style, part_style.style);
        lv_obj_add_style(record.object, &entry.style, entry.selector);

        for (const auto &[state_name, state_style] : part_style.state_styles) {
            auto selector = style_state_selector(state_name, *part_selector);
            if (!selector.has_value()) {
                BROOKESIA_LOGW(
                    "Skipping unsupported style state '%1%' for part '%2%' on node '%3%'",
                    state_name,
                    part_name,
                    record.absolute_path
                );
                continue;
            }
            auto &state_entry = entry.state_styles[state_name];
            state_entry.selector = *selector;
            lv_style_init(&state_entry.style);
            apply_state_style_fields(state_entry.style, state_style);
            lv_obj_add_style(record.object, &state_entry.style, state_entry.selector);
        }
    }
}

static lv_color_t mix_color(uint32_t start_color, uint32_t end_color, int32_t position, int32_t max_position)
{
    if (max_position <= 0) {
        return lv_color_hex(start_color);
    }
    const auto mix_channel = [position, max_position](uint32_t start, uint32_t end) -> uint32_t {
        return (start * static_cast<uint32_t>(max_position - position) +
                end * static_cast<uint32_t>(position)) /
        static_cast<uint32_t>(max_position);
    };
    const auto sr = (start_color >> 16) & 0xff;
    const auto sg = (start_color >> 8) & 0xff;
    const auto sb = start_color & 0xff;
    const auto er = (end_color >> 16) & 0xff;
    const auto eg = (end_color >> 8) & 0xff;
    const auto eb = end_color & 0xff;
    return lv_color_hex((mix_channel(sr, er) << 16) | (mix_channel(sg, eg) << 8) | mix_channel(sb, eb));
}

static void draw_arc_gradient_segmented(
    const lv_draw_arc_dsc_t &source,
    const Record::ArcGradientRecord &gradient)
{
    auto span = static_cast<int32_t>(source.end_angle - source.start_angle);
    if (span < 0) {
        span += 360;
    }
    if (span <= 0) {
        return;
    }

    const auto segments = std::clamp<int32_t>(gradient.segments, 2, 128);
    for (int32_t index = 0; index < segments; ++index) {
        auto segment = source;
        const auto start_offset = (span * index) / segments;
        const auto end_offset = (span * (index + 1)) / segments;
        segment.start_angle = source.start_angle + start_offset;
        segment.end_angle = source.start_angle + end_offset;
        segment.color = mix_color(gradient.start_color, gradient.end_color, index, segments - 1);
        lv_draw_arc(source.base.layer, &segment);
    }
}

static void on_arc_gradient_draw_task_added(lv_event_t *event)
{
    auto *context = static_cast<ArcGradientContext *>(lv_event_get_user_data(event));
    if (context == nullptr || context->impl == nullptr) {
        return;
    }
    auto *record = context->impl->find_record(context->handle);
    if (record == nullptr || record->object == nullptr || record->type != NodeType::Arc) {
        return;
    }

    auto *task = lv_event_get_draw_task(event);
    if (task == nullptr || lv_draw_task_get_type(task) != LV_DRAW_TASK_TYPE_ARC) {
        return;
    }
    auto *arc = lv_draw_task_get_arc_dsc(task);
    if (arc == nullptr || arc->base.obj != record->object) {
        return;
    }

    const char *part_name = nullptr;
    if (arc->base.part == LV_PART_MAIN) {
        part_name = "main";
    } else if (arc->base.part == LV_PART_INDICATOR) {
        part_name = "indicator";
    } else {
        return;
    }

    auto gradient_it = record->arc_gradients.find(part_name);
    if (gradient_it == record->arc_gradients.end() || !gradient_it->second.enabled) {
        return;
    }

    const auto source = *arc;
    arc->opa = LV_OPA_TRANSP;
    draw_arc_gradient_segmented(source, gradient_it->second);
}

static std::optional<Record::ArcGradientRecord> make_arc_gradient_record(const Style &style)
{
    if (!style.arc_gradient_color.has_value()) {
        return std::nullopt;
    }
    auto end_color = parse_color(*style.arc_gradient_color);
    if (!end_color.has_value()) {
        return std::nullopt;
    }

    std::optional<uint32_t> start_color;
    if (style.arc_color.has_value()) {
        start_color = parse_color(*style.arc_color);
    }
    if (!start_color.has_value() && style.line_color.has_value()) {
        start_color = parse_color(*style.line_color);
    }
    if (!start_color.has_value() && style.bg_color.has_value()) {
        start_color = parse_color(*style.bg_color);
    }
    if (!start_color.has_value()) {
        return std::nullopt;
    }

    return Record::ArcGradientRecord{
        .enabled = true,
        .start_color = *start_color,
        .end_color = *end_color,
        .segments = style.arc_gradient_segments.value_or(32),
    };
}

static void apply_arc_gradient(BackendImpl &impl, Record &record, const ResolvedStyle &style)
{
    record.arc_gradients.clear();
    if (auto gradient = make_arc_gradient_record(style.style); gradient.has_value()) {
        record.arc_gradients.emplace("main", *gradient);
    }
    if (auto part_it = style.part_styles.find("indicator"); part_it != style.part_styles.end()) {
        if (auto gradient = make_arc_gradient_record(part_it->second.style); gradient.has_value()) {
            record.arc_gradients.emplace("indicator", *gradient);
        }
    }

    const bool has_gradient = !record.arc_gradients.empty();
    if (has_gradient && !record.arc_gradient_event_registered) {
        record.arc_gradient_context = std::make_unique<ArcGradientContext>(ArcGradientContext{
            .impl = &impl,
            .handle = record.handle,
        });
        lv_obj_add_event_cb(
            record.object,
            on_arc_gradient_draw_task_added,
            LV_EVENT_DRAW_TASK_ADDED,
            record.arc_gradient_context.get()
        );
        record.arc_gradient_event_registered = true;
    }

    if (has_gradient) {
        lv_obj_add_flag(record.object, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    } else {
        lv_obj_remove_flag(record.object, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    }
}

void apply_style(BackendImpl &impl, Record &record, const ResolvedStyle &style, StyleApplyMask mask)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: record(%1%), style(%2%), mask(%3%)", record, style, static_cast<uint32_t>(mask));

    const bool full_apply = mask == StyleApplyMask::All || !record.style_initialized;
    const auto effective_mask = full_apply ? StyleApplyMask::All : mask;
    if (full_apply && record.style_initialized) {
        reset_state_styles(record);
        reset_part_styles(record);
        lv_obj_remove_style(record.object, &record.style, LV_PART_MAIN);
        lv_style_reset(&record.style);
        record.style_initialized = false;
    }

    if (!record.style_initialized) {
        lv_style_init(&record.style);
        record.style_initialized = true;
    }

    if (has_mask(effective_mask, StyleApplyMask::Color)) {
        apply_style_colors(record, style);
    }
    if (has_mask(effective_mask, StyleApplyMask::Border)) {
        apply_style_border(record, style);
    }
    if (has_mask(effective_mask, StyleApplyMask::Radius)) {
        apply_style_radius(record, style);
    }
    if (has_mask(effective_mask, StyleApplyMask::Padding)) {
        apply_style_padding(record, style);
    }
    if (has_mask(effective_mask, StyleApplyMask::Margin)) {
        apply_style_margin(record, style);
    }
    if (has_mask(effective_mask, StyleApplyMask::Shadow)) {
        apply_style_shadow(record, style);
    }
    if (has_mask(effective_mask, StyleApplyMask::Opacity)) {
        apply_style_opacity(record, style);
    }
    if (has_mask(effective_mask, StyleApplyMask::Line)) {
        apply_style_line(record, style);
    }
    if (has_mask(effective_mask, StyleApplyMask::ImageOpacity)) {
        apply_style_image_opacity(record, style);
    }
    if (has_mask(effective_mask, StyleApplyMask::ImageRecolor)) {
        apply_style_image_recolor(record, style);
    }
    if (has_mask(effective_mask, StyleApplyMask::Font)) {
        apply_style_font(impl, record, style);
    }

    if (full_apply) {
        lv_obj_add_style(record.object, &record.style, LV_PART_MAIN);
        apply_state_styles(record, style);
        apply_part_styles(record, style);
        if (record.type == NodeType::Arc) {
            apply_arc_gradient(impl, record, style);
        }
    } else {
        apply_state_styles(record, style);
        apply_part_styles(record, style);
        if (record.type == NodeType::Arc) {
            apply_arc_gradient(impl, record, style);
        }
        lv_obj_refresh_style(record.object, LV_PART_ANY, LV_STYLE_PROP_ANY);
    }

    refresh_text_input_inner_layout(record);
}

void apply_debug_visual(Record &record, bool enabled)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: record(%1%), enabled(%2%)", record, enabled);

    if (record.debug_style_initialized == enabled) {
        return;
    }

    if (record.debug_style_initialized) {
        lv_obj_remove_style(record.object, &record.debug_style, LV_PART_MAIN);
        lv_style_reset(&record.debug_style);
        record.debug_style_initialized = false;
    }

    if (!enabled) {
        return;
    }

    lv_style_init(&record.debug_style);
    record.debug_style_initialized = true;

    lv_style_set_outline_width(&record.debug_style, DEBUG_OUTLINE_WIDTH);
    lv_style_set_outline_pad(&record.debug_style, DEBUG_OUTLINE_PAD);
    lv_style_set_outline_opa(&record.debug_style, DEBUG_OUTLINE_OPACITY);
    lv_style_set_outline_color(&record.debug_style, get_debug_outline_color(record.depth));

    lv_obj_add_style(record.object, &record.debug_style, LV_PART_MAIN);
}

} // namespace esp_brookesia::gui::lvgl
