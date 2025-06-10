// AlarmRuleTemplate.h
#pragma once
#include "IAlarmCondition.h"
#include "IAlarmAction.h"
#include <string>
#include <vector>
#include <memory>

// 代表一个告警模板，用于动态生成具体规则
struct AlarmRuleTemplate {
    std::string templateId;       // 模板的唯一ID, e.g., "tpl-high-cpu"
    std::string metricName;       // 要监控的指标名称, e.g., "cpu_usage_percent"
    std::shared_ptr<IAlarmCondition> condition;
    std::vector<std::shared_ptr<IAlarmAction>> actions;
    std::vector<std::shared_ptr<IAlarmAction>> recoveryActions;
};