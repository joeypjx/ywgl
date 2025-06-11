// AlarmManager.h
#pragma once

#include "AlarmRule.h"
#include "AlarmEventRepository.h"
#include <map>
#include <string>
#include <memory>
#include <iostream>
#include <mutex>
#include <set>
#include <algorithm> // For std::remove_if
#include <thread>
#include <chrono>

class AlarmManager {
public:
    // 构造函数，注入事件仓库
    explicit AlarmManager()
    {}

    // 添加或更新规则
    void addRule(const AlarmRule& rule) {
        std::lock_guard<std::mutex> lock(rulesMutex_);
        rules_[rule.ruleId] = rule;
    }

    // 移除规则
    void removeRule(const std::string& ruleId) {
        std::lock_guard<std::mutex> lock(rulesMutex_);
        rules_.erase(ruleId);
    }
    
    // 获取当前管理的所有规则ID
    std::set<std::string> getManagedRuleIds() const {
        std::lock_guard<std::mutex> lock(rulesMutex_);
        std::set<std::string> ids;
        for(const auto& pair : rules_) {
            ids.insert(pair.first);
        }
        return ids;
    }

    // 定期检查所有告警
    void checkAlarms() {
        std::lock_guard<std::mutex> lock(rulesMutex_);
        for (auto& pair : rules_) {
            auto& rule = pair.second;
            double currentValue = rule.resource->getValue();
            bool conditionMet = rule.condition->isTriggered(currentValue);

            rule.lastTriggeredValue = currentValue; // 记录当前值

            if (conditionMet) {
                // 条件满足，增加连续触发计数
                rule.consecutiveTriggerCount++;

                // 仅当连续触发次数达到阈值，并且当前未处于告警状态时，才触发告警
                if (rule.consecutiveTriggerCount >= rule.triggerCountThreshold && !rule.isTriggeredState) {
                    rule.isTriggeredState = true;
                    for (const auto& action : rule.actions) {
                        action->execute(rule);
                    }
                }
            } else {
                // 条件不满足
                // 如果之前处于告警状态，发送恢复通知
                if (rule.isTriggeredState) {
                    rule.isTriggeredState = false;
                    for (const auto& action : rule.actions) {
                        action->execute(rule);
                    }
                }
                // 重置连续触发计数
                rule.consecutiveTriggerCount = 0;
            }
        }
    }

    void start() {
        workerThread_ = std::thread([this]() {
            while (true) {
                checkAlarms();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
        workerThread_.detach();
    }

    void stop() {
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }

private:
    std::map<std::string, AlarmRule> rules_;
    mutable std::mutex rulesMutex_;
    std::thread workerThread_;
};