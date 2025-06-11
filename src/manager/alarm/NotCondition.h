// NotCondition.h
#pragma once
#include "IAlarmCondition.h"
#include "ILogicalCondition.h"
#include <memory>
#include <utility>

class NotCondition : public ILogicalCondition {
public:
    explicit NotCondition(std::shared_ptr<IAlarmCondition> condition) : condition_(condition) {}

    bool isTriggered(double value) const override {
        if (condition_) {
            return !condition_->isTriggered(value);
        }
        return false;
    }

    std::string getDescription() const override {
        if (condition_) {
            return "not (" + condition_->getDescription() + ")";
        }
        return "not (null)";
    }

    std::string getType() const override { return "Not"; }

    double getThreshold() const override {
        // 对于逻辑组合条件，阈值没有直接意义，可以返回一个特殊值
        return 0.0;
    }
    std::vector<std::shared_ptr<IAlarmCondition>> getConditions() const override {
        if (condition_) {
            return {condition_};
        }
        return {};
    }

private:
    std::shared_ptr<IAlarmCondition> condition_;
};