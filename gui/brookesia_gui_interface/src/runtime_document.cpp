/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/runtime_impl.hpp"

namespace esp_brookesia::gui {

std::shared_ptr<Runtime::Impl::NodeRecord::InteractionState> Runtime::Impl::find_interaction_state(
    const TreeRecord &tree, uint64_t uid)
{
    auto it = std::find_if(
    tree.interaction_records.begin(), tree.interaction_records.end(), [uid](const auto & record) {
        return record.uid == uid;
    }
              );
    return it == tree.interaction_records.end() ? nullptr : it->state;
}
std::shared_ptr<Runtime::Impl::NodeRecord::InteractionState> &Runtime::Impl::ensure_interaction_state(
    TreeRecord &tree, NodeUid uid)
{
    auto it = std::find_if(
    tree.interaction_records.begin(), tree.interaction_records.end(), [uid](const auto & record) {
        return record.uid == uid;
    }
              );
    if (it == tree.interaction_records.end()) {
        tree.interaction_records.push_back(InteractionRecord{
            .uid = uid,
            .state = std::make_shared<NodeRecord::InteractionState>(),
        });
        return tree.interaction_records.back().state;
    }
    return it->state;
}
void Runtime::Impl::erase_interaction_state(TreeRecord &tree, NodeUid uid)
{
    auto it = std::find_if(
    tree.interaction_records.begin(), tree.interaction_records.end(), [uid](const auto & record) {
        return record.uid == uid;
    }
              );
    if (it != tree.interaction_records.end()) {
        tree.interaction_records.erase(it);
    }
}
const Runtime::Impl::NodeRecord *Runtime::Impl::find_root_node_record(const TreeRecord &tree, const NodeRecord &record)
{
    const NodeRecord *current = &record;
    while (current->parent_uid != 0) {
        auto parent_it = tree.nodes.find(current->parent_uid);
        if (parent_it == tree.nodes.end()) {
            return nullptr;
        }
        current = &parent_it->second;
    }
    return current;
}
Path Runtime::Impl::build_relative_node_path(const TreeRecord &tree, const NodeRecord &record)
{
    std::vector<std::string> reversed_segments;
    const NodeRecord *current = &record;
    while (current->parent_uid != 0) {
        reversed_segments.emplace_back(current->node_id());
        auto parent_it = tree.nodes.find(current->parent_uid);
        if (parent_it == tree.nodes.end()) {
            return {};
        }
        current = &parent_it->second;
    }
    std::reverse(reversed_segments.begin(), reversed_segments.end());
    return Path(std::move(reversed_segments));
}
Runtime::Impl::Impl(std::unique_ptr<IBackend> backend_in, RuntimeTaskConfig task_config_in )
    : backend(std::move(backend_in))
    , store(std::make_shared<MemoryDataStore>())
    , task_config(std::move(task_config_in))
{
    if (backend != nullptr) {
        auto sink = [this](const BackendEvent & event) {
            if (try_dispatch_fast_action_event(event)) {
                return;
            }
            if (task_config.task_scheduler && task_config.task_scheduler->is_running() &&
                    !task_config.event_group.empty()) {
                auto backend_event = event;
                auto post_result = task_config.task_scheduler->post(
                [this, backend_event = std::move(backend_event)]() {
                    dispatch_backend_event(backend_event);
                },
                nullptr,
                task_config.event_group
                                   );
                if (!post_result) {
                    BROOKESIA_LOGW("Failed to post backend event, dispatch inline");
                    dispatch_backend_event(event);
                }
                return;
            }
            dispatch_backend_event(event);
        };
        backend->set_event_sink(std::move(sink));
        refresh_global_fonts_from_backend();
        for (const auto &[font_id, unused_font] : global_fonts) {
            (void)unused_font;
            font_registration_order.push_back(font_id);
        }
        std::sort(font_registration_order.begin(), font_registration_order.end());
    }
}
void Runtime::Impl::register_document_subscription(SubscriptionId subscription_id, DocumentId document_id)
{
    if (subscription_id == 0 || !document_id.is_valid()) {
        return;
    }
    subscription_document_ids_[subscription_id] = document_id.value();
}
bool Runtime::Impl::unsubscribe_subscription(SubscriptionId subscription_id)
{
    if (subscription_registry_ == nullptr || subscription_id == 0) {
        return false;
    }

    subscription_document_ids_.erase(subscription_id);

    auto it = subscription_registry_->disconnect_handlers.find(subscription_id);
    if (it == subscription_registry_->disconnect_handlers.end()) {
        return false;
    }

    auto disconnect_handler = it->second;
    subscription_registry_->disconnect_handlers.erase(it);
    if (disconnect_handler == nullptr || !*disconnect_handler) {
        return false;
    }

    auto handler = std::move(*disconnect_handler);
    handler();
    return true;
}
Runtime::Impl::~Impl()
{
    std::vector<DocumentId::Value> document_ids;
    document_ids.reserve(trees.size());
    for (const auto &[document_id, unused_tree] : trees) {
        (void)unused_tree;
        document_ids.push_back(document_id);
    }
    for (auto document_id : document_ids) {
        unload(DocumentId(document_id));
    }
}
std::expected<DocumentId, std::string> Runtime::Impl::load(
    std::string_view file_path_view,
    Document document,
    const Environment &environment,
    bool file_backed,
    std::vector<std::string> dependency_files,
    DocumentLoadOptions options,
    std::map<std::string, std::string> deferred_screen_files,
    std::map<std::string, std::string> view_template_json,
    std::map<std::string, std::string> interaction_template_json)
{
    if (backend == nullptr) {
        return std::unexpected("GUI backend is null");
    }

    const std::string file_path = normalize_file_path(file_path_view);
    const auto total_start = RuntimeProfileClock::now();
    auto stage_start = total_start;
    auto validation = validate_document(document);
    auto stage_end = RuntimeProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI runtime load profile: file(%1%), stage(validate_document), elapsed_ms(%2%), total_ms(%3%)",
        file_path,
        runtime_profile_elapsed_ms(stage_start, stage_end),
        runtime_profile_elapsed_ms(total_start, stage_end)
    );
    if (!validation.success) {
        return std::unexpected(validation.errors.empty() ? "Invalid GUI document" : validation.errors.front());
    }

    const DocumentId document_id(next_document_id_++);

    try {
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        auto hp_snap = []() {
            return ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
        };
        auto hp_log = [document_id](const char *stage, const auto & prev, const auto & cur) {
            BROOKESIA_LOGI(
                "[HeapTrace][gui.load.split] doc(%1%) stage(%2%) "
                "internal_free(%3%) internal_largest(%4%) internal_delta(%5%) "
                "psram_free(%6%) psram_delta(%7%)",
                document_id.value(),
                stage,
                cur.internal_free,
                cur.internal_largest,
                static_cast<int64_t>(cur.internal_free) - static_cast<int64_t>(prev.internal_free),
                cur.external_free,
                static_cast<int64_t>(cur.external_free) - static_cast<int64_t>(prev.external_free)
            );
        };
        auto hp_entry = hp_snap();
#endif
        stage_start = RuntimeProfileClock::now();
        TreeRecord tree;
        tree.document_id = document_id;
        tree.file_path = file_path;
        tree.file_backed = file_backed;
        tree.environment_dependencies = document.environment_dependencies;
        tree.theme_sensitive = document.theme_sensitive;
        tree.constants = std::move(document.constants);
        tree.styles = std::move(document.styles);
        // Freeze the effective defaults at document load time. Runtime-wide theme/language
        // changes only affect documents loaded later; existing trees must not start mixing a
        // new default into nodes created lazily from templates.
        tree.environment = make_effective_environment(environment);
        tree.dependency_files = normalize_dependency_files(std::move(dependency_files));
        stage_end = RuntimeProfileClock::now();
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime load profile: doc(%1%), file(%2%), stage(prepare_tree), "
            "dependencies(%3%), elapsed_ms(%4%), total_ms(%5%)",
            document_id.value(),
            file_path,
            tree.dependency_files.size(),
            runtime_profile_elapsed_ms(stage_start, stage_end),
            runtime_profile_elapsed_ms(total_start, stage_end)
        );

        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime load profile: doc(%1%), file(%2%), stage(capture_dependency_mtimes), "
            "dependencies(%3%), hits(0), skipped(true), elapsed_ms(0), total_ms(%4%)",
            document_id.value(),
            file_path,
            tree.dependency_files.size(),
            runtime_profile_elapsed_ms(total_start, RuntimeProfileClock::now())
        );

        stage_start = RuntimeProfileClock::now();
        boost::unordered_flat_set<std::string> entry_screen_ids;
        for (const auto &flow : document.screen_flows) {
            if (!flow.initial.empty()) {
                entry_screen_ids.insert(flow.initial);
            }
        }
        if (entry_screen_ids.empty() && !document.screens.empty()) {
            entry_screen_ids.insert(document.screens.front().id);
        }
        for (auto &image : document.images) {
            auto image_id = image.id;
            BROOKESIA_LOGD(
                "Loaded document image asset: id='%1%', src='%2%', width=%3%, height=%4%, preload=%5%",
                image.id,
                image.src,
                image.width,
                image.height,
                image.preload
            );
            tree.images.emplace(std::move(image_id), std::move(image));
        }
        for (auto &flow : document.screen_flows) {
            auto flow_id = flow.id;
            tree.screen_flows.emplace(std::move(flow_id), std::move(flow));
        }
        for (auto &node : document.screens) {
            auto node_id = node.id;
            tree.screens.emplace(std::move(node_id), std::move(node));
        }
        for (auto &node : document.templates) {
            auto node_id = node.id;
            tree.templates.emplace(std::move(node_id), std::move(node));
        }
        stage_end = RuntimeProfileClock::now();
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime load profile: doc(%1%), file(%2%), stage(index_assets), "
            "images(%3%), flows(%4%), screens(%5%), templates(%6%), elapsed_ms(%7%), total_ms(%8%)",
            document_id.value(),
            file_path,
            tree.images.size(),
            tree.screen_flows.size(),
            tree.screens.size(),
            tree.templates.size(),
            runtime_profile_elapsed_ms(stage_start, stage_end),
            runtime_profile_elapsed_ms(total_start, stage_end)
        );

        tree.deferred_screen_files = std::move(deferred_screen_files);
        tree.view_template_json = std::move(view_template_json);
        tree.interaction_template_json = std::move(interaction_template_json);
        trees.emplace(document_id.value(), std::move(tree));
        auto *stored_tree = resolve_tree(document_id);
        if (stored_tree == nullptr) {
            return std::unexpected("Failed to store GUI tree");
        }
        stage_start = RuntimeProfileClock::now();
        auto resource_validation = validate_resource_references(*stored_tree);
        stage_end = RuntimeProfileClock::now();
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime load profile: doc(%1%), file(%2%), stage(validate_resource_references), "
            "elapsed_ms(%3%), total_ms(%4%)",
            document_id.value(),
            file_path,
            runtime_profile_elapsed_ms(stage_start, stage_end),
            runtime_profile_elapsed_ms(total_start, stage_end)
        );
        if (!resource_validation) {
            unload(document_id);
            return std::unexpected(resource_validation.error());
        }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        auto hp_before_preload = hp_snap();
        hp_log("after tree move+validate (H_C)", hp_entry, hp_before_preload);
#endif
        stage_start = RuntimeProfileClock::now();
        auto preload_result = preload_tree_image_resources(*stored_tree);
        stage_end = RuntimeProfileClock::now();
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime load profile: doc(%1%), file(%2%), stage(preload_image_resources), "
            "preloaded(%3%), elapsed_ms(%4%), total_ms(%5%)",
            document_id.value(),
            file_path,
            stored_tree->preloaded_images.size(),
            runtime_profile_elapsed_ms(stage_start, stage_end),
            runtime_profile_elapsed_ms(total_start, stage_end)
        );
        if (!preload_result) {
            unload(document_id);
            return std::unexpected(preload_result.error());
        }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        auto hp_after_preload = hp_snap();
        hp_log("after image preload (H_A)", hp_before_preload, hp_after_preload);
#endif

        const auto eager_stage_start = RuntimeProfileClock::now();
        size_t eager_screen_count = 0;
        size_t deferred_screen_count = 0;
        size_t dynamic_screen_count = 0;
        stored_tree->pending_warm_screens.clear();
        stored_tree->idle_warm_scheduled = false;
        for (const auto &[unused_id, screen] : stored_tree->screens) {
            (void)unused_id;
            if (screen.mount_mode == MountMode::Dynamic) {
                ++dynamic_screen_count;
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
                BROOKESIA_LOGI(
                    "[HeapTrace][gui.eager_mount] screen(%1%) mode(dynamic) action(skipped)", screen.id);
#endif
                continue;
            }

            const bool is_entry = entry_screen_ids.contains(screen.id);
            const bool defer_screen =
                !options.build_entry_screens ||
                (options.defer_non_entry_eager_screens && !is_entry);
            if (defer_screen) {
                stored_tree->pending_warm_screens.push_back(screen.id);
                ++deferred_screen_count;
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
                BROOKESIA_LOGI(
                    "[HeapTrace][gui.eager_mount] screen(%1%) mode(eager) action(deferred_warm)", screen.id);
#endif
                continue;
            }

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            auto hp_load_eager_before = hp_snap();
#endif
            stage_start = RuntimeProfileClock::now();
            auto root_uid = ensure_screen_built(document_id, *stored_tree, screen.id);
            if (!root_uid) {
                unload(document_id);
                return std::unexpected(root_uid.error());
            }
            stage_end = RuntimeProfileClock::now();
            const auto screen_build_ms = runtime_profile_elapsed_ms(stage_start, stage_end);
            ++eager_screen_count;
            GUI_INTERFACE_PROFILE_LOGI(
                "GUI runtime load profile: doc(%1%), file(%2%), stage(eager_screen_build), "
                "screen(%3%), elapsed_ms(%4%), total_ms(%5%)",
                document_id.value(),
                file_path,
                screen.id,
                screen_build_ms,
                runtime_profile_elapsed_ms(total_start, stage_end)
            );
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            auto hp_load_eager_after = hp_snap();
            hp_log((std::string("eager build screen ") + screen.id).c_str(),
                   hp_load_eager_before, hp_load_eager_after);
#endif
        }
        stage_end = RuntimeProfileClock::now();
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime load profile: doc(%1%), file(%2%), stage(eager_screen_build_total), "
            "eager_screens(%3%), deferred_screens(%4%), dynamic_screens(%5%), elapsed_ms(%6%), total_ms(%7%)",
            document_id.value(),
            file_path,
            eager_screen_count,
            deferred_screen_count,
            dynamic_screen_count,
            runtime_profile_elapsed_ms(eager_stage_start, stage_end),
            runtime_profile_elapsed_ms(total_start, stage_end)
        );
        if (options.schedule_idle_warm && !stored_tree->pending_warm_screens.empty()) {
            request_idle_screen_warm(document_id);
        }

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        auto hp_after_screens = hp_snap();
        hp_log("after eager screen build (H_B)", hp_after_preload, hp_after_screens);
        hp_log("total root load (H_A+H_B+H_C)", hp_entry, hp_after_screens);
#endif
        stage_end = RuntimeProfileClock::now();
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime load profile: doc(%1%), file(%2%), stage(total), elapsed_ms(%3%), total_ms(%4%)",
            document_id.value(),
            file_path,
            runtime_profile_elapsed_ms(total_start, stage_end),
            runtime_profile_elapsed_ms(total_start, stage_end)
        );
        return document_id;
    } catch (const std::bad_alloc &) {
        unload(document_id);
        return std::unexpected("Out of memory while loading GUI document: " + file_path);
    } catch (const std::exception &e) {
        unload(document_id);
        return std::unexpected(
                   "Exception while loading GUI document: " + file_path + ", error=" + std::string(e.what())
               );
    } catch (...) {
        unload(document_id);
        return std::unexpected("Unknown exception while loading GUI document: " + file_path);
    }
}
std::vector<RuntimeImageResource> Runtime::Impl::collect_image_resources(const TreeRecord &tree) const
{
    boost::unordered_flat_map<std::string, std::size_t> resource_indices;
    std::vector<RuntimeImageResource> resources;
    resources.reserve(tree.images.size() + global_images.size());

    auto append_resource = [&resource_indices, &resources](RuntimeImageResource resource) {
        const auto key = image_resource_cache_key(resource);
        auto [it, inserted] = resource_indices.emplace(key, resources.size());
        if (!inserted) {
            resources[it->second].preload = resources[it->second].preload || resource.preload;
            return;
        }
        resources.push_back(std::move(resource));
    };

    for (const auto &[unused_id, image] : tree.images) {
        (void)unused_id;
        append_resource(RuntimeImageResource{
            .id = image.id,
            .primary_src = image.src,
            .native_src = 0,
            .width = image.width,
            .height = image.height,
            .preload = image.preload,
        });
    }
    for (const auto &[unused_id, image] : global_images) {
        (void)unused_id;
        append_resource(image);
    }
    return resources;
}
std::string Runtime::Impl::image_resource_cache_key(const RuntimeImageResource &resource)
{
    return resource.primary_src.empty() ?
           std::string("native:") + std::to_string(resource.native_src) :
           std::string("file:") + resource.primary_src;
}
bool Runtime::Impl::is_same_image_resource(
    const RuntimeImageResource &lhs,
    const RuntimeImageResource &rhs)
{
    return lhs.primary_src == rhs.primary_src && lhs.native_src == rhs.native_src;
}
bool Runtime::Impl::should_preload_image_resource_automatically(const RuntimeImageResource &resource) const
{
    return resource.preload || (backend != nullptr && backend->requires_preloaded_image_resource(resource));
}
bool Runtime::Impl::has_preloaded_image_resource(const TreeRecord &tree, const RuntimeImageResource &resource) const
{
    return std::any_of(
               tree.preloaded_images.begin(),
               tree.preloaded_images.end(),
    [&resource](const PreloadedImageRecord & preloaded) {
        return is_same_image_resource(preloaded.resource, resource) &&
               (preloaded.automatic_ref_count > 0 || preloaded.manual_ref_count > 0);
    }
           );
}
bool Runtime::Impl::has_automatic_preloaded_image_resource(const TreeRecord &tree, const RuntimeImageResource &resource) const
{
    return std::any_of(
               tree.preloaded_images.begin(),
               tree.preloaded_images.end(),
    [&resource](const PreloadedImageRecord & preloaded) {
        return is_same_image_resource(preloaded.resource, resource) && preloaded.automatic_ref_count > 0;
    }
           );
}
RuntimeImageResource Runtime::Impl::make_runtime_image_resource(
    const ImageProps &image_props,
    const ResolvedImageSpec &resolved_image
)
{
    const auto primary_src = resolved_image.primary_src.empty() ? image_props.src : resolved_image.primary_src;
    return RuntimeImageResource{
        .id = image_props.src,
        .primary_src = primary_src,
        .native_src = resolved_image.native_src,
        .width = resolved_image.width,
        .height = resolved_image.height,
    };
}
RuntimeImageResource Runtime::Impl::make_runtime_image_resource(const Node &node)
{
    return make_runtime_image_resource(node.image_props, node.resolved_image);
}
RuntimeImageResource Runtime::Impl::make_runtime_image_resource(const NodeRecord &record)
{
    return make_runtime_image_resource(record.image_props(), record.resolved_image());
}
std::expected<void, std::string> Runtime::Impl::preload_image_resource_for_tree(
    TreeRecord &tree,
    const RuntimeImageResource &resource,
    ImagePreloadOwner owner)
{
    if (backend == nullptr) {
        return {};
    }

    auto it = std::find_if(
                  tree.preloaded_images.begin(),
                  tree.preloaded_images.end(),
    [&resource](const PreloadedImageRecord & preloaded) {
        return is_same_image_resource(preloaded.resource, resource);
    }
              );
    if (it != tree.preloaded_images.end()) {
        if (owner == ImagePreloadOwner::Automatic) {
            ++it->automatic_ref_count;
        } else {
            ++it->manual_ref_count;
        }
        it->resource.preload = it->resource.preload || resource.preload;
        return {};
    }

    auto result = backend->preload_image_resource(resource);
    if (!result) {
        return result;
    }
    PreloadedImageRecord record {
        .resource = resource,
        .automatic_ref_count = owner == ImagePreloadOwner::Automatic ? 1U : 0U,
        .manual_ref_count = owner == ImagePreloadOwner::Manual ? 1U : 0U,
    };
    tree.preloaded_images.push_back(std::move(record));
    return {};
}
std::expected<void, std::string> Runtime::Impl::preload_tree_image_resources(TreeRecord &tree)
{
    for (const auto &resource : collect_image_resources(tree)) {
        if (!resource.preload) {
            continue;
        }
        auto result = preload_image_resource_for_tree(tree, resource, ImagePreloadOwner::Automatic);
        if (!result) {
            release_tree_image_resources(tree);
            return result;
        }
    }
    return {};
}
std::expected<void, std::string> Runtime::Impl::ensure_image_resource_preloaded_for_tree(
    TreeRecord &tree,
    const RuntimeImageResource &resource)
{
    if (!should_preload_image_resource_automatically(resource) || has_preloaded_image_resource(tree, resource)) {
        return {};
    }

    return preload_image_resource_for_tree(tree, resource, ImagePreloadOwner::Automatic);
}
std::expected<void, std::string> Runtime::Impl::ensure_node_image_resources_preloaded(TreeRecord &tree, const Node &node)
{
    if (node.type == NodeType::Image && !node.image_props.src.empty()) {
        const auto primary_src = node.resolved_image.primary_src.empty() ?
                                 node.image_props.src :
                                 node.resolved_image.primary_src;
        RuntimeImageResource resource{
            .id = node.resolved_image.image_id.empty() ? node.image_props.src : node.resolved_image.image_id,
            .primary_src = primary_src,
            .native_src = node.resolved_image.native_src,
            .width = node.resolved_image.width,
            .height = node.resolved_image.height,
        };
        auto result = ensure_image_resource_preloaded_for_tree(tree, resource);
        if (!result) {
            return std::unexpected(
                       "Failed to preload image resource '" + resource.id + "': " + result.error()
                   );
        }
    }

    if (node.type != NodeType::Keyboard) {
        return {};
    }
    for (const auto &[unused_mode, layout] : node.keyboard_props.layouts) {
        (void)unused_mode;
        for (const auto &row : layout.rows) {
            for (const auto &key : row) {
                if (key.resolved_image.primary_src.empty() && key.resolved_image.native_src == 0) {
                    continue;
                }
                RuntimeImageResource resource{
                    .id = key.resolved_image.image_id.empty() ? key.image : key.resolved_image.image_id,
                    .primary_src = key.resolved_image.primary_src,
                    .native_src = key.resolved_image.native_src,
                    .width = key.resolved_image.width,
                    .height = key.resolved_image.height,
                };
                auto result = ensure_image_resource_preloaded_for_tree(tree, resource);
                if (!result) {
                    return std::unexpected(
                               "Failed to preload keyboard image resource '" + resource.id + "': " +
                               result.error()
                           );
                }
            }
        }
    }
    return {};
}
void Runtime::Impl::release_image_resource_from_tree(
    TreeRecord &tree,
    const RuntimeImageResource &resource,
    ImagePreloadOwner owner )
{
    if (backend == nullptr) {
        return;
    }
    auto it = std::find_if(
                  tree.preloaded_images.begin(),
                  tree.preloaded_images.end(),
    [&resource](const PreloadedImageRecord & preloaded) {
        return is_same_image_resource(preloaded.resource, resource);
    }
              );
    if (it == tree.preloaded_images.end()) {
        return;
    }

    if (owner == ImagePreloadOwner::Automatic) {
        if (it->automatic_ref_count > 0) {
            --it->automatic_ref_count;
        }
    } else if (owner == ImagePreloadOwner::All) {
        it->automatic_ref_count = 0;
    }
    if (owner == ImagePreloadOwner::Manual) {
        if (it->manual_ref_count > 0) {
            --it->manual_ref_count;
        }
    } else if (owner == ImagePreloadOwner::All) {
        it->manual_ref_count = 0;
    }
    if (it->automatic_ref_count > 0 || it->manual_ref_count > 0) {
        return;
    }
    backend->release_image_resource(it->resource);
    tree.preloaded_images.erase(it);
}
bool Runtime::Impl::tree_references_image_resource_except(
    const TreeRecord &tree,
    const NodeRecord &excluded_record,
    std::string_view image_id,
    const RuntimeImageResource &resource) const
{
    for (const auto &[unused_uid, record] : tree.nodes) {
        (void)unused_uid;
        if (&record == &excluded_record) {
            continue;
        }
        if (node_references_image(record, image_id)) {
            return true;
        }
        if (record.node().type == NodeType::Image && !record.image_props().src.empty() &&
                is_same_image_resource(make_runtime_image_resource(record), resource)) {
            return true;
        }
    }
    return false;
}
std::expected<void, std::string> Runtime::Impl::update_image_source(
    TreeRecord &tree,
    NodeRecord &record,
    std::string_view src)
{
    if (record.node().type != NodeType::Image) {
        return std::unexpected("Node is not an image");
    }

    auto &image_props = record.mutable_image_props();
    auto &resolved_image = record.mutable_resolved_image();
    const auto previous_src = image_props.src;
    const auto previous_resolved_image = resolved_image;
    const RuntimeImageResource previous_resource = make_runtime_image_resource(image_props, resolved_image);

    image_props.src = std::string(src);
    resolved_image = resolve_image_spec(tree, image_props.src);

    const RuntimeImageResource next_resource = make_runtime_image_resource(image_props, resolved_image);
    if (!image_props.src.empty() && should_preload_image_resource_automatically(next_resource) &&
            !has_preloaded_image_resource(tree, next_resource)) {
        auto preload_result =
            preload_image_resource_for_tree(tree, next_resource, ImagePreloadOwner::Automatic);
        if (!preload_result) {
            image_props.src = previous_src;
            resolved_image = previous_resolved_image;
            return std::unexpected(
                       "Failed to preload image resource '" + std::string(src) + "': " + preload_result.error()
                   );
        }
    }

    if (!previous_src.empty() && previous_src != image_props.src &&
            !is_same_image_resource(previous_resource, next_resource) &&
            !tree_references_image_resource_except(tree, record, previous_src, previous_resource)) {
        release_image_resource_from_tree(tree, previous_resource, ImagePreloadOwner::Automatic);
    }
    return {};
}
void Runtime::Impl::release_tree_image_resources(TreeRecord &tree)
{
    if (backend != nullptr) {
        for (const auto &record : tree.preloaded_images) {
            backend->release_image_resource(record.resource);
        }
    }
    tree.preloaded_images.clear();
}
std::expected<RuntimeImageResource, std::string> Runtime::Impl::resolve_image_resource_by_id(
    const TreeRecord &tree,
    std::string_view image_id) const
{
    if (image_id.empty()) {
        return std::unexpected("Image id must not be empty");
    }

    RuntimeImageResource resource;
    auto image_it = tree.images.find(std::string(image_id));
    if (image_it != tree.images.end()) {
        resource = RuntimeImageResource{
            .id = image_it->second.id,
            .primary_src = image_it->second.src,
            .native_src = 0,
            .width = image_it->second.width,
            .height = image_it->second.height,
            .preload = image_it->second.preload,
        };
    } else {
        auto global_it = global_images.find(std::string(image_id));
        if (global_it == global_images.end()) {
            return std::unexpected("Image resource is not visible to this document: " + std::string(image_id));
        }
        resource = global_it->second;
    }

    if ((resource.width <= 0 || resource.height <= 0) && backend != nullptr) {
        auto resolved = backend->resolve_image_resource(resource);
        if (!resolved) {
            return std::unexpected("Failed to resolve image resource '" + std::string(image_id) + "': " +
                                   resolved.error());
        }
        resource = std::move(*resolved);
    }
    if (resource.width <= 0 || resource.height <= 0) {
        return std::unexpected("Image resource size must be positive: " + std::string(image_id));
    }
    return resource;
}
std::expected<void, std::string> Runtime::Impl::preload_images(DocumentId document_id, const std::vector<std::string> &image_ids)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return std::unexpected("Document not loaded");
    }

    std::vector<RuntimeImageResource> acquired_resources;
    std::vector<std::string> refreshed_ids;
    acquired_resources.reserve(image_ids.size());
    refreshed_ids.reserve(image_ids.size());
    for (const auto &image_id : image_ids) {
        auto resource = resolve_image_resource_by_id(*tree, image_id);
        if (!resource) {
            for (const auto &acquired : acquired_resources) {
                release_image_resource_from_tree(*tree, acquired, ImagePreloadOwner::Manual);
            }
            return std::unexpected(resource.error());
        }
        auto result = preload_image_resource_for_tree(*tree, *resource, ImagePreloadOwner::Manual);
        if (!result) {
            for (const auto &acquired : acquired_resources) {
                release_image_resource_from_tree(*tree, acquired, ImagePreloadOwner::Manual);
            }
            return std::unexpected(result.error());
        }
        acquired_resources.push_back(std::move(*resource));
        refreshed_ids.push_back(image_id);
    }
    boost::unordered_flat_set<std::string> refreshed;
    for (const auto &image_id : refreshed_ids) {
        if (refreshed.insert(image_id).second) {
            refresh_image_references(*tree, image_id);
        }
    }
    return {};
}
std::expected<void, std::string> Runtime::Impl::release_preloaded_images(
    DocumentId document_id,
    const std::vector<std::string> &image_ids)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return std::unexpected("Document not loaded");
    }

    std::vector<std::pair<std::string, RuntimeImageResource>> resources;
    resources.reserve(image_ids.size());
    for (const auto &image_id : image_ids) {
        auto resource = resolve_image_resource_by_id(*tree, image_id);
        if (!resource) {
            return std::unexpected(resource.error());
        }
        resources.emplace_back(image_id, std::move(*resource));
    }
    boost::unordered_flat_set<std::string> refreshed;
    for (const auto &[image_id, resource] : resources) {
        release_image_resource_from_tree(*tree, resource, ImagePreloadOwner::Manual);
        if (refreshed.insert(image_id).second) {
            refresh_image_references(*tree, image_id);
        }
    }
    return {};
}
std::expected<void, std::string> Runtime::Impl::enable_live_preview(DocumentId document_id, const LivePreviewOptions &options)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return std::unexpected("GUI document not loaded");
    }
    if (!tree->file_backed || tree->file_path.empty()) {
        return std::unexpected("Live preview is only available for documents loaded via load_file(...)");
    }
    tree->live_preview_enabled = true;
    tree->live_preview_options = options;
    tree->last_live_preview_poll = std::chrono::steady_clock::time_point::min();
    tree->dependency_mtimes = capture_dependency_mtimes(tree->dependency_files);
    return {};
}
bool Runtime::Impl::disable_live_preview(DocumentId document_id)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return false;
    }
    tree->live_preview_enabled = false;
    return true;
}
bool Runtime::Impl::unload(DocumentId document_id)
{
    auto tree_it = trees.find(document_id.value());
    if (tree_it == trees.end()) {
        return false;
    }

    pop_transient_screens_for_document(document_id);
    stop_screen_flows_for_document(document_id);
    clear_dynamic_screen_keep_alive_for_document(document_id);
    const auto event_animation_prefix = std::to_string(document_id.value()) + '\x1f';
    std::vector<std::string> event_animation_keys;
    for (const auto &[key, unused_subscription_id] : event_animation_ids_) {
        (void)unused_subscription_id;
        if (key.rfind(event_animation_prefix, 0) == 0) {
            event_animation_keys.push_back(key);
        }
    }
    for (const auto &key : event_animation_keys) {
        auto animation_it = event_animation_ids_.find(key);
        if (animation_it == event_animation_ids_.end()) {
            continue;
        }
        (void)unsubscribe_subscription(animation_it->second);
        event_animation_ids_.erase(key);
    }
    unload_partial_tree(tree_it->second);
    release_tree_image_resources(tree_it->second);
    trees.erase(tree_it);

    std::vector<std::string> mounted_targets_to_erase;
    for (const auto &[target_key, mounted_ref] : mounted_screens_) {
        if (mounted_ref.document_id == document_id) {
            mounted_targets_to_erase.push_back(target_key);
        }
    }
    for (const auto &target_key : mounted_targets_to_erase) {
        mounted_screens_.erase(target_key);
    }

    {
        std::lock_guard lock(action_registry_->mutex);
        const auto prefix = build_event_action_route_prefix(document_id);
        for (auto route_it = action_registry_->routes.begin();
                route_it != action_registry_->routes.end();) {
            if (route_it->key.compare(0, prefix.size(), prefix) != 0) {
                ++route_it;
                continue;
            }
            route_it->bucket.disconnect_all();
            route_it = action_registry_->routes.erase(route_it);
        }
        if (action_registry_->routes.empty()) {
            std::vector<ActionRoute>().swap(action_registry_->routes);
        }
    }
    if (store != nullptr) {
        store->forget_document(document_id);
    }
    // Release any per-document subscriptions whose owners forgot to unsubscribe (e.g.
    // fire-and-forget animations whose RuntimeAnimationStartResult was discarded).
    {
        const auto doc_value = document_id.value();
        std::vector<SubscriptionId> stale_subscriptions;
        stale_subscriptions.reserve(subscription_document_ids_.size());
        for (const auto &[subscription_id, owning_doc] : subscription_document_ids_) {
            if (owning_doc == doc_value) {
                stale_subscriptions.push_back(subscription_id);
            }
        }
        for (auto subscription_id : stale_subscriptions) {
            (void)unsubscribe_subscription(subscription_id);
        }
    }
    return true;
}
void Runtime::Impl::set_view_debug_enabled(bool enabled)
{
    if (view_debug_enabled_ == enabled) {
        return;
    }
    view_debug_enabled_ = enabled;
    apply_view_debug_to_all_nodes();
}
bool Runtime::Impl::is_view_debug_enabled() const
{
    return view_debug_enabled_;
}
void Runtime::Impl::poll_live_preview(Runtime *runtime)
{
    const auto now = std::chrono::steady_clock::now();
    for (auto &[unused_document_id, tree] : trees) {
        (void)unused_document_id;
        if (!tree.live_preview_enabled || !tree.file_backed || tree.file_path.empty()) {
            continue;
        }

        const auto interval_ms = std::max<int32_t>(0, tree.live_preview_options.poll_interval_ms);
        const auto interval = std::chrono::milliseconds(interval_ms);
        if (tree.last_live_preview_poll != std::chrono::steady_clock::time_point::min() &&
                now - tree.last_live_preview_poll < interval) {
            continue;
        }
        tree.last_live_preview_poll = now;

        std::string changed_file;
        bool changed = false;
        for (const auto &dependency_file : tree.dependency_files) {
            auto current_info = StorageHelper::fs_stat(dependency_file);
            if (!current_info || !current_info->exists) {
                changed = true;
                changed_file = dependency_file;
                break;
            }
            auto previous_it = tree.dependency_mtimes.find(dependency_file);
            if (previous_it == tree.dependency_mtimes.end() ||
                    previous_it->second != current_info->mtime_ms) {
                changed = true;
                changed_file = dependency_file;
                break;
            }
        }
        if (!changed) {
            continue;
        }

        if (tree.live_preview_options.log_reload) {
            BROOKESIA_LOGI(
                "Live preview reload triggered: document_id=%1%, root='%2%', changed_file='%3%'",
                tree.document_id,
                tree.file_path,
                changed_file
            );
        }

        const auto parse_environment = make_parse_environment(tree.environment);
        auto parsed_document = parse_document_file_with_metadata(tree.file_path, parse_environment);
        if (!parsed_document) {
            BROOKESIA_LOGW(
                "Live preview parse failed: document_id=%1%, root='%2%', error=%3%",
                tree.document_id,
                tree.file_path,
                parsed_document.error()
            );
            continue;
        }

        auto update_result = update(runtime, tree.document_id, tree.file_path, tree.environment, *parsed_document);
        if (!update_result) {
            BROOKESIA_LOGW(
                "Live preview update failed: document_id=%1%, root='%2%', error=%3%",
                tree.document_id,
                tree.file_path,
                update_result.error()
            );
            continue;
        }

        if (tree.live_preview_options.log_reload) {
            BROOKESIA_LOGI(
                "Live preview reload applied: document_id=%1%, root='%2%'",
                tree.document_id,
                tree.file_path
            );
        }
    }
}
}
