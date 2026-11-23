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
#include <cstring>
#include <new>
#include <string>
#include <utility>

// lv_image_cache_drop() lives in an instance header that lvgl.h does not aggregate.
#if __has_include("lvgl/src/misc/cache/instance/lv_image_cache.h")
#   include "lvgl/src/misc/cache/instance/lv_image_cache.h"
#elif __has_include("src/misc/cache/instance/lv_image_cache.h")
#   include "src/misc/cache/instance/lv_image_cache.h"
#endif

// LVGL 9.5.0 keeps the active event on this stack until lv_obj_send_event()
// returns. Public headers have no accessor, and destroy_node's counter does
// not cover a native lv_obj_delete().
#if __has_include("lvgl/src/core/lv_global.h")
#   include "lvgl/src/core/lv_global.h"
#elif __has_include("src/core/lv_global.h")
#   include "src/core/lv_global.h"
#else
#   error "LVGL lv_global.h not found"
#endif
#if __has_include("lvgl/src/misc/lv_event_private.h")
#   include "lvgl/src/misc/lv_event_private.h"
#elif __has_include("src/misc/lv_event_private.h")
#   include "src/misc/lv_event_private.h"
#else
#   error "LVGL lv_event_private.h not found"
#endif
// screen_cnt and the inactive-screen list are not in the public display API.
#if __has_include("lvgl/src/display/lv_display_private.h")
#   include "lvgl/src/display/lv_display_private.h"
#elif __has_include("src/display/lv_display_private.h")
#   include "src/display/lv_display_private.h"
#else
#   error "LVGL lv_display_private.h not found"
#endif
// map_p is not returned by a public call that skips lv_obj_is_valid().
#if __has_include("lvgl/src/widgets/buttonmatrix/lv_buttonmatrix_private.h")
#   include "lvgl/src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
#elif __has_include("src/widgets/buttonmatrix/lv_buttonmatrix_private.h")
#   include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
#else
#   error "LVGL lv_buttonmatrix_private.h not found"
#endif

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

bool requires_mount_placement_reapply(const Record &record)
{
    const auto &placement = lvgl::get_record_placement(record);
    return placement.mode == PlacementMode::Relative ||
           placement.x.mode == PlacementOffsetMode::Percent ||
           placement.y.mode == PlacementOffsetMode::Percent ||
           placement.aspect_ratio.has_value() ||
           record.type == NodeType::TextInput;
}

bool requires_mount_refresh(const Record &record)
{
    // FrameView output creation may be deferred while its screen is attached to the zero-sized
    // staging root. Retry every FrameView after the mounted flex/grid tree has reached final size.
    return record.type == NodeType::FrameView || requires_mount_placement_reapply(record);
}

// Line points are addresses owned by the record. A null point list is valid and does not
// allocate. Canvas pixels and grid tracks live on the widget lifetime instead.
void detach_borrowed_widget_buffers(Record &record)
{
    if (record.type != NodeType::Line) {
        return;
    }
    auto *line = record.get_type_payload<Record::LinePayload>();
    if (line != nullptr && lv_line_get_points(record.object) == line->points.data()) {
        lv_line_set_points(record.object, nullptr, 0);
    }
}

bool suppress_image_source_owner_for_test = false;

WidgetLifetime *pending_widget_lifetimes = nullptr;
uint32_t lifetime_release_serial = 0;
int lifetime_destroy_depth = 0;
bool lifetime_draining = false;
// Same as the ESP adapter's default task_max_delay_ms, so this timer does not
// wake that task any sooner than the loop already runs.
constexpr uint32_t LIFETIME_DRAIN_PERIOD_MS = 15;
lv_timer_t *lifetime_drain_timer = nullptr;
int lifetime_drain_timer_token = 0;

// The pointer survives esp_lv_adapter_deinit()/lv_deinit(), which frees the timer
// list. Only a timer still on this LVGL instance's list belongs to us. Comparing
// against lv_timer_get_next() does not dereference a timer from the previous instance.
bool lifetime_drain_timer_is_current()
{
    if (lifetime_drain_timer == nullptr || !lv_is_initialized()) {
        return false;
    }
    for (lv_timer_t *timer = lv_timer_get_next(nullptr); timer != nullptr; timer = lv_timer_get_next(timer)) {
        if (timer == lifetime_drain_timer && lv_timer_get_user_data(timer) == &lifetime_drain_timer_token) {
            return true;
        }
    }
    return false;
}

// LV_EVENT_DELETE stays on the stack until every listener of that send returns.
// Children are deleted only after the parent event is popped, and their own
// delete events keep this non-zero until the outer lv_obj_delete() returns.
int native_delete_depth()
{
    if (!lv_is_initialized()) {
        return 0;
    }
    int depth = 0;
    for (lv_event_t *event = LV_GLOBAL_DEFAULT()->event_header; event != nullptr; event = event->prev) {
        const auto code = static_cast<uint32_t>(event->code) & ~static_cast<uint32_t>(LV_EVENT_PREPROCESS);
        if (code == static_cast<uint32_t>(LV_EVENT_DELETE)) {
            ++depth;
        }
    }
    return depth;
}

void release_widget_lifetime(WidgetLifetime *life)
{
    if (life->image != nullptr) {
        lv_image_cache_drop(&life->image->descriptor);
        life->image.reset();
    }
    lv_free(life->grid_columns);
    lv_free(life->grid_rows);
    lv_free(life->canvas);
    delete life;
}

void release_widget_lifetimes_after(uint32_t serial)
{
    WidgetLifetime **link = &pending_widget_lifetimes;
    while (*link != nullptr) {
        auto *life = *link;
        if (life->release_serial <= serial) {
            link = &life->next;
            continue;
        }
        *link = life->next;
        life->next = nullptr;
        life->queued = false;
        release_widget_lifetime(life);
    }
}

void enqueue_widget_lifetime(WidgetLifetime *life)
{
    if (life == nullptr || life->queued) {
        return;
    }
    life->release_serial = ++lifetime_release_serial;
    if (life->release_serial == 0) {
        life->release_serial = ++lifetime_release_serial;
    }
    life->queued = true;
    life->next = pending_widget_lifetimes;
    pending_widget_lifetimes = life;
}

// lv_event_add() returns the descriptor even when the event array cannot grow.
// With LV_USE_ASSERT_NULL disabled that looks like success while the callback is absent.
bool callback_was_stored(lv_obj_t *object, uint32_t events_before, lv_event_dsc_t *descriptor)
{
    const auto events_after = lv_obj_get_event_count(object);
    if (descriptor != nullptr && events_after > events_before) {
        return true;
    }
    if (descriptor != nullptr && events_after == events_before) {
        lv_free(descriptor);
    }
    return false;
}

// Registered once, in place of the record callback, when the lifetime allocation succeeds.
// LVGL 9.5.0 runs ordinary delete listeners in registration order and asserts if a new
// descriptor cannot be allocated, so this callback must not free buffers or register another.
void on_widget_lifetime_delete(lv_event_t *event)
{
    auto *life = static_cast<WidgetLifetime *>(lv_event_get_user_data(event));
    if (life == nullptr) {
        return;
    }
    if (life->record != nullptr) {
        life->record->object = nullptr;
        life->record->lifetime = nullptr;
        life->record = nullptr;
    }
    enqueue_widget_lifetime(life);
}

void retain_image_source(Record &record)
{
    auto *image = record.get_type_payload<Record::ImagePayload>();
    if (record.type != NodeType::Image || image == nullptr || image->binary_src == nullptr) {
        return;
    }
    if (lv_image_get_src(record.object) != &image->binary_src->descriptor) {
        return;
    }
    if (suppress_image_source_owner_for_test || record.lifetime == nullptr || !record.lifetime->retain_image) {
        lv_image_set_src(record.object, nullptr);
        if (record.lifetime != nullptr && record.lifetime->image != nullptr) {
            lv_image_cache_drop(&record.lifetime->image->descriptor);
            record.lifetime->image.reset();
        }
        return;
    }
    record.lifetime->image = image->binary_src;
}

// child_handles can still name a widget after lv_obj_set_parent() moved it out of this
// subtree. The LVGL object then outlives the Record, so drop every callback and buffer
// that still points into the record. Leave the widget itself under its new parent.
void detach_reparented_native_object(Record &record)
{
    if (record.object == nullptr || !lv_obj_is_valid(record.object)) {
        if (record.lifetime != nullptr) {
            record.lifetime->record = nullptr;
            record.lifetime = nullptr;
        }
        return;
    }

    if (record.lifetime == nullptr) {
        lv_obj_remove_event_cb_with_user_data(
            record.object,
            &BackendImpl::on_native_object_delete,
            &record
        );
    }
    release_events(record);
    detach_keyboard_callbacks(record);
    detach_arc_gradient_callback(record);
    retain_image_source(record);
    detach_borrowed_widget_buffers(record);
    if (record.style_initialized) {
        lv_obj_remove_style(record.object, &record.style, LV_PART_MAIN);
    }
    if (auto *style_extras = record.style_extras_payload.get(); style_extras != nullptr) {
        for (auto &[unused_state, state_style] : style_extras->state_styles) {
            (void)unused_state;
            lv_obj_remove_style(record.object, &state_style.style, state_style.selector);
        }
        for (auto &[unused_part, part_style] : style_extras->part_styles) {
            (void)unused_part;
            lv_obj_remove_style(record.object, &part_style.style, part_style.selector);
            for (auto &[unused_state, state_style] : part_style.state_styles) {
                (void)unused_state;
                lv_obj_remove_style(record.object, &state_style.style, state_style.selector);
            }
        }
    }
    if (record.debug_style_payload != nullptr) {
        lv_obj_remove_style(record.object, &record.debug_style_payload->style, LV_PART_MAIN);
    }
    if (record.lifetime != nullptr) {
        record.lifetime->record = nullptr;
        record.lifetime = nullptr;
    }
    record.object = nullptr;
}

// Same width as LVGL 9.5.0's static kb_map[10]. Modes we never publish stay nullptr.
constexpr size_t PUBLISHED_KEYBOARD_MODE_COUNT = 10;

struct PublishedKeyboardMap {
    const char *const *map = nullptr;
    const lv_buttonmatrix_ctrl_t *controls = nullptr;
};

PublishedKeyboardMap published_keyboard_maps[PUBLISHED_KEYBOARD_MODE_COUNT];
std::vector<BackendImpl *> keyboard_layout_owners;
// destroy_node can run from an LV_EVENT_DELETE listener. The record is then gone while
// LVGL still has the global pointers. The backing object keeps itself alive and is linked
// here until a later release. Linking does not allocate.
Record::KeyboardLayoutStorage *retained_keyboard_layouts = nullptr;
// Bumped before each display walk. A storage whose display_scan matches was seen on a
// keyboard. The counter lives with the process, so the walk does not allocate a set.
uint32_t keyboard_display_scan_generation = 0;

const char *const *keyboard_layout_map(const Record::KeyboardLayoutStorage &storage)
{
    return storage.map.empty() ? nullptr : storage.map.data();
}

const lv_buttonmatrix_ctrl_t *keyboard_layout_controls(const Record::KeyboardLayoutStorage &storage)
{
    return storage.controls.empty() ? nullptr : storage.controls.data();
}

bool keyboard_map_is_published(const char *const *map, const lv_buttonmatrix_ctrl_t *controls)
{
    if (map == nullptr && controls == nullptr) {
        return false;
    }
    for (const auto &published : published_keyboard_maps) {
        if ((map != nullptr && published.map == map) || (controls != nullptr && published.controls == controls)) {
            return true;
        }
    }
    return false;
}

template <typename Visitor>
void visit_keyboard_layout_storage(Visitor &&visitor)
{
    for (auto *impl : keyboard_layout_owners) {
        for (auto &entry : impl->keyboard_layout_backing_store) {
            if (entry.storage != nullptr) {
                visitor(*entry.storage);
            }
        }
    }
    for (auto *storage = retained_keyboard_layouts; storage != nullptr; storage = storage->retain_next) {
        visitor(*storage);
    }
}

bool keyboard_layout_is_published(const Record::KeyboardLayoutStorage &storage)
{
    return keyboard_map_is_published(keyboard_layout_map(storage), keyboard_layout_controls(storage));
}

void begin_keyboard_display_scan()
{
    if (keyboard_display_scan_generation == static_cast<uint32_t>(-1)) {
        visit_keyboard_layout_storage([](Record::KeyboardLayoutStorage & storage) {
            storage.display_scan = 0;
        });
        keyboard_display_scan_generation = 1;
        return;
    }
    ++keyboard_display_scan_generation;
}

void mark_displayed_keyboard_map(const char *const *map, size_t &unmarked)
{
    if (map == nullptr || unmarked == 0) {
        return;
    }
    const auto generation = keyboard_display_scan_generation;
    visit_keyboard_layout_storage([&](Record::KeyboardLayoutStorage & storage) {
        if (storage.display_scan == generation || keyboard_layout_map(storage) != map) {
            return;
        }
        const auto published = keyboard_layout_is_published(storage);
        storage.display_scan = generation;
        if (!published && unmarked > 0) {
            --unmarked;
        }
    });
}

// One walk of every display's screen list. Layers, the active screen, and screens created
// with lv_obj_create(nullptr) are already in that list, so they are not visited again.
// Child pointers come from spec_attr. lv_obj_get_child() and lv_obj_get_child_count() run
// LV_ASSERT_OBJ, and an enabled LV_USE_ASSERT_OBJ calls lv_obj_is_valid() from the first
// display. lv_obj_has_class() matches a Keyboard subclass, which still runs the base
// constructor and borrows the global map. map_p is read directly. The result is a generation
// on the storage itself, so the walk does not allocate and cannot fail out of destroy.
void note_displayed_keyboard_maps(lv_obj_t *object, size_t &unmarked)
{
    if (object == nullptr || unmarked == 0) {
        return;
    }
    if (lv_obj_has_class(object, &lv_keyboard_class)) {
        const auto *matrix = reinterpret_cast<const lv_buttonmatrix_t *>(object);
        mark_displayed_keyboard_map(matrix->map_p, unmarked);
        if (unmarked == 0) {
            return;
        }
    }
    const auto *spec = object->spec_attr;
    if (spec == nullptr || spec->children == nullptr || spec->child_cnt == 0) {
        return;
    }
    const auto count = spec->child_cnt;
    auto *const *children = spec->children;
    for (uint16_t index = 0; index < count; ++index) {
        note_displayed_keyboard_maps(children[index], unmarked);
        if (unmarked == 0) {
            return;
        }
    }
}

void scan_displayed_keyboard_maps()
{
    begin_keyboard_display_scan();
    if (!lv_is_initialized()) {
        return;
    }
    size_t unmarked = 0;
    visit_keyboard_layout_storage([&](const Record::KeyboardLayoutStorage & storage) {
        if (keyboard_layout_map(storage) != nullptr && !keyboard_layout_is_published(storage)) {
            ++unmarked;
        }
    });
    if (unmarked == 0) {
        return;
    }
    for (lv_display_t *display = lv_display_get_next(nullptr); display != nullptr && unmarked > 0;
            display = lv_display_get_next(display)) {
        if (display->screens == nullptr) {
            continue;
        }
        for (uint32_t index = 0; index < display->screen_cnt && unmarked > 0; ++index) {
            note_displayed_keyboard_maps(display->screens[index], unmarked);
        }
    }
}

bool keyboard_layout_is_referenced(const Record::KeyboardLayoutStorage &storage)
{
    // A native delete does not clear kb_map[]. Keep that backing until a normal
    // publish_keyboard_map() replaces the slot. Do not create a widget to do it:
    // lv_obj_class_create_obj() dereferences a failed children realloc.
    if (keyboard_layout_is_published(storage)) {
        return true;
    }
    return keyboard_display_scan_generation != 0 &&
           storage.display_scan == keyboard_display_scan_generation &&
           keyboard_layout_map(storage) != nullptr;
}

void retain_keyboard_layout(std::shared_ptr<Record::KeyboardLayoutStorage> storage)
{
    if (storage == nullptr || storage->retained_self != nullptr) {
        return;
    }
    storage->retain_next = retained_keyboard_layouts;
    retained_keyboard_layouts = storage.get();
    storage->retained_self = std::move(storage);
}

void detach_keyboard_globals(Record &record)
{
    if (record.type != NodeType::Keyboard || record.object == nullptr || !lv_obj_is_valid(record.object)) {
        return;
    }

    static const char *const EMPTY_MAP[] = {""};
    static const lv_buttonmatrix_ctrl_t EMPTY_CONTROLS[] = {static_cast<lv_buttonmatrix_ctrl_t>(0)};

    lv_keyboard_set_textarea(record.object, nullptr);
    publish_keyboard_map(record.object, LV_KEYBOARD_MODE_TEXT_LOWER, EMPTY_MAP, EMPTY_CONTROLS);
    publish_keyboard_map(record.object, LV_KEYBOARD_MODE_TEXT_UPPER, EMPTY_MAP, EMPTY_CONTROLS);
    publish_keyboard_map(record.object, LV_KEYBOARD_MODE_NUMBER, EMPTY_MAP, EMPTY_CONTROLS);
    publish_keyboard_map(record.object, LV_KEYBOARD_MODE_SPECIAL, EMPTY_MAP, EMPTY_CONTROLS);
}

} // namespace

void publish_keyboard_map(
    lv_obj_t *object,
    lv_keyboard_mode_t mode,
    const char *const *map,
    const lv_buttonmatrix_ctrl_t *controls
)
{
    lv_keyboard_set_map(object, mode, map, controls);
    const auto index = static_cast<size_t>(mode);
    if (index < PUBLISHED_KEYBOARD_MODE_COUNT) {
        published_keyboard_maps[index].map = map;
        published_keyboard_maps[index].controls = controls;
    }
}

void release_unreferenced_keyboard_layouts(BackendImpl &impl)
{
    (void)impl;
    scan_displayed_keyboard_maps();
    auto unreferenced = [](const BackendImpl::KeyboardLayoutBacking & entry) {
        return entry.storage == nullptr || !keyboard_layout_is_referenced(*entry.storage);
    };
    for (auto *owner : keyboard_layout_owners) {
        std::erase_if(owner->keyboard_layout_backing_store, unreferenced);
    }
    Record::KeyboardLayoutStorage *previous = nullptr;
    for (auto *current = retained_keyboard_layouts; current != nullptr; ) {
        auto *next = current->retain_next;
        if (keyboard_layout_is_referenced(*current)) {
            previous = current;
            current = next;
            continue;
        }
        if (previous == nullptr) {
            retained_keyboard_layouts = next;
        } else {
            previous->retain_next = next;
        }
        current->retain_next = nullptr;
        auto owned = std::move(current->retained_self);
        current = next;
        owned.reset();
    }
}

bool keyboard_layout_storage_retains(const char *const *map)
{
    if (map == nullptr) {
        return false;
    }
    for (auto *owner : keyboard_layout_owners) {
        for (const auto &entry : owner->keyboard_layout_backing_store) {
            if (entry.storage != nullptr && keyboard_layout_map(*entry.storage) == map) {
                return true;
            }
        }
    }
    for (auto *storage = retained_keyboard_layouts; storage != nullptr; storage = storage->retain_next) {
        if (keyboard_layout_map(*storage) == map) {
            return true;
        }
    }
    return false;
}

void drain_widget_lifetimes()
{
    ThreadLockGuard lock;
    if (lifetime_draining || lifetime_destroy_depth > 0 || native_delete_depth() > 0) {
        return;
    }
    lifetime_draining = true;
    release_widget_lifetimes_after(0);
    lifetime_draining = false;
}

bool ensure_widget_lifetime_drain_timer()
{
    if (lifetime_drain_timer_is_current()) {
        return true;
    }
    // Drop a pointer left by the previous LVGL instance before creating one for this one.
    lifetime_drain_timer = nullptr;
    if (!lv_is_initialized()) {
        return false;
    }
    lifetime_drain_timer = lv_timer_create(
    [](lv_timer_t *timer) {
        (void)timer;
        drain_widget_lifetimes();
    },
    LIFETIME_DRAIN_PERIOD_MS,
    &lifetime_drain_timer_token
                           );
    if (lifetime_drain_timer == nullptr) {
        BROOKESIA_LOGE("Failed to create the widget lifetime drain timer");
        return false;
    }
    return true;
}

void forget_widget_lifetime_drain_timer()
{
    if (lifetime_drain_timer_is_current()) {
        lv_timer_delete(lifetime_drain_timer);
    }
    lifetime_drain_timer = nullptr;
}

void reset_widget_lifetime_state()
{
    ThreadLockGuard lock;
    if (lifetime_draining) {
        return;
    }
    lifetime_draining = true;
    release_widget_lifetimes_after(0);
    lifetime_release_serial = 0;
    lifetime_draining = false;
}

void suppress_image_source_owner(bool suppress)
{
    suppress_image_source_owner_for_test = suppress;
}

bool BackendImpl::is_staged_object(const lv_obj_t *object) const
{
    if (staging_root == nullptr || object == nullptr) {
        return false;
    }
    for (auto *current = object; current != nullptr; current = lv_obj_get_parent(current)) {
        if (current == staging_root || current == staging_screen) {
            return true;
        }
    }
    return false;
}

void BackendImpl::refresh_relative_placements_for_record(const Record &record)
{
    // Building under staging must not re-layout the active display on every apply_* —
    // that was causing continuous flicker during idle screen warm / eager builds.
    if (is_staged_object(record.object)) {
        relative_refresh_deferred = true;
        return;
    }
    lvgl::refresh_relative_placements(*this);
}

BackendImpl::BackendImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ThreadLockGuard lock;

    register_default_creators(*this);
    // Detached screen so warm/build trees never invalidate the active display tree.
    staging_screen = lv_obj_create(nullptr);
    staging_root = lv_obj_create(staging_screen);
    lv_obj_add_flag(staging_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(staging_root, 0, 0);

    if (auto *display = lv_display_get_default(); display != nullptr) {
        (void)register_display("Default", display, true);
    }
    (void)ensure_widget_lifetime_drain_timer();
    keyboard_layout_owners.push_back(this);
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

    if (native_delete_depth() == 0) {
        release_unreferenced_keyboard_layouts(*this);
        for (auto &entry : keyboard_layout_backing_store) {
            retain_keyboard_layout(std::move(entry.storage));
        }
    } else {
        scan_displayed_keyboard_maps();
        for (auto &entry : keyboard_layout_backing_store) {
            if (entry.storage != nullptr && keyboard_layout_is_referenced(*entry.storage)) {
                retain_keyboard_layout(std::move(entry.storage));
            }
        }
    }
    keyboard_layout_backing_store.clear();
    std::erase(keyboard_layout_owners, this);

    if ((staging_screen != nullptr) && lv_obj_is_valid(staging_screen)) {
        lv_obj_delete(staging_screen);
        staging_screen = nullptr;
        staging_root = nullptr;
    } else if ((staging_root != nullptr) && lv_obj_is_valid(staging_root)) {
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
    record.is_top_level_screen = is_top_level && node.type == NodeType::Screen;
    record.scope_root = handle;
    if (!is_top_level) {
        auto *parent_record = find_record(parent);
        if (parent_record != nullptr) {
            record.depth = parent_record->depth + 1;
            const bool is_scope_root = parent_path.empty() ?
                                       scope_root_absolute_path.size() == node.id.size() + 1 &&
                                       scope_root_absolute_path.front() == '/' &&
                                       scope_root_absolute_path.substr(1) == node.id :
                                       scope_root_absolute_path.size() == parent_path.size() + node.id.size() &&
                                       scope_root_absolute_path.starts_with(parent_path) &&
                                       scope_root_absolute_path.substr(parent_path.size()) == node.id;
            if (!is_scope_root) {
                record.scope_root = parent_record->scope_root;
            }
        }
    }

    records.emplace(handle.value(), std::move(record));
    if (parent.is_valid()) {
        child_handles[parent.value()].push_back(handle.value());
    }
    auto *created_record = find_record(handle);
    if (created_record != nullptr) {
        // Invalidate this handle the moment LVGL frees the widget, including direct lv_obj_delete/
        // lv_obj_clean from a native view callback. Without this, lv_obj_is_valid()'s address-only
        // check would accept a recycled address and hand a different widget to a later callback.
        // One registration only: a second lv_obj_add_event_cb() can assert inside lv_event_add().
        bool delete_callback_installed = false;
        auto *life = new (std::nothrow) WidgetLifetime();
        if (life == nullptr) {
            const auto events_before = lv_obj_get_event_count(object);
            auto *descriptor = lv_obj_add_event_cb(
                                   object, &BackendImpl::on_native_object_delete, LV_EVENT_DELETE, created_record
                               );
            delete_callback_installed = callback_was_stored(object, events_before, descriptor);
        } else {
            life->record = created_record;
            life->retain_image = !suppress_image_source_owner_for_test;
            const auto events_before = lv_obj_get_event_count(object);
            auto *descriptor = lv_obj_add_event_cb(object, on_widget_lifetime_delete, LV_EVENT_DELETE, life);
            if (callback_was_stored(object, events_before, descriptor)) {
                created_record->lifetime = life;
                delete_callback_installed = true;
            } else {
                delete life;
            }
        }
        if (!delete_callback_installed) {
            // Keyboard's constructor already occupies an event slot. If the array cannot grow,
            // the descriptor comes back unstored. Drop the node so a recycled address cannot
            // resolve this handle.
            if (parent.is_valid()) {
                auto children_it = child_handles.find(parent.value());
                if (children_it != child_handles.end()) {
                    std::erase(children_it->second, handle.value());
                    if (children_it->second.empty()) {
                        child_handles.erase(children_it);
                    }
                }
            }
            records.erase(handle.value());
            if (lv_obj_is_valid(object)) {
                lv_obj_delete(object);
            }
            BROOKESIA_LOGE("Delete notification was not installed for node id='%1%'", node.id);
            return BackendHandle();
        }
        if (requires_mount_refresh(*created_record)) {
            adjust_mount_refresh_ancestors(handle, true);
        }
    }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    BROOKESIA_LOGI(
        "[HeapTrace][gui.lvgl_record] action(create) type(%1%) record_size(%2%) "
        "placement_entries(%3%) placement_hits(%4%) placement_misses(%5%)",
        BROOKESIA_DESCRIBE_TO_STR(node.type),
        sizeof(Record),
        placement_cache.size(),
        placement_cache_hits,
        placement_cache_misses
    );
#endif
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

    const auto parent = it->second.parent;
    const auto removed_refresh_count = it->second.mount_refresh_subtree_count;
    if (removed_refresh_count > 0) {
        adjust_mount_refresh_ancestors(parent, false, removed_refresh_count);
    }

    std::vector<BackendHandle::Value> subtree_handles;
    collect_subtree_handles(handle, subtree_handles);

    if (parent.is_valid()) {
        auto children_it = child_handles.find(parent.value());
        if (children_it != child_handles.end()) {
            std::erase(children_it->second, handle.value());
            if (children_it->second.empty()) {
                child_handles.erase(children_it);
            }
        }
    }

    mounted_targets.erase(handle.value());

    bool release_keyboard_layouts = !keyboard_layout_backing_store.empty() ||
                                    retained_keyboard_layouts != nullptr;
    for (auto handle_value : subtree_handles) {
        auto record_it = records.find(handle_value);
        if (record_it == records.end()) {
            continue;
        }
        if (record_it->second.type == NodeType::Keyboard) {
            release_keyboard_layouts = true;
        }
        // A live object can be pointed at the static empty map. A native delete has already
        // cleared Record::object, and LVGL keeps the previous global entry, so that array
        // stays until release below finds it unreferenced.
        detach_keyboard_globals(record_it->second);
    }
    if (release_keyboard_layouts) {
        release_unreferenced_keyboard_layouts(*this);
    }

    ++lifetime_destroy_depth;
    const auto lifetime_mark = lifetime_release_serial;
    if (it->second.object != nullptr) {
        if (lv_obj_is_valid(it->second.object)) {
            lv_obj_delete(it->second.object);
        }
        it->second.object = nullptr;
    }
    // Listeners registered after the lifetime callback have now returned, including
    // listeners on children deleted inside this call. Only release lifetimes queued
    // by this call. An outer native delete stays queued until its LV_EVENT_DELETE
    // leaves the event stack and a port poll runs.
    release_widget_lifetimes_after(lifetime_mark);
    --lifetime_destroy_depth;

    for (auto handle_value : subtree_handles) {
        auto record_it = records.find(handle_value);
        if (record_it != records.end()) {
            release_frame_view(record_it->second);
            detach_reparented_native_object(record_it->second);
            release_font(*this, record_it->second);
            if (record_it->second.style_initialized) {
                lv_style_reset(&record_it->second.style);
            }
            if (auto *style_extras = record_it->second.style_extras_payload.get(); style_extras != nullptr) {
                for (auto &[unused_state, state_style] : style_extras->state_styles) {
                    (void)unused_state;
                    lv_style_reset(&state_style.style);
                }
                for (auto &[unused_part, part_style] : style_extras->part_styles) {
                    (void)unused_part;
                    lv_style_reset(&part_style.style);
                    for (auto &[unused_state, state_style] : part_style.state_styles) {
                        (void)unused_state;
                        lv_style_reset(&state_style.style);
                    }
                }
            }
            if (record_it->second.debug_style_payload != nullptr) {
                lv_style_reset(&record_it->second.debug_style_payload->style);
            }
            release_record_placement(*this, record_it->second);
            records.erase(record_it);
        }
        child_handles.erase(handle_value);
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
    refresh_relative_placements_for_record(*record);
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
        // The port already drives lv_timer_handler on its own cadence, so forcing a
        // refresh here made every host tick invalidate the whole screen and all of its
        // layers: a carrier calling process_timers() every 15 ms measured local widget
        // animations under a full-screen repaint the scenario never asked for.
        // Release buffers queued by a native delete once its LV_EVENT_DELETE has
        // left the event stack. A listener of that event must not free them, and
        // this path does not refresh.
        drain_widget_lifetimes();
        return;
    }

    ThreadLockGuard lock;

    lv_timer_handler();
    // A nested lv_timer_handler() returns immediately. The drain still refuses
    // while the outer LV_EVENT_DELETE remains on the event stack.
    drain_widget_lifetimes();
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
    refresh_relative_placements_for_record(*record);
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

    const bool refresh_was_required = requires_mount_refresh(*record);
    lvgl::apply_placement(*this, *record, placement, mask);
    const bool refresh_is_required = requires_mount_refresh(*record);
    if (refresh_was_required != refresh_is_required) {
        adjust_mount_refresh_ancestors(handle, refresh_is_required);
    }
    if (mask == PlacementApplyMask::All || has_mask(mask, PlacementApplyMask::Size) ||
            has_mask(mask, PlacementApplyMask::Align) || has_mask(mask, PlacementApplyMask::RelativeTo)) {
        refresh_relative_placements_for_record(*record);
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
    refresh_relative_placements_for_record(*record);
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
    // lv_obj_update_layout() walks the parentless ancestor of its argument. After set_parent the
    // screen and the layer share that ancestor, so passing the screen would not skip the other
    // screens kept on the layer.
    lv_obj_update_layout(layer_parent);

    std::vector<BackendHandle::Value> refresh_handles;
    refresh_handles.reserve(record->mount_refresh_subtree_count);
    if (record->mount_refresh_subtree_count > 0) {
        collect_mount_refresh_handles(handle, refresh_handles);
    }
    for (auto handle_value : refresh_handles) {
        auto *subtree_record = find_record(BackendHandle(handle_value));
        if (subtree_record == nullptr || subtree_record->object == nullptr ||
                !lv_obj_is_valid(subtree_record->object)) {
            continue;
        }
        if (!requires_mount_placement_reapply(*subtree_record)) {
            continue;
        }
        const auto &placement = lvgl::get_record_placement(*subtree_record);
        // FrameView registration happens after the whole mounted tree reaches its final layout.
        lvgl::apply_placement(*this, *subtree_record, placement, PlacementApplyMask::All, false);
    }

    lv_obj_update_layout(layer_parent);
    for (auto handle_value : refresh_handles) {
        auto *subtree_record = find_record(BackendHandle(handle_value));
        if (subtree_record == nullptr || subtree_record->object == nullptr ||
                !lv_obj_is_valid(subtree_record->object)) {
            continue;
        }
        auto *frame_view = subtree_record->get_type_payload<Record::FrameViewPayload>();
        if (frame_view != nullptr) {
            refresh_frame_view(*this, *subtree_record, frame_view->props);
        }
    }

    mounted_targets.insert_or_assign(handle.value(), MountTarget{
        .display_id = resolved_display_id,
        .layer = target.layer,
        .mount_mode = target.mount_mode,
        .z_order = target.z_order,
    });
    // Flush any relative placement work deferred while the tree lived under staging.
    relative_refresh_deferred = false;
    lvgl::refresh_relative_placements(*this);
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
        const auto *image = record.get_type_payload<Record::ImagePayload>();
        if (image != nullptr && image->src.rfind(path_prefix, 0) == 0) {
            BROOKESIA_LOGE("Cannot unmount image drive '%1%' while image view '%2%' still references it",
                           fs_letter,
                           build_absolute_path(record));
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

Record *BackendImpl::find_record_by_absolute_path(std::string_view absolute_path)
{
    if (absolute_path.size() < 2 || absolute_path.front() != '/' || absolute_path.back() == '/') {
        return nullptr;
    }

    for (auto &[unused_handle, record] : records) {
        (void)unused_handle;
        const Record *current = &record;
        size_t path_end = absolute_path.size();
        while (current != nullptr) {
            const auto separator = absolute_path.rfind('/', path_end - 1);
            if (separator == std::string_view::npos ||
                    absolute_path.substr(separator + 1, path_end - separator - 1) != current->node_id) {
                break;
            }
            if (!current->parent.is_valid()) {
                if (separator == 0) {
                    return &record;
                }
                break;
            }
            if (separator == 0) {
                break;
            }
            path_end = separator;
            current = find_record(current->parent);
        }
    }
    return nullptr;
}

std::string BackendImpl::build_absolute_path(const Record &record) const
{
    const Record *current = &record;
    size_t path_size = 0;
    while (current != nullptr) {
        path_size += current->node_id.size() + 1;
        if (!current->parent.is_valid()) {
            break;
        }
        current = find_record(current->parent);
    }

    std::string absolute_path(path_size, '\0');
    size_t write_offset = path_size;
    current = &record;
    while (current != nullptr) {
        write_offset -= current->node_id.size();
        std::copy(current->node_id.begin(), current->node_id.end(), absolute_path.begin() + write_offset);
        absolute_path[--write_offset] = '/';
        if (!current->parent.is_valid()) {
            break;
        }
        current = find_record(current->parent);
    }
    return absolute_path;
}

std::string BackendImpl::build_scope_root_absolute_path(const Record &record) const
{
    const auto *scope_root_record = find_record(record.scope_root);
    return build_absolute_path(scope_root_record == nullptr ? record : *scope_root_record);
}

void BackendImpl::collect_subtree_handles(BackendHandle root, std::vector<BackendHandle::Value> &handles)
{
    handles.push_back(root.value());

    auto children_it = child_handles.find(root.value());
    if (children_it == child_handles.end()) {
        return;
    }
    for (auto handle_value : children_it->second) {
        collect_subtree_handles(BackendHandle(handle_value), handles);
    }
}

void BackendImpl::collect_mount_refresh_handles(
    BackendHandle root, std::vector<BackendHandle::Value> &handles
)
{
    auto *record = find_record(root);
    if (record == nullptr || record->mount_refresh_subtree_count == 0) {
        return;
    }
    if (requires_mount_refresh(*record)) {
        handles.push_back(root.value());
    }

    auto children_it = child_handles.find(root.value());
    if (children_it == child_handles.end()) {
        return;
    }
    for (auto handle_value : children_it->second) {
        collect_mount_refresh_handles(BackendHandle(handle_value), handles);
    }
}

void BackendImpl::adjust_mount_refresh_ancestors(BackendHandle handle, bool add, std::size_t count)
{
    while (handle.is_valid()) {
        auto *record = find_record(handle);
        if (record == nullptr) {
            break;
        }
        if (add) {
            record->mount_refresh_subtree_count += count;
        } else if (record->mount_refresh_subtree_count >= count) {
            record->mount_refresh_subtree_count -= count;
        } else {
            BROOKESIA_LOGE(
                "Mount refresh count underflow: handle(%1%), count(%2%), remove(%3%)",
                handle,
                record->mount_refresh_subtree_count,
                count
            );
            record->mount_refresh_subtree_count = 0;
        }
        handle = record->parent;
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

uint32_t Backend::get_input_idle_ms() const
{
    ThreadLockGuard lock;
    return lv_display_get_inactive_time(nullptr);
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
void *BackendImpl::get_native_object(BackendHandle handle)
{
    ThreadLockGuard lock;

    if (!handle.is_valid()) {
        return nullptr;
    }
    auto *record = find_record(handle);
    if (record == nullptr || record->object == nullptr || !lv_obj_is_valid(record->object)) {
        return nullptr;
    }
    return record->object;
}

void BackendImpl::on_native_object_delete(lv_event_t *event)
{
    auto *record = static_cast<Record *>(lv_event_get_user_data(event));
    if (record != nullptr) {
        // The widget is being freed by LVGL; drop the stale address so a later recycled address can
        // never pass lv_obj_is_valid() for this handle. destroy_node also clears it, but native view
        // callbacks may delete widgets without going through destroy_node.
        record->object = nullptr;
    }
}

void *Backend::get_native_object(BackendHandle handle)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%)", handle.value());

    return impl_->get_native_object(handle);
}

} // namespace esp_brookesia::gui::lvgl
