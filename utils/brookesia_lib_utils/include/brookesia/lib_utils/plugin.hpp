/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <functional>

namespace esp_brookesia::lib_utils {

/**
 * @brief Plugin information structure for unified registry
 */
template <typename T>
struct PluginInfo {
    std::function<std::shared_ptr<T>()> factory;    ///< Factory function to create instances
    std::shared_ptr<T> instance = nullptr;          ///< Plugin instance (may be null if not created yet)

    PluginInfo(std::function<std::shared_ptr<T>()> f)
        : factory(std::move(f)) {}
};

/**
 * @brief Plugin registration and management class
 * Uses unified map-based storage for all plugin information (keyed by name)
 *
 * @tparam T Base class type for plugins
 */
template <typename T>
class PluginRegistry {
public:
    using FactoryFunc = std::function<std::shared_ptr<T>()>;
    using PluginInfoType = PluginInfo<T>;

private:
    static std::map<std::string /* name */, PluginInfoType> &get_plugins()
    {
        static std::map<std::string, PluginInfoType> plugins;
        return plugins;
    }

    static std::recursive_mutex &get_mutex()
    {
        static std::recursive_mutex mtx;
        return mtx;
    }

public:
    /**
     * @brief Get instance by registered name
     * Returns cached instance if available, otherwise creates and caches a new one
     *
     * @param[in] name Plugin name to get
     * @return Shared pointer to the plugin instance (shared ownership with Registry)
     *
     * @note The returned shared_ptr shares ownership with the Registry.
     *       The instance remains valid as long as either the Registry or any returned
     *       shared_ptr holds a reference, ensuring thread-safe access even if
     *       release_instance() or remove_plugin() is called.
     */
    static std::shared_ptr<T> get_instance(const std::string &name)
    {
        std::lock_guard<std::recursive_mutex> lock(get_mutex());
        auto it = get_plugins().find(name);
        if (it == get_plugins().end()) {
            return nullptr;
        }
        auto &plugin = it->second;
        // Return cached instance if available
        if (plugin.instance) {
            return plugin.instance;
        }
        // Create and cache new instance
        if (plugin.factory) {
            plugin.instance = plugin.factory();
            return plugin.instance;
        }
        return nullptr;
    }

    /**
     * @brief Get all registered plugin instances
     * Returns all cached instances (creates them if not yet cached)
     *
     * @return Map of (name, shared_ptr) for all registered plugins
     *
     * @note Each returned shared_ptr shares ownership with the Registry.
     *       The instances remain valid as long as either the Registry or any returned
     *       shared_ptr holds a reference, ensuring thread-safe access.
     */
    static std::map<std::string, std::shared_ptr<T>> get_all_instances()
    {
        std::lock_guard<std::recursive_mutex> lock(get_mutex());
        std::map<std::string, std::shared_ptr<T>> instances;
        for (auto &[name, plugin] : get_plugins()) {
            instances[name] = get_instance(name);
        }
        return instances;
    }

    /**
     * @brief Get count of registered plugins
     *
     * @return Total number of registered plugins
     */
    static size_t get_plugin_count()
    {
        std::lock_guard<std::recursive_mutex> lock(get_mutex());
        return get_plugins().size();
    }

    /**
     * @brief Check if a plugin with the given name is registered
     *
     * @param[in] name Plugin name to check
     * @return true if plugin is registered, false otherwise
     */
    static bool has_plugin(const std::string &name)
    {
        std::lock_guard<std::recursive_mutex> lock(get_mutex());
        return get_plugins().find(name) != get_plugins().end();
    }

    /**
     * @brief Release cached instance for a plugin by name
     * Does not remove the plugin registration, only clears the cached instance
     *
     * @param[in] name Plugin name
     */
    static void release_instance(const std::string &name)
    {
        std::lock_guard<std::recursive_mutex> lock(get_mutex());
        auto it = get_plugins().find(name);
        if (it != get_plugins().end() && it->second.instance) {
            it->second.instance.reset();
        }
    }

    /**
     * @brief Release all cached instances
     *
     */
    static void release_all_instances()
    {
        std::lock_guard<std::recursive_mutex> lock(get_mutex());
        for (auto &[_, plugin] : get_plugins()) {
            plugin.instance.reset();
        }
    }

    /**
     * @brief Remove plugin by name
     *
     * @param[in] name Plugin name to remove
     */
    static void remove_plugin(const std::string &name)
    {
        std::lock_guard<std::recursive_mutex> lock(get_mutex());
        get_plugins().erase(name);
    }

    /**
     * @brief Remove all registered plugins
     *
     */
    static void remove_all_plugins()
    {
        std::lock_guard<std::recursive_mutex> lock(get_mutex());
        get_plugins().clear();
    }

    /**
     * @brief Register a plugin with factory function
     *
     * @tparam PluginType Specific plugin type to register
     * @param[in] name Plugin name (must be unique)
     * @param[in] factory Factory function to create instances
     */
    template <typename PluginType>
    static void register_plugin(const std::string &name, FactoryFunc factory)
    {
        static_assert(std::is_base_of_v<T, PluginType>, "PluginType must inherit from base type T");

        std::lock_guard<std::recursive_mutex> lock(get_mutex());
        get_plugins().try_emplace(name, std::move(factory));
    }
};

} // namespace esp_brookesia::lib_utils

#if !defined(BROOKESIA_PLUGIN_CONCAT)
#   define _BROOKESIA_PLUGIN_CONCAT(a, b) a##b
#   define BROOKESIA_PLUGIN_CONCAT(a, b) _BROOKESIA_PLUGIN_CONCAT(a, b)
#endif

/**
 * @brief Internal macro to create fixed symbol name for linker
 * Creates a C function with the specified name that can be referenced via -u option
 * The function references a static variable to ensure it's not optimized away
 *
 * @note The function is defined outside any namespace to ensure proper linking.
 *       The static variable reference ensures the registrar instance is not optimized away.
 */
#define BROOKESIA_PLUGIN_CREATE_SYMBOL(symbol_name, static_var_name) \
    extern "C" void symbol_name() __attribute__((used)); \
    extern "C" void symbol_name() { \
        /* Reference the static variable to prevent it from being optimized away */ \
        /* The addressof operator ensures a real reference is created */ \
        /* Using volatile and __attribute__((used)) to prevent optimization */ \
        static volatile void* __attribute__((used)) _ref = (void*)&(static_var_name); \
        (void)_ref; \
        /* Additional reference to ensure the variable is not optimized away */ \
        volatile void* _ref2 = (void*)&(static_var_name); \
        (void)_ref2; \
    }

/**
 * @brief Registration macro with custom constructor (supports specifying constructor arguments)
 *
 * @param BaseType Base type for the plugin registry
 * @param PluginType Plugin type to register
 * @param name Plugin name
 * @param creator Custom creator function that returns a shared pointer to the plugin instance
 * @param symbol_name Symbol name for the plugin
 *
 * @note Automatically creates a fixed symbol name based on PluginType for linker -u option
 */
#define BROOKESIA_PLUGIN_REGISTER_WITH_CONSTRUCTOR(BaseType, PluginType, name, creator, symbol_name) \
    namespace { \
        template<typename T> \
        static std::shared_ptr<BaseType> BROOKESIA_PLUGIN_CONCAT(_convert_ptr_, __LINE__)(T&& ptr) { \
            using DecayedT = std::decay_t<T>; \
            if constexpr (std::is_same_v<DecayedT, std::shared_ptr<PluginType>> || \
                          std::is_same_v<DecayedT, std::shared_ptr<BaseType>>) { \
                return std::forward<T>(ptr); \
            } else if constexpr (std::is_same_v<DecayedT, std::unique_ptr<PluginType>> || \
                                 std::is_same_v<DecayedT, std::unique_ptr<BaseType>>) { \
                return std::shared_ptr<BaseType>(ptr.release()); \
            } else { \
                static_assert(std::is_convertible_v<DecayedT, std::shared_ptr<BaseType>>, \
                    "Creator function must return std::shared_ptr<PluginType>, std::shared_ptr<BaseType>, " \
                    "std::unique_ptr<PluginType>, or std::unique_ptr<BaseType>"); \
                return std::forward<T>(ptr); \
            } \
        } \
        struct BROOKESIA_PLUGIN_CONCAT(_##PluginType##_registrar_helper_, __LINE__) { \
            BROOKESIA_PLUGIN_CONCAT(_##PluginType##_registrar_helper_, __LINE__)() { \
                static_assert(std::is_base_of_v<BaseType, PluginType>, "PluginType must inherit from base type BaseType"); \
                auto creator_func = creator; \
                esp_brookesia::lib_utils::PluginRegistry<BaseType>::template register_plugin<PluginType>(name, \
                    [creator_func]() mutable -> std::shared_ptr<BaseType> { \
                        return BROOKESIA_PLUGIN_CONCAT(_convert_ptr_, __LINE__)(creator_func()); \
                    }); \
            } \
        }; \
        static BROOKESIA_PLUGIN_CONCAT(_##PluginType##_registrar_helper_, __LINE__) \
            BROOKESIA_PLUGIN_CONCAT(_##PluginType##_registrar_instance_, __LINE__); \
    } \
    BROOKESIA_PLUGIN_CREATE_SYMBOL( \
        symbol_name, \
        BROOKESIA_PLUGIN_CONCAT(_##PluginType##_registrar_instance_, __LINE__) \
    )

/**
 * @brief Registration macro (supports specifying constructor arguments)
 *
 * @param BaseType Base type for the plugin registry
 * @param PluginType Plugin type to register
 * @param name Plugin name
 * @param ... Constructor arguments (optional)
 *
 * @code
 * // Example usage:
 * BROOKESIA_PLUGIN_REGISTER(BaseService, MyPlugin, "my_plugin");
 * BROOKESIA_PLUGIN_REGISTER(BaseService, MyPlugin, "my_plugin", arg1, arg2);
 * @endcode
 */
#define BROOKESIA_PLUGIN_REGISTER(BaseType, PluginType, name, ...)                 \
    BROOKESIA_PLUGIN_REGISTER_WITH_CONSTRUCTOR(BaseType, PluginType, name, []() {  \
        return std::make_shared<PluginType>(__VA_ARGS__); \
    }, BROOKESIA_PLUGIN_CONCAT(_##PluginType##_symbol_, __LINE__))

/**
 * @brief Registration macro for singleton pattern
 *
 * This macro allows registering a singleton instance to the plugin registry.
 * It uses a custom no-op deleter to prevent the shared_ptr from destroying the singleton.
 * Automatically generates a fixed symbol name based on PluginType for linker -u option.
 *
 * @param BaseType Base type for the plugin registry
 * @param PluginType Singleton type to register (must inherit from BaseType)
 * @param name Plugin name (must be unique)
 * @param instance_expr Expression to get singleton instance reference (e.g., SingletonType::get_instance())
 *
 * @code
 * // Example usage:
 * BROOKESIA_PLUGIN_REGISTER_SINGLETON(
 *     BaseService,
 *     MySingleton,
 *     "my_singleton",
 *     MySingleton::get_instance()
 * );
 * // Creates symbol: _MySingleton_symbol_<line_number>
 * // Use: target_link_libraries(${COMPONENT_LIB} INTERFACE "-u _MySingleton_symbol_<line_number>")
 * @endcode
 */
#define BROOKESIA_PLUGIN_REGISTER_SINGLETON(BaseType, PluginType, name, instance_expr) \
    BROOKESIA_PLUGIN_REGISTER_WITH_CONSTRUCTOR(BaseType, PluginType, name, ([]() -> std::shared_ptr<BaseType> { \
        static_assert(std::is_base_of_v<BaseType, PluginType>, "PluginType must inherit from BaseType"); \
        auto& instance = (instance_expr); \
        return std::shared_ptr<BaseType>(                                                \
            static_cast<BaseType*>(std::addressof(instance)),                             \
            [](BaseType*) { /* no-op deleter for singleton */ }                          \
        );                                                                                \
    }), BROOKESIA_PLUGIN_CONCAT(_##PluginType##_symbol_, __LINE__))

/**
 * @brief Registration macro with custom symbol name (supports specifying constructor arguments)
 *
 * @param BaseType Base type for the plugin registry
 * @param PluginType Plugin type to register
 * @param name Plugin name
 * @param symbol_name Custom symbol name for the plugin
 * @param ... Constructor arguments (optional)
 *
 * @code
 * // Example usage:
 * BROOKESIA_PLUGIN_REGISTER_WITH_SYMBOL(BaseService, MyPlugin, "my_plugin", "custom_symbol_name");
 * BROOKESIA_PLUGIN_REGISTER_WITH_SYMBOL(BaseService, MyPlugin, "my_plugin", "custom_symbol_name", arg1, arg2);
 * // Use: target_link_libraries(${COMPONENT_LIB} INTERFACE "-u custom_symbol_name")
 * @endcode
 */
#define BROOKESIA_PLUGIN_REGISTER_WITH_SYMBOL(BaseType, PluginType, name, symbol_name, ...)                 \
    BROOKESIA_PLUGIN_REGISTER_WITH_CONSTRUCTOR(BaseType, PluginType, name, []() {  \
        return std::make_shared<PluginType>(__VA_ARGS__); \
    }, symbol_name)

/**
 * @brief Registration macro for singleton pattern with custom symbol name
 *
 * This macro allows registering a singleton instance to the plugin registry.
 * It uses a custom no-op deleter to prevent the shared_ptr from destroying the singleton.
 * Uses the provided custom symbol name for linker -u option.
 *
 * @param BaseType Base type for the plugin registry
 * @param PluginType Singleton type to register (must inherit from BaseType)
 * @param name Plugin name (must be unique)
 * @param instance_expr Expression to get singleton instance reference (e.g., SingletonType::get_instance())
 * @param symbol_name Custom symbol name for the plugin
 *
 * @code
 * // Example usage:
 * BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
 *     BaseService,
 *     MySingleton,
 *     "my_singleton",
 *     MySingleton::get_instance(),
 *     "custom_symbol_name"
 * );
 * // Use: target_link_libraries(${COMPONENT_LIB} INTERFACE "-u custom_symbol_name")
 * @endcode
 */
#define BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(BaseType, PluginType, name, instance_expr, symbol_name) \
    BROOKESIA_PLUGIN_REGISTER_WITH_CONSTRUCTOR(BaseType, PluginType, name, ([]() -> std::shared_ptr<BaseType> { \
        static_assert(std::is_base_of_v<BaseType, PluginType>, "PluginType must inherit from BaseType"); \
        auto& instance = (instance_expr); \
        return std::shared_ptr<BaseType>(                                                \
            static_cast<BaseType*>(std::addressof(instance)),                             \
            [](BaseType*) { /* no-op deleter for singleton */ }                          \
        );                                                                                \
    }), symbol_name)
