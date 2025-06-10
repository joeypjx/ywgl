// LogAction.h
#pragma once
#include "IAlarmAction.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <cstring>

class LogAction : public IAlarmAction {
public:
    void execute(const std::string& ruleId, const std::string& resourceName) override {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char time_str[30];
        // ctime_s(time_str, sizeof(time_str), &now);
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        time_str[strlen(time_str) - 1] = '\0'; // Remove newline

        std::cout << "\033[1;31m" // Set color to bold red
                  << "[ALARM TRIGGERED] " << time_str
                  << " | Rule ID: " << ruleId
                  << " | Details: " << resourceName
                  << "\033[0m" << std::endl; // Reset color
    }
};