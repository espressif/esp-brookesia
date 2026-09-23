/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "boost/unordered/unordered_flat_map.hpp"
#include "brookesia/lib_utils/signal.hpp"
#if __has_include("lvgl/lvgl.h")
#   include "lvgl/lvgl.h"
#else
#   include "lvgl.h"
#endif
#if __has_include("esp_lv_adapter.h") && __has_include("esp_mmap_assets.h")
#   define BROOKESIA_GUI_LVGL_HAS_ESP_FONT_BACKEND 1
#   include "esp_lv_adapter.h"
#   include "esp_mmap_assets.h"
#else
#   define BROOKESIA_GUI_LVGL_HAS_ESP_FONT_BACKEND 0
#endif

#if BROOKESIA_GUI_LVGL_HAS_ESP_FONT_BACKEND && defined(CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS) && \
        CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS
#   define BROOKESIA_GUI_LVGL_HAS_ESP_FONT_ASSET_MOUNT 1
#   define BROOKESIA_GUI_LVGL_HAS_ESP_IMAGE_ASSET_MOUNT 1
#else
#   define BROOKESIA_GUI_LVGL_HAS_ESP_FONT_ASSET_MOUNT 0
#   define BROOKESIA_GUI_LVGL_HAS_ESP_IMAGE_ASSET_MOUNT 0
#endif
#include "brookesia/gui_lvgl/backend.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_display/service_display.hpp"

namespace esp_brookesia::gui::lvgl {

struct EnumHash {
    template <typename T>
    std::size_t operator()(T value) const
    {
        return static_cast<std::size_t>(value);
    }
};

class BackendImpl;
struct FontCacheEntry;

struct EventContext {
    BackendImpl *impl = nullptr;
    BackendHandle handle;
    EventType type = EventType::Clicked;
    std::string action;
};

BROOKESIA_DESCRIBE_STRUCT(EventContext, (), (impl, handle, type, action))

struct ArcGradientContext {
    BackendImpl *impl = nullptr;
    BackendHandle handle;
};

struct BinaryImageSource {
    std::vector<uint8_t> data;
    lv_image_dsc_t descriptor {};
};

struct BinaryImageCacheEntry {
    std::shared_ptr<BinaryImageSource> source;
    std::size_t ref_count = 0;
};

struct DisplayRegistration {
    std::string id;
    lv_display_t *display = nullptr;
    bool is_default = false;
};

BROOKESIA_DESCRIBE_STRUCT(DisplayRegistration, (), (id, display, is_default))

struct PlacementCacheEntry {
    Placement value;
    std::size_t ref_count = 0;
};

struct Record {
    BackendHandle handle;
    BackendHandle parent;
    // The backend can reconstruct an absolute path from the stable parent chain and node_id.
    // Retain only the scope root handle so ordinary nodes do not own two duplicate path strings.
    BackendHandle scope_root;
    std::string node_id;
    NodeType type = NodeType::Container;
    lv_obj_t *object = nullptr;
    // Borrowed from BackendImpl::font_cache while this Record contributes one ref_count.
    FontCacheEntry *font_cache_entry = nullptr;
    uint32_t depth = 0;
    std::size_t mount_refresh_subtree_count = 0;
    // Most nodes share one of a small number of placements. Shared entries stay immutable, while an
    // exclusively owned entry may be updated in place. BackendImpl owns and reference-counts them.
    PlacementCacheEntry *placement_cache_entry = nullptr;
    lv_style_t style {};
    struct StateStyleRecord {
        lv_style_t style {};
        uint32_t selector = 0;
    };
    struct PartStyleRecord {
        lv_style_t style {};
        uint32_t selector = 0;
        std::unordered_map<std::string, StateStyleRecord> state_styles;
    };
    struct ArcGradientRecord {
        bool enabled = false;
        uint32_t start_color = 0;
        uint32_t end_color = 0;
        int32_t segments = 32;
    };
    struct StyleExtrasPayload {
        std::unordered_map<std::string, StateStyleRecord> state_styles;
        std::unordered_map<std::string, PartStyleRecord> part_styles;
        std::unordered_map<std::string, ArcGradientRecord> arc_gradients;
        std::unique_ptr<ArcGradientContext> arc_gradient_context;
        bool arc_gradient_event_registered = false;
    };
    // State/part styles and Arc gradients are uncommon, so keep their maps and callback context
    // out of the common Record footprint until a node actually needs one.
    std::unique_ptr<StyleExtrasPayload> style_extras_payload;
    struct DebugStylePayload {
        lv_style_t style {};
    };
    // View debugging is normally disabled. Avoid embedding an unused LVGL style in every node.
    std::unique_ptr<DebugStylePayload> debug_style_payload;
    struct ImagePayload {
        std::string src;
        std::shared_ptr<BinaryImageSource> binary_src;
        uintptr_t native_src = 0;
        int32_t width = 0;
        int32_t height = 0;
    };
    // These retain data handed to LVGL. Allocate them only for the node or layout feature
    // that uses them so the common Record footprint stays small.
    struct AnimationPayload {
        std::vector<Animation> values;
    };
    std::unique_ptr<AnimationPayload> animation_payload;
    struct LinePayload {
        std::vector<lv_point_precise_t> points;
    };
    struct GridPayload {
        std::vector<int32_t> columns;
        std::vector<int32_t> rows;
    };
    std::unique_ptr<GridPayload> grid_payload;
    struct CanvasPayload {
        std::vector<lv_color_t> buffer;
    };
    struct FrameViewPayload {
        FrameViewProps props;
        std::string output_name;
        std::vector<uint8_t> buffer;
        std::vector<uint8_t> shadow_buffer;
        lv_image_dsc_t descriptor {};
        int32_t width = 0;
        int32_t height = 0;
        bool ready = false;
        bool registered_output = false;
        esp_brookesia::lib_utils::connection frame_connection;
        esp_brookesia::lib_utils::connection output_registered_connection;
        esp_brookesia::lib_utils::connection output_unregistered_connection;
    };
    struct KeyboardLayoutStorage {
        std::vector<std::string> labels;
        std::vector<KeyboardKey> keys;
        std::vector<const char *> map;
        std::vector<lv_buttonmatrix_ctrl_t> controls;
    };
    struct KeyboardPayload {
        std::array<KeyboardLayoutStorage, 4> layouts;
        std::vector<std::string> allowed_modes;
        std::unordered_map<std::string, KeyboardKeyStyle> key_styles;
        std::unordered_map<uint32_t, lv_area_t> key_fill_areas;
        int32_t icon_size = 0;
        std::string target_path;
        lv_obj_t *target_object = nullptr;
        std::string current_mode = "text";
        bool event_registered = false;
        bool draw_event_registered = false;
        std::unique_ptr<EventContext> value_event_context;
        std::unique_ptr<EventContext> draw_event_context;
    };
    // Keyboard layouts alone contain sixteen vectors. Keep all keyboard-only bookkeeping in
    // one payload because the overwhelming majority of GUI nodes are not keyboards.
    using TypePayload = std::variant <
                        std::monostate,
                        std::unique_ptr<ImagePayload>,
                        std::unique_ptr<LinePayload>,
                        std::unique_ptr<CanvasPayload>,
                        std::unique_ptr<FrameViewPayload>,
                        std::unique_ptr<KeyboardPayload>
                        >;
    // A node has exactly one immutable NodeType, so these five view-only payloads are mutually
    // exclusive. Keep one tagged owner instead of reserving five pointers in every Record.
    TypePayload type_payload;

    template <typename T>
    T *get_type_payload()
    {
        auto *payload = std::get_if<std::unique_ptr<T>>(&type_payload);
        return payload != nullptr ? payload->get() : nullptr;
    }

    template <typename T>
    const T *get_type_payload() const
    {
        const auto *payload = std::get_if<std::unique_ptr<T>>(&type_payload);
        return payload != nullptr ? payload->get() : nullptr;
    }

    template <typename T>
    T &ensure_type_payload()
    {
        auto *payload = std::get_if<std::unique_ptr<T>>(&type_payload);
        if (payload == nullptr) {
            type_payload.template emplace<std::unique_ptr<T>>(std::make_unique<T>());
            payload = std::get_if<std::unique_ptr<T>>(&type_payload);
        } else if (*payload == nullptr) {
            *payload = std::make_unique<T>();
        }
        return **payload;
    }
    // Static nodes without event bindings should not retain an empty vector.
    std::unique_ptr<std::vector<std::unique_ptr<EventContext>>> event_contexts;
    // Only the previous layout type is needed to decide whether a partial update must become a
    // full apply. Store its small enum value with the common flags; Grid tracks retained by LVGL
    // live in the type-specific payload above.
    uint8_t layout_type = static_cast<uint8_t>(LayoutType::None);
    // Keep common flags together so they do not each introduce pointer-alignment padding.
    bool style_initialized = false;
    bool hidden = false;
    bool is_top_level_screen = false;
};

BROOKESIA_DESCRIBE_STRUCT(
    Record, (),
    (handle, parent, scope_root, node_id, type, object, style_initialized, is_top_level_screen)
)

struct FontCacheEntry {
    std::string cache_key;
    std::vector<lv_font_t *> chain;
    std::vector<void *> platform_font_handles;
    struct FontSource {
        std::vector<uint8_t> data;
        lv_fs_path_ex_t memfs_path {};
    };
    enum class FontKind {
        FreeType,
        ImageFont,
    };
    struct ImageFontGlyphSource {
        uint32_t codepoint = 0;
        std::shared_ptr<BinaryImageSource> source;
    };
    struct ImageFontContext {
        std::vector<ImageFontGlyphSource> glyph_sources;
    };
    std::vector<FontKind> font_kinds;
    std::vector<std::shared_ptr<FontSource>> font_sources;
    std::vector<std::unique_ptr<ImageFontContext>> image_font_contexts;
    std::size_t ref_count = 0;
};

BROOKESIA_DESCRIBE_STRUCT(FontCacheEntry, (), (cache_key, ref_count))

struct FontAssetsMountRecord {
    char fs_letter = '\0';
    std::string partition_label;
    int max_files = 0;
    uint32_t checksum = 0;
    void *assets_handle = nullptr;
    void *fs_handle = nullptr;
};

BROOKESIA_DESCRIBE_STRUCT(FontAssetsMountRecord, (), (fs_letter, partition_label, max_files, checksum))

struct ImageAssetsMountRecord {
    char fs_letter = '\0';
    std::string partition_label;
    int max_files = 0;
    uint32_t checksum = 0;
    void *assets_handle = nullptr;
    void *fs_handle = nullptr;
};

BROOKESIA_DESCRIBE_STRUCT(ImageAssetsMountRecord, (), (fs_letter, partition_label, max_files, checksum))

class BackendImpl {
public:
    using Creator = lv_obj_t *(*)(lv_obj_t *);

    BackendImpl();
    ~BackendImpl();

    void set_event_sink(IBackend::EventSink sink);
    std::optional<IBackend::ThreadGuard> get_thread_guard() const;
    BackendHandle create_node(
        const Node &node,
        BackendHandle parent,
        std::string_view parent_path,
        std::string_view scope_root_absolute_path
    );
    void destroy_node(BackendHandle handle);
    void apply_props(BackendHandle handle, const Node &node, PropsApplyMask mask = PropsApplyMask::All);
    void apply_layout(BackendHandle handle, const Layout &layout, LayoutApplyMask mask = LayoutApplyMask::All);
    void apply_placement(
        BackendHandle handle, const Placement &placement, PlacementApplyMask mask = PlacementApplyMask::All
    );
    void apply_style(BackendHandle handle, const ResolvedStyle &style, StyleApplyMask mask = StyleApplyMask::All);
    void apply_debug_visual(BackendHandle handle, bool enabled);
    void apply_animations(BackendHandle handle, const std::vector<Animation> &animations);
    std::optional<BackendAnimationStartResult> start_animation(
        BackendHandle handle,
        const Animation &animation,
        std::function<void()> completed_handler = {});
    void bind_events(BackendHandle handle, const std::vector<EventBinding> &events);
    std::vector<GuiDisplayInfo> list_displays() const;
    std::vector<GuiLayer> list_layers() const;
    lv_obj_t *resolve_layer_parent(lv_display_t *display, GuiLayer layer) const;
    bool mount_screen(BackendHandle handle, const MountTarget &target);
    bool unmount_screen(BackendHandle handle);
    std::vector<RuntimeFontResource> list_font_resources() const;
    std::optional<ViewFrame> get_node_frame(BackendHandle handle) const;
    bool mount_font_assets(const EspFontMountConfig &config);
    bool unmount_font_assets(char fs_letter);
    bool register_font_resource_from_file(const EspFontRegistrationConfig &config);
    bool register_font_resource(const FontRegistrationConfig &config);
    bool register_font_resource(const RuntimeFontResource &resource);
    std::expected<void, std::string> preload_image_resource(const RuntimeImageResource &resource);
    void release_image_resource(const RuntimeImageResource &resource);
    std::expected<RuntimeImageResource, std::string> resolve_image_resource(RuntimeImageResource resource) const;
    bool requires_preloaded_image_resource(const RuntimeImageResource &resource) const;
    void process_timers();
    bool scroll_node_to(BackendHandle handle, int32_t x, int32_t y, bool animated);
    bool scroll_node_to_visible(BackendHandle handle, bool animated);
    bool mount_image_assets(const EspImageMountConfig &config);
    bool unmount_image_assets(char fs_letter);
    bool register_display(std::string id, lv_display_t *display, bool set_default);
    std::string resolve_display_id(std::string_view requested_id) const;
    lv_display_t *resolve_display(std::string_view requested_id) const;
    lv_display_t *default_display() const;

    Record *find_record(BackendHandle handle);
    const Record *find_record(BackendHandle handle) const;
    Record *find_record_by_absolute_path(std::string_view absolute_path);
    std::string build_absolute_path(const Record &record) const;
    std::string build_scope_root_absolute_path(const Record &record) const;
    void collect_subtree_handles(BackendHandle root, std::vector<BackendHandle::Value> &handles);
    void collect_mount_refresh_handles(BackendHandle root, std::vector<BackendHandle::Value> &handles);
    void adjust_mount_refresh_ancestors(BackendHandle handle, bool add, std::size_t count = 1);

    boost::unordered_flat_map<NodeType, Creator, EnumHash> creators;
    std::unordered_map<BackendHandle::Value, Record> records;
    // Keep tree traversal proportional to the visited subtree. Storing child lists only for
    // parents avoids adding a vector to every Record, including leaf-heavy JSON documents.
    std::unordered_map<BackendHandle::Value, std::vector<BackendHandle::Value>> child_handles;
    // Record borrows pointers to values in this node-based container. Rehashing preserves those
    // pointers; any future erase/clear path must first release every borrowing Record.
    std::unordered_map<std::string, FontCacheEntry> font_cache;
    std::unordered_map<std::string, std::weak_ptr<FontCacheEntry::FontSource>> font_source_cache;
    std::unordered_set<std::string> failed_font_cache;
    std::unordered_map<std::string, RuntimeFontResource> font_resources;
    std::unordered_map<std::string, BinaryImageCacheEntry> binary_image_cache;
    std::unordered_map<std::string, BinaryImageCacheEntry> decoded_image_cache;
    // The pointees stay stable when the vector grows. Entries are erased as soon as the last Record
    // releases them so unloading a document does not retain app-specific placement values.
    std::vector<std::unique_ptr<PlacementCacheEntry>> placement_cache;
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    std::size_t placement_cache_hits = 0;
    std::size_t placement_cache_misses = 0;
#endif
    std::vector<std::shared_ptr<Record::KeyboardLayoutStorage>> keyboard_layout_backing_store;
    std::unordered_map<char, FontAssetsMountRecord> font_asset_mounts;
    std::unordered_map<char, ImageAssetsMountRecord> image_asset_mounts;
    std::unordered_map<BackendHandle::Value, MountTarget> mounted_targets;
    std::unordered_map<std::string, DisplayRegistration> displays;
    std::string default_display_id;
    IBackend::EventSink event_sink;
    /** Never-loaded LVGL screen; keeps warm/build trees off the active display. */
    lv_obj_t *staging_screen = nullptr;
    lv_obj_t *staging_root = nullptr;
    /** Set when relative refresh was skipped for staged (unmounted) nodes. */
    bool relative_refresh_deferred = false;
    BackendHandle::Value next_handle = 1;

    bool is_staged_object(const lv_obj_t *object) const;
    void refresh_relative_placements_for_record(const Record &record);
};

std::optional<uint32_t> parse_color(std::string_view color);
lv_flex_flow_t to_lvgl_flex_flow(FlexFlow flow);
lv_flex_align_t to_lvgl_flex_align(Align align);
const lv_font_t *get_font(BackendImpl &impl, Record &record, const ResolvedStyle &style);
void release_font(BackendImpl &impl, Record &record);

void register_default_creators(BackendImpl &impl);
std::expected<void, std::string> preload_image_resource(BackendImpl &impl, const RuntimeImageResource &resource);
void release_image_resource(BackendImpl &impl, const RuntimeImageResource &resource);
std::expected<RuntimeImageResource, std::string> resolve_image_resource(RuntimeImageResource resource);
bool requires_preloaded_image_resource(const RuntimeImageResource &resource);
std::expected<std::shared_ptr<BinaryImageSource>, std::string> load_image_source(std::string_view path);
void apply_props(BackendImpl &impl, Record &record, const Node &node, PropsApplyMask mask);
void refresh_frame_view(BackendImpl &impl, Record &record, FrameViewProps props);
void release_frame_view(Record &record);
void apply_layout(Record &record, const Layout &layout, LayoutApplyMask mask);
const Placement &get_record_placement(const Record &record);
void release_record_placement(BackendImpl &impl, Record &record);
void apply_placement(
    BackendImpl &impl,
    Record &record,
    const Placement &placement,
    PlacementApplyMask mask,
    bool refresh_frame = true
);
void refresh_relative_placements(BackendImpl &impl);
void apply_image_sizing(Record &record, const Placement &placement);
void apply_style(BackendImpl &impl, Record &record, const ResolvedStyle &style, StyleApplyMask mask);
void refresh_text_input_inner_layout(Record &record);
void apply_debug_visual(Record &record, bool enabled);
void apply_animations(Record &record, const std::vector<Animation> &animations);
void run_animations(Record &record, AnimationTrigger trigger);
std::optional<BackendAnimationStartResult> start_animation(
    Record &record, const Animation &animation, std::function<void()> completed_handler = {}
);
void bind_events(BackendImpl &impl, Record &record, const std::vector<EventBinding> &events);

} // namespace esp_brookesia::gui::lvgl
