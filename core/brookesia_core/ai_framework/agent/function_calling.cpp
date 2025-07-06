/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "boost/thread.hpp"
#include "private/esp_brookesia_ai_agent_utils.hpp"
#include "function_calling.hpp"

#define THREAD_STACK_SIZE_BIG   (10 * 1024)

namespace esp_brookesia::ai_framework {

// FunctionParameter implementation
FunctionParameter::FunctionParameter(const std::string &name, const std::string &description, ValueType type, bool required)
    : name_(name), description_(description), type_(type), required_(required)
{
    ESP_UTILS_LOGD("Param: name(%s), description(%s), type(%d), required(%d)", name.c_str(), description.c_str(),
                   static_cast<int>(type), static_cast<int>(required)
                  );
}

void FunctionParameter::setBoolean(bool value)
{
    ESP_UTILS_LOGD("Set boolean parameter %s: %d", name_.c_str(), value);
    boolean_ = value;
}

void FunctionParameter::setNumber(int value)
{
    number_ = value;
    ESP_UTILS_LOGD("Set number parameter %s: %d", name_.c_str(), value);
}

void FunctionParameter::setString(const std::string &value)
{
    string_ = value;
    ESP_UTILS_LOGD("Set string parameter %s: %s", name_.c_str(), value.c_str());
}

std::string FunctionParameter::getDescriptorJson()
{
    std::string json_str = "{";
    json_str += "\"type\":\"";
    if (type_ == ValueType::Boolean) {
        json_str += "boolean";
    } else if (type_ == ValueType::Number) {
        json_str += "integer";
    } else if (type_ == ValueType::String) {
        json_str += "string";
    }
    json_str += "\",";
    json_str += "\"description\":\"" + description_ + "\"";
    if (type_ == ValueType::Number) {
        json_str += ",\"minimum\":0,\"maximum\":100";
    }
    json_str += "}";
    ESP_UTILS_LOGD("FunctionParameter %s JSON descriptor: %s", name_.c_str(), json_str.c_str());
    return json_str;
}

// FunctionDefinition implementation
FunctionDefinition::FunctionDefinition(const std::string &name, const std::string &description)
    : name_(name), description_(description)
{
    ESP_UTILS_LOGD("Created function definition: %s", name.c_str());
}

void FunctionDefinition::addParameter(
    const std::string &name, const std::string &description, FunctionParameter::ValueType type, bool required
)
{
    ESP_UTILS_LOGD("Param: name(%s), description(%s), type(%d), required(%d)", name.c_str(), description.c_str(),
                   static_cast<int>(type), static_cast<int>(required)
                  );
    parameters_.push_back(FunctionParameter(name, description, type, required));
}

void FunctionDefinition::setCallback(Callback callback, std::optional<CallbackThreadConfig> thread_config)
{
#if ESP_UTILS_CONF_LOG_LEVEL == ESP_UTILS_LOG_LEVEL_DEBUG
    if (thread_config != std::nullopt) {
        thread_config->dump();
    }
#endif

    callback_ = callback;
    thread_config_ = thread_config;
}

bool FunctionDefinition::invoke(const cJSON *args) const
{
    ESP_UTILS_LOGD("Invoking function: %s", name_.c_str());
    if (!callback_) {
        ESP_UTILS_LOGW("Function %s has no callback", name_.c_str());
        return false;
    }

    // char *param_str = cJSON_Print(args);
    // if (param_str) {
    //     ESP_UTILS_LOGI("Parameters JSON: %s", param_str);
    //     free(param_str);
    // } else {
    //     ESP_UTILS_LOGE("Failed to print parameters JSON");
    // }

    std::vector<FunctionParameter> params = parameters_;
    for (auto &param : params) {
        cJSON *value = cJSON_GetObjectItem(args, param.name().c_str());
        auto name = param.name().c_str();
        if (!value) {
            ESP_UTILS_CHECK_FALSE_RETURN(!param.required(), false, "Required parameter %s not found", name);
            continue;
        }

        switch (param.type()) {
        case FunctionParameter::ValueType::Boolean:
            ESP_UTILS_CHECK_FALSE_RETURN(
                cJSON_IsBool(value), false, "FunctionParameter %s type mismatch: expected boolean", name
            );
            param.setBoolean(cJSON_IsTrue(value));
            break;

        case FunctionParameter::ValueType::Number:
            ESP_UTILS_CHECK_FALSE_RETURN(
                cJSON_IsNumber(value), false, "FunctionParameter %s type mismatch: expected number", name
            );
            param.setNumber(value->valueint);
            break;

        case FunctionParameter::ValueType::String:
            ESP_UTILS_CHECK_FALSE_RETURN(
                cJSON_IsString(value), false, "FunctionParameter %s type mismatch: expected string", name
            );
            param.setString(value->valuestring);
            break;
        }
    }

    if (thread_config_ != std::nullopt) {
        esp_utils::thread_config_guard thread_config(*thread_config_);
        boost::thread([this, params]() {
            ESP_UTILS_LOG_TRACE_GUARD();
            callback_(params);
        }).detach();
    } else {
        callback_(params);
    }

    return true;
}

std::string FunctionDefinition::getJson() const
{
    std::string json = "{";
    json += "\"name\":\"" + name_ + "\",";
    json += "\"description\":\"" + description_ + "\",";
    json += "\"parameters\":{";
    json += "\"type\":\"object\",";
    json += "\"properties\":{";

    // Add properties
    for (size_t i = 0; i < parameters_.size(); ++i) {
        const auto &param = parameters_[i];
        json += "\"" + param.name() + "\":{";
        json += "\"type\":\"";
        if (param.type() == FunctionParameter::ValueType::Boolean) {
            json += "boolean";
        } else if (param.type() == FunctionParameter::ValueType::Number) {
            json += "integer";
        } else if (param.type() == FunctionParameter::ValueType::String) {
            json += "string";
        }
        json += "\",";
        json += "\"description\":\"" + param.description() + "\"";

        // Add min/max for number type
        if (param.type() == FunctionParameter::ValueType::Number) {
            json += ",\"minimum\":0,\"maximum\":100";
        }
        json += "}";
        if (i < parameters_.size() - 1) {
            json += ",";
        }
    }
    json += "},";

    // Add required fields
    json += "\"required\":[";
    bool first = true;
    for (const auto &param : parameters_) {
        if (param.required()) {
            if (!first) {
                json += ",";
            }
            json += "\"" + param.name() + "\"";
            first = false;
        }
    }
    json += "]";
    json += "}";
    json += "}";

    ESP_UTILS_LOGD("Function %s JSON descriptor: %s", name_.c_str(), json.c_str());
    return json;
}

// FunctionDefinitionList implementation
FunctionDefinitionList &FunctionDefinitionList::requestInstance()
{
    static FunctionDefinitionList instance;
    return instance;
}

void FunctionDefinitionList::addFunction(const FunctionDefinition &func)
{
    function_index_[func.name()] = functions_.size();
    functions_.push_back(func);
    ESP_UTILS_LOGD("Added function to list: %s, index: %zu", func.name().c_str(), functions_.size() - 1);
}

bool FunctionDefinitionList::invokeFunction(const cJSON *function_call) const
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_NULL_RETURN(function_call, false, "Function call parameter is NULL");

    cJSON *function = cJSON_GetObjectItem(function_call, "function");
    ESP_UTILS_CHECK_NULL_RETURN(function, false, "Function field not found");

    cJSON *name = cJSON_GetObjectItem(function, "name");
    cJSON *arguments = cJSON_GetObjectItem(function, "arguments");
    ESP_UTILS_CHECK_FALSE_RETURN((name != nullptr) && cJSON_IsString(name), false, "Name field invalid");
    ESP_UTILS_CHECK_NULL_RETURN(arguments, false, "Arguments field not found");

    ESP_UTILS_LOGD("Processing function call: %s", name->valuestring);

    if (cJSON_IsString(arguments)) {
        ESP_UTILS_LOGD("Arguments is string: %s", arguments->valuestring);

        // Parse arguments JSON string
        cJSON *args_obj = cJSON_Parse(arguments->valuestring);
        if (args_obj == nullptr) {
            goto next;
        }

        {
            // Check if contains action_json_str field
            cJSON *action_json_str = cJSON_GetObjectItem(args_obj, "action_json_str");
            if (action_json_str && cJSON_IsString(action_json_str)) {
                ESP_UTILS_LOGD("Found action_json_str: %s", action_json_str->valuestring);

                // Parse JSON from action_json_str
                cJSON *action_obj = cJSON_Parse(action_json_str->valuestring);
                if (!action_obj) {
                    ESP_UTILS_LOGE("Failed to parse action_json_str: %s", action_json_str->valuestring);
                    cJSON_Delete(args_obj);
                    return false;
                }

                // Get actual function name and arguments
                cJSON *actual_name = cJSON_GetObjectItem(action_obj, "name");
                cJSON *actual_args = cJSON_GetObjectItem(action_obj, "arguments");

                if (!actual_name || !cJSON_IsString(actual_name) || !actual_args) {
                    ESP_UTILS_LOGE("Action JSON missing required fields or wrong types");
                    cJSON_Delete(action_obj);
                    cJSON_Delete(args_obj);
                    return false;
                }

                // Print debug information
                char *debug_str = cJSON_Print(action_obj);
                if (debug_str) {
                    ESP_UTILS_LOGD("Parsed action JSON: %s", debug_str);
                    free(debug_str);
                }

                // Find and call the corresponding function
                auto it = function_index_.find(actual_name->valuestring);
                if (it == function_index_.end()) {
                    ESP_UTILS_LOGE("Function not found: %s", actual_name->valuestring);
                    cJSON_Delete(action_obj);
                    cJSON_Delete(args_obj);
                    return false;
                }

                ESP_UTILS_LOGD("Found function %s, index: %zu", actual_name->valuestring, it->second);

                // Call the callback function
                bool result = functions_[it->second].invoke(actual_args);

                cJSON_Delete(action_obj);
                cJSON_Delete(args_obj);
                return result;
            }
        }

next:
        // Standard JSON parameter handling
        auto it = function_index_.find(name->valuestring);
        if (it == function_index_.end()) {
            ESP_UTILS_LOGE("Function not found: %s", name->valuestring);
            if (args_obj != nullptr) {
                cJSON_Delete(args_obj);
            }
            return false;
        }

        ESP_UTILS_LOGD("Found function %s, index: %zu", name->valuestring, it->second);

        // Call the callback function
        bool result = functions_[it->second].invoke(args_obj);

        if (args_obj != nullptr) {
            cJSON_Delete(args_obj);
        }
        return result;
    } else if (cJSON_IsObject(arguments)) {
        // Process object type parameters directly
        auto it = function_index_.find(name->valuestring);
        if (it == function_index_.end()) {
            ESP_UTILS_LOGE("Function not found: %s", name->valuestring);
            return false;
        }

        ESP_UTILS_LOGD("Found function %s, index: %zu", name->valuestring, it->second);

        // Call the callback function
        bool result = functions_[it->second].invoke(arguments);
        return result;
    }

    ESP_UTILS_LOGE("Arguments is neither string nor object");
    return false;
}

std::string FunctionDefinitionList::getJson() const
{
    std::string json = "{\"functions\":[";
    for (size_t i = 0; i < functions_.size(); ++i) {
        json += functions_[i].getJson();
        if (i < functions_.size() - 1) {
            json += ",";
        }
    }
    json += "]}";
    return json;
}

} // namespace esp_brookesia::ai_framework
