/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_EXPANSION_MANAGER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>

#include "private/utils.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "runtime.hpp"

namespace esp_brookesia::hal::expansion {

struct ModuleLease::Impl {
    detail::ExpansionRuntime *runtime = nullptr;
    ModuleInfo active_info;
    bool pause_all_scanning = false;
};

static_assert(std::is_nothrow_move_constructible_v<ModuleInfo>);
static_assert(std::is_nothrow_move_assignable_v<ModuleInfo>);

namespace detail {

namespace {

using SlotKey = std::pair<std::string, std::string>;
constexpr const char *SCAN_TASK_GROUP = "Expansion.Scan";
thread_local const void *current_event_dispatcher = nullptr;
thread_local const void *current_runtime_stopper = nullptr;

void set_error(std::string *error_message, std::string_view message) noexcept
{
    if (error_message == nullptr) {
        return;
    }

    try {
        error_message->assign(message.data(), message.size());
    } catch (...) {
        // Error reporting must never prevent resource cleanup or ownership handoff.
    }
}

void set_detailed_error(
    std::string *error_message, std::string_view prefix, std::string_view detail
) noexcept
{
    if (error_message == nullptr) {
        return;
    }

    try {
        error_message->assign(prefix.data(), prefix.size());
        error_message->append(detail.data(), detail.size());
    } catch (...) {
        set_error(error_message, prefix);
    }
}

void log_warning_noexcept(const char *message, const char *detail = nullptr) noexcept
{
    try {
        if (detail != nullptr) {
            BROOKESIA_LOGW("%1%: %2%", message, detail);
        } else {
            BROOKESIA_LOGW("%1%", message);
        }
    } catch (...) {
        // Logging may allocate; cleanup and lease handoff must remain noexcept.
    }
}

bool is_scan_state(ModuleState state)
{
    return (state == ModuleState::Empty) || (state == ModuleState::Invalid) ||
           (state == ModuleState::Unsupported) || (state == ModuleState::Ready);
}

bool scan_infos_equal(const ScanInfo &lhs, const ScanInfo &rhs)
{
    return (lhs.state == rhs.state) && (lhs.type == rhs.type) &&
           (lhs.board_id == rhs.board_id) && (lhs.board_name == rhs.board_name) &&
           (lhs.identity == rhs.identity);
}

bool module_payloads_equal(const ModuleInfo &lhs, const ModuleInfo &rhs)
{
    return (lhs.provider == rhs.provider) && (lhs.slot == rhs.slot) &&
           (lhs.type == rhs.type) && (lhs.board_id == rhs.board_id) &&
           (lhs.board_name == rhs.board_name) && (lhs.state == rhs.state);
}

ScanInfo make_error_sample()
{
    ScanInfo sample;
    sample.state = ModuleState::Error;
    return sample;
}

} // namespace

class ExpansionRuntime::Impl {
public:
    ~Impl()
    {
        if (stop_thread_.joinable()) {
            stop_thread_.join();
        }
        if (pending_stop_scheduler_) {
            complete_runtime_stop(pending_stop_scheduler_, pending_stop_providers_);
        }
    }

    struct ProviderEntry {
        std::shared_ptr<ModuleProvider> provider;
        std::vector<std::string> slots;
    };

    struct SlotRecord {
        ModuleInfo stable;
        uint64_t stable_identity = 0;
        std::optional<ScanInfo> candidate;
        size_t candidate_count = 0;
    };

    struct LeaseData {
        ModuleInfo active_info;
        bool pause_all_scanning = false;
    };

    struct ListenerRecord {
        ModuleManagerIface::EventListener callback;
        size_t pending_count = 0;
        bool removed = false;
    };

    struct QueuedEvent {
        ModuleInfo info;
        std::vector<std::shared_ptr<ListenerRecord>> listeners;
    };

    bool register_provider(std::shared_ptr<ModuleProvider> provider)
    {
        BROOKESIA_CHECK_NULL_RETURN(provider, false, "Expansion provider is null");

        std::string name;
        std::vector<std::string> slots;
        try {
            name = provider->get_name();
            slots = provider->get_slots();
        } catch (const std::exception &e) {
            BROOKESIA_LOGE("Expansion provider description failed: %1%", e.what());
            return false;
        } catch (...) {
            BROOKESIA_LOGE("Expansion provider description failed");
            return false;
        }

        BROOKESIA_CHECK_FALSE_RETURN(!name.empty(), false, "Expansion provider name is empty");
        BROOKESIA_CHECK_FALSE_RETURN(!slots.empty(), false, "Expansion provider '%1%' has no slots", name);
        std::sort(slots.begin(), slots.end());
        BROOKESIA_CHECK_FALSE_RETURN(
        std::none_of(slots.begin(), slots.end(), [](const auto & slot) {
            return slot.empty();
        }),
        false, "Expansion provider '%1%' has an empty slot name", name
        );
        BROOKESIA_CHECK_FALSE_RETURN(
            std::adjacent_find(slots.begin(), slots.end()) == slots.end(), false,
            "Expansion provider '%1%' has duplicate slot names", name
        );

        std::unique_lock lifecycle_lock(lifecycle_mutex_);
        if (!wait_for_runtime_stop(lifecycle_lock)) {
            return false;
        }
        std::lock_guard state_lock(state_mutex_);
        BROOKESIA_CHECK_FALSE_RETURN(
            !running_ && (consumer_count_ == 0), false,
            "Cannot register expansion provider '%1%' while running", name
        );
        BROOKESIA_CHECK_FALSE_RETURN(
            providers_.find(name) == providers_.end(), false,
            "Expansion provider '%1%' is already registered", name
        );

        providers_.emplace(name, ProviderEntry{.provider = std::move(provider), .slots = slots});
        for (const auto &slot : slots) {
            SlotRecord record;
            record.stable.provider = name;
            record.stable.slot = slot;
            slots_.emplace(SlotKey{name, slot}, std::move(record));
        }
        BROOKESIA_LOGI("Registered expansion provider '%1%' with %2% slots", name, slots.size());
        return true;
    }

    bool unregister_provider(std::string_view provider_name)
    {
        std::unique_lock lifecycle_lock(lifecycle_mutex_);
        if (!wait_for_runtime_stop(lifecycle_lock)) {
            return false;
        }
        std::lock_guard state_lock(state_mutex_);
        BROOKESIA_CHECK_FALSE_RETURN(
            !running_ && (consumer_count_ == 0), false,
            "Cannot unregister expansion provider while running"
        );

        const std::string name(provider_name);
        const auto provider_it = providers_.find(name);
        if (provider_it == providers_.end()) {
            return false;
        }
        for (const auto &slot : provider_it->second.slots) {
            slots_.erase(SlotKey{name, slot});
        }
        providers_.erase(provider_it);
        BROOKESIA_LOGI("Unregistered expansion provider '%1%'", name);
        return true;
    }

    bool has_providers() const
    {
        std::lock_guard lock(state_mutex_);
        return !providers_.empty();
    }

    bool retain_consumer(std::string *error_message)
    {
        std::unique_lock lifecycle_lock(lifecycle_mutex_);
        if (!wait_for_runtime_stop(lifecycle_lock, error_message)) {
            return false;
        }

        std::vector<std::shared_ptr<ModuleProvider>> providers;
        std::vector<std::shared_ptr<ModuleProvider>> active_providers;
        std::shared_ptr<lib_utils::TaskScheduler> scheduler;
        std::optional<lib_utils::TaskScheduler::StartConfig> scheduler_config;
        try {
            std::lock_guard state_lock(state_mutex_);
            if (consumer_count_ > 0) {
                ++consumer_count_;
                return true;
            }
            if (providers_.empty()) {
                set_error(error_message, "No expansion module provider is registered");
                return false;
            }
            providers.reserve(providers_.size());
            for (const auto &[_, entry] : providers_) {
                providers.push_back(entry.provider);
            }
            // Keep a separately prepared ownership list for the running state.
            // Both vectors are allocated before any provider acquires hardware.
            active_providers = providers;
            // Allocate scheduler ownership before any provider acquires hardware.
            scheduler = std::make_shared<lib_utils::TaskScheduler>();
            scheduler_config.emplace(lib_utils::TaskScheduler::StartConfig{
                .worker_configs = {{
                        .name = "ExpansionScan",
                        .stack_size = BROOKESIA_HAL_ADAPTOR_EXPANSION_SCAN_TASK_STACK_SIZE,
                    }
                },
            });
        } catch (const std::exception &e) {
            set_detailed_error(error_message, "Failed to prepare expansion runtime: ", e.what());
            return false;
        } catch (...) {
            set_error(error_message, "Failed to prepare expansion runtime");
            return false;
        }

        size_t started_count = 0;
        bool provider_start_failed = false;
        try {
            std::lock_guard operation_lock(operation_mutex_);
            for (const auto &provider : providers) {
                try {
                    const auto result = provider->start();
                    if (!result) {
                        set_detailed_error(
                            error_message, "Failed to start expansion provider: ", result.error()
                        );
                        provider_start_failed = true;
                        break;
                    }
                } catch (const std::exception &e) {
                    set_detailed_error(error_message, "Failed to start expansion provider: ", e.what());
                    provider_start_failed = true;
                    break;
                } catch (...) {
                    set_error(error_message, "Failed to start expansion provider: unknown exception");
                    provider_start_failed = true;
                    break;
                }
                ++started_count;
            }
        } catch (const std::exception &e) {
            set_detailed_error(error_message, "Failed while starting expansion providers: ", e.what());
            provider_start_failed = true;
        } catch (...) {
            set_error(error_message, "Failed while starting expansion providers");
            provider_start_failed = true;
        }

        if (provider_start_failed || (started_count != providers.size())) {
            stop_providers(providers, started_count);
            return false;
        }

        bool scheduler_started = false;
        try {
            scheduler_started = scheduler->start(*scheduler_config);
        } catch (const std::exception &e) {
            set_detailed_error(error_message, "Failed to start expansion scan scheduler: ", e.what());
        } catch (...) {
            set_error(error_message, "Failed to start expansion scan scheduler");
        }
        if (!scheduler_started) {
            set_error(error_message, "Failed to start expansion scan scheduler");
            stop_scheduler_noexcept(scheduler);
            stop_providers(providers, started_count);
            return false;
        }

        bool scheduler_configured = false;
        try {
            scheduler_configured = scheduler->configure_group(
                                       SCAN_TASK_GROUP, {.enable_serial_execution = true}
                                   );
        } catch (const std::exception &e) {
            set_detailed_error(error_message, "Failed to configure expansion scan task group: ", e.what());
        } catch (...) {
            set_error(error_message, "Failed to configure expansion scan task group");
        }
        if (!scheduler_configured) {
            set_error(error_message, "Failed to configure expansion scan task group");
            stop_scheduler_noexcept(scheduler);
            stop_providers(providers, started_count);
            return false;
        }

        try {
            std::lock_guard state_lock(state_mutex_);
            scheduler_ = scheduler;
            active_providers_ = std::move(active_providers);
            running_ = true;
            consumer_count_ = 1;
        } catch (const std::exception &e) {
            set_detailed_error(error_message, "Failed to commit expansion runtime state: ", e.what());
            stop_scheduler_noexcept(scheduler);
            stop_providers(providers, started_count);
            return false;
        } catch (...) {
            set_error(error_message, "Failed to commit expansion runtime state");
            stop_scheduler_noexcept(scheduler);
            stop_providers(providers, started_count);
            return false;
        }

        lib_utils::TaskScheduler::TaskId scan_task_id = 0;
        bool scan_scheduled = false;
        const auto scan_callback = [this]() -> bool {
            scan_all_once();
            std::lock_guard lock(state_mutex_);
            return running_;
        };
        try {
            scan_scheduled = scheduler->post_periodic(
                                 scan_callback, BROOKESIA_HAL_ADAPTOR_EXPANSION_SCAN_INTERVAL_MS,
                                 &scan_task_id, SCAN_TASK_GROUP
                             );
        } catch (const std::exception &e) {
            set_detailed_error(error_message, "Failed to schedule expansion scans: ", e.what());
        } catch (...) {
            set_error(error_message, "Failed to schedule expansion scans");
        }
        if (!scan_scheduled) {
            {
                std::lock_guard state_lock(state_mutex_);
                running_ = false;
                consumer_count_ = 0;
                scheduler_.reset();
                active_providers_.clear();
            }
            state_cv_.notify_all();
            stop_scheduler_noexcept(scheduler);
            stop_providers(providers, started_count);
            set_error(error_message, "Failed to schedule expansion scans");
            return false;
        }

        // lifecycle_mutex_ prevents the last release until retain completes.
        scan_task_id_ = scan_task_id;
        try {
            BROOKESIA_LOGI("Expansion module scanning started");
        } catch (...) {
            // Diagnostics must not turn a successfully started runtime into a failed retain.
        }
        return true;
    }

    void release_consumer()
    {
        std::unique_lock lifecycle_lock(lifecycle_mutex_);
        if (!wait_for_runtime_stop(lifecycle_lock)) {
            log_warning_noexcept("Ignore expansion runtime release while its worker is stopping");
            return;
        }

        std::shared_ptr<lib_utils::TaskScheduler> scheduler;
        std::vector<std::shared_ptr<ModuleProvider>> providers;
        {
            std::lock_guard state_lock(state_mutex_);
            if (consumer_count_ == 0) {
                log_warning_noexcept("Ignore unmatched expansion runtime release");
                return;
            }
            if (consumer_count_ > 1) {
                --consumer_count_;
                return;
            }

            // The ownership list was fully allocated before providers started.
            // Moving it cannot fail, so the last release never changes runtime
            // state and then loses the ability to stop a provider.
            providers = std::move(active_providers_);
            running_ = false;
            consumer_count_ = 0;
            scheduler = std::move(scheduler_);
            scan_task_id_ = 0;
        }
        state_cv_.notify_all();

        stopping_ = true;
        stopping_scheduler_ = scheduler;
        if (scheduler && (scheduler->is_current_thread_worker() || (current_event_dispatcher == this))) {
            try {
                auto thread_config = lib_utils::ThreadConfig::get_applied_config();
                thread_config.name = "ExpansionStop";
                thread_config.stack_size = BROOKESIA_HAL_ADAPTOR_EXPANSION_SCAN_TASK_STACK_SIZE;
                BROOKESIA_THREAD_CONFIG_GUARD(thread_config);
                stop_thread_ = std::thread(
                [this, scheduler, providers]() {
                    complete_runtime_stop(scheduler, providers);
                }
                               );
            } catch (const std::exception &e) {
                // Keep ownership so a later non-worker lifecycle call can finish
                // the stop without ever joining the scan worker from itself.
                pending_stop_scheduler_ = std::move(scheduler);
                pending_stop_providers_ = std::move(providers);
                log_warning_noexcept("Failed to defer expansion runtime stop", e.what());
            } catch (...) {
                pending_stop_scheduler_ = std::move(scheduler);
                pending_stop_providers_ = std::move(providers);
                log_warning_noexcept("Failed to defer expansion runtime stop");
            }
            return;
        }
        lifecycle_lock.unlock();
        complete_runtime_stop(scheduler, providers);
    }

    std::vector<ModuleInfo> get_module_infos() const
    {
        std::lock_guard lock(state_mutex_);
        std::vector<ModuleInfo> infos;
        infos.reserve(slots_.size());
        for (const auto &[_, record] : slots_) {
            infos.push_back(record.stable);
        }
        return infos;
    }

    ModuleManagerIface::EventListenerId add_event_listener(ModuleManagerIface::EventListener listener)
    {
        if (!listener) {
            return 0;
        }
        std::lock_guard lock(event_mutex_);
        auto id = next_listener_id_++;
        if (id == 0) {
            id = next_listener_id_++;
        }
        listeners_.emplace(
            id, std::make_shared<ListenerRecord>(ListenerRecord{.callback = std::move(listener)})
        );
        return id;
    }

    bool remove_event_listener(ModuleManagerIface::EventListenerId id)
    {
        if (id == 0) {
            return false;
        }
        std::unique_lock lock(event_mutex_);
        const auto listener_it = listeners_.find(id);
        if (listener_it == listeners_.end()) {
            return false;
        }

        auto listener = listener_it->second;
        listener->removed = true;
        listeners_.erase(listener_it);

        // A callback may remove itself (or another listener). The current
        // serialized dispatcher will skip every later queued invocation, so it
        // must not wait for its own in-flight callback here.
        if (current_event_dispatcher == this) {
            return true;
        }
        event_cv_.wait(lock, [&]() {
            return listener->pending_count == 0;
        });
        return true;
    }

    std::optional<LeaseData> try_claim_retained(
        std::string_view provider_name, std::string_view slot_name, uint64_t expected_generation,
        std::string *error_message
    )
    {
        std::vector<ModuleInfo> success_events;
        std::vector<ModuleInfo> failure_events;
        std::optional<LeaseData> prepared_lease;
        bool claim_succeeded = false;

        {
            std::lock_guard operation_lock(operation_mutex_);

            std::shared_ptr<ModuleProvider> provider;
            SlotRecord *record = nullptr;
            uint64_t ready_identity = 0;
            try {
                const SlotKey slot_key{std::string(provider_name), std::string(slot_name)};
                std::lock_guard state_lock(state_mutex_);
                const auto slot_it = slots_.find(slot_key);
                if (slot_it == slots_.end()) {
                    set_error(error_message, "Expansion module slot was not found");
                    return std::nullopt;
                }
                if ((slot_it->second.stable.state != ModuleState::Ready) ||
                        (slot_it->second.stable.generation != expected_generation)) {
                    set_error(error_message, "Expansion module snapshot is not ready or its generation is stale");
                    return std::nullopt;
                }
                const auto provider_it = providers_.find(slot_key.first);
                if (provider_it == providers_.end()) {
                    set_error(error_message, "Expansion module provider was not found");
                    return std::nullopt;
                }

                provider = provider_it->second.provider;
                record = &slot_it->second;
                ready_identity = slot_it->second.stable_identity;

                LeaseData lease_data{
                    .active_info = record->stable,
                };
                lease_data.active_info.state = ModuleState::Active;
                ++lease_data.active_info.generation;
                prepared_lease.emplace(std::move(lease_data));

                success_events.reserve(1);
                success_events.push_back(prepared_lease->active_info);

                ModuleInfo failure_info = record->stable;
                failure_info.state = ModuleState::Error;
                ++failure_info.generation;
                failure_events.reserve(1);
                failure_events.push_back(std::move(failure_info));
            } catch (const std::exception &e) {
                set_detailed_error(error_message, "Failed to prepare expansion module claim: ", e.what());
                return std::nullopt;
            } catch (...) {
                set_error(error_message, "Failed to prepare expansion module claim");
                return std::nullopt;
            }

            ClaimOptions claim_options;
            try {
                const auto result = provider->claim(record->stable, ready_identity);
                if (result) {
                    claim_options = *result;
                    claim_succeeded = true;
                } else {
                    set_detailed_error(error_message, "Failed to claim expansion module: ", result.error());
                }
            } catch (const std::exception &e) {
                set_detailed_error(error_message, "Failed to claim expansion module: ", e.what());
            } catch (...) {
                set_error(error_message, "Failed to claim expansion module: unknown exception");
            }

            {
                std::lock_guard state_lock(state_mutex_);
                if (!claim_succeeded) {
                    record->stable.state = ModuleState::Error;
                    ++record->stable.generation;
                    record->candidate.reset();
                    record->candidate_count = 0;
                } else {
                    record->stable.state = ModuleState::Active;
                    ++record->stable.generation;
                    record->candidate.reset();
                    record->candidate_count = 0;
                    prepared_lease->pause_all_scanning = claim_options.pause_all_scanning;
                    if (claim_options.pause_all_scanning) {
                        ++pause_all_count_;
                    }
                }
            }
            state_cv_.notify_all();
            queue_events(claim_succeeded ? success_events : failure_events);
        }

        drain_events_noexcept();
        return claim_succeeded ? std::move(prepared_lease) : std::nullopt;
    }

    std::optional<ModuleInfo> wait_for_match(
        std::string_view provider, std::string_view slot, std::string_view type, uint32_t timeout_ms
    )
    {
        const auto find_match = [&]() -> std::optional<ModuleInfo> {
            for (const auto &[_, record] : slots_)
            {
                const auto &info = record.stable;
                if ((info.state != ModuleState::Ready) ||
                        (!provider.empty() && (info.provider != provider)) ||
                        (!slot.empty() && (info.slot != slot)) ||
                        (!type.empty() && (info.type != type))) {
                    continue;
                }
                return info;
            }
            return std::nullopt;
        };

        std::unique_lock lock(state_mutex_);
        if (auto info = find_match()) {
            return info;
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (running_ && state_cv_.wait_until(lock, deadline) != std::cv_status::timeout) {
            if (auto info = find_match()) {
                return info;
            }
        }
        return find_match();
    }

    bool release_lease(const ModuleInfo &active_info, bool pause_all_scanning, std::string *error_message)
    {
        bool provider_success = false;

        {
            std::lock_guard operation_lock(operation_mutex_);
            std::shared_ptr<ModuleProvider> provider;
            SlotRecord *record = nullptr;
            {
                std::lock_guard state_lock(state_mutex_);
                const auto slot_it = std::find_if(
                slots_.begin(), slots_.end(), [&active_info](const auto & entry) {
                    return (entry.first.first == active_info.provider) &&
                           (entry.first.second == active_info.slot);
                }
                                     );
                if ((slot_it == slots_.end()) ||
                        (slot_it->second.stable.state != ModuleState::Active) ||
                        (slot_it->second.stable.generation != active_info.generation)) {
                    set_error(error_message, "Expansion module lease generation is stale");
                    return false;
                }
                const auto provider_it = providers_.find(active_info.provider);
                if (provider_it == providers_.end()) {
                    set_error(error_message, "Expansion module provider was not found");
                    return false;
                }
                provider = provider_it->second.provider;
                record = &slot_it->second;
            }

            try {
                const auto result = provider->release(active_info);
                provider_success = result.has_value();
                if (!result) {
                    set_detailed_error(
                        error_message, "Failed to release expansion module: ", result.error()
                    );
                }
            } catch (const std::exception &e) {
                set_detailed_error(error_message, "Failed to release expansion module: ", e.what());
            } catch (...) {
                set_error(error_message, "Failed to release expansion module: unknown exception");
            }

            {
                std::lock_guard state_lock(state_mutex_);
                record->stable.state = ModuleState::Unknown;
                record->stable.type.clear();
                record->stable.board_id = 0;
                record->stable.board_name.clear();
                ++record->stable.generation;
                record->candidate.reset();
                record->candidate_count = 0;
                if (pause_all_scanning && (pause_all_count_ > 0)) {
                    --pause_all_count_;
                }
            }
            state_cv_.notify_all();
            queue_event(record->stable);
        }

        // Releasing a lease always restores scanning immediately. The fresh result
        // is still published only after the normal three-sample debounce.
        try {
            scan_all_once();
        } catch (const std::exception &e) {
            log_warning_noexcept("Failed to rescan after releasing expansion module", e.what());
        } catch (...) {
            log_warning_noexcept("Failed to rescan after releasing expansion module");
        }
        // scan_all_once() may return early while another active lease keeps all
        // scanning paused; the queued Unknown event must still be delivered.
        drain_events_noexcept();
        return provider_success;
    }

    bool request_rescan()
    {
        std::shared_ptr<lib_utils::TaskScheduler> scheduler;
        {
            std::lock_guard lock(state_mutex_);
            if (!running_ || !scheduler_) {
                return false;
            }
            scheduler = scheduler_;
        }
        return scheduler->post([this]() {
            scan_all_once();
        }, nullptr, SCAN_TASK_GROUP);
    }

private:
    bool wait_for_runtime_stop(
        std::unique_lock<std::mutex> &lifecycle_lock, std::string *error_message = nullptr
    )
    {
        while (stopping_) {
            const bool current_scan_worker = stopping_scheduler_ &&
                                             stopping_scheduler_->is_current_thread_worker();
            if ((current_event_dispatcher == this) || (current_runtime_stopper == this) ||
                    current_scan_worker) {
                set_error(error_message, "Expansion runtime is stopping on the current execution context");
                return false;
            }

            if (pending_stop_scheduler_) {
                auto scheduler = std::move(pending_stop_scheduler_);
                auto providers = std::move(pending_stop_providers_);
                lifecycle_lock.unlock();
                complete_runtime_stop(scheduler, providers);
                lifecycle_lock.lock();
                continue;
            }
            lifecycle_cv_.wait(lifecycle_lock, [&]() {
                return !stopping_ || static_cast<bool>(pending_stop_scheduler_);
            });
        }

        if (stop_thread_.joinable()) {
            auto completed_stop_thread = std::move(stop_thread_);
            lifecycle_lock.unlock();
            completed_stop_thread.join();
            lifecycle_lock.lock();
        }
        return true;
    }

    void complete_runtime_stop(
        const std::shared_ptr<lib_utils::TaskScheduler> &scheduler,
        const std::vector<std::shared_ptr<ModuleProvider>> &providers
    )
    {
        struct StopperGuard {
            explicit StopperGuard(const void *runtime)
            {
                current_runtime_stopper = runtime;
            }

            ~StopperGuard()
            {
                current_runtime_stopper = nullptr;
            }
        } stopper_guard(this);

        // Never hold lifecycle_mutex_ while waiting for an in-flight callback
        // or scheduler worker. Those callbacks may attempt a lifecycle action;
        // stopping_ makes that action fail immediately in this execution context.
        quiesce_runtime_events();
        finish_runtime_stop(scheduler, providers);

        {
            std::lock_guard lifecycle_lock(lifecycle_mutex_);
            stopping_scheduler_.reset();
            stopping_ = false;
        }
        lifecycle_cv_.notify_all();
    }

    void quiesce_runtime_events()
    {
        // A scan publishes its events before releasing operation_mutex_. Once
        // this barrier passes, drain_events() either follows that scan or helps
        // it drain the same ordered queue.
        {
            std::lock_guard operation_lock(operation_mutex_);
        }
        drain_events();
    }

    void finish_runtime_stop(
        const std::shared_ptr<lib_utils::TaskScheduler> &scheduler,
        const std::vector<std::shared_ptr<ModuleProvider>> &providers
    )
    {
        stop_scheduler_noexcept(scheduler);
        stop_providers(providers, providers.size());

        {
            std::lock_guard state_lock(state_mutex_);
            pause_all_count_ = 0;
            for (auto &[_, record] : slots_) {
                record.candidate.reset();
                record.candidate_count = 0;
                if (record.stable.state != ModuleState::Unknown) {
                    record.stable.state = ModuleState::Unknown;
                    record.stable.type.clear();
                    record.stable.board_id = 0;
                    record.stable.board_name.clear();
                    ++record.stable.generation;
                }
            }
        }
        try {
            BROOKESIA_LOGI("Expansion module scanning stopped");
        } catch (...) {
            // Diagnostics are best effort during resource cleanup.
        }
    }

    void stop_scheduler_noexcept(const std::shared_ptr<lib_utils::TaskScheduler> &scheduler) noexcept
    {
        if (!scheduler) {
            return;
        }

        try {
            scheduler->stop();
        } catch (const std::exception &e) {
            log_warning_noexcept("Expansion scan scheduler stop threw", e.what());
        } catch (...) {
            log_warning_noexcept("Expansion scan scheduler stop threw an unknown exception");
        }
    }

    void stop_providers(
        const std::vector<std::shared_ptr<ModuleProvider>> &providers, size_t started_count
    ) noexcept
    {
        try {
            std::lock_guard operation_lock(operation_mutex_);
            while (started_count > 0) {
                const auto &provider = providers[--started_count];
                try {
                    const auto result = provider->stop();
                    if (!result) {
                        log_warning_noexcept("Failed to stop expansion provider", result.error().c_str());
                    }
                } catch (const std::exception &e) {
                    log_warning_noexcept("Expansion provider stop threw", e.what());
                } catch (...) {
                    log_warning_noexcept("Expansion provider stop threw an unknown exception");
                }
            }
        } catch (const std::exception &e) {
            log_warning_noexcept("Failed to lock expansion provider cleanup", e.what());
        } catch (...) {
            log_warning_noexcept("Failed to lock expansion provider cleanup");
        }
    }

    void scan_all_once()
    {
        std::vector<ModuleInfo> events;
        {
            std::lock_guard operation_lock(operation_mutex_);

            std::vector<std::pair<std::string, ProviderEntry>> providers;
            {
                std::lock_guard state_lock(state_mutex_);
                if (!running_ || (pause_all_count_ > 0)) {
                    return;
                }
                providers.assign(providers_.begin(), providers_.end());
            }

            for (const auto &[provider_name, entry] : providers) {
                for (const auto &slot_name : entry.slots) {
                    {
                        std::lock_guard state_lock(state_mutex_);
                        if (!running_ || (pause_all_count_ > 0)) {
                            break;
                        }
                        if (slots_.at(SlotKey{provider_name, slot_name}).stable.state == ModuleState::Active) {
                            continue;
                        }
                    }

                    ScanInfo sample;
                    try {
                        auto result = entry.provider->scan(slot_name);
                        sample = result ? std::move(*result) : make_error_sample();
                    } catch (...) {
                        sample = make_error_sample();
                    }
                    if (!is_scan_state(sample.state)) {
                        sample = make_error_sample();
                    }
                    if (sample.state == ModuleState::Empty || sample.state == ModuleState::Error) {
                        sample.type.clear();
                        sample.board_id = 0;
                        sample.board_name.clear();
                    }

                    std::lock_guard state_lock(state_mutex_);
                    auto &record = slots_.at(SlotKey{provider_name, slot_name});
                    if (record.stable.state == ModuleState::Active) {
                        continue;
                    }
                    if (record.candidate && scan_infos_equal(*record.candidate, sample)) {
                        ++record.candidate_count;
                    } else {
                        record.candidate = sample;
                        record.candidate_count = 1;
                    }
                    if (record.candidate_count < BROOKESIA_HAL_ADAPTOR_EXPANSION_SCAN_DEBOUNCE_COUNT) {
                        continue;
                    }

                    ModuleInfo next{
                        .provider = provider_name,
                        .slot = slot_name,
                        .type = sample.type,
                        .board_id = sample.board_id,
                        .board_name = sample.board_name,
                        .generation = record.stable.generation,
                        .state = sample.state,
                    };
                    if (module_payloads_equal(record.stable, next) &&
                            (record.stable_identity == sample.identity)) {
                        continue;
                    }
                    next.generation = record.stable.generation + 1;
                    record.stable = std::move(next);
                    record.stable_identity = sample.identity;
                    events.push_back(record.stable);
                    state_cv_.notify_all();
                }
            }
            queue_events(events);
        }

        drain_events();
    }

    void queue_event(const ModuleInfo &event) noexcept
    {
        try {
            std::lock_guard lock(event_mutex_);
            queue_event_locked(event);
        } catch (const std::exception &e) {
            log_warning_noexcept("Failed to queue expansion module event", e.what());
        } catch (...) {
            log_warning_noexcept("Failed to queue expansion module event");
        }
    }

    void queue_events(const std::vector<ModuleInfo> &events) noexcept
    {
        // Callers enqueue while holding operation_mutex_. This makes queue order
        // identical to the serialized scan/claim/release state-transition order.
        try {
            std::lock_guard lock(event_mutex_);
            for (const auto &event : events) {
                queue_event_locked(event);
            }
        } catch (const std::exception &e) {
            log_warning_noexcept("Failed to queue expansion module events", e.what());
        } catch (...) {
            log_warning_noexcept("Failed to queue expansion module events");
        }
    }

    void queue_event_locked(const ModuleInfo &event)
    {
        std::vector<std::shared_ptr<ListenerRecord>> listeners;
        listeners.reserve(listeners_.size());
        for (const auto &[_, listener] : listeners_) {
            if (!listener->removed) {
                listeners.push_back(listener);
            }
        }
        event_queue_.push_back(QueuedEvent{
            .info = event,
            .listeners = std::move(listeners),
        });
        for (const auto &listener : event_queue_.back().listeners) {
            ++listener->pending_count;
        }
    }

    void drain_events_noexcept() noexcept
    {
        try {
            drain_events();
        } catch (const std::exception &e) {
            log_warning_noexcept("Failed to dispatch expansion module events", e.what());
        } catch (...) {
            log_warning_noexcept("Failed to dispatch expansion module events");
        }
    }

    void drain_events()
    {
        if (current_event_dispatcher == this) {
            return;
        }

        // Only one thread drains callbacks. Reentrant state changes append to the
        // same queue and are picked up by the outer dispatcher.
        std::lock_guard dispatch_lock(event_dispatch_mutex_);
        struct DispatcherGuard {
            explicit DispatcherGuard(const void *dispatcher)
            {
                current_event_dispatcher = dispatcher;
            }

            ~DispatcherGuard()
            {
                current_event_dispatcher = nullptr;
            }
        } dispatcher_guard(this);

        while (true) {
            QueuedEvent event;
            {
                std::lock_guard lock(event_mutex_);
                if (event_queue_.empty()) {
                    break;
                }
                event = std::move(event_queue_.front());
                event_queue_.pop_front();
            }

            try {
                BROOKESIA_LOGI("Expansion module state changed: %1%", event.info);
            } catch (...) {
                // Event delivery must not depend on diagnostic string allocation.
            }
            for (const auto &listener : event.listeners) {
                bool should_invoke = false;
                {
                    std::lock_guard lock(event_mutex_);
                    should_invoke = !listener->removed;
                }
                if (should_invoke) {
                    try {
                        listener->callback(event.info);
                    } catch (const std::exception &e) {
                        log_warning_noexcept("Expansion module listener threw", e.what());
                    } catch (...) {
                        log_warning_noexcept("Expansion module listener threw an unknown exception");
                    }
                }

                {
                    std::lock_guard lock(event_mutex_);
                    if (listener->pending_count > 0) {
                        --listener->pending_count;
                    }
                }
                event_cv_.notify_all();
            }
        }
    }

    mutable std::mutex lifecycle_mutex_;
    std::condition_variable lifecycle_cv_;
    mutable std::mutex state_mutex_;
    std::mutex operation_mutex_;
    std::condition_variable state_cv_;
    std::mutex event_mutex_;
    std::mutex event_dispatch_mutex_;
    std::condition_variable event_cv_;
    std::map<std::string, ProviderEntry> providers_;
    std::map<SlotKey, SlotRecord> slots_;
    std::map<ModuleManagerIface::EventListenerId, std::shared_ptr<ListenerRecord>> listeners_;
    std::deque<QueuedEvent> event_queue_;
    ModuleManagerIface::EventListenerId next_listener_id_ = 1;
    std::shared_ptr<lib_utils::TaskScheduler> scheduler_;
    std::shared_ptr<lib_utils::TaskScheduler> stopping_scheduler_;
    std::shared_ptr<lib_utils::TaskScheduler> pending_stop_scheduler_;
    std::vector<std::shared_ptr<ModuleProvider>> active_providers_;
    std::vector<std::shared_ptr<ModuleProvider>> pending_stop_providers_;
    std::thread stop_thread_;
    lib_utils::TaskScheduler::TaskId scan_task_id_ = 0;
    size_t consumer_count_ = 0;
    size_t pause_all_count_ = 0;
    bool running_ = false;
    bool stopping_ = false;
};

ExpansionRuntime &ExpansionRuntime::get_instance()
{
    static ExpansionRuntime instance;
    return instance;
}

ExpansionRuntime::ExpansionRuntime()
    : impl_(std::make_unique<Impl>())
{
}

ExpansionRuntime::~ExpansionRuntime() = default;

bool ExpansionRuntime::register_provider(std::shared_ptr<ModuleProvider> provider)
{
    return impl_->register_provider(std::move(provider));
}

bool ExpansionRuntime::unregister_provider(std::string_view provider_name)
{
    return impl_->unregister_provider(provider_name);
}

bool ExpansionRuntime::has_providers() const
{
    return impl_->has_providers();
}

bool ExpansionRuntime::retain_consumer(std::string *error_message)
{
    return impl_->retain_consumer(error_message);
}

void ExpansionRuntime::release_consumer()
{
    impl_->release_consumer();
}

std::vector<ModuleInfo> ExpansionRuntime::get_module_infos() const
{
    return impl_->get_module_infos();
}

ModuleManagerIface::EventListenerId ExpansionRuntime::add_event_listener(ModuleManagerIface::EventListener listener)
{
    return impl_->add_event_listener(std::move(listener));
}

bool ExpansionRuntime::remove_event_listener(ModuleManagerIface::EventListenerId id)
{
    return impl_->remove_event_listener(id);
}

ModuleLease ExpansionRuntime::claim_module(
    std::string_view provider, std::string_view slot, uint64_t expected_generation,
    std::string *error_message
)
{
    std::unique_ptr<ModuleLease::Impl> lease_impl;
    try {
        // Allocate ownership before claiming hardware. After a successful claim
        // no allocation may be allowed to strand an Active module.
        lease_impl = std::make_unique<ModuleLease::Impl>();
    } catch (const std::exception &e) {
        set_detailed_error(error_message, "Failed to allocate expansion module lease: ", e.what());
        return {};
    } catch (...) {
        set_error(error_message, "Failed to allocate expansion module lease");
        return {};
    }

    if (!retain_consumer(error_message)) {
        return {};
    }
    auto result = impl_->try_claim_retained(provider, slot, expected_generation, error_message);
    if (!result) {
        release_consumer();
        return {};
    }
    lease_impl->runtime = this;
    lease_impl->active_info = std::move(result->active_info);
    lease_impl->pause_all_scanning = result->pause_all_scanning;
    return ModuleLease(std::move(lease_impl));
}

ModuleLease ExpansionRuntime::claim_matching(
    std::string_view provider, std::string_view slot, std::string_view type,
    uint32_t timeout_ms, std::string *error_message
)
{
    std::unique_ptr<ModuleLease::Impl> lease_impl;
    try {
        lease_impl = std::make_unique<ModuleLease::Impl>();
    } catch (const std::exception &e) {
        set_detailed_error(error_message, "Failed to allocate expansion module lease: ", e.what());
        return {};
    } catch (...) {
        set_error(error_message, "Failed to allocate expansion module lease");
        return {};
    }

    if (!retain_consumer(error_message)) {
        return {};
    }
    auto ready_info = impl_->wait_for_match(provider, slot, type, timeout_ms);
    if (!ready_info) {
        set_error(error_message, "Timed out waiting for a stable expansion module");
        release_consumer();
        return {};
    }
    auto result = impl_->try_claim_retained(
                      ready_info->provider, ready_info->slot, ready_info->generation, error_message
                  );
    if (!result) {
        release_consumer();
        return {};
    }
    lease_impl->runtime = this;
    lease_impl->active_info = std::move(result->active_info);
    lease_impl->pause_all_scanning = result->pause_all_scanning;
    return ModuleLease(std::move(lease_impl));
}

bool ExpansionRuntime::release_lease(
    const ModuleInfo &active_info, bool pause_all_scanning, std::string *error_message
)
{
    return impl_->release_lease(active_info, pause_all_scanning, error_message);
}

bool ExpansionRuntime::request_rescan()
{
    return impl_->request_rescan();
}

} // namespace detail

ModuleLease::ModuleLease() = default;

ModuleLease::ModuleLease(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl))
{
}

ModuleLease::~ModuleLease()
{
    reset();
}

ModuleLease::ModuleLease(ModuleLease &&other) noexcept = default;

ModuleLease &ModuleLease::operator=(ModuleLease &&other) noexcept
{
    if (this != &other) {
        reset();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

ModuleLease::operator bool() const noexcept
{
    return impl_ != nullptr;
}

const ModuleInfo &ModuleLease::get_info() const noexcept
{
    static const ModuleInfo empty;
    return impl_ ? impl_->active_info : empty;
}

bool ModuleLease::reset(std::string *error_message)
{
    if (!impl_) {
        return true;
    }

    auto impl = std::move(impl_);
    bool result = false;
    try {
        result = impl->runtime->release_lease(
                     impl->active_info, impl->pause_all_scanning, error_message
                 );
    } catch (const std::exception &e) {
        detail::set_detailed_error(error_message, "Unexpected expansion module release failure: ", e.what());
        detail::log_warning_noexcept("Unexpected expansion module release failure", e.what());
    } catch (...) {
        detail::set_error(error_message, "Unexpected expansion module release failure");
        detail::log_warning_noexcept("Unexpected expansion module release failure");
    }

    try {
        impl->runtime->release_consumer();
    } catch (const std::exception &e) {
        result = false;
        detail::set_detailed_error(error_message, "Unexpected expansion runtime release failure: ", e.what());
        detail::log_warning_noexcept("Unexpected expansion runtime release failure", e.what());
    } catch (...) {
        result = false;
        detail::set_error(error_message, "Unexpected expansion runtime release failure");
        detail::log_warning_noexcept("Unexpected expansion runtime release failure");
    }
    return result;
}

bool register_module_provider(std::shared_ptr<ModuleProvider> provider)
{
    return detail::ExpansionRuntime::get_instance().register_provider(std::move(provider));
}

bool unregister_module_provider(std::string_view provider_name)
{
    return detail::ExpansionRuntime::get_instance().unregister_provider(provider_name);
}

ModuleLease claim_module(
    std::string_view provider, std::string_view slot, uint64_t expected_generation,
    std::string *error_message
)
{
    return detail::ExpansionRuntime::get_instance().claim_module(
               provider, slot, expected_generation, error_message
           );
}

ModuleLease claim_matching(
    std::string_view provider, std::string_view slot, std::string_view type,
    uint32_t timeout_ms, std::string *error_message
)
{
    return detail::ExpansionRuntime::get_instance().claim_matching(
               provider, slot, type, timeout_ms, error_message
           );
}

bool request_rescan()
{
    return detail::ExpansionRuntime::get_instance().request_rescan();
}

} // namespace esp_brookesia::hal::expansion
