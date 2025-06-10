// GreaterThanCondition.h
#pragma once
#include "IAlarmCondition.h"
#include <sstream>

class GreaterThanCondition : public IAlarmCondition {
private:
    double threshold_;
public:
    explicit GreaterThanCondition(double t) : threshold_(t) {}
    bool isTriggered(double value) const override {
        return value > threshold_;
    }
    std::string getDescription() const override {
        std::ostringstream oss;
        oss << "is greater than " << threshold_;
        return oss.str();
    }
};