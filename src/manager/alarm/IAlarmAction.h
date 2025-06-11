// IAlarmAction.h
#pragma once
#include <string>
#include <memory>

class AlarmRule; // 前向声明以避免循环依赖

// 告警动作接口，定义了告警触发后要执行的操作
class IAlarmAction
{
public:
    virtual ~IAlarmAction() = default;

    // 执行动作
    virtual void execute(const AlarmRule& rule) = 0;

    // 获取动作的描述
    virtual std::string getDescription() const = 0;
};