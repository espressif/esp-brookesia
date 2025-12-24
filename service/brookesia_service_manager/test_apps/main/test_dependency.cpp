/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <atomic>
#include <memory>
#include <string>
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/service_manager.hpp"
#include "common_def.hpp"

using namespace esp_brookesia::service;

static auto &service_manager = ServiceManager::get_instance();
static int global_init_order = 0;
static int global_start_order = 0;
static int global_stop_order = 0;
static int global_deinit_order = 0;

// ============================================================================
// Service dependency test
// ============================================================================

TEST_CASE("Test Dependency: basic dependency and order", "[brookesia][service][dependency][basic]")
{
    BROOKESIA_LOGI("=== Test basic service dependency ===");

    // Create a test service with dependencies
    class ServiceA : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceA() : ServiceBase({
            .name = "ServiceA",
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceA initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceA started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceA stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceA deinitialized (order: %d)", deinit_order);
        }
    };

    class ServiceB : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceB() : ServiceBase({
            .name = "ServiceB",
            .dependencies = {"ServiceA"}
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceB initialized (order: %d, depends on ServiceA)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceB started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceB stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceB deinitialized (order: %d)", deinit_order);
        }
    };

    // Reset counters
    global_init_order = 0;
    global_start_order = 0;
    global_stop_order = 0;
    global_deinit_order = 0;

    // Register services (note: order is not important, the framework will automatically sort)
    ServiceRegistry::register_plugin<ServiceB>("ServiceB", []() {
        return std::make_unique<ServiceB>();
    });
    ServiceRegistry::register_plugin<ServiceA>("ServiceA", []() {
        return std::make_unique<ServiceA>();
    });

    // init will automatically initialize in dependency order: A -> B
    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    // bind will automatically bind dependencies and start in order
    auto binding_b = service_manager.bind("ServiceB");
    TEST_ASSERT_TRUE(binding_b.is_valid());

    auto service_a = std::dynamic_pointer_cast<ServiceA>(service_manager.bind("ServiceA").get_service());
    auto service_b = std::dynamic_pointer_cast<ServiceB>(binding_b.get_service());

    TEST_ASSERT_NOT_NULL(service_a);
    TEST_ASSERT_NOT_NULL(service_b);

    // Verify initialization order: A < B
    TEST_ASSERT_GREATER_THAN(service_a->init_order, service_b->init_order);

    // Verify startup order: A < B (dependencies start first)
    TEST_ASSERT_GREATER_THAN(service_a->start_order, service_b->start_order);

    TEST_ASSERT_TRUE(service_a->is_initialized());
    TEST_ASSERT_TRUE(service_b->is_initialized());
    TEST_ASSERT_TRUE(service_a->is_running());
    TEST_ASSERT_TRUE(service_b->is_running());

    service_manager.stop();
    service_manager.deinit();

    // Verify stop order: B > A (dependencies stop last)
    TEST_ASSERT_GREATER_THAN(service_b->stop_order, service_a->stop_order);

    // Verify deinitialization order: B > A (dependencies deinitialize last)
    TEST_ASSERT_GREATER_THAN(service_b->deinit_order, service_a->deinit_order);

    // Clean up registered services
    ServiceRegistry::remove_plugin("ServiceA");
    ServiceRegistry::remove_plugin("ServiceB");
}

TEST_CASE("Test Dependency: circular dependency detection", "[brookesia][service][dependency][circular]")
{
    BROOKESIA_LOGI("=== Test circular dependency detection ===");

    // Create circular dependency: X -> Z -> Y -> X
    class ServiceX : public ServiceBase {
    public:
        ServiceX() : ServiceBase({
            .name = "ServiceX",
            .dependencies = {"ServiceZ"}
        }) {}
        bool on_init() override
        {
            BROOKESIA_LOGI("ServiceX initialized");
            return true;
        }
    };

    class ServiceY : public ServiceBase {
    public:
        ServiceY() : ServiceBase({
            .name = "ServiceY",
            .dependencies = {"ServiceX"}
        }) {}
        bool on_init() override
        {
            BROOKESIA_LOGI("ServiceY initialized");
            return true;
        }
    };

    class ServiceZ : public ServiceBase {
    public:
        ServiceZ() : ServiceBase({
            .name = "ServiceZ",
            .dependencies = {"ServiceY"}
        }) {}
        bool on_init() override
        {
            BROOKESIA_LOGI("ServiceZ initialized");
            return true;
        }
    };

    // Register circular dependency services
    ServiceRegistry::register_plugin<ServiceX>("ServiceX", []() {
        return std::make_unique<ServiceX>();
    });
    ServiceRegistry::register_plugin<ServiceY>("ServiceY", []() {
        return std::make_unique<ServiceY>();
    });
    ServiceRegistry::register_plugin<ServiceZ>("ServiceZ", []() {
        return std::make_unique<ServiceZ>();
    });

    // init should detect circular dependency and handle it (return empty initialization order)
    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    // Services should not be bindable (because they were not successfully initialized)
    auto binding_x = service_manager.bind("ServiceX");
    auto binding_y = service_manager.bind("ServiceY");
    auto binding_z = service_manager.bind("ServiceZ");

    TEST_ASSERT_FALSE(binding_x.is_valid());
    TEST_ASSERT_FALSE(binding_y.is_valid());
    TEST_ASSERT_FALSE(binding_z.is_valid());

    service_manager.stop();
    service_manager.deinit();

    // Clean up registered services
    ServiceRegistry::remove_plugin("ServiceX");
    ServiceRegistry::remove_plugin("ServiceY");
    ServiceRegistry::remove_plugin("ServiceZ");
}

TEST_CASE("Test Dependency: multi-level chain", "[brookesia][service][dependency][chain]")
{
    BROOKESIA_LOGI("=== Test multi-level dependency chain ===");

    global_init_order = 0;   // Reset counters
    global_start_order = 0;
    global_stop_order = 0;
    global_deinit_order = 0;

    // Create multi-level dependency chain: D -> C -> B -> A
    class ServiceDepA : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceDepA() : ServiceBase({
            .name = "ServiceDepA",
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceDepA initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceDepA started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceDepA stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceDepA deinitialized (order: %d)", deinit_order);
        }
    };

    class ServiceDepB : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceDepB() : ServiceBase({
            .name = "ServiceDepB",
            .dependencies = {"ServiceDepA"}
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceDepB initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceDepB started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceDepB stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceDepB deinitialized (order: %d)", deinit_order);
        }
    };

    class ServiceDepC : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceDepC() : ServiceBase({
            .name = "ServiceDepC",
            .dependencies = {"ServiceDepB"}
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceDepC initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceDepC started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceDepC stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceDepC deinitialized (order: %d)", deinit_order);
        }
    };

    class ServiceDepD : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceDepD() : ServiceBase({
            .name = "ServiceDepD",
            .dependencies = {"ServiceDepC"}
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceDepD initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceDepD started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceDepD stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceDepD deinitialized (order: %d)", deinit_order);
        }
    };

    // Register in reverse order (test if the framework can correctly sort)
    ServiceRegistry::release_all_instances();
    ServiceRegistry::register_plugin<ServiceDepD>("ServiceDepD", []() {
        return std::make_unique<ServiceDepD>();
    });
    ServiceRegistry::register_plugin<ServiceDepC>("ServiceDepC", []() {
        return std::make_unique<ServiceDepC>();
    });
    ServiceRegistry::register_plugin<ServiceDepB>("ServiceDepB", []() {
        return std::make_unique<ServiceDepB>();
    });
    ServiceRegistry::register_plugin<ServiceDepA>("ServiceDepA", []() {
        return std::make_unique<ServiceDepA>();
    });

    // init will automatically initialize in dependency order: A -> B -> C -> D
    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    // bind D will automatically bind the entire dependency chain: A, B, C, D
    auto binding_d = service_manager.bind("ServiceDepD");
    TEST_ASSERT_TRUE(binding_d.is_valid());

    // Get services and verify
    auto binding_a = service_manager.bind("ServiceDepA");
    auto binding_b = service_manager.bind("ServiceDepB");
    auto binding_c = service_manager.bind("ServiceDepC");

    auto service_a = std::dynamic_pointer_cast<ServiceDepA>(binding_a.get_service());
    auto service_b = std::dynamic_pointer_cast<ServiceDepB>(binding_b.get_service());
    auto service_c = std::dynamic_pointer_cast<ServiceDepC>(binding_c.get_service());
    auto service_d = std::dynamic_pointer_cast<ServiceDepD>(binding_d.get_service());

    TEST_ASSERT_NOT_NULL(service_a);
    TEST_ASSERT_NOT_NULL(service_b);
    TEST_ASSERT_NOT_NULL(service_c);
    TEST_ASSERT_NOT_NULL(service_d);

    // Verify initialization order: A < B < C < D
    TEST_ASSERT_GREATER_THAN(service_a->init_order, service_b->init_order);
    TEST_ASSERT_GREATER_THAN(service_b->init_order, service_c->init_order);
    TEST_ASSERT_GREATER_THAN(service_c->init_order, service_d->init_order);

    // Verify startup order: A < B < C < D (dependency chain starts in order)
    TEST_ASSERT_GREATER_THAN(service_a->start_order, service_b->start_order);
    TEST_ASSERT_GREATER_THAN(service_b->start_order, service_c->start_order);
    TEST_ASSERT_GREATER_THAN(service_c->start_order, service_d->start_order);

    service_manager.stop();
    service_manager.deinit();

    // Verify stop order: D < C < B < A (reverse order stop)
    TEST_ASSERT_GREATER_THAN(service_d->stop_order, service_c->stop_order);
    TEST_ASSERT_GREATER_THAN(service_c->stop_order, service_b->stop_order);
    TEST_ASSERT_GREATER_THAN(service_b->stop_order, service_a->stop_order);

    // Verify deinitialization order: D < C < B < A (reverse order deinitialize)
    TEST_ASSERT_GREATER_THAN(service_d->deinit_order, service_c->deinit_order);
    TEST_ASSERT_GREATER_THAN(service_c->deinit_order, service_b->deinit_order);
    TEST_ASSERT_GREATER_THAN(service_b->deinit_order, service_a->deinit_order);

    // Clean up registered services
    ServiceRegistry::remove_plugin("ServiceDepA");
    ServiceRegistry::remove_plugin("ServiceDepB");
    ServiceRegistry::remove_plugin("ServiceDepC");
    ServiceRegistry::remove_plugin("ServiceDepD");
}

TEST_CASE("Test Dependency: diamond dependency", "[brookesia][service][dependency][diamond]")
{
    BROOKESIA_LOGI("=== Test diamond dependency ===");

    global_init_order = 0;   // Reset counters
    global_start_order = 0;
    global_stop_order = 0;
    global_deinit_order = 0;

    /*
    // Create diamond dependency:
    //      A
    //     / \
    //    B   C
    //     \ /
    //      D
    */
    class ServiceDiaA : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceDiaA() : ServiceBase({
            .name = "ServiceDiaA",
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceDiaA initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceDiaA started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceDiaA stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceDiaA deinitialized (order: %d)", deinit_order);
        }
    };

    class ServiceDiaB : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceDiaB() : ServiceBase({
            .name = "ServiceDiaB",
            .dependencies = {"ServiceDiaA"}
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceDiaB initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceDiaB started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceDiaB stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceDiaB deinitialized (order: %d)", deinit_order);
        }
    };

    class ServiceDiaC : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceDiaC() : ServiceBase({
            .name = "ServiceDiaC",
            .dependencies = {"ServiceDiaA"}
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceDiaC initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceDiaC started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceDiaC stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceDiaC deinitialized (order: %d)", deinit_order);
        }
    };

    class ServiceDiaD : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceDiaD()
            : ServiceBase({
            .name = "ServiceDiaD",
            .dependencies = {"ServiceDiaB", "ServiceDiaC"}
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceDiaD initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceDiaD started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceDiaD stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceDiaD deinitialized (order: %d)", deinit_order);
        }
    };

    // Register services
    ServiceRegistry::release_all_instances();
    ServiceRegistry::register_plugin<ServiceDiaD>("ServiceDiaD", []() {
        return std::make_unique<ServiceDiaD>();
    });
    ServiceRegistry::register_plugin<ServiceDiaC>("ServiceDiaC", []() {
        return std::make_unique<ServiceDiaC>();
    });
    ServiceRegistry::register_plugin<ServiceDiaB>("ServiceDiaB", []() {
        return std::make_unique<ServiceDiaB>();
    });
    ServiceRegistry::register_plugin<ServiceDiaA>("ServiceDiaA", []() {
        return std::make_unique<ServiceDiaA>();
    });

    // init will automatically handle diamond dependency
    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    // bind D will automatically bind all dependencies: A, B, C, D
    auto binding_d = service_manager.bind("ServiceDiaD");
    TEST_ASSERT_TRUE(binding_d.is_valid());

    // Get services and verify
    auto binding_a = service_manager.bind("ServiceDiaA");
    auto binding_b = service_manager.bind("ServiceDiaB");
    auto binding_c = service_manager.bind("ServiceDiaC");

    auto service_a = std::dynamic_pointer_cast<ServiceDiaA>(binding_a.get_service());
    auto service_b = std::dynamic_pointer_cast<ServiceDiaB>(binding_b.get_service());
    auto service_c = std::dynamic_pointer_cast<ServiceDiaC>(binding_c.get_service());
    auto service_d = std::dynamic_pointer_cast<ServiceDiaD>(binding_d.get_service());

    TEST_ASSERT_NOT_NULL(service_a);
    TEST_ASSERT_NOT_NULL(service_b);
    TEST_ASSERT_NOT_NULL(service_c);
    TEST_ASSERT_NOT_NULL(service_d);

    // A must be initialized first
    TEST_ASSERT_GREATER_THAN(service_a->init_order, service_b->init_order);
    TEST_ASSERT_GREATER_THAN(service_a->init_order, service_c->init_order);

    // D must be initialized after B and C
    TEST_ASSERT_GREATER_THAN(service_b->init_order, service_d->init_order);
    TEST_ASSERT_GREATER_THAN(service_c->init_order, service_d->init_order);

    // A must be started first
    TEST_ASSERT_GREATER_THAN(service_a->start_order, service_b->start_order);
    TEST_ASSERT_GREATER_THAN(service_a->start_order, service_c->start_order);

    // D must be started after B and C
    TEST_ASSERT_GREATER_THAN(service_b->start_order, service_d->start_order);
    TEST_ASSERT_GREATER_THAN(service_c->start_order, service_d->start_order);

    service_manager.stop();
    service_manager.deinit();

    // Verify stop order (opposite of startup)
    // D must be stopped first
    TEST_ASSERT_GREATER_THAN(service_d->stop_order, service_b->stop_order);
    TEST_ASSERT_GREATER_THAN(service_d->stop_order, service_c->stop_order);

    // A must be stopped last
    TEST_ASSERT_GREATER_THAN(service_b->stop_order, service_a->stop_order);
    TEST_ASSERT_GREATER_THAN(service_c->stop_order, service_a->stop_order);

    // Verify deinitialization order (opposite of initialization)
    // D must be deinitialized first
    TEST_ASSERT_GREATER_THAN(service_d->deinit_order, service_b->deinit_order);
    TEST_ASSERT_GREATER_THAN(service_d->deinit_order, service_c->deinit_order);

    // A must be deinitialized last
    TEST_ASSERT_GREATER_THAN(service_b->deinit_order, service_a->deinit_order);
    TEST_ASSERT_GREATER_THAN(service_c->deinit_order, service_a->deinit_order);

    // Clean up registered services
    ServiceRegistry::remove_plugin("ServiceDiaA");
    ServiceRegistry::remove_plugin("ServiceDiaB");
    ServiceRegistry::remove_plugin("ServiceDiaC");
    ServiceRegistry::remove_plugin("ServiceDiaD");
}

TEST_CASE("Test Dependency: missing dependency error", "[brookesia][service][dependency][missing]")
{
    BROOKESIA_LOGI("=== Test missing dependency error ===");

    // Create service with missing dependency
    class ServiceMissing : public ServiceBase {
    public:
        ServiceMissing() : ServiceBase({
            .name = "ServiceMissing",
            .dependencies = {"NonExistentService"}
        }) {}
        bool on_init() override
        {
            BROOKESIA_LOGI("ServiceMissing initialized (despite missing dependency)");
            return true;
        }
    };

    // Register services (dependency service does not exist)
    ServiceRegistry::release_all_instances();
    ServiceRegistry::register_plugin<ServiceMissing>("ServiceMissing", []() {
        return std::make_unique<ServiceMissing>();
    });

    // init will emit warning but continue initialization
    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    // Services should still be able to initialize and bind (framework will emit error)
    auto binding = service_manager.bind("ServiceMissing");
    TEST_ASSERT_FALSE(binding.is_valid());

    service_manager.stop();
    service_manager.deinit();

    // Clean up registered services
    ServiceRegistry::remove_plugin("ServiceMissing");
}

TEST_CASE("Test Dependency: complex dependency graph", "[brookesia][service][dependency][complex]")
{
    BROOKESIA_LOGI("=== Test complex dependency graph ===");

    global_init_order = 0;   // Reset counters
    global_start_order = 0;
    global_stop_order = 0;
    global_deinit_order = 0;

    // Create complex dependency graph:
    //      E1 (no dep)
    //      E2 (no dep)
    //      E3 -> E1
    //      E4 -> E1, E2
    //      E5 -> E3, E4
    class ServiceE1 : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceE1() : ServiceBase({
            .name = "ServiceE1",
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceE1 initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceE1 started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceE1 stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceE1 deinitialized (order: %d)", deinit_order);
        }
    };

    class ServiceE2 : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceE2() : ServiceBase({
            .name = "ServiceE2",
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceE2 initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceE2 started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceE2 stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceE2 deinitialized (order: %d)", deinit_order);
        }
    };

    class ServiceE3 : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceE3() : ServiceBase({
            .name = "ServiceE3",
            .dependencies = {"ServiceE1"}
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceE3 initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceE3 started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceE3 stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceE3 deinitialized (order: %d)", deinit_order);
        }
    };

    class ServiceE4 : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceE4() : ServiceBase({
            .name = "ServiceE4",
            .dependencies = {"ServiceE1", "ServiceE2"}
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceE4 initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceE4 started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceE4 stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceE4 deinitialized (order: %d)", deinit_order);
        }
    };

    class ServiceE5 : public ServiceBase {
    public:
        int init_order = 0;
        int start_order = 0;
        int stop_order = 0;
        int deinit_order = 0;

        ServiceE5() : ServiceBase({
            .name = "ServiceE5",
            .dependencies = {"ServiceE3", "ServiceE4"}
        }) {}
        bool on_init() override
        {
            init_order = ++global_init_order;
            BROOKESIA_LOGI("ServiceE5 initialized (order: %d)", init_order);
            return true;
        }
        bool on_start() override
        {
            start_order = ++global_start_order;
            BROOKESIA_LOGI("ServiceE5 started (order: %d)", start_order);
            return true;
        }
        void on_stop() override
        {
            stop_order = ++global_stop_order;
            BROOKESIA_LOGI("ServiceE5 stopped (order: %d)", stop_order);
        }
        void on_deinit() override
        {
            deinit_order = ++global_deinit_order;
            BROOKESIA_LOGI("ServiceE5 deinitialized (order: %d)", deinit_order);
        }
    };

    // Register services in random order
    ServiceRegistry::release_all_instances();
    ServiceRegistry::register_plugin<ServiceE5>("ServiceE5", []() {
        return std::make_unique<ServiceE5>();
    });
    ServiceRegistry::register_plugin<ServiceE3>("ServiceE3", []() {
        return std::make_unique<ServiceE3>();
    });
    ServiceRegistry::register_plugin<ServiceE1>("ServiceE1", []() {
        return std::make_unique<ServiceE1>();
    });
    ServiceRegistry::register_plugin<ServiceE4>("ServiceE4", []() {
        return std::make_unique<ServiceE4>();
    });
    ServiceRegistry::register_plugin<ServiceE2>("ServiceE2", []() {
        return std::make_unique<ServiceE2>();
    });

    // init will automatically handle complex dependency
    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    // bind E5 will automatically bind all dependencies
    auto binding_e5 = service_manager.bind("ServiceE5");
    TEST_ASSERT_TRUE(binding_e5.is_valid());

    // Get services and verify
    auto binding_e1 = service_manager.bind("ServiceE1");
    auto binding_e2 = service_manager.bind("ServiceE2");
    auto binding_e3 = service_manager.bind("ServiceE3");
    auto binding_e4 = service_manager.bind("ServiceE4");

    auto service_e1 = std::dynamic_pointer_cast<ServiceE1>(binding_e1.get_service());
    auto service_e2 = std::dynamic_pointer_cast<ServiceE2>(binding_e2.get_service());
    auto service_e3 = std::dynamic_pointer_cast<ServiceE3>(binding_e3.get_service());
    auto service_e4 = std::dynamic_pointer_cast<ServiceE4>(binding_e4.get_service());
    auto service_e5 = std::dynamic_pointer_cast<ServiceE5>(binding_e5.get_service());

    TEST_ASSERT_NOT_NULL(service_e1);
    TEST_ASSERT_NOT_NULL(service_e2);
    TEST_ASSERT_NOT_NULL(service_e3);
    TEST_ASSERT_NOT_NULL(service_e4);
    TEST_ASSERT_NOT_NULL(service_e5);

    // Verify initialization order
    // E3 must be initialized after E1
    TEST_ASSERT_GREATER_THAN(service_e1->init_order, service_e3->init_order);

    // E4 must be initialized after E1 and E2
    TEST_ASSERT_GREATER_THAN(service_e1->init_order, service_e4->init_order);
    TEST_ASSERT_GREATER_THAN(service_e2->init_order, service_e4->init_order);

    // E5 must be initialized after E3 and E4
    TEST_ASSERT_GREATER_THAN(service_e3->init_order, service_e5->init_order);
    TEST_ASSERT_GREATER_THAN(service_e4->init_order, service_e5->init_order);

    // Verify startup order
    // E3 must be started after E1
    TEST_ASSERT_GREATER_THAN(service_e1->start_order, service_e3->start_order);

    // E4 must be started after E1 and E2
    TEST_ASSERT_GREATER_THAN(service_e1->start_order, service_e4->start_order);
    TEST_ASSERT_GREATER_THAN(service_e2->start_order, service_e4->start_order);

    // E5 must be started after E3 and E4
    TEST_ASSERT_GREATER_THAN(service_e3->start_order, service_e5->start_order);
    TEST_ASSERT_GREATER_THAN(service_e4->start_order, service_e5->start_order);

    service_manager.stop();
    service_manager.deinit();

    // Verify stop order (opposite of startup)
    // E5 must be stopped first
    TEST_ASSERT_GREATER_THAN(service_e5->stop_order, service_e3->stop_order);
    TEST_ASSERT_GREATER_THAN(service_e5->stop_order, service_e4->stop_order);

    // E3 and E4 must be stopped before E1/E2
    TEST_ASSERT_GREATER_THAN(service_e3->stop_order, service_e1->stop_order);
    TEST_ASSERT_GREATER_THAN(service_e4->stop_order, service_e1->stop_order);
    TEST_ASSERT_GREATER_THAN(service_e4->stop_order, service_e2->stop_order);

    // Verify deinitialization order (opposite of initialization)
    // E5 must be deinitialized first
    TEST_ASSERT_GREATER_THAN(service_e5->deinit_order, service_e3->deinit_order);
    TEST_ASSERT_GREATER_THAN(service_e5->deinit_order, service_e4->deinit_order);

    // E3 and E4 must be deinitialized before E1/E2
    TEST_ASSERT_GREATER_THAN(service_e3->deinit_order, service_e1->deinit_order);
    TEST_ASSERT_GREATER_THAN(service_e4->deinit_order, service_e1->deinit_order);
    TEST_ASSERT_GREATER_THAN(service_e4->deinit_order, service_e2->deinit_order);

    // Clean up registered services
    ServiceRegistry::remove_plugin("ServiceE1");
    ServiceRegistry::remove_plugin("ServiceE2");
    ServiceRegistry::remove_plugin("ServiceE3");
    ServiceRegistry::remove_plugin("ServiceE4");
    ServiceRegistry::remove_plugin("ServiceE5");
}
