/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "esp_newlib.h"
#include "unity.h"
#include "unity_test_utils.h"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_adaptor.hpp"

using namespace esp_brookesia;

#if BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES

namespace {

constexpr const char *FAKE_PROVIDER_NAME = "fake-expansion";
constexpr const char *FAKE_SLOT_NAME = "slot0";
constexpr const char *FAKE_MODULE_TYPE = "fake-camera";
constexpr size_t RUNTIME_WARMUP_ITERATIONS = 2;
constexpr size_t RUNTIME_MEASURED_ITERATIONS = 1;
constexpr uint32_t RUNTIME_SETTLE_DELAY_MS = 20;

hal::expansion::ScanInfo make_scan_info(
    hal::expansion::ModuleState state, std::string type = {}, uint32_t board_id = 0,
    std::string board_name = {}, uint64_t identity = 0
)
{
    return {
        .state = state,
        .type = std::move(type),
        .board_id = board_id,
        .board_name = std::move(board_name),
        .identity = identity,
    };
}

class FakeModuleProvider final: public hal::expansion::ModuleProvider {
public:
    std::string_view get_name() const override
    {
        return FAKE_PROVIDER_NAME;
    }

    std::vector<std::string> get_slots() const override
    {
        return {FAKE_SLOT_NAME};
    }

    std::expected<void, std::string> start() override
    {
        std::lock_guard lock(mutex_);
        ++start_count_;
        condition_.notify_all();
        return {};
    }

    std::expected<void, std::string> stop() override
    {
        std::lock_guard lock(mutex_);
        ++stop_count_;
        condition_.notify_all();
        return {};
    }

    std::expected<hal::expansion::ScanInfo, std::string> scan(std::string_view slot) override
    {
        std::lock_guard lock(mutex_);
        if (slot != FAKE_SLOT_NAME) {
            return std::unexpected("invalid slot");
        }
        ++scan_count_;
        condition_.notify_all();
        if (scan_error_) {
            return std::unexpected("injected scan failure");
        }
        return scan_info_;
    }

    std::expected<hal::expansion::ClaimOptions, std::string> claim(
        const hal::expansion::ModuleInfo &, uint64_t
    ) override
    {
        std::lock_guard lock(mutex_);
        ++claim_count_;
        if (claim_error_) {
            return std::unexpected("injected claim failure");
        }
        return hal::expansion::ClaimOptions{.pause_all_scanning = true};
    }

    std::expected<void, std::string> release(const hal::expansion::ModuleInfo &) override
    {
        std::lock_guard lock(mutex_);
        ++release_count_;
        return {};
    }

    size_t set_scan_info(hal::expansion::ScanInfo info)
    {
        std::lock_guard lock(mutex_);
        scan_error_ = false;
        scan_info_ = std::move(info);
        return scan_count_;
    }

    size_t set_scan_error(bool enabled)
    {
        std::lock_guard lock(mutex_);
        scan_error_ = enabled;
        return scan_count_;
    }

    void set_claim_error(bool enabled)
    {
        std::lock_guard lock(mutex_);
        claim_error_ = enabled;
    }

    size_t scan_count() const
    {
        std::lock_guard lock(mutex_);
        return scan_count_;
    }

    size_t claim_count() const
    {
        std::lock_guard lock(mutex_);
        return claim_count_;
    }

    size_t start_count() const
    {
        std::lock_guard lock(mutex_);
        return start_count_;
    }

    size_t stop_count() const
    {
        std::lock_guard lock(mutex_);
        return stop_count_;
    }

    size_t release_count() const
    {
        std::lock_guard lock(mutex_);
        return release_count_;
    }

    bool wait_for_scan_count(size_t target, uint32_t timeout_ms = 2000)
    {
        std::unique_lock lock(mutex_);
        return condition_.wait_for(
                   lock, std::chrono::milliseconds(timeout_ms),
        [&]() {
            return scan_count_ >= target;
        }
               );
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    hal::expansion::ScanInfo scan_info_ = make_scan_info(hal::expansion::ModuleState::Empty);
    size_t start_count_ = 0;
    size_t stop_count_ = 0;
    size_t scan_count_ = 0;
    size_t claim_count_ = 0;
    size_t release_count_ = 0;
    bool scan_error_ = false;
    bool claim_error_ = false;
};

std::shared_ptr<FakeModuleProvider> get_fake_provider()
{
    static auto provider = std::make_shared<FakeModuleProvider>();
    return provider;
}

std::optional<hal::expansion::ModuleInfo> get_fake_info(hal::expansion::ModuleManagerIface &manager)
{
    for (const auto &info : manager.get_module_infos()) {
        if ((info.provider == FAKE_PROVIDER_NAME) && (info.slot == FAKE_SLOT_NAME)) {
            return info;
        }
    }
    return std::nullopt;
}

bool is_fake_info(const hal::expansion::ModuleInfo &info)
{
    return (info.provider == FAKE_PROVIDER_NAME) && (info.slot == FAKE_SLOT_NAME);
}

bool wait_for_state(
    hal::expansion::ModuleManagerIface &manager, hal::expansion::ModuleState state,
    uint32_t timeout_ms = 2000
)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    do {
        const auto info = get_fake_info(manager);
        if (info && (info->state == state)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

} // namespace

ESP_BROOKESIA_HAL_EXPANSION_REGISTER_PROVIDER(fake_expansion_provider_symbol, get_fake_provider());

namespace {

void run_expansion_runtime_scenario()
{
    auto fake = get_fake_provider();
    const auto initial_start_count = fake->start_count();
    const auto initial_stop_count = fake->stop_count();
    const auto initial_release_count = fake->release_count();
    fake->set_claim_error(false);
    fake->set_scan_info(make_scan_info(hal::expansion::ModuleState::Empty));

    auto manager = hal::acquire_first_interface<hal::expansion::ModuleManagerIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(manager));
    TEST_ASSERT_TRUE(wait_for_state(*manager, hal::expansion::ModuleState::Empty));
    TEST_ASSERT_EQUAL_size_t(initial_start_count + 1, fake->start_count());

    std::atomic<size_t> event_count = 0;
    std::atomic<size_t> ready_event_scan_count = 0;
    std::atomic<bool> callback_query_succeeded = false;
    const auto listener_id = manager->add_event_listener(
    [&](const hal::expansion::ModuleInfo & info) {
        if (!is_fake_info(info)) {
            return;
        }
        ++event_count;
        callback_query_succeeded = get_fake_info(*manager).has_value();
        if (info.state == hal::expansion::ModuleState::Ready) {
            ready_event_scan_count = fake->scan_count();
        }
    }
                             );
    TEST_ASSERT_NOT_EQUAL(0, listener_id);

    std::mutex serialized_listener_mutex;
    std::condition_variable serialized_listener_condition;
    bool block_ready_callback = false;
    bool ready_callback_entered = false;
    bool allow_ready_callback_exit = false;
    std::atomic<bool> active_callback_seen = false;
    size_t callback_sequence = 0;
    size_t ready_callback_sequence = 0;
    size_t active_callback_sequence = 0;
    const auto serialized_listener_id = manager->add_event_listener(
    [&](const hal::expansion::ModuleInfo & info) {
        if (!is_fake_info(info)) {
            return;
        }
        if (info.state == hal::expansion::ModuleState::Active) {
            // Set this before taking the test mutex so concurrent callback
            // dispatch cannot accidentally serialize itself in the test.
            active_callback_seen = true;
            std::lock_guard lock(serialized_listener_mutex);
            active_callback_sequence = ++callback_sequence;
            serialized_listener_condition.notify_all();
            return;
        }

        std::unique_lock lock(serialized_listener_mutex);
        if ((info.state == hal::expansion::ModuleState::Ready) && block_ready_callback) {
            ready_callback_entered = true;
            serialized_listener_condition.notify_all();
            serialized_listener_condition.wait(lock, [&]() {
                return allow_ready_callback_exit;
            });
            block_ready_callback = false;
            ready_callback_sequence = ++callback_sequence;
        }
        serialized_listener_condition.notify_all();
    }
                                        );
    TEST_ASSERT_NOT_EQUAL(0, serialized_listener_id);

    fake->set_scan_info(make_scan_info(
                            hal::expansion::ModuleState::Invalid, "unknown", 7, "Invalid Module"
                        ));
    TEST_ASSERT_TRUE(wait_for_state(*manager, hal::expansion::ModuleState::Invalid));
    fake->set_scan_info(make_scan_info(
                            hal::expansion::ModuleState::Unsupported, "unsupported", 8, "Unsupported Module"
                        ));
    TEST_ASSERT_TRUE(wait_for_state(*manager, hal::expansion::ModuleState::Unsupported));

    {
        std::lock_guard lock(serialized_listener_mutex);
        block_ready_callback = true;
    }
    const auto before_ready = fake->set_scan_info(make_scan_info(
                                  hal::expansion::ModuleState::Ready, FAKE_MODULE_TYPE, 42, "Fake Camera"
                              ));
    TEST_ASSERT_TRUE(fake->wait_for_scan_count(before_ready + 3));
    TEST_ASSERT_TRUE(wait_for_state(*manager, hal::expansion::ModuleState::Ready));
    bool ready_callback_did_enter = false;
    {
        std::unique_lock lock(serialized_listener_mutex);
        ready_callback_did_enter = serialized_listener_condition.wait_for(
        lock, std::chrono::milliseconds(200), [&]() {
            return ready_callback_entered;
        }
                                   );
    }
    TEST_ASSERT_TRUE(ready_callback_did_enter);
    const auto ready_event_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while ((ready_event_scan_count.load() == 0) && (std::chrono::steady_clock::now() < ready_event_deadline)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(before_ready + 3, ready_event_scan_count.load());

    auto ready = get_fake_info(*manager);
    TEST_ASSERT_TRUE(ready.has_value());
    TEST_ASSERT_EQUAL_STRING(FAKE_MODULE_TYPE, ready->type.c_str());
    TEST_ASSERT_TRUE(callback_query_succeeded.load());
    TEST_ASSERT_GREATER_THAN_size_t(0, event_count.load());

    const auto claims_before_stale = fake->claim_count();
    std::string error_message;
    auto stale_lease = hal::expansion::claim_module(
                           ready->provider, ready->slot, ready->generation - 1, &error_message
                       );
    TEST_ASSERT_FALSE(static_cast<bool>(stale_lease));
    TEST_ASSERT_EQUAL_size_t(claims_before_stale, fake->claim_count());

    hal::expansion::ModuleLease lease;
    std::thread claim_thread([&]() {
        lease = hal::expansion::claim_module(
                    ready->provider, ready->slot, ready->generation, &error_message
                );
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    bool active_seen_while_ready_blocked = false;
    {
        std::lock_guard lock(serialized_listener_mutex);
        active_seen_while_ready_blocked = active_callback_seen.load();
        allow_ready_callback_exit = true;
    }
    serialized_listener_condition.notify_all();
    claim_thread.join();
    TEST_ASSERT_FALSE(active_seen_while_ready_blocked);
    TEST_ASSERT_TRUE(static_cast<bool>(lease));
    TEST_ASSERT_EQUAL(hal::expansion::ModuleState::Active, lease.get_info().state);
    TEST_ASSERT_TRUE(wait_for_state(*manager, hal::expansion::ModuleState::Active));
    // Unity's uint64 comparison helpers require CONFIG_UNITY_ENABLE_64BIT,
    // which is intentionally not needed by this test application.
    TEST_ASSERT_TRUE(lease.get_info().generation > ready->generation);
    bool active_callback_did_run = false;
    size_t observed_ready_sequence = 0;
    size_t observed_active_sequence = 0;
    {
        std::lock_guard lock(serialized_listener_mutex);
        active_callback_did_run = active_callback_seen.load();
        observed_ready_sequence = ready_callback_sequence;
        observed_active_sequence = active_callback_sequence;
    }
    TEST_ASSERT_TRUE(active_callback_did_run);
    TEST_ASSERT_GREATER_THAN_size_t(0, observed_ready_sequence);
    TEST_ASSERT_GREATER_THAN_size_t(observed_ready_sequence, observed_active_sequence);
    TEST_ASSERT_TRUE(manager->remove_event_listener(serialized_listener_id));

    const auto paused_scan_count = fake->scan_count();
    std::this_thread::sleep_for(std::chrono::milliseconds(
                                    BROOKESIA_HAL_ADAPTOR_EXPANSION_SCAN_INTERVAL_MS * 3
                                ));
    TEST_ASSERT_EQUAL_size_t(paused_scan_count, fake->scan_count());

    TEST_ASSERT_TRUE(lease.reset(&error_message));
    TEST_ASSERT_EQUAL_size_t(initial_release_count + 1, fake->release_count());
    TEST_ASSERT_TRUE(wait_for_state(*manager, hal::expansion::ModuleState::Ready));

    // A provider-private identity change must invalidate an otherwise identical
    // public snapshot (for example, swapping two same-model modules).
    const auto identity_before = get_fake_info(*manager);
    TEST_ASSERT_TRUE(identity_before.has_value());
    const auto claims_before_identity_change = fake->claim_count();
    const auto before_identity_change = fake->set_scan_info(make_scan_info(
                                            hal::expansion::ModuleState::Ready, FAKE_MODULE_TYPE, 42, "Fake Camera", 1
                                        ));
    TEST_ASSERT_TRUE(fake->wait_for_scan_count(before_identity_change + 3));
    const auto identity_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    std::optional<hal::expansion::ModuleInfo> identity_after;
    do {
        identity_after = get_fake_info(*manager);
        if (identity_after && (identity_after->generation > identity_before->generation)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < identity_deadline);
    TEST_ASSERT_TRUE(identity_after.has_value());
    TEST_ASSERT_TRUE(identity_after->generation > identity_before->generation);
    auto replaced_module_lease = hal::expansion::claim_module(
                                     identity_before->provider, identity_before->slot,
                                     identity_before->generation, &error_message
                                 );
    TEST_ASSERT_FALSE(static_cast<bool>(replaced_module_lease));
    TEST_ASSERT_EQUAL_size_t(claims_before_identity_change, fake->claim_count());

    std::mutex blocking_listener_mutex;
    std::condition_variable blocking_listener_condition;
    bool error_callback_entered = false;
    bool allow_error_callback_exit = false;
    bool error_callback_finished = false;
    const auto blocking_listener_id = manager->add_event_listener(
    [&](const hal::expansion::ModuleInfo & info) {
        if (!is_fake_info(info) || (info.state != hal::expansion::ModuleState::Error)) {
            return;
        }
        std::unique_lock lock(blocking_listener_mutex);
        error_callback_entered = true;
        blocking_listener_condition.notify_all();
        blocking_listener_condition.wait(lock, [&]() {
            return allow_error_callback_exit;
        });
        error_callback_finished = true;
        blocking_listener_condition.notify_all();
    }
                                      );
    TEST_ASSERT_NOT_EQUAL(0, blocking_listener_id);

    const auto before_error = fake->set_scan_error(true);
    TEST_ASSERT_TRUE(fake->wait_for_scan_count(before_error + 3));
    bool error_callback_did_enter = false;
    {
        std::unique_lock lock(blocking_listener_mutex);
        error_callback_did_enter = blocking_listener_condition.wait_for(
        lock, std::chrono::milliseconds(200), [&]() {
            return error_callback_entered;
        }
                                   );
    }
    TEST_ASSERT_TRUE(error_callback_did_enter);

    std::atomic<bool> remove_started = false;
    std::atomic<bool> remove_returned = false;
    bool remove_succeeded = false;
    std::thread remove_thread([&]() {
        remove_started = true;
        remove_succeeded = manager->remove_event_listener(blocking_listener_id);
        remove_returned = true;
    });
    while (!remove_started.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const bool remove_returned_before_callback = remove_returned.load();
    {
        std::lock_guard lock(blocking_listener_mutex);
        allow_error_callback_exit = true;
    }
    blocking_listener_condition.notify_all();
    remove_thread.join();
    TEST_ASSERT_FALSE(remove_returned_before_callback);
    TEST_ASSERT_TRUE(remove_succeeded);
    TEST_ASSERT_TRUE(error_callback_finished);
    TEST_ASSERT_TRUE(wait_for_state(*manager, hal::expansion::ModuleState::Error));

    hal::expansion::ModuleLease callback_lease;
    std::atomic<bool> callback_claim_finished = false;
    std::string callback_claim_error;
    const auto callback_claim_listener_id = manager->add_event_listener(
    [&](const hal::expansion::ModuleInfo & info) {
        if (!is_fake_info(info) || (info.state != hal::expansion::ModuleState::Ready)) {
            return;
        }
        callback_lease = hal::expansion::claim_module(
                             info.provider, info.slot, info.generation, &callback_claim_error
                         );
        callback_claim_finished = true;
    }
                                            );
    TEST_ASSERT_NOT_EQUAL(0, callback_claim_listener_id);

    fake->set_scan_info(make_scan_info(hal::expansion::ModuleState::Ready, FAKE_MODULE_TYPE));
    const auto callback_claim_deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (!callback_claim_finished.load() &&
            (std::chrono::steady_clock::now() < callback_claim_deadline)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    TEST_ASSERT_TRUE(callback_claim_finished.load());
    TEST_ASSERT_TRUE(static_cast<bool>(callback_lease));
    TEST_ASSERT_EQUAL(hal::expansion::ModuleState::Active, callback_lease.get_info().state);
    TEST_ASSERT_TRUE(manager->remove_event_listener(callback_claim_listener_id));
    TEST_ASSERT_TRUE(callback_lease.reset(&callback_claim_error));
    TEST_ASSERT_TRUE(wait_for_state(*manager, hal::expansion::ModuleState::Ready));

    ready = get_fake_info(*manager);
    TEST_ASSERT_TRUE(ready.has_value());
    fake->set_claim_error(true);
    auto failed_lease = hal::expansion::claim_module(
                            ready->provider, ready->slot, ready->generation, &error_message
                        );
    TEST_ASSERT_FALSE(static_cast<bool>(failed_lease));
    TEST_ASSERT_TRUE(wait_for_state(*manager, hal::expansion::ModuleState::Error));
    fake->set_claim_error(false);

    TEST_ASSERT_TRUE(manager->remove_event_listener(listener_id));
    const auto events_after_remove = event_count.load();

    std::atomic<size_t> self_remove_count = 0;
    std::atomic<bool> self_remove_succeeded = false;
    hal::expansion::ModuleManagerIface::EventListenerId self_removing_listener_id = 0;
    self_removing_listener_id = manager->add_event_listener(
    [&](const hal::expansion::ModuleInfo & info) {
        if (!is_fake_info(info)) {
            return;
        }
        ++self_remove_count;
        self_remove_succeeded = manager->remove_event_listener(self_removing_listener_id);
    }
                                );
    TEST_ASSERT_NOT_EQUAL(0, self_removing_listener_id);

    fake->set_scan_info(make_scan_info(hal::expansion::ModuleState::Empty));
    TEST_ASSERT_TRUE(wait_for_state(*manager, hal::expansion::ModuleState::Empty));
    TEST_ASSERT_EQUAL_size_t(events_after_remove, event_count.load());
    const auto self_remove_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (!self_remove_succeeded.load() && (std::chrono::steady_clock::now() < self_remove_deadline)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    TEST_ASSERT_TRUE(self_remove_succeeded.load());
    TEST_ASSERT_EQUAL_size_t(1, self_remove_count.load());

    fake->set_scan_info(make_scan_info(hal::expansion::ModuleState::Invalid));
    TEST_ASSERT_TRUE(wait_for_state(*manager, hal::expansion::ModuleState::Invalid));
    TEST_ASSERT_EQUAL_size_t(1, self_remove_count.load());

    const auto scans_before_requested_scan = fake->scan_count();
    TEST_ASSERT_TRUE(hal::expansion::request_rescan());
    TEST_ASSERT_TRUE(fake->wait_for_scan_count(scans_before_requested_scan + 1));

    std::mutex stop_race_mutex;
    std::condition_variable stop_race_condition;
    bool stop_race_callback_entered = false;
    bool allow_stop_race_claim = false;
    bool stop_race_callback_finished = false;
    std::atomic<bool> stop_race_listener_removed = false;
    std::atomic<bool> stop_race_claim_succeeded = false;
    hal::expansion::ModuleManagerIface::EventListenerId stop_race_listener_id = 0;
    stop_race_listener_id = manager->add_event_listener(
    [&](const hal::expansion::ModuleInfo & info) {
        if (!is_fake_info(info) || (info.state != hal::expansion::ModuleState::Ready)) {
            return;
        }

        // Remove the listener before the other thread destroys the manager.
        // This lets that thread reach release_consumer() while this callback
        // is still in flight, which is the deadlock scenario under test.
        stop_race_listener_removed = manager->remove_event_listener(stop_race_listener_id);
        {
            std::unique_lock lock(stop_race_mutex);
            stop_race_callback_entered = true;
            stop_race_condition.notify_all();
            stop_race_condition.wait(lock, [&]() {
                return allow_stop_race_claim;
            });
        }

        std::string stop_race_claim_error;
        auto stop_race_lease = hal::expansion::claim_module(
                                   info.provider, info.slot, info.generation,
                                   &stop_race_claim_error
                               );
        stop_race_claim_succeeded = static_cast<bool>(stop_race_lease);
        {
            std::lock_guard lock(stop_race_mutex);
            stop_race_callback_finished = true;
        }
        stop_race_condition.notify_all();
    }
                            );
    TEST_ASSERT_NOT_EQUAL(0, stop_race_listener_id);

    fake->set_scan_info(make_scan_info(
                            hal::expansion::ModuleState::Ready, FAKE_MODULE_TYPE, 42, "Fake Camera"
                        ));
    bool stop_race_callback_did_enter = false;
    {
        std::unique_lock lock(stop_race_mutex);
        stop_race_callback_did_enter = stop_race_condition.wait_for(
        lock, std::chrono::milliseconds(2000), [&]() {
            return stop_race_callback_entered;
        }
                                       );
    }
    TEST_ASSERT_TRUE(stop_race_callback_did_enter);
    TEST_ASSERT_TRUE(stop_race_listener_removed.load());

    std::atomic<bool> external_release_started = false;
    std::atomic<bool> external_release_finished = false;
    std::thread external_release_thread([&]() {
        external_release_started = true;
        manager.reset();
        external_release_finished = true;
    });
    while (!external_release_started.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const bool external_release_finished_before_callback = external_release_finished.load();
    {
        std::lock_guard lock(stop_race_mutex);
        allow_stop_race_claim = true;
    }
    stop_race_condition.notify_all();
    external_release_thread.join();

    TEST_ASSERT_FALSE(external_release_finished_before_callback);
    TEST_ASSERT_TRUE(stop_race_callback_finished);
    TEST_ASSERT_FALSE(stop_race_claim_succeeded.load());
    TEST_ASSERT_TRUE(external_release_finished.load());
    TEST_ASSERT_EQUAL_size_t(initial_stop_count + 1, fake->stop_count());

    fake->set_scan_info(make_scan_info(hal::expansion::ModuleState::Invalid));
    manager = hal::acquire_first_interface<hal::expansion::ModuleManagerIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(manager));
    TEST_ASSERT_TRUE(wait_for_state(*manager, hal::expansion::ModuleState::Invalid));
    TEST_ASSERT_EQUAL_size_t(initial_start_count + 2, fake->start_count());

    std::atomic<bool> stop_listener_removed = false;
    std::atomic<bool> manager_release_requested = false;
    hal::expansion::ModuleManagerIface::EventListenerId stop_listener_id = 0;
    stop_listener_id = manager->add_event_listener(
    [&](const hal::expansion::ModuleInfo & info) {
        if (!is_fake_info(info) || (info.state != hal::expansion::ModuleState::Empty)) {
            return;
        }
        stop_listener_removed = manager->remove_event_listener(stop_listener_id);
        manager.reset();
        manager_release_requested = true;
    }
                       );
    TEST_ASSERT_NOT_EQUAL(0, stop_listener_id);

    fake->set_scan_info(make_scan_info(hal::expansion::ModuleState::Empty));
    const auto release_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (!manager_release_requested.load() && (std::chrono::steady_clock::now() < release_deadline)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    TEST_ASSERT_TRUE(manager_release_requested.load());
    TEST_ASSERT_TRUE(stop_listener_removed.load());
    const auto stop_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while ((fake->stop_count() < (initial_stop_count + 2)) &&
            (std::chrono::steady_clock::now() < stop_deadline)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto scans_after_stop = fake->scan_count();
    TEST_ASSERT_EQUAL_size_t(initial_stop_count + 2, fake->stop_count());
    std::this_thread::sleep_for(std::chrono::milliseconds(
                                    BROOKESIA_HAL_ADAPTOR_EXPANSION_SCAN_INTERVAL_MS * 2
                                ));
    TEST_ASSERT_EQUAL_size_t(scans_after_stop, fake->scan_count());

    fake->set_scan_info(make_scan_info(hal::expansion::ModuleState::Ready, FAKE_MODULE_TYPE));
    auto matching_lease = hal::expansion::claim_matching(
                              FAKE_PROVIDER_NAME, FAKE_SLOT_NAME, FAKE_MODULE_TYPE, 1200, &error_message
                          );
    TEST_ASSERT_TRUE(static_cast<bool>(matching_lease));
    TEST_ASSERT_EQUAL_size_t(initial_start_count + 3, fake->start_count());
    TEST_ASSERT_EQUAL_size_t(initial_stop_count + 2, fake->stop_count());
    TEST_ASSERT_TRUE(matching_lease.reset(&error_message));
    TEST_ASSERT_EQUAL_size_t(initial_stop_count + 3, fake->stop_count());
}

void settle_expansion_runtime_scenario()
{
    // Workers that exit by deleting themselves leave their final FreeRTOS
    // cleanup to the idle task.
    std::this_thread::sleep_for(std::chrono::milliseconds(RUNTIME_SETTLE_DELAY_MS));
    esp_reent_cleanup();
}

} // namespace

TEST_CASE("HAL adaptor: expansion module runtime debounce claim and lifecycle", "[hal][adaptor][expansion]")
{
    // C++ threads, Board Manager and ESP-IDF allocate bounded runtime state on
    // their first two complete lifecycles. Measure only after that state reaches
    // its observed plateau, while keeping Unity's leak threshold at zero.
    for (size_t i = 0; i < RUNTIME_WARMUP_ITERATIONS; i++) {
        run_expansion_runtime_scenario();
        settle_expansion_runtime_scenario();
    }
    unity_utils_record_free_mem();

    for (size_t i = 0; i < RUNTIME_MEASURED_ITERATIONS; i++) {
        run_expansion_runtime_scenario();
        settle_expansion_runtime_scenario();
    }
}

#else

TEST_CASE("HAL adaptor: expansion module runtime disabled", "[hal][adaptor][expansion]")
{
    TEST_IGNORE_MESSAGE("Expansion modules disabled");
}

#endif
