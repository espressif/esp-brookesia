/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <string>
#include <optional>
#include <functional>
#include <vector>
#include <mutex>
#include "cJSON.h"
#include "thread/esp_utils_thread.hpp"

namespace esp_brookesia::ai_framework {

class FunctionParameter {
public:
    enum class ValueType {
        Boolean,
        Number,
        String
    };

    FunctionParameter(const std::string &name, const std::string &description, ValueType type, bool required = true);

    const std::string &name() const
    {
        return name_;
    }
    const std::string &description() const
    {
        return description_;
    }
    ValueType type() const
    {
        return type_;
    }
    bool required() const
    {
        return required_;
    }

    bool boolean() const
    {
        return boolean_;
    }
    int number() const
    {
        return number_;
    }
    const std::string &string() const
    {
        return string_;
    }

    void setBoolean(bool value);
    void setNumber(int value);
    void setString(const std::string &value);

    std::string getDescriptorJson();

private:
    std::string name_;
    std::string description_;
    ValueType type_;
    bool required_;
    bool boolean_;
    int number_;
    std::string string_;
};

class FunctionDefinition {
public:
    using CallbackThreadConfig = esp_utils::ThreadConfig;
    using Callback = std::function<void(const std::vector<FunctionParameter>&)>;

    FunctionDefinition(const std::string &name, const std::string &description);

    void addParameter(
        const std::string &name, const std::string &description, FunctionParameter::ValueType type, bool required = true
    );
    void setCallback(Callback callback, std::optional<CallbackThreadConfig> thread_config = std::nullopt);
    bool invoke(const cJSON *args) const;
    std::string name() const
    {
        return name_;
    }
    std::string getJson() const;

private:
    std::string name_;
    std::string description_;
    std::vector<FunctionParameter> parameters_;
    Callback callback_;
    std::optional<CallbackThreadConfig> thread_config_;
};

class FunctionDefinitionList {
public:
    FunctionDefinitionList(const FunctionDefinitionList &) = delete;
    FunctionDefinitionList &operator=(const FunctionDefinitionList &) = delete;

    static FunctionDefinitionList &requestInstance();
    void addFunction(const FunctionDefinition &func);
    bool invokeFunction(const cJSON *function_call) const;
    std::string getJson() const;

private:
    FunctionDefinitionList() = default;

    std::vector<FunctionDefinition> functions_;
    std::map<std::string, size_t> function_index_;
};

} // namespace esp_brookesia::ai_framework
