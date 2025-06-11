// AlarmRule.h
#pragma once
#include "IResource.h"
#include "IAlarmCondition.h"
#include "IAlarmAction.h"
#include <string>
#include <vector>
#include <memory>

// 代表一个具体的、已实例化的告警规则
struct AlarmRule {
    std::string ruleId; // 规则的唯一实例ID, e.g., "tpl-high-cpu:node-01"
    std::string templateId; // 规则的模板ID, e.g., "tpl-high-cpu"
    std::string nodeId; // 规则的节点ID, e.g., "node-01"
    std::string metricName; // 规则的指标名称, e.g., "cpu_usage_percent"
    std::string alarmType; // 规则的告警类型, e.g., "system"
    std::string alarmLevel; // 规则的告警级别, e.g., "critical"
    std::string contentTemplate; // 规则的告警内容模板, e.g., "节点 {resourceName} 发生 {alarmLevel} 告警"
    int triggerCountThreshold; // 规则的触发阈值, e.g., 3
    std::shared_ptr<IResource> resource;
    std::shared_ptr<IAlarmCondition> condition;
    std::vector<std::shared_ptr<IAlarmAction>> actions;
    
    // 用于状态管理，防止重复告警
    bool isTriggeredState = false; // 当前是否处于触发状态
    double lastTriggeredValue = 0.0; // 最近一次触发或恢复时的值
    int consecutiveTriggerCount = 0; // 当前连续触发次数
};