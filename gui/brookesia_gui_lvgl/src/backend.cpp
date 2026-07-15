/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/types.hpp"
#include "port/private/threading.hpp"
#include "brookesia/gui_lvgl/macro_configs.h"
#if !BROOKESIA_GUI_LVGL_BACKEND_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

#include "brookesia/lib_utils/function_guard.hpp"

#if LV_USE_FREETYPE
#   if __has_include("lvgl/font/lv_freetype.h")
#       include "lvgl/font/lv_freetype.h"
#   elif __has_include("src/libs/freetype/lv_freetype.h")
#       include "src/libs/freetype/lv_freetype.h"
#   elif __has_include("lvgl/src/libs/freetype/lv_freetype.h")
#       include "lvgl/src/libs/freetype/lv_freetype.h"
#   else
#       error "LVGL FreeType header not found"
#   endif
#endif

#include <algorithm>
#include <string>
#include <utility>

namespace esp_brookesia::gui::lvgl {
namespace {

class ThreadLockGuard {
public:
    ThreadLockGuard()
    {
        lock_thread();
    }

    ~ThreadLockGuard()
    {
        unlock_thread();
    }

    ThreadLockGuard(const ThreadLockGuard &) = delete;
    ThreadLockGuard &operator=(const ThreadLockGuard &) = delete;
};

void detach_keyboard_globals(Record &record)
{
    if (record.type != NodeType::Keyboard || record.object == nullptr || !lv_obj_is_valid(record.object)) {
        return;
    }

    static const char *const EMPTY_MAP[] = {""};
    static const lv_buttonmatrix_ctrl_t EMPTY_CONTROLS[] = {static_cast<lv_buttonmatrix_ctrl_t>(0)};

    lv_keyboard_set_textarea(record.object, nullptr);
    lv_keyboard_set_map(record.object, LV_KEYBOARD_MODE_TEXT_LOWER, EMPTY_MAP, EMPTY_CONTROLS);
    lv_keyboard_set_map(record.object, LV_KEYBOARD_MODE_TEXT_UPPER, EMPTY_MAP, EMPTY_CONTROLS);
    lv_keyboard_set_map(record.object, LV_KEYBOARD_MODE_NUMBER, EMPTY_MAP, EMPTY_CONTROLS);
    lv_keyboard_set_map(record.object, LV_KEYBOARD_MODE_SPECIAL, EMPTY_MAP, EMPTY_CONTROLS);
}

} // namespace

BackendImpl::BackendImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    register_default_creators(*this);
    staging_root = lv_obj_create(lv_screen_active());
    lv_obj_add_flag(staging_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(staging_root, 0, 0);

    if (auto *display = lv_display_get_default(); display != nullptr) {
        (void)register_display("Default", display, true);
    }
}

BackendImpl::~BackendImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    while (!records.empty()) {
        auto root_it = records.end();
        for (auto it = records.begin(); it != records.end(); ++it) {
            if (!it->second.parent.is_valid() || records.find(it->second.parent.value()) == records.end()) {
                root_it = it;
                break;
            }
        }

        if (root_it == records.end()) {
            root_it = records.begin();
        }

        destroy_node(root_it->second.handle);
    }

    if ((staging_root != nullptr) && lv_obj_is_valid(staging_root)) {
        lv_obj_delete(staging_root);
        staging_root = nullptr;
    }

    for (auto &[font_id, unused_resource] : font_resources) {
        (void)font_id;
        (void)unused_resource;
    }
    font_resources.clear();

#if BROOKESIA_GUI_LVGL_HAS_ESP_FONT_ASSET_MOUNT
    for (auto &[unused_letter, mount_record] : font_asset_mounts) {
        (void)unused_letter;
        if (mount_record.fs_handle != nullptr) {
            esp_lv_adapter_fs_unmount(static_cast<esp_lv_fs_handle_t>(mount_record.fs_handle));
        }
        if (mount_record.assets_handle != nullptr) {
            mmap_assets_del(static_cast<mmap_assets_handle_t>(mount_record.assets_handle));
        }
    }
#endif
    font_asset_mounts.clear();

#if BROOKESIA_GUI_LVGL_HAS_ESP_IMAGE_ASSET_MOUNT
    for (auto &[unused_letter, mount_record] : image_asset_mounts) {
        (void)unused_letter;
        if (mount_record.fs_handle != nullptr) {
            esp_lv_adapter_fs_unmount(static_cast<esp_lv_fs_handle_t>(mount_record.fs_handle));
        }
        if (mount_record.assets_handle != nullptr) {
            mmap_assets_del(static_cast<mmap_assets_handle_t>(mount_record.assets_handle));
        }
    }
#endif
    image_asset_mounts.clear();
}

void BackendImpl::set_event_sink(IBackend::EventSink sink)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: sink(valid=%1%)", static_cast<bool>(sink));

    event_sink = std::move(sink);
}

std::optional<IBackend::ThreadGuard> BackendImpl::get_thread_guard() const
{
    return IBackend::ThreadGuard{
        .lock = lock_thread,
        .unlock = unlock_thread,
    };
}

BackendHandle BackendImpl::create_node(
    const Node &node,
    BackendHandle parent,
    std::string_view parent_path,
    std::string_view scope_root_absolute_path
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    // BROOKESIA_LOGD(
    //     "Params: node(%1%), parent(%2%), parent_path(%3%), scope_root_absolute_path(%4%)",
    //     node, parent, parent_path, scope_root_absolute_path
    // );

    auto creator_it = creators.find(node.type);
    if (creator_it == creators.end()) {
        BROOKESIA_LOGE("Unsupported LVGL node type: %1%", BROOKESIA_DESCRIBE_TO_STR(node.type));
        return BackendHandle();
    }

    lv_obj_t *object = nullptr;
    const bool is_top_level = !parent.is_valid();
    if (is_top_level) {
        if (node.type != NodeType::Screen) {
            BROOKESIA_LOGE(
                "Only top-level screen nodes can be created without a parent (got type=%1%, id='%2%')",
                BROOKESIA_DESCRIBE_TO_STR(node.type), node.id
            );
            return BackendHandle();
        }
        object = creator_it->second(staging_root);
        if (object != nullptr) {
            lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        auto *parent_record = find_record(parent);
        if (parent_record == nullptr || parent_record->object == nullptr) {
            BROOKESIA_LOGE("Parent node not found for child: id='%1%', parent=%2%", node.id, parent);
            return BackendHandle();
        }
        object = creator_it->second(parent_record->object);
    }

    if (object == nullptr) {
        BROOKESIA_LOGE("LVGL creator returned null object for node id='%1%'", node.id);
        return BackendHandle();
    }

#if LV_USE_OBJ_NAME
    if (!parent_path.empty()) {
        lv_obj_set_name(object, (std::string(parent_path) + node.id).c_str());
    }
#endif

    BackendHandle handle(next_handle++);
    Record record;
    record.handle = handle;
    record.parent = parent;
    record.node_id = node.id;
    record.type = node.type;
    record.object = object;
    record.absolute_path = parent_path.empty() ? "/" + node.id : std::string(parent_path) + node.id;
    record.scope_root_absolute_path = std::string(scope_root_absolute_path);
    record.is_top_level_screen = is_top_level && node.type == NodeType::Screen;
    if (!is_top_level) {
        auto *parent_record = find_record(parent);
        if (parent_record != nullptr) {
            record.depth = parent_record->depth + 1;
        }
    }

    records.emplace(handle.value(), std::move(record));
    return handle;
}

void BackendImpl::destroy_node(BackendHandle handle)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: handle(%1%)", handle);

    auto it = records.find(handle.value());
    if (it == records.end()) {
        return;
    }

    std::vector<BackendHandle::Value> subtree_handles;
    collect_subtree_handles(handle, subtree_handles);

    mounted_targets.erase(handle.value());

    for (auto handle_value : subtree_handles) {
        auto record_it = records.find(handle_value);
        if (record_it != records.end()) {
            detach_keyboard_globals(record_it->second);
        }
    }

    if (it->second.object != nullptr) {
        if (lv_obj_is_valid(it->second.object)) {
            lv_obj_delete(it->second.object);
        }
        it->second.object = nullptr;
    }

    for (auto handle_value : subtree_handles) {
        auto record_it = records.find(handle_value);
        if (record_it != records.end()) {
            release_frame_view(record_it->second);
            release_font(*this, record_it->second);
            if (record_it->second.style_initialized) {
                lv_style_reset(&record_it->second.style);
            }
            for (auto &[unused_state, state_style] : record_it->second.state_styles) {
                (void)unused_state;
                lv_style_reset(&state_style.style);
            }
            for (auto &[unused_part, part_style] : record_it->second.part_styles) {
                (void)unused_part;
                lv_style_reset(&part_style.style);
                for (auto &[unused_state, state_style] : part_style.state_styles) {
                    (void)unused_state;
                    lv_style_reset(&state_style.style);
                }
            }
            if (record_it->second.debug_style_initialized) {
                lv_style_reset(&record_it->second.debug_style);
            }
            records.erase(record_it);
        }
    }
}

void BackendImpl::apply_props(BackendHandle handle, const Node &node, PropsApplyMask mask)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    // BROOKESIA_LOGD("Params: handle(%1%), node(%2%), mask(%3%)", handle, node, static_cast<uint64_t>(mask));

    auto *record = find_record(handle);
    if (record == nullptr || record->object == nullptr) {
        return;
    }

    lvgl::apply_props(*this, *record, node, mask);
    lvgl::refresh_relative_placements(*this);
}

std::expected<void, std::string> BackendImpl::preload_image_resource(const RuntimeImageResource &resource)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: resource(%1%)", resource);

    return lvgl::preload_image_resource(*this, resource);
}

void BackendImpl::release_image_resource(const RuntimeImageResource &resource)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: resource(%1%)", resource);

    lvgl::release_image_resource(*this, resource);
}

std::expected<RuntimeImageResource, std::string> BackendImpl::resolve_image_resource(
    RuntimeImageResource resource
) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: resource(%1%)", resource);

    return lvgl::resolve_image_resource(std::move(resource));
}

bool BackendImpl::requires_preloaded_image_resource(const RuntimeImageResource &resource) const
{
    ThreadLockGuard lock;

    return lvgl::requires_preloaded_image_resource(resource);
}

void BackendImpl::process_timers()
{
    if (is_timer_managed_by_port()) {
        request_refresh_now();
        return;
    }

    ThreadLockGuard lock;

    lv_timer_handler();
}

void BackendImpl::apply_layout(BackendHandle handle, const Layout &layout, LayoutApplyMask mask)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: handle(%1%), layout(%2%), mask(%3%)", handle, layout, static_cast<uint32_t>(mask));

    auto *record = find_record(handle);
    if (record == nullptr || record->object == nullptr) {
        return;
    }

    lvgl::apply_layout(*record, layout, mask);
    lvgl::refresh_relative_placements(*this);
}

void BackendImpl::apply_placement(BackendHandle handle, const Placement &placement, PlacementApplyMask mask)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD(
        "Params: handle(%1%), placement(%2%), mask(%3%)", handle, placement, static_cast<uint32_t>(mask)
    );

    auto *record = find_record(handle);
    if (record == nullptr || record->object == nullptr) {
        return;
    }

    lvgl::apply_placement(*this, *record, placement, mask);
    if (mask == PlacementApplyMask::All || has_mask(mask, PlacementApplyMask::Size) ||
            has_mask(mask, PlacementApplyMask::Align) || has_mask(mask, PlacementApplyMask::RelativeTo)) {
        lvgl::refresh_relative_placements(*this);
    }
}

void BackendImpl::apply_style(BackendHandle handle, const ResolvedStyle &style, StyleApplyMask mask)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: handle(%1%), style(%2%), mask(%3%)", handle, style, static_cast<uint32_t>(mask));

    auto *record = find_record(handle);
    if (record == nullptr || record->object == nullptr) {
        return;
    }

    lvgl::apply_style(*this, *record, style, mask);
    lvgl::refresh_relative_placements(*this);
}

void BackendImpl::apply_debug_visual(BackendHandle handle, bool enabled)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: handle(%1%), enabled(%2%)", handle, enabled);

    auto *record = find_record(handle);
    if (record == nullptr || record->object == nullptr) {
        return;
    }

    lvgl::apply_debug_visual(*record, enabled);
}

void BackendImpl::apply_animations(BackendHandle handle, const std::vector<Animation> &animations)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: handle(%1%), animations(count=%2%)", handle, animations.size());

    auto *record = find_record(handle);
    if (record == nullptr || record->object == nullptr) {
        return;
    }

    lvgl::apply_animations(*record, animations);
}

std::optional<BackendAnimationStartResult> BackendImpl::start_animation(
    BackendHandle handle,
    const Animation &animation,
    std::function<void()> completed_handler)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    auto *record = find_record(handle);
    if (record == nullptr) {
        return std::nullopt;
    }

    return lvgl::start_animation(*record, animation, std::move(completed_handler));
}

void BackendImpl::bind_events(BackendHandle handle, const std::vector<EventBinding> &events)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: handle(%1%), events(count=%2%)", handle, events.size());

    auto *record = find_record(handle);
    if (record == nullptr || record->object == nullptr) {
        return;
    }

    lvgl::bind_events(*this, *record, events);
}

std::vector<GuiLayer> BackendImpl::list_layers() const
{
    ThreadLockGuard lock;

    return {
        GuiLayer::Default,
        GuiLayer::Top,
        GuiLayer::System,
        GuiLayer::Bottom,
    };
}

std::vector<GuiDisplayInfo> BackendImpl::list_displays() const
{
    ThreadLockGuard lock;

    std::vector<GuiDisplayInfo> result;
    result.reserve(displays.size());
    for (const auto &[unused_id, registration] : displays) {
        (void)unused_id;
        if (registration.display == nullptr) {
            continue;
        }

        result.push_back(GuiDisplayInfo{
            .id = registration.id,
            .width_px = static_cast<int32_t>(lv_display_get_horizontal_resolution(registration.display)),
            .height_px = static_cast<int32_t>(lv_display_get_vertical_resolution(registration.display)),
            .is_default = registration.is_default,
        });
    }
    std::sort(result.begin(), result.end(), [](const GuiDisplayInfo & lhs, const GuiDisplayInfo & rhs) {
        return lhs.id < rhs.id;
    });
    return result;
}

lv_obj_t *BackendImpl::resolve_layer_parent(lv_display_t *display, GuiLayer layer) const
{
    ThreadLockGuard lock;

    if (display == nullptr) {
        return nullptr;
    }

    switch (layer) {
    case GuiLayer::Default:
        return lv_display_get_screen_active(display);
    case GuiLayer::Top:
        return lv_display_get_layer_top(display);
    case GuiLayer::System:
        return lv_display_get_layer_sys(display);
    case GuiLayer::Bottom:
        return lv_display_get_layer_bottom(display);
    case GuiLayer::Max:
        break;
    }
    return nullptr;
}

bool BackendImpl::mount_screen(BackendHandle handle, const MountTarget &target)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: handle(%1%), target(%2%)", handle, target);

    auto *record = find_record(handle);
    if (record == nullptr || !record->is_top_level_screen || record->object == nullptr) {
        BROOKESIA_LOGE(
            "Cannot mount screen: handle=%1%, record=%2%, is_top_level_screen=%3%",
            handle,
            static_cast<void *>(record),
            record != nullptr && record->is_top_level_screen
        );
        return false;
    }

    const auto resolved_display_id = resolve_display_id(target.display_id);
    auto *display = resolve_display(resolved_display_id);
    auto *layer_parent = resolve_layer_parent(display, target.layer);
    if (layer_parent == nullptr) {
        BROOKESIA_LOGE(
            "Cannot resolve layer parent for display_id='%1%', layer=%2%",
            resolved_display_id,
            BROOKESIA_DESCRIBE_ENUM_TO_STR(target.layer)
        );
        return false;
    }

    lv_obj_set_parent(record->object, layer_parent);
    lv_obj_remove_flag(record->object, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(record->object);
    lv_obj_update_layout(layer_parent);

    std::vector<BackendHandle::Value> subtree_handles;
    collect_subtree_handles(handle, subtree_handles);
    for (auto handle_value : subtree_handles) {
        auto *subtree_record = find_record(BackendHandle(handle_value));
        if (subtree_record == nullptr || subtree_record->object == nullptr || !lv_obj_is_valid(subtree_record->object)) {
            continue;
        }
        lvgl::apply_placement(*this, *subtree_record, subtree_record->placement, PlacementApplyMask::All);
    }

    mounted_targets.insert_or_assign(handle.value(), MountTarget{
        .display_id = resolved_display_id,
        .layer = target.layer,
        .mount_mode = target.mount_mode,
        .z_order = target.z_order,
    });
    return true;
}

bool BackendImpl::unmount_screen(BackendHandle handle)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: handle(%1%)", handle);

    auto *record = find_record(handle);
    if (record == nullptr || !record->is_top_level_screen) {
        BROOKESIA_LOGE(
            "Cannot unmount screen: handle=%1%, record=%2%, is_top_level_screen=%3%",
            handle,
            static_cast<void *>(record),
            record != nullptr && record->is_top_level_screen
        );
        return false;
    }

    lv_obj_add_flag(record->object, LV_OBJ_FLAG_HIDDEN);
    mounted_targets.erase(handle.value());
    return true;
}

bool BackendImpl::register_display(std::string id, lv_display_t *display, bool set_default)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_CHECK_FALSE_RETURN(!id.empty(), false, "Display id must not be empty");
    BROOKESIA_CHECK_NULL_RETURN(display, false, "Display must not be null");

    if (auto *active_screen = lv_display_get_screen_active(display); active_screen != nullptr) {
        lv_obj_set_style_bg_opa(active_screen, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_remove_flag(active_screen, LV_OBJ_FLAG_SCROLLABLE);
    }

    if (set_default) {
        for (auto &[unused_display_id, registration] : displays) {
            (void)unused_display_id;
            registration.is_default = false;
        }
        default_display_id = id;
    } else if (default_display_id.empty()) {
        default_display_id = id;
        set_default = true;
    }

    const std::string key = id;
    displays.insert_or_assign(key, DisplayRegistration{
        .id = std::move(id),
        .display = display,
        .is_default = set_default,
    });
    return true;
}

std::string BackendImpl::resolve_display_id(std::string_view requested_id) const
{
    ThreadLockGuard lock;

    if (!requested_id.empty()) {
        if (displays.contains(std::string(requested_id))) {
            return std::string(requested_id);
        }
        return {};
    }

    if (!default_display_id.empty()) {
        return default_display_id;
    }

    if (auto *display = lv_display_get_default(); display != nullptr) {
        for (const auto &[unused_id, registration] : displays) {
            (void)unused_id;
            if (registration.display == display) {
                return registration.id;
            }
        }
    }

    if (!displays.empty()) {
        return displays.begin()->second.id;
    }

    return {};
}

lv_display_t *BackendImpl::resolve_display(std::string_view requested_id) const
{
    ThreadLockGuard lock;

    const auto resolved_id = resolve_display_id(requested_id);
    if (resolved_id.empty()) {
        return nullptr;
    }

    auto it = displays.find(resolved_id);
    return it == displays.end() ? nullptr : it->second.display;
}

lv_display_t *BackendImpl::default_display() const
{
    ThreadLockGuard lock;

    return resolve_display({});
}

std::vector<RuntimeFontResource> BackendImpl::list_font_resources() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Resources: fonts(count=%1%)", font_resources.size());

    std::vector<RuntimeFontResource> resources;
    resources.reserve(font_resources.size());
    for (const auto &[unused_id, resource] : font_resources) {
        (void)unused_id;
        resources.push_back(resource);
    }
    return resources;
}

std::optional<ViewFrame> BackendImpl::get_node_frame(BackendHandle handle) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: handle(%1%)", handle);

    const auto *record = find_record(handle);
    if (record == nullptr || record->object == nullptr || !lv_obj_is_valid(record->object)) {
        return std::nullopt;
    }

    lv_obj_update_layout(record->object);

    return ViewFrame{
        .x = lv_obj_get_x(record->object),
        .y = lv_obj_get_y(record->object),
        .width = lv_obj_get_width(record->object),
        .height = lv_obj_get_height(record->object),
    };
}

bool BackendImpl::scroll_node_to_visible(BackendHandle handle, bool animated)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: handle(%1%), animated(%2%)", handle, animated);

    auto *record = find_record(handle);
    if (record == nullptr || record->object == nullptr || !lv_obj_is_valid(record->object)) {
        return false;
    }

    lv_obj_t *screen = lv_obj_get_screen(record->object);
    if (screen != nullptr && lv_obj_is_valid(screen)) {
        lv_obj_update_layout(screen);
    }
    lv_obj_update_layout(record->object);
    lv_obj_scroll_to_view_recursive(record->object, animated ? LV_ANIM_ON : LV_ANIM_OFF);
    return true;
}

bool BackendImpl::scroll_node_to(BackendHandle handle, int32_t x, int32_t y, bool animated)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: handle(%1%), x(%2%), y(%3%), animated(%4%)", handle, x, y, animated);

    auto *record = find_record(handle);
    if (record == nullptr || record->object == nullptr || !lv_obj_is_valid(record->object)) {
        return false;
    }

    lv_obj_t *screen = lv_obj_get_screen(record->object);
    if (screen != nullptr && lv_obj_is_valid(screen)) {
        lv_obj_update_layout(screen);
    }
    lv_obj_update_layout(record->object);
    lv_obj_scroll_to(record->object, x, y, animated ? LV_ANIM_ON : LV_ANIM_OFF);
    return true;
}

bool BackendImpl::mount_font_assets(const EspFontMountConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: config(%1%)", config);

#if BROOKESIA_GUI_LVGL_HAS_ESP_FONT_ASSET_MOUNT
    if (config.partition_label.empty()) {
        BROOKESIA_LOGE("Font assets partition_label must not be empty");
        return false;
    }
    if (config.max_files <= 0) {
        BROOKESIA_LOGE("Font assets max_files must be positive");
        return false;
    }
    if (config.fs_letter == '\0') {
        BROOKESIA_LOGE("Font assets fs_letter must not be empty");
        return false;
    }

    auto mount_it = font_asset_mounts.find(config.fs_letter);
    if (mount_it != font_asset_mounts.end()) {
        const auto &existing = mount_it->second;
        if (existing.partition_label == config.partition_label &&
                existing.max_files == config.max_files &&
                existing.checksum == config.checksum) {
            return true;
        }
        BROOKESIA_LOGE("Font assets drive '%1%' is already mounted with a different configuration", config.fs_letter);
        return false;
    }

    mmap_assets_config_t asset_config = {
        .partition_label = config.partition_label.c_str(),
        .max_files = config.max_files,
        .checksum = config.checksum,
        .flags = {},
    };
    mmap_assets_handle_t assets_handle = nullptr;
    if (mmap_assets_new(&asset_config, &assets_handle) != ESP_OK || assets_handle == nullptr) {
        BROOKESIA_LOGE("Failed to map font assets partition '%1%'", config.partition_label);
        return false;
    }
    const auto stored_file_count = mmap_assets_get_stored_files(assets_handle);
    BROOKESIA_LOGD(
        "Mapped font assets partition '%1%': files=%2%, fs_letter=%3%",
        config.partition_label,
        stored_file_count,
        config.fs_letter
    );
    if (stored_file_count == 0) {
        BROOKESIA_LOGE("Font assets partition '%1%' does not contain any files", config.partition_label);
        mmap_assets_del(assets_handle);
        return false;
    }

    fs_cfg_t fs_config = {
        .fs_letter = config.fs_letter,
        .fs_nums = static_cast<int>(stored_file_count),
        .fs_assets = assets_handle,
    };
    esp_lv_fs_handle_t fs_handle = nullptr;
    BROOKESIA_LOGD("Mounting font filesystem '%1%:' with %2% file(s)", config.fs_letter, stored_file_count);
    if (esp_lv_adapter_fs_mount(&fs_config, &fs_handle) != ESP_OK || fs_handle == nullptr) {
        BROOKESIA_LOGE("Failed to mount font filesystem for partition '%1%'", config.partition_label);
        mmap_assets_del(assets_handle);
        return false;
    }
    BROOKESIA_LOGD("Mounted font filesystem '%1%:'", config.fs_letter);

    font_asset_mounts.emplace(config.fs_letter, FontAssetsMountRecord {
        .fs_letter = config.fs_letter,
        .partition_label = config.partition_label,
        .max_files = config.max_files,
        .checksum = config.checksum,
        .assets_handle = assets_handle,
        .fs_handle = fs_handle,
    });
    failed_font_cache.clear();
    return true;
#else
    (void)config;
    BROOKESIA_LOGW("ESP font assets mounting is unavailable because esp_lv_adapter FS support is disabled");
    return false;
#endif
}

bool BackendImpl::unmount_font_assets(char fs_letter)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: fs_letter(%1%)", fs_letter);

#if BROOKESIA_GUI_LVGL_HAS_ESP_FONT_ASSET_MOUNT
    auto mount_it = font_asset_mounts.find(fs_letter);
    if (mount_it == font_asset_mounts.end()) {
        return false;
    }

    const std::string path_prefix = std::string(1, fs_letter) + ":";
    for (const auto &[cache_key, cache_entry] : font_cache) {
        if (cache_entry.ref_count > 0 && cache_key.find(path_prefix) != std::string::npos) {
            BROOKESIA_LOGE("Cannot unmount font drive '%1%' while cached fonts are still referenced", fs_letter);
            return false;
        }
    }

    for (auto it = font_resources.begin(); it != font_resources.end();) {
        if (it->second.primary_src.rfind(path_prefix, 0) == 0) {
            it = font_resources.erase(it);
        } else {
            ++it;
        }
    }

    bool success = true;
    if (mount_it->second.fs_handle != nullptr) {
        success = esp_lv_adapter_fs_unmount(static_cast<esp_lv_fs_handle_t>(mount_it->second.fs_handle)) == ESP_OK;
    }
    if (mount_it->second.assets_handle != nullptr) {
        success = (mmap_assets_del(static_cast<mmap_assets_handle_t>(mount_it->second.assets_handle)) == ESP_OK) && success;
    }
    font_asset_mounts.erase(mount_it);
    failed_font_cache.clear();
    return success;
#else
    (void)fs_letter;
    BROOKESIA_LOGW("ESP font assets unmount is unavailable because esp_lv_adapter FS support is disabled");
    return false;
#endif
}

bool BackendImpl::register_font_resource_from_file(const EspFontRegistrationConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: config(%1%)", config);

    if (config.font_id.empty()) {
        BROOKESIA_LOGE("Font resource id must not be empty");
        return false;
    }
    if (config.file_name.empty()) {
        BROOKESIA_LOGE("Font file name must not be empty");
        return false;
    }

    const auto mount_it = font_asset_mounts.find(config.fs_letter);
    if (mount_it == font_asset_mounts.end()) {
        BROOKESIA_LOGE("Font drive '%1%' is not mounted", config.fs_letter);
        return false;
    }

    std::string file_name = config.file_name;
    while (!file_name.empty() && file_name.front() == '/') {
        file_name.erase(file_name.begin());
    }

    RuntimeFontResource resource {
        .id = config.font_id,
        .kind = "file",
        .primary_src = std::string(1, config.fs_letter) + ":" + file_name,
        .languages = config.languages,
        .fallback_ids = config.fallback_ids,
        .native_fonts = {},
        .image_font_height = 0,
        .image_font_glyphs = {},
        .image_font_sizes = {},
    };
    auto existing_resource_it = font_resources.find(resource.id);
    if (existing_resource_it != font_resources.end()) {
        resource.native_fonts = existing_resource_it->second.native_fonts;
    }
    font_resources.insert_or_assign(resource.id, std::move(resource));
    failed_font_cache.clear();
    return true;
}

bool BackendImpl::register_font_resource(const FontRegistrationConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: config(%1%)", config);

    if (config.font_id.empty()) {
        BROOKESIA_LOGE("Font resource id must not be empty");
        return false;
    }
    if (config.native_src == 0) {
        BROOKESIA_LOGE("Native font source must not be empty");
        return false;
    }
    if (config.native_size <= 0) {
        BROOKESIA_LOGE("Native font size must be positive");
        return false;
    }

    auto &resource = font_resources[config.font_id];
    resource.id = config.font_id;
    resource.kind = "file";
    if (!config.languages.empty()) {
        resource.languages = config.languages;
    }
    if (!config.primary_src.empty()) {
        resource.primary_src = config.primary_src;
    } else if (resource.primary_src.empty()) {
        resource.primary_src = config.font_id;
    }
    if (!config.fallback_ids.empty()) {
        resource.fallback_ids = config.fallback_ids;
    }

    NativeFontVariant variant {
        .native_src = config.native_src,
        .native_size = config.native_size,
    };
    auto variant_it = std::find_if(
                          resource.native_fonts.begin(),
                          resource.native_fonts.end(),
    [native_size = config.native_size](const NativeFontVariant & item) {
        return item.native_size == native_size;
    }
                      );
    if (variant_it != resource.native_fonts.end()) {
        *variant_it = variant;
    } else {
        resource.native_fonts.push_back(variant);
    }
    std::sort(
        resource.native_fonts.begin(),
        resource.native_fonts.end(),
    [](const NativeFontVariant & lhs, const NativeFontVariant & rhs) {
        return lhs.native_size < rhs.native_size;
    }
    );
    failed_font_cache.clear();
    return true;
}

bool BackendImpl::register_font_resource(const RuntimeFontResource &resource)
{
    ThreadLockGuard lock;

    if (resource.kind == "imageFont") {
        if (resource.id.empty()) {
            BROOKESIA_LOGE("Font resource id must not be empty");
            return false;
        }
        if (resource.image_font_sizes.empty() && resource.image_font_height <= 0) {
            BROOKESIA_LOGE("imageFont resource height must be positive");
            return false;
        }
        if (resource.image_font_sizes.empty() && resource.image_font_glyphs.empty()) {
            BROOKESIA_LOGE("imageFont resource glyphs must not be empty");
            return false;
        }
        font_resources.insert_or_assign(resource.id, resource);
        failed_font_cache.clear();
        return true;
    }

    FontRegistrationConfig config{
        .font_id = resource.id,
        .primary_src = resource.primary_src,
        .native_src = 0,
        .native_size = 0,
        .languages = resource.languages,
        .fallback_ids = resource.fallback_ids,
    };
    if (!resource.native_fonts.empty()) {
        config.native_src = resource.native_fonts.front().native_src;
        config.native_size = resource.native_fonts.front().native_size;
    }
    auto success = register_font_resource(config);
    if (!success) {
        return false;
    }
    auto &stored_resource = font_resources[resource.id];
    stored_resource.kind = resource.kind;
    stored_resource.languages = resource.languages;
    stored_resource.fallback_ids = resource.fallback_ids;
    stored_resource.primary_src = resource.primary_src;
    stored_resource.image_font_height = resource.image_font_height;
    stored_resource.image_font_glyphs = resource.image_font_glyphs;
    stored_resource.image_font_sizes = resource.image_font_sizes;
    if (!resource.native_fonts.empty()) {
        stored_resource.native_fonts = resource.native_fonts;
    }
    failed_font_cache.clear();
    return true;
}

bool BackendImpl::mount_image_assets(const EspImageMountConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: config(%1%)", config);

#if BROOKESIA_GUI_LVGL_HAS_ESP_IMAGE_ASSET_MOUNT
    if (config.partition_label.empty()) {
        BROOKESIA_LOGE("Image assets partition_label must not be empty");
        return false;
    }
    if (config.max_files <= 0) {
        BROOKESIA_LOGE("Image assets max_files must be positive");
        return false;
    }
    if (config.fs_letter == '\0') {
        BROOKESIA_LOGE("Image assets fs_letter must not be empty");
        return false;
    }

    auto mount_it = image_asset_mounts.find(config.fs_letter);
    if (mount_it != image_asset_mounts.end()) {
        const auto &existing = mount_it->second;
        if (existing.partition_label == config.partition_label &&
                existing.max_files == config.max_files &&
                existing.checksum == config.checksum) {
            return true;
        }
        BROOKESIA_LOGE("Image assets drive '%1%' is already mounted with a different configuration", config.fs_letter);
        return false;
    }

    mmap_assets_config_t asset_config = {
        .partition_label = config.partition_label.c_str(),
        .max_files = config.max_files,
        .checksum = config.checksum,
        .flags = {
            .mmap_enable = 1,
            .use_fs = 0,
            .app_bin_check = 0,
            .full_check = 0,
            .metadata_check = 0,
            .reserved = 0,
        },
    };
    mmap_assets_handle_t assets_handle = nullptr;
    if (mmap_assets_new(&asset_config, &assets_handle) != ESP_OK || assets_handle == nullptr) {
        BROOKESIA_LOGE("Failed to map image assets partition '%1%'", config.partition_label);
        return false;
    }
    const auto stored_file_count = mmap_assets_get_stored_files(assets_handle);
    BROOKESIA_LOGD(
        "Mapped image assets partition '%1%': files=%2%, fs_letter=%3%",
        config.partition_label,
        stored_file_count,
        config.fs_letter
    );
    if (stored_file_count == 0) {
        BROOKESIA_LOGE("Image assets partition '%1%' does not contain any files", config.partition_label);
        mmap_assets_del(assets_handle);
        return false;
    }

    fs_cfg_t fs_config = {
        .fs_letter = config.fs_letter,
        .fs_nums = static_cast<int>(stored_file_count),
        .fs_assets = assets_handle,
    };
    esp_lv_fs_handle_t fs_handle = nullptr;
    BROOKESIA_LOGD("Mounting image filesystem '%1%:' with %2% file(s)", config.fs_letter, stored_file_count);
    if (esp_lv_adapter_fs_mount(&fs_config, &fs_handle) != ESP_OK || fs_handle == nullptr) {
        BROOKESIA_LOGE("Failed to mount image filesystem for partition '%1%'", config.partition_label);
        mmap_assets_del(assets_handle);
        return false;
    }
    BROOKESIA_LOGD("Mounted image filesystem '%1%:'", config.fs_letter);

    image_asset_mounts.emplace(config.fs_letter, ImageAssetsMountRecord {
        .fs_letter = config.fs_letter,
        .partition_label = config.partition_label,
        .max_files = config.max_files,
        .checksum = config.checksum,
        .assets_handle = assets_handle,
        .fs_handle = fs_handle,
    });
    return true;
#else
    (void)config;
    BROOKESIA_LOGW("ESP image assets mounting is unavailable because esp_lv_adapter FS support is disabled");
    return false;
#endif
}

bool BackendImpl::unmount_image_assets(char fs_letter)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    BROOKESIA_LOGD("Params: fs_letter(%1%)", fs_letter);

#if BROOKESIA_GUI_LVGL_HAS_ESP_IMAGE_ASSET_MOUNT
    auto mount_it = image_asset_mounts.find(fs_letter);
    if (mount_it == image_asset_mounts.end()) {
        return false;
    }

    const std::string path_prefix = std::string(1, fs_letter) + ":";
    for (const auto &[unused_handle, record] : records) {
        (void)unused_handle;
        if (record.image_src.rfind(path_prefix, 0) == 0) {
            BROOKESIA_LOGE("Cannot unmount image drive '%1%' while image view '%2%' still references it",
                           fs_letter,
                           record.absolute_path);
            return false;
        }
    }

    bool success = true;
    if (mount_it->second.fs_handle != nullptr) {
        success = esp_lv_adapter_fs_unmount(static_cast<esp_lv_fs_handle_t>(mount_it->second.fs_handle)) == ESP_OK;
    }
    if (mount_it->second.assets_handle != nullptr) {
        success = (mmap_assets_del(static_cast<mmap_assets_handle_t>(mount_it->second.assets_handle)) == ESP_OK) && success;
    }
    image_asset_mounts.erase(mount_it);
    return success;
#else
    (void)fs_letter;
    BROOKESIA_LOGW("ESP image assets unmount is unavailable because esp_lv_adapter FS support is disabled");
    return false;
#endif
}

Record *BackendImpl::find_record(BackendHandle handle)
{
    auto it = records.find(handle.value());
    if (it == records.end()) {
        return nullptr;
    }
    return &it->second;
}

const Record *BackendImpl::find_record(BackendHandle handle) const
{
    auto it = records.find(handle.value());
    if (it == records.end()) {
        return nullptr;
    }
    return &it->second;
}

void BackendImpl::collect_subtree_handles(BackendHandle root, std::vector<BackendHandle::Value> &handles)
{
    handles.push_back(root.value());

    for (const auto &[handle_value, record] : records) {
        if (record.parent == root) {
            collect_subtree_handles(BackendHandle(handle_value), handles);
        }
    }
}

Backend::Backend()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lock_thread();
    lib_utils::FunctionGuard unlock_guard(unlock_thread);
    impl_ = std::make_unique<BackendImpl>();
}

Backend::~Backend()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lock_thread();
    lib_utils::FunctionGuard unlock_guard(unlock_thread);
    impl_.reset();
}

void Backend::set_event_sink(EventSink sink)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: sink(valid=%1%)", static_cast<bool>(sink));

    impl_->set_event_sink(std::move(sink));
}

std::optional<IBackend::ThreadGuard> Backend::get_thread_guard() const
{
    return impl_->get_thread_guard();
}

BackendHandle Backend::create_node(
    const Node &node,
    BackendHandle parent,
    std::string_view parent_path,
    std::string_view scope_root_absolute_path
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD(
    //     "Params: node(%1%), parent(%2%), parent_path(%3%), scope_root_absolute_path(%4%)",
    //     node, parent, parent_path, scope_root_absolute_path
    // );

    return impl_->create_node(node, parent, parent_path, scope_root_absolute_path);
}

void Backend::destroy_node(BackendHandle handle)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%)", handle);

    impl_->destroy_node(handle);
}

void Backend::apply_props(BackendHandle handle, const Node &node, PropsApplyMask mask)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: handle(%1%), node(%2%), mask(%3%)", handle, node, static_cast<uint64_t>(mask));

    impl_->apply_props(handle, node, mask);
}

std::expected<void, std::string> Backend::preload_image_resource(const RuntimeImageResource &resource)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: resource(%1%)", resource);

    return impl_->preload_image_resource(resource);
}

void Backend::release_image_resource(const RuntimeImageResource &resource)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: resource(%1%)", resource);

    impl_->release_image_resource(resource);
}

std::expected<RuntimeImageResource, std::string> Backend::resolve_image_resource(
    RuntimeImageResource resource
) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: resource(%1%)", resource);

    return impl_->resolve_image_resource(std::move(resource));
}

bool Backend::requires_preloaded_image_resource(const RuntimeImageResource &resource) const
{
    return impl_->requires_preloaded_image_resource(resource);
}

void Backend::process_timers()
{
    impl_->process_timers();
}

void Backend::apply_layout(BackendHandle handle, const Layout &layout, LayoutApplyMask mask)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%), layout(%2%), mask(%3%)", handle, layout, static_cast<uint32_t>(mask));

    impl_->apply_layout(handle, layout, mask);
}

void Backend::apply_placement(BackendHandle handle, const Placement &placement, PlacementApplyMask mask)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: handle(%1%), placement(%2%), mask(%3%)", handle, placement, static_cast<uint32_t>(mask)
    );

    impl_->apply_placement(handle, placement, mask);
}

void Backend::apply_style(BackendHandle handle, const ResolvedStyle &style, StyleApplyMask mask)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%), style(%2%), mask(%3%)", handle, style, static_cast<uint32_t>(mask));

    impl_->apply_style(handle, style, mask);
}

void Backend::apply_debug_visual(BackendHandle handle, bool enabled)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%), enabled(%2%)", handle, enabled);

    impl_->apply_debug_visual(handle, enabled);
}

void Backend::apply_animations(BackendHandle handle, const std::vector<Animation> &animations)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%), animations(count=%2%)", handle, animations.size());

    impl_->apply_animations(handle, animations);
}

std::optional<BackendAnimationStartResult> Backend::start_animation(
    BackendHandle handle,
    const Animation &animation,
    std::function<void()> completed_handler)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%), animation(%2%)", handle, animation);

    return impl_->start_animation(handle, animation, std::move(completed_handler));
}

void Backend::bind_events(BackendHandle handle, const std::vector<EventBinding> &events)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%), events(count=%2%)", handle, events.size());

    impl_->bind_events(handle, events);
}

std::vector<GuiLayer> Backend::list_layers() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return impl_->list_layers();
}

std::vector<GuiDisplayInfo> Backend::list_displays() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return impl_->list_displays();
}

bool Backend::mount_screen(BackendHandle handle, const MountTarget &target)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%), target(%2%)", handle, target);

    return impl_->mount_screen(handle, target);
}

bool Backend::unmount_screen(BackendHandle handle)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%)", handle);

    return impl_->unmount_screen(handle);
}

std::vector<RuntimeFontResource> Backend::list_font_resources() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return impl_->list_font_resources();
}

bool Backend::register_display(std::string id, lv_display_t *display, bool set_default)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: id(%1%), display(%2%), set_default(%3%)",
        id,
        static_cast<void *>(display),
        set_default
    );

    return impl_->register_display(std::move(id), display, set_default);
}

std::optional<ViewFrame> Backend::get_node_frame(BackendHandle handle) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%)", handle);

    return impl_->get_node_frame(handle);
}

bool Backend::scroll_node_to_visible(BackendHandle handle, bool animated)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%), animated(%2%)", handle, animated);

    return impl_->scroll_node_to_visible(handle, animated);
}

bool Backend::scroll_node_to(BackendHandle handle, int32_t x, int32_t y, bool animated)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%), x(%2%), y(%3%), animated(%4%)", handle, x, y, animated);

    return impl_->scroll_node_to(handle, x, y, animated);
}

bool Backend::mount_font_assets(const EspFontMountConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", config);

    return impl_->mount_font_assets(config);
}

bool Backend::unmount_font_assets(char fs_letter)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: fs_letter(%1%)", fs_letter);

    return impl_->unmount_font_assets(fs_letter);
}

bool Backend::register_font_resource_from_file(const EspFontRegistrationConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(font_id=%1%, fs_letter=%2%, file_name=%3%, languages=%4%, fallback_ids=%5%)",
                   config.font_id, config.fs_letter, config.file_name, config.languages, config.fallback_ids);

    return impl_->register_font_resource_from_file(config);
}

bool Backend::register_font_resource(const RuntimeFontResource &resource)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: resource(id=%1%, primary_src=%2%, languages=%3%, fallback_ids=%4%, native_fonts=%5%)",
        resource.id,
        resource.primary_src,
        resource.languages,
        resource.fallback_ids,
        resource.native_fonts.size()
    );

    return impl_->register_font_resource(resource);
}

bool Backend::register_font_resource(const FontRegistrationConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: config(font_id=%1%, primary_src=%2%, native_src=%3%, native_size=%4%, languages=%5%, fallback_ids=%6%)",
        config.font_id,
        config.primary_src,
        config.native_src,
        config.native_size,
        config.languages,
        config.fallback_ids
    );

    return impl_->register_font_resource(config);
}

bool Backend::mount_image_assets(const EspImageMountConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", config);

    return impl_->mount_image_assets(config);
}

bool Backend::unmount_image_assets(char fs_letter)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: fs_letter(%1%)", fs_letter);

    return impl_->unmount_image_assets(fs_letter);
}

} // namespace esp_brookesia::gui::lvgl
