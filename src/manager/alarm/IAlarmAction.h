// IAlarmAction.h
#pragma once
#include <string>

// 告警动作接口，定义了告警触发后要执行的操作
class IAlarmAction {
public:
    virtual ~IAlarmAction() = default;
    
    // 执行动作
    virtual void execute(const std::string& ruleId, const std::string& resourceName) = 0;
};