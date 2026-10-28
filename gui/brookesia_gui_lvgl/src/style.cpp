/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/style_impl.hpp"

namespace esp_brookesia::gui::lvgl {
using namespace style_detail;


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

std::optional<uint32_t> style_state_selector(std::string_view state, uint32_t part)
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

namespace {

void destroy_font_cache_entry(FontCacheEntry &entry)
{
    for (size_t index = entry.chain.size(); index > 0; --index) {
        const auto font_index = index - 1;
        const auto font_kind = font_index < entry.font_kinds.size() ?
                               entry.font_kinds[font_index] :
                               FontCacheEntry::FontKind::FreeType;
        auto *font = entry.chain[font_index];
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
        auto *font_handle = font_index < entry.platform_font_handles.size() ?
                            static_cast<esp_lv_adapter_ft_font_handle_t>(entry.platform_font_handles[font_index]) :
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
}

} // namespace

void release_font(BackendImpl &impl, Record &record)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: record(%1%)", record);

    auto *entry = std::exchange(record.font_cache_entry, nullptr);
    if (entry == nullptr) {
        return;
    }

    auto cache_it = impl.font_cache.find(entry->cache_key);
    if (cache_it == impl.font_cache.end() || &cache_it->second != entry) {
        BROOKESIA_LOGE("Font cache entry invariant violated: entry(%1%)", static_cast<const void *>(entry));
        return;
    }

    if (cache_it->second.ref_count > 0) {
        --cache_it->second.ref_count;
    }

    if (cache_it->second.ref_count == 0) {
        destroy_font_cache_entry(cache_it->second);
        impl.font_cache.erase(cache_it);
    }
}

const lv_font_t *get_font(BackendImpl &impl, Record &record, const ResolvedStyle &style)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: record(%1%), style(%2%)", record, style);

    release_font(impl, record);
    const auto absolute_path = impl.build_absolute_path(record);

    const auto font_size = style.style.font_size.value_or(16);
    const auto image_font_size = style.style.image_font_size.value_or(0);
    if (!style.resolved_font.native_fonts.empty()) {
        const auto *native_variant = select_native_font_variant(style.resolved_font, font_size);
        if (native_variant != nullptr) {
            auto *font = reinterpret_cast<const lv_font_t *>(native_variant->native_src);
            if (native_variant->native_size != font_size) {
                log_font_warning_once(
                    "Native font size mismatch: node='" + absolute_path + "', font_id='" +
                    style.resolved_font.font_id + "', requested_size=" + std::to_string(font_size) +
                    ", selected_size=" + std::to_string(native_variant->native_size)
                );
            }
            log_font_info_once(
                "Using native LVGL font: node='" + absolute_path + "', font_id='" +
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
            record.font_cache_entry = &cache_it->second;
            log_font_info_once(
                "Reusing imageFont cache: node='" + absolute_path + "', font_id='" +
                style.resolved_font.font_id + "', size=" + std::to_string(font_size)
            );
            return cache_it->second.chain.front();
        }

        std::unique_ptr<FontCacheEntry> entry(
            create_image_font_cache_entry(impl, style.resolved_font, font_size, image_font_size)
        );
        if (entry == nullptr || entry->chain.empty()) {
            log_font_warning_once(
                "Failed to resolve imageFont for node '" + absolute_path + "', font_id='" +
                style.resolved_font.font_id + "', fallback to built-in Montserrat"
            );
            return get_builtin_font(font_size);
        }

        entry->ref_count = 1;
        auto [insert_it, inserted] = impl.font_cache.try_emplace(cache_key, std::move(*entry));
        if (!inserted) {
            destroy_font_cache_entry(*entry);
            ++insert_it->second.ref_count;
        }
        record.font_cache_entry = &insert_it->second;
        auto *font = insert_it->second.chain.front();
        log_font_info_once(
            "Created imageFont cache: node='" + absolute_path + "', font_id='" +
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
            "Using built-in LVGL font: node='" + absolute_path + "', requested_font_id='" +
            style.style.font.value_or("") + "', size=" + std::to_string(font_size)
        );
        return get_builtin_font(font_size);
    }

#if LV_USE_FREETYPE
    const auto cache_key = make_font_cache_key(style.resolved_font, font_size);
    auto cache_it = impl.font_cache.find(cache_key);
    if (cache_it != impl.font_cache.end()) {
        ++cache_it->second.ref_count;
        record.font_cache_entry = &cache_it->second;
        log_font_info_once(
            "Reusing font cache: node='" + absolute_path + "', font_id='" + style.resolved_font.font_id +
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
            "Failed to resolve FreeType font for node '" + absolute_path + "', font_id='" +
            style.resolved_font.font_id + "', fallback to built-in Montserrat"
        );
        return get_builtin_font(font_size);
    }
    entry->ref_count = 1;
    auto [insert_it, inserted] = impl.font_cache.try_emplace(cache_key, std::move(*entry));
    if (!inserted) {
        destroy_font_cache_entry(*entry);
        ++insert_it->second.ref_count;
    }
    record.font_cache_entry = &insert_it->second;
    auto *font = insert_it->second.chain.front();
    log_font_info_once(
        "Created font cache: node='" + absolute_path + "', font_id='" + style.resolved_font.font_id +
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
        apply_state_styles(impl, record, style);
        apply_part_styles(impl, record, style);
        if (record.type == NodeType::Arc) {
            apply_arc_gradient(impl, record, style);
        }
    } else {
        // A partial mask only updates fields in the base style. Nested state/part bindings and Arc
        // gradients use StyleApplyMask::All, so rebuilding their LVGL styles here is both redundant
        // and expensive during high-frequency opacity/color binding updates.
        lv_obj_refresh_style(record.object, LV_PART_ANY, LV_STYLE_PROP_ANY);
    }

    refresh_text_input_inner_layout(record);
}

void apply_debug_visual(Record &record, bool enabled)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: record(%1%), enabled(%2%)", record, enabled);

    if ((record.debug_style_payload != nullptr) == enabled) {
        return;
    }

    if (record.debug_style_payload != nullptr) {
        lv_obj_remove_style(record.object, &record.debug_style_payload->style, LV_PART_MAIN);
        lv_style_reset(&record.debug_style_payload->style);
        record.debug_style_payload.reset();
    }

    if (!enabled) {
        return;
    }

    record.debug_style_payload = std::make_unique<Record::DebugStylePayload>();
    auto &debug_style = record.debug_style_payload->style;
    lv_style_init(&debug_style);

    lv_style_set_outline_width(&debug_style, DEBUG_OUTLINE_WIDTH);
    lv_style_set_outline_pad(&debug_style, DEBUG_OUTLINE_PAD);
    lv_style_set_outline_opa(&debug_style, DEBUG_OUTLINE_OPACITY);
    lv_style_set_outline_color(&debug_style, get_debug_outline_color(record.depth));

    lv_obj_add_style(record.object, &debug_style, LV_PART_MAIN);
}

} // namespace esp_brookesia::gui::lvgl
