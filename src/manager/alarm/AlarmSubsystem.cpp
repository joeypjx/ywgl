#include "AlarmSubsystem.h"
#include <iostream>
#include <random>
#include <nlohmann/json.hpp>
#include <vector>

// 包含所有具体的实现
#include "MetricCache.h"
#include "AlarmManager.h"
#include "RuleProvisioner.h"
#include "TemplateRepository.h"
#include "AlarmEventRepository.h"
#include "../database_manager.h"
#include "GreaterThanCondition.h"
#include "LessThanCondition.h"
#include "AndCondition.h"
#include "OrCondition.h"
#include "NotCondition.h"
#include "LogAction.h"
#include "DatabaseAction.h"

using json = nlohmann::json;

AlarmSubsystem::AlarmSubsystem(std::shared_ptr<DatabaseManager> dbManager)
    : dbManager_(dbManager) {}

AlarmSubsystem::~AlarmSubsystem() {
    // 确保在析构时停止线程
    if (!stop_signal_) {
        stop();
    }
}

void AlarmSubsystem::initialize() {
    eventRepo_ = std::make_shared<AlarmEventRepository>(dbManager_);
    cache_ = std::make_shared<MetricCache>();
    manager_ = std::make_shared<AlarmManager>(); // manager现在不需要repo
    templateRepo_ = std::make_shared<TemplateRepository>(dbManager_, eventRepo_);
    provisioner_ = std::make_shared<RuleProvisioner>(manager_, cache_);

    templateRepo_->createTables();

    // 创建示例告警模板
    json high_cpu_tpl = {
        {"templateId", "CPU使用率-严重"},
        {"metricName", "cpu.usage_percent"},
        {"alarmType", "system"},
        {"alarmLevel", "critical"},
        {"triggerCountThreshold", 3},
        {"contentTemplate", "{state} 节点 {nodeId} 发生 {alarmLevel} 告警，指标 {metricName} 值为 {value}"},
        {"condition", {{"type", "GreaterThan"}, {"threshold", 5.0}}},
        {"actions", {{{"type", "Log"}}, {{"type", "Database"}}}}
    };
    json high_disk_tpl = {
        {"templateId", "磁盘使用率-警告"},
        {"metricName", "disk[mount_point=/].usage_percent"},
        {"alarmType", "system"},
        {"alarmLevel", "warning"},
        {"triggerCountThreshold", 2},
        {"contentTemplate", "{state} 节点 {nodeId} 发生 {alarmLevel} 告警，指标 {metricName} 值为 {value}"},
        {"condition", {{"type", "And"}, {"conditions", {
            {{"type", "GreaterThan"}, {"threshold", 80.0}},
            {{"type", "LessThan"}, {"threshold", 95.0}}
        }}}},
        {"actions", {{{"type", "Log"}}, {{"type", "Database"}}}}
    };

    std::cout << "high_cpu_tpl: " << high_cpu_tpl.dump(4) << std::endl;
    std::cout << "high_disk_tpl: " << high_disk_tpl.dump(4) << std::endl;
    
    try {
        templateRepo_->saveTemplate(high_cpu_tpl);
        templateRepo_->saveTemplate(high_disk_tpl);
    } catch (const std::exception& e) {
        std::cerr << "Failed to save demo templates: " << e.what() << std::endl;
    }

    // 从数据库加载所有模板到供应器
    for (const auto& tpl : templateRepo_->loadAllTemplates()) {
        provisioner_->addTemplate(tpl);
    }
}

void AlarmSubsystem::start() {
    manager_->start();
    provisioner_->start();
    std::cout << "[AlarmSubsystem] Started." << std::endl;
}

void AlarmSubsystem::stop() {
    stop_signal_ = true;
    provisioner_->stop();
    manager_->stop();
    std::cout << "[AlarmSubsystem] Stopped." << std::endl;
}

void AlarmSubsystem::updateNodeMetrics(const std::string& nodeId, const json& metrics) {
    if (cache_) {
        cache_->updateNodeMetrics(nodeId, metrics);
    }
}

void AlarmSubsystem::addAlarmTemplate(const nlohmann::json& ruleJson) {
    if (templateRepo_) {
        // 调用底层的Repository来保存从JSON构造的模板
        templateRepo_->saveTemplate(ruleJson);
    } else {
        // 如果出于某种原因，templateRepo_ 未被初始化，则抛出异常
        throw std::runtime_error("TemplateRepository is not initialized.");
    }
}

nlohmann::json AlarmSubsystem::getAllAlarmTemplatesAsJson() {
    if (templateRepo_) {
        return templateRepo_->loadAllTemplatesAsJson();
    }
    throw std::runtime_error("TemplateRepository is not initialized.");
}

nlohmann::json AlarmSubsystem::getAllAlarmEventsAsJson(int limit) {
    if (eventRepo_) {
        return eventRepo_->getAllEventsAsJson(limit);
    }
    throw std::runtime_error("AlarmEventRepository is not initialized.");
} 