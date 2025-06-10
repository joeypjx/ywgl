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
    std::shared_ptr<IResource> resource;
    std::shared_ptr<IAlarmCondition> condition;
    std::vector<std::shared_ptr<IAlarmAction>> actions;
    std::vector<std::shared_ptr<IAlarmAction>> recoveryActions; // 新增：恢复时执行的动作
    
    // 用于状态管理，防止重复告警
    bool isCurrentlyTriggered = false;
};