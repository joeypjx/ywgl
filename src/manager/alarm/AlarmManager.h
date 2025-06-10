// AlarmManager.h
#pragma once
#include "AlarmRule.h"
#include <map>
#include <string>
#include <set>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iostream>

// 告警管理器，负责检查已实例化的规则
class AlarmManager {
private:
    std::map<std::string, AlarmRule> rules_;
    mutable std::mutex rulesMutex_;
    std::atomic<bool> stopRequested_{false};
    std::thread workerThread_;

    void checkAlarms() {
        // 复制一份规则列表进行检查，避免长时间锁定
        std::map<std::string, AlarmRule> rulesToCheck;
        {
            std::lock_guard<std::mutex> lock(rulesMutex_);
            rulesToCheck = rules_;
        }

        for (auto& pair : rulesToCheck) {
            AlarmRule& rule = pair.second;
            double currentValue = rule.resource->getValue();
            bool triggered = rule.condition->isTriggered(currentValue);

            if (triggered && !rule.isCurrentlyTriggered) {
                // 状态从 Normal -> Triggered
                for (const auto& action : rule.actions) {
                    action->execute(rule.ruleId, rule.resource->getName());
                }
                updateRuleState(rule.ruleId, true); // 更新主列表中的状态
            } else if (!triggered && rule.isCurrentlyTriggered) {
                // 状态从 Triggered -> Normal
                std::cout << "[INFO] Alarm '" << rule.ruleId << "' has recovered." << std::endl;
                    for (const auto& action : rule.recoveryActions) { // 执行恢复动作
                    action->execute(rule.ruleId, rule.resource->getName());
                }
                updateRuleState(rule.ruleId, false);
            }
        }
    }
    
    void updateRuleState(const std::string& ruleId, bool isTriggered) {
        std::lock_guard<std::mutex> lock(rulesMutex_);
        auto it = rules_.find(ruleId);
        if (it != rules_.end()) {
            it->second.isCurrentlyTriggered = isTriggered;
        }
    }

    void run() {
        while (!stopRequested_) {
            checkAlarms();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

public:
    void addRule(const AlarmRule& rule) {
        std::lock_guard<std::mutex> lock(rulesMutex_);
        rules_[rule.ruleId] = rule;
    }

    void removeRule(const std::string& ruleId) {
        std::lock_guard<std::mutex> lock(rulesMutex_);
        rules_.erase(ruleId);
    }

    std::set<std::string> getManagedRuleIds() const {
        std::lock_guard<std::mutex> lock(rulesMutex_);
        std::set<std::string> ids;
        for(const auto& pair : rules_) {
            ids.insert(pair.first);
        }
        return ids;
    }

    void start() {
        if (workerThread_.joinable()) return;
        stopRequested_ = false;
        workerThread_ = std::thread(&AlarmManager::run, this);
        std::cout << "[AlarmManager] Started." << std::endl;
    }

    void stop() {
        stopRequested_ = true;
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
        std::cout << "[AlarmManager] Stopped." << std::endl;
    }
};