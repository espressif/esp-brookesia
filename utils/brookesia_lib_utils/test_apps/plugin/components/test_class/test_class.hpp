/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string>

// ==================== Test base class and plugin class definitions ====================

constexpr int PLUGIN_A_DEFAULT_VALUE = 1;
constexpr int PLUGIN_B_DEFAULT_VALUE = 2;

constexpr int MACRO_SINGLETON_A_DEFAULT_VALUE = 4;
constexpr int MACRO_SINGLETON_B_DEFAULT_VALUE = 5;

// Base plugin interface
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual std::string get_name() const = 0;
    virtual int get_value() const = 0;
};

// Concrete plugin implementation
class PluginA : public IPlugin {
public:
    PluginA() : value_(PLUGIN_A_DEFAULT_VALUE)
    {
    }

    explicit PluginA(int val) : value_(val)
    {
    }

    ~PluginA() override
    {
    }

    std::string get_name() const override
    {
        return "PluginA";
    }
    int get_value() const override
    {
        return value_;
    }

private:
    int value_;
};

class PluginB : public IPlugin {
public:
    PluginB() : value_(PLUGIN_B_DEFAULT_VALUE)
    {
    }

    explicit PluginB(int val) : value_(val)
    {
    }

    ~PluginB() override
    {
    }

    std::string get_name() const override
    {
        return "PluginB";
    }
    int get_value() const override
    {
        return value_;
    }

private:
    int value_;
};

class PluginC : public IPlugin {
public:
    PluginC(const std::string &name, int val) : name_(name), value_(val)
    {
    }

    ~PluginC() override
    {
    }

    std::string get_name() const override
    {
        return name_;
    }
    int get_value() const override
    {
        return value_;
    }

private:
    std::string name_;
    int value_;
};

// Singleton plugin implementation
class PluginSingletonA : public IPlugin {
public:
    static PluginSingletonA &get_instance()
    {
        static PluginSingletonA instance;
        return instance;
    }

    std::string get_name() const override
    {
        return "PluginSingletonA";
    }

    int get_value() const override
    {
        return value_;
    }

private:
    PluginSingletonA() : value_(MACRO_SINGLETON_A_DEFAULT_VALUE)
    {
        // Private constructor for singleton pattern
    }

    ~PluginSingletonA() override = default;
    PluginSingletonA(const PluginSingletonA &) = delete;
    PluginSingletonA &operator=(const PluginSingletonA &) = delete;

    int value_;
};

// Singleton plugin implementation
class PluginSingletonB : public IPlugin {
public:
    static PluginSingletonB &get_instance()
    {
        static PluginSingletonB instance;
        return instance;
    }

    std::string get_name() const override
    {
        return "PluginSingletonB";
    }

    int get_value() const override
    {
        return value_;
    }

private:
    PluginSingletonB() : value_(MACRO_SINGLETON_B_DEFAULT_VALUE)
    {
        // Private constructor for singleton pattern
    }

    ~PluginSingletonB() override = default;
    PluginSingletonB(const PluginSingletonB &) = delete;
    PluginSingletonB &operator=(const PluginSingletonB &) = delete;

    int value_;
};
