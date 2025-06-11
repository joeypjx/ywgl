// AndCondition.h
#pragma once
#include "IAlarmCondition.h"
#include "ILogicalCondition.h"
#include <vector>
#include <memory>
#include <sstream>

class AndCondition : public ILogicalCondition {
private:
    std::vector<std::shared_ptr<IAlarmCondition>> conditions_;

public:
    explicit AndCondition(std::vector<std::shared_ptr<IAlarmCondition>> conds)
        : conditions_(std::move(conds)) {}

    bool isTriggered(double value) const override {
        for (const auto& cond : conditions_) {
            if (!cond->isTriggered(value)) {
                return false; // AND逻辑：一旦有false，整体即为false
            }
        }
        return true; // 所有都为true
    }

    std::string getDescription() const override {
        std::stringstream ss;
        ss << "(";
        for (size_t i = 0; i < conditions_.size(); ++i) {
            ss << conditions_[i]->getDescription();
            if (i < conditions_.size() - 1) {
                ss << " AND ";
            }
        }
        ss << ")";
        return ss.str();
    }

    std::string getType() const override { return "And"; }

    double getThreshold() const override {
        // 对于逻辑组合条件，阈值没有直接意义，可以返回一个特殊值
        return 0.0;
    }

    std::vector<std::shared_ptr<IAlarmCondition>> getConditions() const override {
        return conditions_;
    }
};