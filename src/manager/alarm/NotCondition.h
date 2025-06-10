// NotCondition.h
#pragma once
#include "IAlarmCondition.h"
#include <memory>
#include <utility>

class NotCondition : public IAlarmCondition {
private:
    std::shared_ptr<IAlarmCondition> condition_;

public:
    explicit NotCondition(std::shared_ptr<IAlarmCondition> cond) 
        : condition_(std::move(cond)) {}

    bool isTriggered(double value) const override {
        return !condition_->isTriggered(value);
    }

    std::string getDescription() const override {
        return "NOT (" + condition_->getDescription() + ")";
    }
};