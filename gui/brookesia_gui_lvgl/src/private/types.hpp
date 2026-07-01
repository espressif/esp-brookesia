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

struct Record {
    BackendHandle handle;
    BackendHandle parent;
    std::string node_id;
    NodeType type = NodeType::Container;
    lv_obj_t *object = nullptr;
    std::string absolute_path;
    std::string scope_root_absolute_path;
    std::string font_cache_key;
    uint32_t depth = 0;
    Layout layout;
    Placement placement;
    lv_style_t style {};
    bool style_initialized = false;
    struct StateStyleRecord {
        lv_style_t style {};
        uint32_t selector = 0;
    };
    std::unordered_map<std::string, StateStyleRecord> state_styles;
    struct PartStyleRecord {
        lv_style_t style {};
        uint32_t selector = 0;
        std::unordered_map<std::string, StateStyleRecord> state_styles;
    };
    std::unordered_map<std::string, PartStyleRecord> part_styles;
    struct ArcGradientRecord {
        bool enabled = false;
        uint32_t start_color = 0;
        uint32_t end_color = 0;
        int32_t segments = 32;
    };
    std::unordered_map<std::string, ArcGradientRecord> arc_gradients;
    std::unique_ptr<ArcGradientContext> arc_gradient_context;
    bool arc_gradient_event_registered = false;
    lv_style_t debug_style {};
    bool debug_style_initialized = false;
    std::string image_src;
    std::shared_ptr<std::string> image_src_storage;
    std::shared_ptr<BinaryImageSource> image_binary_src;
    uintptr_t image_native_src = 0;
    int32_t image_width = 0;
    int32_t image_height = 0;
    bool hidden = false;
    std::vector<Animation> animations;
    std::vector<lv_point_precise_t> line_points;
    std::vector<int32_t> grid_columns;
    std::vector<int32_t> grid_rows;
    std::vector<lv_color_t> canvas_buffer;
    FrameViewProps frame_view_props;
    std::string frame_view_output_name;
    std::vector<uint8_t> frame_view_buffer;
    lv_image_dsc_t frame_view_descriptor {};
    int32_t frame_view_width = 0;
    int32_t frame_view_height = 0;
    bool frame_view_ready = false;
    bool frame_view_registered_output = false;
    esp_brookesia::lib_utils::connection frame_view_frame_connection;
    esp_brookesia::lib_utils::connection frame_view_output_registered_connection;
    esp_brookesia::lib_utils::connection frame_view_output_unregistered_connection;
    struct KeyboardLayoutStorage {
        std::vector<std::string> labels;
        std::vector<KeyboardKey> keys;
        std::vector<const char *> map;
        std::vector<lv_buttonmatrix_ctrl_t> controls;
    };
    std::array<KeyboardLayoutStorage, 4> keyboard_layouts;
    std::vector<std::string> keyboard_allowed_modes;
    std::unordered_map<std::string, KeyboardKeyStyle> keyboard_key_styles;
    std::unordered_map<uint32_t, lv_area_t> keyboard_key_fill_areas;
    std::unordered_map<std::string, std::shared_ptr<std::string>> keyboard_image_src_storages;
    int32_t keyboard_icon_size = 0;
    std::string keyboard_target_path;
    lv_obj_t *keyboard_target_object = nullptr;
    std::string keyboard_current_mode = "text";
    bool keyboard_event_registered = false;
    bool keyboard_draw_event_registered = false;
    std::unique_ptr<EventContext> keyboard_value_event_context;
    std::unique_ptr<EventContext> keyboard_draw_event_context;
    std::vector<std::unique_ptr<EventContext>> event_contexts;
    bool is_top_level_screen = false;
};

BROOKESIA_DESCRIBE_STRUCT(
    Record, (),
    (handle, parent, node_id, type, object, absolute_path, scope_root_absolute_path, font_cache_key, style_initialized, image_src,
     image_native_src, image_width, image_height, is_top_level_screen)
)

struct FontCacheEntry {
    std::string cache_key;
    std::vector<lv_font_t *> chain;
    std::vector<void *> platform_font_handles;
    enum class FontKind {
        FreeType,
        ImageFont,
    };
    struct ImageFontContext {
        std::vector<ImageFontGlyph> glyphs;
    };
    std::vector<FontKind> font_kinds;
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
    bool scroll_node_to_visible(BackendHandle handle, bool animated);
    bool mount_image_assets(const EspImageMountConfig &config);
    bool unmount_image_assets(char fs_letter);
    bool register_display(std::string id, lv_display_t *display, bool set_default);
    std::string resolve_display_id(std::string_view requested_id) const;
    lv_display_t *resolve_display(std::string_view requested_id) const;
    lv_display_t *default_display() const;

    Record *find_record(BackendHandle handle);
    const Record *find_record(BackendHandle handle) const;
    void collect_subtree_handles(BackendHandle root, std::vector<BackendHandle::Value> &handles);

    boost::unordered_flat_map<NodeType, Creator, EnumHash> creators;
    std::unordered_map<BackendHandle::Value, Record> records;
    std::unordered_map<std::string, FontCacheEntry> font_cache;
    std::unordered_map<std::string, RuntimeFontResource> font_resources;
    std::unordered_map<std::string, BinaryImageCacheEntry> binary_image_cache;
    std::unordered_map<char, FontAssetsMountRecord> font_asset_mounts;
    std::unordered_map<char, ImageAssetsMountRecord> image_asset_mounts;
    std::unordered_map<BackendHandle::Value, MountTarget> mounted_targets;
    std::unordered_map<std::string, DisplayRegistration> displays;
    std::string default_display_id;
    IBackend::EventSink event_sink;
    lv_obj_t *staging_root = nullptr;
    BackendHandle::Value next_handle = 1;
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
void apply_props(BackendImpl &impl, Record &record, const Node &node, PropsApplyMask mask);
void refresh_frame_view(BackendImpl &impl, Record &record, FrameViewProps props);
void release_frame_view(Record &record);
void apply_layout(Record &record, const Layout &layout, LayoutApplyMask mask);
void apply_placement(BackendImpl &impl, Record &record, const Placement &placement, PlacementApplyMask mask);
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
