#pragma once

#include "IAlarmCondition.h"

// 这是一个标记接口，用于将And, Or, Not等逻辑组合条件归类
class ILogicalCondition : public virtual IAlarmCondition {
public:
    virtual ~ILogicalCondition() = default;
}; 