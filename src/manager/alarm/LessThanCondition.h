// LessThanCondition.h
#pragma once
#include "IAlarmCondition.h"
#include <sstream>
#include <string>

class LessThanCondition : public IAlarmCondition {
public:
    explicit LessThanCondition(double threshold) : threshold_(threshold) {}

    bool isTriggered(double value) const override {
        return value < threshold_;
    }
    std::string getDescription() const override {
        std::ostringstream oss;
        oss << "is less than " << threshold_;
        return oss.str();
    }
    std::string getType() const override { return "LessThan"; }
    std::string getMetric() const { return ""; }
    double getThreshold() const override {
        return threshold_;
    }
    std::vector<std::shared_ptr<IAlarmCondition>> getConditions() const override {
        return {};
    }
private:
    double threshold_;
};