/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>

#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/system/impl.hpp"

#include "brookesia/gui_interface/parser.hpp"
#include "brookesia/service_helper.hpp"

namespace esp_brookesia::system::core {
namespace {

constexpr const char *APP_ICON_IMAGE_DIR = "images";
constexpr const char *IMAGE_DESCRIPTOR_EXTENSION = ".json";
constexpr uint32_t STORAGE_FS_TIMEOUT_MS = 5000;

bool append_unique_path(std::vector<std::filesystem::path> &paths, const std::filesystem::path &path)
{
    const auto normalized = path.lexically_normal();
    const auto path_string = normalized.generic_string();
    for (const auto &existing : paths) {
        if (existing.generic_string() == path_string) {
            return false;
        }
    }
    paths.push_back(normalized);
    return true;
}

std::string join_resource_path(std::string_view resource_dir, std::string_view path)
{
    if (path.empty()) {
        return {};
    }
    std::filesystem::path source(path);
    if (source.is_absolute() || resource_dir.empty()) {
        return source.lexically_normal().generic_string();
    }
    return (std::filesystem::path(resource_dir) / source).lexically_normal().generic_string();
}

std::string resolve_source_path(std::string_view resource_dir, const std::string &src)
{
    if (src.empty()) {
        return {};
    }
    std::filesystem::path source(src);
    if (source.is_absolute() || resource_dir.empty()) {
        return source.lexically_normal().generic_string();
    }
    return (std::filesystem::path(resource_dir) / source).lexically_normal().generic_string();
}

gui::RuntimeFontResource resolve_font_resource_sources(
    gui::RuntimeFontResource resource,
    std::string_view resource_dir
)
{
    resource.primary_src = resolve_source_path(resource_dir, resource.primary_src);
    for (auto &glyph : resource.image_font_glyphs) {
        glyph.src = resolve_source_path(resource_dir, glyph.src);
    }
    for (auto &size : resource.image_font_sizes) {
        for (auto &glyph : size.glyphs) {
            glyph.src = resolve_source_path(resource_dir, glyph.src);
        }
    }
    return resource;
}

gui::RuntimeImageResource resolve_image_resource_sources(
    gui::RuntimeImageResource resource,
    std::string_view resource_dir
)
{
    resource.primary_src = resolve_source_path(resource_dir, resource.primary_src);
    return resource;
}

std::vector<std::filesystem::path> make_icon_search_dirs(
    const AppManifest &manifest,
    const std::filesystem::path &resource_dir
)
{
    std::vector<std::filesystem::path> dirs;
    if (!resource_dir.empty()) {
        append_unique_path(dirs, resource_dir / APP_ICON_IMAGE_DIR);
        append_unique_path(dirs, resource_dir);
    }
    if (manifest.kind == AppKind::Runtime && !manifest.app_path.empty()) {
        append_unique_path(dirs, std::filesystem::path(manifest.app_path) / APP_ICON_IMAGE_DIR);
    }
    return dirs;
}

std::expected<void, std::string> collect_image_descriptor_paths(
    const std::filesystem::path &dir,
    std::vector<std::filesystem::path> &paths
)
{
    auto dir_info = service::helper::Storage::fs_stat(dir.generic_string(), STORAGE_FS_TIMEOUT_MS);
    if (!dir_info || !dir_info->exists || dir_info->type != service::helper::Storage::FileType::Directory) {
        return {};
    }

    auto entries = service::helper::Storage::fs_list(dir.generic_string(), STORAGE_FS_TIMEOUT_MS);
    if (!entries) {
        return std::unexpected(entries.error());
    }
    for (const auto &entry : entries.value()) {
        auto entry_path = dir / entry.name;
        if (entry.info.type == service::helper::Storage::FileType::Directory) {
            auto child_result = collect_image_descriptor_paths(entry_path, paths);
            if (!child_result) {
                return child_result;
            }
            continue;
        }
        if (entry.info.type == service::helper::Storage::FileType::File &&
                entry_path.extension() == IMAGE_DESCRIPTOR_EXTENSION) {
            paths.push_back(std::move(entry_path));
        }
    }
    return {};
}

std::optional<std::filesystem::path> find_icon_descriptor_path(
    const AppManifest &manifest,
    const std::filesystem::path &resource_dir
)
{
    if (manifest.icon_id.empty()) {
        return std::nullopt;
    }

    for (const auto &dir : make_icon_search_dirs(manifest, resource_dir)) {
        std::vector<std::filesystem::path> descriptor_paths;
        auto collect_result = collect_image_descriptor_paths(dir, descriptor_paths);
        if (!collect_result) {
            BROOKESIA_LOGW("Failed to scan app icon directory: path(%1%), error(%2%)", dir.string(), collect_result.error());
            continue;
        }
        for (const auto &path : descriptor_paths) {
            auto images = gui::parse_image_asset_set_file(path.generic_string());
            if (!images) {
                continue;
            }
            auto matches_icon_id = [&manifest](const gui::ImageAsset & image) {
                return image.id == manifest.icon_id;
            };
            const auto matched = std::find_if(images->begin(), images->end(), matches_icon_id);
            if (matched != images->end()) {
                return path;
            }
        }
    }
    return std::nullopt;
}

std::expected<std::optional<gui::ImageAsset>, std::string> load_app_icon_image(
    AppManifest &manifest,
    const std::filesystem::path &resource_dir
)
{
    if (manifest.icon_id.empty()) {
        return std::optional<gui::ImageAsset> {};
    }

    const auto icon_anchor = manifest.kind == AppKind::Runtime && !manifest.app_path.empty() ?
                             std::filesystem::path(manifest.app_path) :
                             resource_dir;
    std::filesystem::path icon_file;
    if (!manifest.icon_path.empty()) {
        std::filesystem::path icon_path(manifest.icon_path);
        icon_file = icon_path.is_absolute() ?
                    icon_path.lexically_normal() :
                    (icon_anchor / icon_path).lexically_normal();
    } else {
        auto discovered = find_icon_descriptor_path(manifest, resource_dir);
        if (!discovered) {
            return std::optional<gui::ImageAsset> {};
        }
        icon_file = discovered->lexically_normal();
        if (!icon_anchor.empty()) {
            manifest.icon_path = icon_file.lexically_relative(icon_anchor).generic_string();
        } else {
            manifest.icon_path = icon_file.generic_string();
        }
    }

    auto images = gui::parse_image_asset_set_file(icon_file.generic_string());
    if (!images) {
        return std::unexpected("Failed to parse app icon resource: " + images.error());
    }
    auto matches_icon_id = [&manifest](const gui::ImageAsset & image) {
        return image.id == manifest.icon_id;
    };
    const auto matched = std::find_if(images->begin(), images->end(), matches_icon_id);
    if (matched == images->end()) {
        return std::unexpected("App icon image id does not match manifest icon_id");
    }
    return *matched;
}

} // namespace

std::string System::Impl::resolve_app_resource_dir(const AppManifest &manifest) const
{
    if (manifest.kind == AppKind::Runtime && !manifest.app_path.empty()) {
        return join_resource_path(manifest.app_path, manifest.resource_dir);
    }

    std::filesystem::path resource_dir(manifest.resource_dir);
    if (resource_dir.is_absolute()) {
        return resource_dir.lexically_normal().generic_string();
    }
    const auto app_root = storage_layout_.internal.available ?
                          (std::filesystem::path(storage_layout_.internal.root_path) / "apps").lexically_normal() :
                          std::filesystem::path();
    if (manifest.resource_dir.empty()) {
        return app_root.generic_string();
    }
    return (app_root / resource_dir).lexically_normal().generic_string();
}

std::string System::Impl::resolve_app_resource_path(
    const AppManifest &manifest,
    std::string_view path
) const
{
    return join_resource_path(resolve_app_resource_dir(manifest), path);
}

void System::Impl::unregister_app_gui_resources(AppRecord &record)
{
    if (!gui_runtime_) {
        record.registered_image_resource_ids.clear();
        record.registered_font_resource_ids.clear();
        return;
    }
    const auto app_id = record.info.app_id;
    for (auto it = record.registered_image_resource_ids.rbegin();
            it != record.registered_image_resource_ids.rend(); ++it) {
        auto owner_it = image_resource_owners_.find(*it);
        if (owner_it != image_resource_owners_.end() && owner_it->second == app_id) {
            gui_runtime_->unregister_image(*it);
            image_resource_owners_.erase(owner_it);
        }
    }
    for (auto it = record.registered_font_resource_ids.rbegin();
            it != record.registered_font_resource_ids.rend(); ++it) {
        auto owner_it = font_resource_owners_.find(*it);
        if (owner_it != font_resource_owners_.end() && owner_it->second == app_id) {
            gui_runtime_->unregister_font(*it);
            font_resource_owners_.erase(owner_it);
        }
    }
    record.registered_image_resource_ids.clear();
    record.registered_font_resource_ids.clear();
}


void System::Impl::unregister_app_icon_resource(AppRecord &record)
{
    if (!gui_runtime_ || record.registered_icon_resource_id.empty()) {
        record.registered_icon_resource_id.clear();
        return;
    }
    auto owner_it = image_resource_owners_.find(record.registered_icon_resource_id);
    if (owner_it != image_resource_owners_.end() && owner_it->second == record.info.app_id) {
        gui_runtime_->unregister_image(record.registered_icon_resource_id);
        image_resource_owners_.erase(owner_it);
    }
    record.registered_icon_resource_id.clear();
}


std::expected<void, std::string> System::Impl::register_app_icon_resource(AppRecord &record)
{
    if (!record.info.manifest.visible) {
        return {};
    }
    if (record.info.manifest.icon_path.empty() && record.info.manifest.icon_id.empty()) {
        return {};
    }
    if (!record.registered_icon_resource_id.empty()) {
        return {};
    }
    if (!gui_runtime_) {
        return {};
    }

    auto image = load_app_icon_image(record.info.manifest, resolve_app_resource_dir(record.info.manifest));
    if (!image) {
        return std::unexpected(image.error());
    }
    if (!image->has_value()) {
        return {};
    }

    const auto &resource_id = resolve_app_icon_resource_id(record.info.manifest);
    if (!resource_id.empty()) {
        auto owner_it = image_resource_owners_.find(resource_id);
        if (owner_it != image_resource_owners_.end() && owner_it->second != record.info.app_id) {
            return std::unexpected("Image resource id already registered by another app: " + resource_id);
        }
    }

    auto result = gui_runtime_->register_image(gui::RuntimeImageResource{
        .id = resource_id,
        .primary_src = (*image)->src,
        .native_src = 0,
        .width = (*image)->width,
        .height = (*image)->height,
    });
    if (!result) {
        return std::unexpected("Failed to register app icon resource: " + result.error());
    }
    if (!resource_id.empty()) {
        image_resource_owners_.insert_or_assign(resource_id, record.info.app_id);
        record.registered_icon_resource_id = resource_id;
    }
    BROOKESIA_LOGD(
        "Registered app icon resource: app(%1%), local_icon_id(%2%), resource_id(%3%)",
        record.info.manifest.id,
        record.info.manifest.icon_id,
        resource_id
    );
    return {};
}


std::expected<void, std::string> System::Impl::register_app_gui_resources(AppRecord &record)
{
    if (record.info.manifest.kind != AppKind::Native || !record.native_app) {
        return {};
    }
    if (!record.registered_image_resource_ids.empty() || !record.registered_font_resource_ids.empty()) {
        return {};
    }

    const auto &resources = record.gui_descriptor.resources;
    if (resources.images.empty() && resources.fonts.empty()) {
        return {};
    }
    if (!gui_runtime_) {
        return std::unexpected("GUI runtime is not available");
    }

    std::set<std::string> font_ids;
    for (const auto &font : resources.fonts) {
        if (!font_ids.insert(font.id).second) {
            return std::unexpected("Duplicate app font resource id: " + font.id);
        }
        auto owner_it = font_resource_owners_.find(font.id);
        if (owner_it != font_resource_owners_.end() && owner_it->second != record.info.app_id) {
            return std::unexpected("Font resource id already registered by another app: " + font.id);
        }
    }

    std::set<std::string> image_ids;
    for (const auto &image : resources.images) {
        if (!image_ids.insert(image.id).second) {
            return std::unexpected("Duplicate app image resource id: " + image.id);
        }
        auto owner_it = image_resource_owners_.find(image.id);
        if (owner_it != image_resource_owners_.end() && owner_it->second != record.info.app_id) {
            return std::unexpected("Image resource id already registered by another app: " + image.id);
        }
    }

    const auto resource_dir = resolve_app_resource_dir(record.info.manifest);
    for (const auto &font : resources.fonts) {
        auto result = gui_runtime_->register_font(resolve_font_resource_sources(font, resource_dir));
        if (!result) {
            unregister_app_gui_resources(record);
            return std::unexpected("Failed to register app font resource '" + font.id + "': " + result.error());
        }
        font_resource_owners_.insert_or_assign(font.id, record.info.app_id);
        record.registered_font_resource_ids.push_back(font.id);
    }
    for (const auto &image : resources.images) {
        auto result = gui_runtime_->register_image(resolve_image_resource_sources(image, resource_dir));
        if (!result) {
            unregister_app_gui_resources(record);
            return std::unexpected("Failed to register app image resource '" + image.id + "': " + result.error());
        }
        image_resource_owners_.insert_or_assign(image.id, record.info.app_id);
        record.registered_image_resource_ids.push_back(image.id);
    }
    return {};
}



} // namespace esp_brookesia::system::core
