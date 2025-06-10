// LessThanCondition.h
#pragma once
#include "IAlarmCondition.h"
#include <sstream>

class LessThanCondition : public IAlarmCondition {
private:
    double threshold_;
public:
    explicit LessThanCondition(double t) : threshold_(t) {}
    bool isTriggered(double value) const override {
        return value < threshold_;
    }
    std::string getDescription() const override {
        std::ostringstream oss;
        oss << "is less than " << threshold_;
        return oss.str();
    }
};