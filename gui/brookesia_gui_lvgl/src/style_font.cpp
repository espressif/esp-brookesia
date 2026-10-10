/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/style_impl.hpp"

#include <chrono>

namespace esp_brookesia::gui::lvgl::style_detail {

lv_color_t get_debug_outline_color(uint32_t depth)
{
    constexpr std::array<uint32_t, 4> palette = {
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

    const auto read_start = std::chrono::steady_clock::now();
    auto source = std::make_shared<FontCacheEntry::FontSource>();
    source->data.resize(static_cast<size_t>(info->size));
    const auto alloc_done = std::chrono::steady_clock::now();
    auto read_result = StorageHelper::fs_read(
                           font_path_string,
                           service::RawBuffer(source->data.data(), source->data.size())
                       );
    const auto read_done = std::chrono::steady_clock::now();
    BROOKESIA_LOGI(
        "font load profile: path(%1%), bytes(%2%), alloc_ms(%3%), read_ms(%4%)",
        font_path_string,
        static_cast<int64_t>(info->size),
        std::chrono::duration_cast<std::chrono::milliseconds>(alloc_done - read_start).count(),
        std::chrono::duration_cast<std::chrono::milliseconds>(read_done - alloc_done).count());
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
    std::unordered_set<std::string> logged_messages;

    const auto [_, inserted] = logged_messages.emplace(message);
    if (inserted) {
        BROOKESIA_LOGW("%1%", message);
    }
}

void log_font_info_once(std::string_view message)
{
    std::unordered_set<std::string> logged_messages;

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

std::string make_font_cache_key(const ResolvedFontSpec &font_spec, int32_t font_size, int32_t image_font_size)
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

}
