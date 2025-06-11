// OrCondition.h
#pragma once
#include "IAlarmCondition.h"
#include "ILogicalCondition.h"
#include <vector>
#include <memory>
#include <sstream>

class OrCondition : public ILogicalCondition {
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

    std::string getType() const override { return "Or"; }

    double getThreshold() const override {
        // 对于逻辑组合条件，阈值没有直接意义，可以返回一个特殊值
        return 0.0;
    }

    std::vector<std::shared_ptr<IAlarmCondition>> getConditions() const override {
        return conditions_;
    }
};