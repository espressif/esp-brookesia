/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/types.hpp"
#include "brookesia/gui_lvgl/macro_configs.h"
#if !BROOKESIA_GUI_LVGL_LAYOUT_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>

namespace esp_brookesia::gui::lvgl {

namespace {

static lv_coord_t to_lvgl_size(const Dimension &dimension)
{
    switch (dimension.mode) {
    case SizeMode::Fixed:
        return dimension.value;
    case SizeMode::Match:
        return lv_pct(100);
    case SizeMode::Percent:
        return lv_pct(dimension.value);
    case SizeMode::Wrap:
    case SizeMode::Max:
    default:
        return LV_SIZE_CONTENT;
    }
}

static int32_t to_lvgl_grid_track(const Dimension &dimension)
{
    switch (dimension.mode) {
    case SizeMode::Fixed:
        return dimension.value;
    case SizeMode::Match:
        return LV_GRID_FR(1);
    case SizeMode::Percent:
        return dimension.value <= 0 ? 0 : LV_GRID_FR(dimension.value);
    case SizeMode::Wrap:
    case SizeMode::Max:
    default:
        return LV_GRID_CONTENT;
    }
}

static std::optional<int32_t> resolve_parent_based_dimension_px(
    const Dimension &dimension, lv_obj_t *parent, bool is_width)
{
    switch (dimension.mode) {
    case SizeMode::Fixed:
        return std::max<int32_t>(0, dimension.value);
    case SizeMode::Match:
    case SizeMode::Percent:
        break;
    case SizeMode::Wrap:
    case SizeMode::Max:
    default:
        return std::nullopt;
    }

    if (parent == nullptr || !lv_obj_is_valid(parent)) {
        return std::nullopt;
    }

    lv_obj_update_layout(parent);
    auto base = is_width ? lv_obj_get_content_width(parent) : lv_obj_get_content_height(parent);
    if (base <= 0) {
        base = is_width ? lv_obj_get_width(parent) : lv_obj_get_height(parent);
    }
    if (base <= 0) {
        return std::nullopt;
    }

    const auto percent = dimension.mode == SizeMode::Match ? 100 : dimension.value;
    return static_cast<int32_t>(std::lround(static_cast<float>(base) * static_cast<float>(percent) / 100.0F));
}

static std::optional<std::pair<int32_t, int32_t>> resolve_aspect_fit_size(
            const Placement &placement, lv_obj_t *parent)
{
    if (!placement.aspect_ratio.has_value() || *placement.aspect_ratio <= 0.0F) {
        return std::nullopt;
    }

    auto resolved_width = resolve_parent_based_dimension_px(placement.width, parent, true);
    auto resolved_height = resolve_parent_based_dimension_px(placement.height, parent, false);
    if (!resolved_width.has_value() || !resolved_height.has_value() ||
            *resolved_width <= 0 || *resolved_height <= 0) {
        return std::nullopt;
    }

    const auto aspect_ratio = *placement.aspect_ratio;
    const auto height_from_width = static_cast<int32_t>(
                                       std::lround(static_cast<float>(*resolved_width) / aspect_ratio)
                                   );
    const auto width_from_height = static_cast<int32_t>(
                                       std::lround(static_cast<float>(*resolved_height) * aspect_ratio)
                                   );
    if (height_from_width <= *resolved_height) {
        return std::make_pair(*resolved_width, std::max<int32_t>(1, height_from_width));
    }
    return std::make_pair(std::max<int32_t>(1, width_from_height), *resolved_height);
}

static int32_t resolve_parent_based_offset_px(const PlacementOffset &offset, lv_obj_t *parent, bool is_x)
{
    switch (offset.mode) {
    case PlacementOffsetMode::Fixed:
        return offset.value;
    case PlacementOffsetMode::Percent:
        break;
    case PlacementOffsetMode::Max:
    default:
        return 0;
    }

    if (parent == nullptr || !lv_obj_is_valid(parent)) {
        return 0;
    }

    lv_obj_update_layout(parent);
    auto base = is_x ? lv_obj_get_content_width(parent) : lv_obj_get_content_height(parent);
    if (base <= 0) {
        base = is_x ? lv_obj_get_width(parent) : lv_obj_get_height(parent);
    }
    if (base <= 0) {
        return 0;
    }
    return static_cast<int32_t>(std::lround(static_cast<float>(base) * static_cast<float>(offset.value) / 100.0F));
}

static std::pair<int32_t, int32_t> resolve_placement_offsets_px(const Record &record, const Placement &placement)
{
    auto *parent = lv_obj_get_parent(record.object);
    return {
        resolve_parent_based_offset_px(placement.x, parent, true),
        resolve_parent_based_offset_px(placement.y, parent, false),
    };
}

static lv_grid_align_t to_lvgl_grid_align(Align align)
{
    switch (align) {
    case Align::Start:
        return LV_GRID_ALIGN_START;
    case Align::Center:
        return LV_GRID_ALIGN_CENTER;
    case Align::End:
        return LV_GRID_ALIGN_END;
    case Align::SpaceBetween:
        return LV_GRID_ALIGN_SPACE_BETWEEN;
    case Align::SpaceAround:
        return LV_GRID_ALIGN_SPACE_AROUND;
    case Align::SpaceEvenly:
        return LV_GRID_ALIGN_SPACE_EVENLY;
    case Align::Stretch:
        return LV_GRID_ALIGN_STRETCH;
    case Align::Max:
    default:
        return LV_GRID_ALIGN_START;
    }
}

static lv_align_t to_lvgl_align(PlacementAlign align)
{
    switch (align) {
    case PlacementAlign::TopLeft:
        return LV_ALIGN_TOP_LEFT;
    case PlacementAlign::TopMid:
        return LV_ALIGN_TOP_MID;
    case PlacementAlign::TopRight:
        return LV_ALIGN_TOP_RIGHT;
    case PlacementAlign::BottomLeft:
        return LV_ALIGN_BOTTOM_LEFT;
    case PlacementAlign::BottomMid:
        return LV_ALIGN_BOTTOM_MID;
    case PlacementAlign::BottomRight:
        return LV_ALIGN_BOTTOM_RIGHT;
    case PlacementAlign::LeftMid:
        return LV_ALIGN_LEFT_MID;
    case PlacementAlign::RightMid:
        return LV_ALIGN_RIGHT_MID;
    case PlacementAlign::Center:
        return LV_ALIGN_CENTER;
    case PlacementAlign::OutTopLeft:
        return LV_ALIGN_OUT_TOP_LEFT;
    case PlacementAlign::OutTopMid:
        return LV_ALIGN_OUT_TOP_MID;
    case PlacementAlign::OutTopRight:
        return LV_ALIGN_OUT_TOP_RIGHT;
    case PlacementAlign::OutBottomLeft:
        return LV_ALIGN_OUT_BOTTOM_LEFT;
    case PlacementAlign::OutBottomMid:
        return LV_ALIGN_OUT_BOTTOM_MID;
    case PlacementAlign::OutBottomRight:
        return LV_ALIGN_OUT_BOTTOM_RIGHT;
    case PlacementAlign::OutLeftTop:
        return LV_ALIGN_OUT_LEFT_TOP;
    case PlacementAlign::OutLeftMid:
        return LV_ALIGN_OUT_LEFT_MID;
    case PlacementAlign::OutLeftBottom:
        return LV_ALIGN_OUT_LEFT_BOTTOM;
    case PlacementAlign::OutRightTop:
        return LV_ALIGN_OUT_RIGHT_TOP;
    case PlacementAlign::OutRightMid:
        return LV_ALIGN_OUT_RIGHT_MID;
    case PlacementAlign::OutRightBottom:
        return LV_ALIGN_OUT_RIGHT_BOTTOM;
    case PlacementAlign::Max:
    default:
        return LV_ALIGN_TOP_LEFT;
    }
}

static std::string view_reference_path_to_absolute(
    std::string_view scope_root_absolute_path,
    std::string_view relative_to)
{
    std::string absolute_path(scope_root_absolute_path);
    if (absolute_path.empty() || absolute_path.back() != '/') {
        absolute_path.push_back('/');
    }
    for (char ch : relative_to) {
        absolute_path.push_back(ch == '.' ? '/' : ch);
    }
    return absolute_path;
}

static bool object_is_valid(const Record &record)
{
    return record.object != nullptr && lv_obj_is_valid(record.object);
}

static void release_placement_cache_entry(BackendImpl &impl, PlacementCacheEntry *entry)
{
    if (entry == nullptr) {
        return;
    }

    auto cache_it = std::find_if(
                        impl.placement_cache.begin(), impl.placement_cache.end(),
    [entry](const auto & candidate) {
        return candidate.get() == entry;
    }
                    );
    if (cache_it == impl.placement_cache.end() || (*cache_it)->ref_count == 0) {
        BROOKESIA_LOGE("Attempted to release an invalid placement cache entry: %1%", static_cast<void *>(entry));
        return;
    }

    --(*cache_it)->ref_count;
    if ((*cache_it)->ref_count == 0) {
        impl.placement_cache.erase(cache_it);
    }
}

static PlacementCacheEntry *replace_record_placement_impl(
    BackendImpl &impl, Record &record, const Placement &placement)
{
    auto *old_entry = record.placement_cache_entry;
    if (old_entry != nullptr && old_entry->value == placement) {
        return old_entry;
    }

    for (auto &entry : impl.placement_cache) {
        if (entry.get() == old_entry || !(entry->value == placement)) {
            continue;
        }
        auto *new_entry = entry.get();
        ++entry->ref_count;
        record.placement_cache_entry = new_entry;
        release_placement_cache_entry(impl, old_entry);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        ++impl.placement_cache_hits;
#endif
        return new_entry;
    }

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    ++impl.placement_cache_misses;
#endif
    // A binding may update x/y every frame. If this Record is the sole owner, preserve the stable
    // cache allocation and replace the value in place instead of creating allocator churn.
    if (old_entry != nullptr && old_entry->ref_count == 1) {
        old_entry->value = placement;
        return old_entry;
    }

    auto new_entry = std::make_unique<PlacementCacheEntry>();
    new_entry->value = placement;
    new_entry->ref_count = 1;
    auto *result = new_entry.get();
    impl.placement_cache.push_back(std::move(new_entry));
    record.placement_cache_entry = result;
    release_placement_cache_entry(impl, old_entry);
    return result;
}

static PlacementCacheEntry *replace_record_placement(
    BackendImpl &impl, Record &record, const Placement &placement)
{
    const bool was_relative = get_record_placement(record).mode == PlacementMode::Relative;
    auto *result = replace_record_placement_impl(impl, record, placement);
    const bool is_relative = get_record_placement(record).mode == PlacementMode::Relative;
    if (was_relative != is_relative) {
        if (is_relative) {
            ++impl.relative_placement_record_count;
        } else if (impl.relative_placement_record_count > 0) {
            --impl.relative_placement_record_count;
        }
    }
    return result;
}

static bool align_relative_record(BackendImpl &impl, Record &record, bool warn_if_missing)
{
    const auto &placement = get_record_placement(record);
    if (!object_is_valid(record) || placement.mode != PlacementMode::Relative || placement.relative_to.empty()) {
        return false;
    }

    const auto target_path = view_reference_path_to_absolute(
                                 impl.build_scope_root_absolute_path(record),
                                 placement.relative_to
                             );
    auto *target = impl.find_record_by_absolute_path(target_path);
    if (target == nullptr || !object_is_valid(*target)) {
        if (warn_if_missing) {
            BROOKESIA_LOGW(
                "Relative placement target not found: node='%1%', relative_to='%2%', target_path='%3%'",
                impl.build_absolute_path(record),
                placement.relative_to,
                target_path
            );
        }
        return false;
    }

    lv_obj_update_layout(target->object);
    lv_obj_update_layout(record.object);
    const auto [offset_x, offset_y] = resolve_placement_offsets_px(record, placement);
    lv_obj_align_to(
        record.object,
        target->object,
        to_lvgl_align(placement.align),
        offset_x,
        offset_y
    );
    return true;
}

static void apply_flex_layout(Record &record, const Layout &layout, LayoutApplyMask mask)
{
    if (has_mask(mask, LayoutApplyMask::FlexFlow)) {
        lv_obj_set_flex_flow(record.object, to_lvgl_flex_flow(layout.flex_flow));
    }
    if (has_mask(mask, LayoutApplyMask::Align)) {
        lv_obj_set_flex_align(
            record.object,
            to_lvgl_flex_align(layout.main_align),
            to_lvgl_flex_align(layout.cross_align),
            to_lvgl_flex_align(layout.cross_align)
        );
    }
    if (has_mask(mask, LayoutApplyMask::Gap)) {
        lv_obj_set_style_pad_gap(record.object, layout.gap, LV_PART_MAIN);
    }
}

static void apply_grid_tracks(Record &record, const Layout &layout)
{
    if (record.grid_payload == nullptr) {
        record.grid_payload = std::make_unique<Record::GridPayload>();
    }
    auto &grid = *record.grid_payload;
    grid.columns.clear();
    grid.rows.clear();
    for (const auto &column : layout.grid_template_columns) {
        grid.columns.push_back(to_lvgl_grid_track(column));
    }
    for (const auto &row : layout.grid_template_rows) {
        grid.rows.push_back(to_lvgl_grid_track(row));
    }
    grid.columns.push_back(LV_GRID_TEMPLATE_LAST);
    grid.rows.push_back(LV_GRID_TEMPLATE_LAST);
    lv_obj_set_grid_dsc_array(record.object, grid.columns.data(), grid.rows.data());
}

static void apply_grid_layout(Record &record, const Layout &layout, LayoutApplyMask mask)
{
    if (has_mask(mask, LayoutApplyMask::GridTracks)) {
        apply_grid_tracks(record, layout);
    }
    if (has_mask(mask, LayoutApplyMask::Align)) {
        lv_obj_set_grid_align(record.object, to_lvgl_grid_align(layout.main_align), to_lvgl_grid_align(layout.cross_align));
    }
    if (has_mask(mask, LayoutApplyMask::Gap)) {
        lv_obj_set_style_pad_gap(record.object, layout.gap, LV_PART_MAIN);
    }
}

static void apply_placement_size(Record &record, const Placement &placement)
{
    lv_obj_set_width(record.object, to_lvgl_size(placement.width));
    lv_obj_set_height(record.object, to_lvgl_size(placement.height));
    if (placement.aspect_ratio.has_value()) {
        auto *parent = lv_obj_get_parent(record.object);
        auto aspect_fit_size = resolve_aspect_fit_size(placement, parent);
        if (aspect_fit_size.has_value()) {
            lv_obj_set_size(record.object, aspect_fit_size->first, aspect_fit_size->second);
            apply_image_sizing(record, placement);
            return;
        }
        if (parent != nullptr && lv_obj_is_valid(parent)) {
            lv_obj_update_layout(parent);
        }
        lv_obj_update_layout(record.object);
        const auto resolved_width = lv_obj_get_width(record.object);
        const auto resolved_height = lv_obj_get_height(record.object);
        const auto aspect_ratio = *placement.aspect_ratio;
        if (resolved_width > 0 && resolved_height > 0 && aspect_ratio > 0.0F) {
            const auto height_from_width = static_cast<int32_t>(
                                               std::lround(static_cast<float>(resolved_width) / aspect_ratio)
                                           );
            const auto width_from_height = static_cast<int32_t>(
                                               std::lround(static_cast<float>(resolved_height) * aspect_ratio)
                                           );
            if (height_from_width <= resolved_height) {
                lv_obj_set_size(record.object, resolved_width, std::max<int32_t>(1, height_from_width));
            } else {
                lv_obj_set_size(record.object, std::max<int32_t>(1, width_from_height), resolved_height);
            }
        }
    }
    apply_image_sizing(record, placement);
}

static void apply_placement_position(BackendImpl &impl, Record &record, const Placement &placement)
{
    const auto [offset_x, offset_y] = resolve_placement_offsets_px(record, placement);
    if (placement.mode == PlacementMode::Absolute) {
        lv_obj_set_pos(record.object, offset_x, offset_y);
        return;
    }
    if (placement.mode != PlacementMode::Relative) {
        return;
    }

    const auto align = to_lvgl_align(placement.align);
    if (placement.relative_to.empty()) {
        lv_obj_align(record.object, align, offset_x, offset_y);
    } else {
        (void)align_relative_record(impl, record, true);
    }
}

static void apply_placement_grid_cell(Record &record, const Placement &placement)
{
    if (!record.parent.is_valid()) {
        return;
    }

    lv_obj_set_grid_cell(
        record.object,
        to_lvgl_grid_align(placement.align_self),
        placement.grid_column,
        std::max<int32_t>(1, placement.grid_column_span),
        to_lvgl_grid_align(placement.align_self),
        placement.grid_row,
        std::max<int32_t>(1, placement.grid_row_span)
    );
}

static void apply_placement_flex_grow(Record &record, const Placement &placement)
{
    lv_obj_set_flex_grow(
        record.object,
        static_cast<uint8_t>(std::clamp<int32_t>(placement.flex_grow, 0, 255))
    );
}

static void update_known_layer_layouts(const BackendImpl &impl)
{
    const std::array<GuiLayer, 4> layers = {
        GuiLayer::Default,
        GuiLayer::Top,
        GuiLayer::System,
        GuiLayer::Bottom,
    };

    for (const auto &display : impl.list_displays()) {
        auto *resolved_display = impl.resolve_display(display.id);
        if (resolved_display == nullptr) {
            continue;
        }
        for (auto layer : layers) {
            auto *object = impl.resolve_layer_parent(resolved_display, layer);
            if (object != nullptr && lv_obj_is_valid(object)) {
                lv_obj_update_layout(object);
            }
        }
    }
    if (impl.staging_root != nullptr && lv_obj_is_valid(impl.staging_root)) {
        lv_obj_update_layout(impl.staging_root);
    }
}

} // namespace

const Placement &get_record_placement(const Record &record)
{
    static const Placement DEFAULT_PLACEMENT;
    return record.placement_cache_entry != nullptr ? record.placement_cache_entry->value : DEFAULT_PLACEMENT;
}

void release_record_placement(BackendImpl &impl, Record &record)
{
    if (get_record_placement(record).mode == PlacementMode::Relative && impl.relative_placement_record_count > 0) {
        --impl.relative_placement_record_count;
    }
    auto *entry = std::exchange(record.placement_cache_entry, nullptr);
    release_placement_cache_entry(impl, entry);
}

lv_flex_flow_t to_lvgl_flex_flow(FlexFlow flow)
{
    switch (flow) {
    case FlexFlow::Row:
        return LV_FLEX_FLOW_ROW;
    case FlexFlow::Column:
        return LV_FLEX_FLOW_COLUMN;
    case FlexFlow::RowWrap:
        return LV_FLEX_FLOW_ROW_WRAP;
    case FlexFlow::ColumnWrap:
        return LV_FLEX_FLOW_COLUMN_WRAP;
    case FlexFlow::Max:
    default:
        return LV_FLEX_FLOW_COLUMN;
    }
}

lv_flex_align_t to_lvgl_flex_align(Align align)
{
    switch (align) {
    case Align::Start:
        return LV_FLEX_ALIGN_START;
    case Align::Center:
        return LV_FLEX_ALIGN_CENTER;
    case Align::End:
        return LV_FLEX_ALIGN_END;
    case Align::SpaceBetween:
        return LV_FLEX_ALIGN_SPACE_BETWEEN;
    case Align::SpaceAround:
        return LV_FLEX_ALIGN_SPACE_AROUND;
    case Align::SpaceEvenly:
        return LV_FLEX_ALIGN_SPACE_EVENLY;
    case Align::Stretch:
    case Align::Max:
    default:
        return LV_FLEX_ALIGN_START;
    }
}

void apply_layout(Record &record, const Layout &layout, LayoutApplyMask mask)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: record(%1%), layout(%2%), mask(%3%)", record, layout, static_cast<uint32_t>(mask));

    const bool requires_full_apply = mask == LayoutApplyMask::All ||
                                     has_mask(mask, LayoutApplyMask::Type) ||
                                     record.layout_type != static_cast<uint8_t>(layout.type);
    if (layout.type == LayoutType::Flex) {
        if (requires_full_apply) {
            lv_obj_set_layout(record.object, LV_LAYOUT_FLEX);
        }
        apply_flex_layout(record, layout, requires_full_apply ? LayoutApplyMask::All : mask);
    } else if (layout.type == LayoutType::Grid) {
        if (requires_full_apply) {
            lv_obj_set_layout(record.object, LV_LAYOUT_GRID);
        }
        apply_grid_layout(record, layout, requires_full_apply ? LayoutApplyMask::All : mask);
    } else {
        lv_obj_set_layout(record.object, 0);
    }

    record.layout_type = static_cast<uint8_t>(layout.type);
}

void apply_placement(
    BackendImpl &impl,
    Record &record,
    const Placement &placement,
    PlacementApplyMask mask,
    bool refresh_frame
)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD(
        "Params: record(%1%), placement(%2%), mask(%3%)", record, placement, static_cast<uint32_t>(mask)
    );

    const bool requires_full_apply = mask == PlacementApplyMask::All ||
                                     has_mask(mask, PlacementApplyMask::Mode) ||
                                     get_record_placement(record).mode != placement.mode;
    const auto effective_mask = requires_full_apply ? PlacementApplyMask::All : mask;

    auto *stored_entry = replace_record_placement(impl, record, placement);
    const auto &stored_placement = stored_entry->value;

    if (has_mask(effective_mask, PlacementApplyMask::Size)) {
        apply_placement_size(record, stored_placement);
        const auto *frame_view = record.get_type_payload<Record::FrameViewPayload>();
        if (refresh_frame && frame_view != nullptr) {
            refresh_frame_view(impl, record, frame_view->props);
        }
    }
    if (has_mask(effective_mask, PlacementApplyMask::Position) ||
            has_mask(effective_mask, PlacementApplyMask::Align) ||
            has_mask(effective_mask, PlacementApplyMask::RelativeTo)) {
        apply_placement_position(impl, record, stored_placement);
    }
    if (has_mask(effective_mask, PlacementApplyMask::GridCell) ||
            has_mask(effective_mask, PlacementApplyMask::AlignSelf)) {
        apply_placement_grid_cell(record, stored_placement);
    }
    if (has_mask(effective_mask, PlacementApplyMask::FlexGrow)) {
        apply_placement_flex_grow(record, stored_placement);
    }
    refresh_text_input_inner_layout(record);
}

void refresh_relative_placements(BackendImpl &impl)
{
    BROOKESIA_LOG_TRACE_GUARD();

    if (impl.relative_placement_record_count == 0) {
        return;
    }

    bool has_relative_target = false;
    for (const auto &[unused_handle, record] : impl.records) {
        (void)unused_handle;
        const auto &placement = get_record_placement(record);
        if (placement.mode == PlacementMode::Relative && !placement.relative_to.empty()) {
            has_relative_target = true;
            break;
        }
    }
    if (!has_relative_target) {
        return;
    }

    update_known_layer_layouts(impl);

    for (auto &[unused_handle, record] : impl.records) {
        (void)unused_handle;
        (void)align_relative_record(impl, record, false);
    }

    update_known_layer_layouts(impl);
}

} // namespace esp_brookesia::gui::lvgl
