// LogAction.h
#pragma once
#include "IAlarmAction.h"
#include "AlarmRule.h"
#include <iostream>
#include <ctime>
#include <cstring> // For strlen
#include <string>
#include <vector>

namespace {
// 辅助函数，用于替换字符串中的所有占位符
std::string& replace_all(std::string& str, const std::string& old_value, const std::string& new_value) {
    size_t pos = 0;
    while ((pos = str.find(old_value, pos)) != std::string::npos) {
        str.replace(pos, old_value.length(), new_value);
        pos += new_value.length();
    }
    return str;
}
}

class LogAction : public IAlarmAction {
public:
    void execute(const AlarmRule& rule) override {
        // 获取当前时间
        time_t now = time(nullptr);
        char* time_str_c = ctime(&now);
        std::string time_str(time_str_c);
        time_str.pop_back(); // 移除 ctime 产生的换行符

        // 准备替换的占位符和值
        const char* state = rule.isTriggeredState ? "TRIGGERED" : "RECOVERED";
        std::string message = rule.contentTemplate;

        // 替换所有占位符
        replace_all(message, "{ruleId}", rule.ruleId);
        replace_all(message, "{metricName}", rule.metricName);
        replace_all(message, "{alarmType}", rule.alarmType);
        replace_all(message, "{alarmLevel}", rule.alarmLevel);
        replace_all(message, "{resourceName}", rule.resource->getName());
        replace_all(message, "{value}", std::to_string(rule.lastTriggeredValue));
        replace_all(message, "{condition}", rule.condition->getDescription());
        replace_all(message, "{state}", state);
        replace_all(message, "{nodeId}", rule.nodeId);
        
        std::cout << "[" << time_str << "] " << message << std::endl;
    }

    std::string getDescription() const override {
        return "Log";
    }
};