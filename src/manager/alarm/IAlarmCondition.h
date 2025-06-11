// IAlarmCondition.h
#pragma once
#include <string>
#include <vector>
#include <memory>

// 告警条件接口，定义了触发告警的逻辑
class IAlarmCondition {
public:
    virtual ~IAlarmCondition() = default;
    
    // 检查给定值是否触发条件
    virtual bool isTriggered(double value) const = 0;
    
    // 获取条件的文字描述
    virtual std::string getDescription() const = 0;

    // 获取条件的阈值
    virtual double getThreshold() const = 0;

    // 获取条件的子条件
    virtual std::vector<std::shared_ptr<IAlarmCondition>> getConditions() const = 0;
};