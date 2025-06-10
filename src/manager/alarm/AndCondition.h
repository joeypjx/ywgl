// AndCondition.h
#pragma once
#include "IAlarmCondition.h"
#include <vector>
#include <memory>
#include <sstream>

class AndCondition : public IAlarmCondition {
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
};