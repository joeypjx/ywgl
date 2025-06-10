// OrCondition.h
#pragma once
#include "IAlarmCondition.h"
#include <vector>
#include <memory>
#include <sstream>

class OrCondition : public IAlarmCondition {
private:
    std::vector<std::shared_ptr<IAlarmCondition>> conditions_;

public:
    explicit OrCondition(std::vector<std::shared_ptr<IAlarmCondition>> conds)
        : conditions_(std::move(conds)) {}

    bool isTriggered(double value) const override {
        for (const auto& cond : conditions_) {
            if (cond->isTriggered(value)) {
                return true; // OR逻辑：一旦有true，整体即为true
            }
        }
        return false; // 所有都为false
    }

    std::string getDescription() const override {
        std::stringstream ss;
        ss << "(";
        for (size_t i = 0; i < conditions_.size(); ++i) {
            ss << conditions_[i]->getDescription();
            if (i < conditions_.size() - 1) {
                ss << " OR ";
            }
        }
        ss << ")";
        return ss.str();
    }
};