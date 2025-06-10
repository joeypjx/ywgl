// RuleProvisioner.h
#pragma once
#include "AlarmRuleTemplate.h"
#include "AlarmManager.h"
#include "MetricCache.h"
#include "AgentResource.h" // 需要用它来创建Resource实例
#include <vector>
#include <thread>
#include <atomic>
#include <iostream>

// 规则供应器，负责动态管理告警规则实例
class RuleProvisioner {
private:
    std::vector<AlarmRuleTemplate> templates_;
    std::shared_ptr<AlarmManager> alarmManager_;
    std::shared_ptr<MetricCache> metricCache_;
    std::atomic<bool> stopRequested_{false};
    std::thread workerThread_;

    void synchronizeRules() {
        std::set<std::string> activeNodeIds = metricCache_->getActiveNodeIds();
        std::set<std::string> existingRuleIds = alarmManager_->getManagedRuleIds();
        std::set<std::string> requiredRuleIds;

        for (const auto& tpl : templates_) {
            for (const auto& nodeId : activeNodeIds) {
                std::string ruleId = tpl.templateId + ":" + nodeId;
                requiredRuleIds.insert(ruleId);

                if (existingRuleIds.find(ruleId) == existingRuleIds.end()) {
                    std::cout << "[Provisioner] New rule needed for '" << ruleId << "'. Creating..." << std::endl;
                    AlarmRule newRule;
                    newRule.ruleId = ruleId;
                    newRule.resource = std::make_shared<AgentResource>(nodeId, tpl.metricName, metricCache_);
                    newRule.condition = tpl.condition;
                    newRule.actions = tpl.actions;
                    alarmManager_->addRule(newRule);
                }
            }
        }

        for (const auto& existingId : existingRuleIds) {
            if (requiredRuleIds.find(existingId) == requiredRuleIds.end()) {
                if (existingId.find(':') != std::string::npos) { // 只删除由模板生成的规则
                    std::cout << "[Provisioner] Stale rule found: '" << existingId << "'. Removing..." << std::endl;
                    alarmManager_->removeRule(existingId);
                }
            }
        }
    }

    void run() {
        while (!stopRequested_) {
            synchronizeRules();
            std::this_thread::sleep_for(std::chrono::seconds(20)); // 同步不需要太频繁
        }
    }

public:
    RuleProvisioner(std::shared_ptr<AlarmManager> am, std::shared_ptr<MetricCache> mc)
        : alarmManager_(std::move(am)), metricCache_(std::move(mc)) {}
    
    void addTemplate(const AlarmRuleTemplate& tpl) {
        templates_.push_back(tpl);
    }
    
    void start() {
        if (workerThread_.joinable()) return;
        stopRequested_ = false;
        workerThread_ = std::thread(&RuleProvisioner::run, this);
        std::cout << "[RuleProvisioner] Started." << std::endl;
    }

    void stop() {
        stopRequested_ = true;
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
        std::cout << "[RuleProvisioner] Stopped." << std::endl;
    }
};