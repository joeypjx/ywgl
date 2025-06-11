// AlarmEventRepository.h
#pragma once
#include <SQLiteCpp/SQLiteCpp.h>
#include <string>
#include <mutex>
#include <iostream>
#include "DatabaseManager.h"
#include "AlarmRule.h"

// 告警事件类型
enum class AlarmEventType {
    TRIGGERED,
    RECOVERED
};

// 线程安全的数据库操作类
class AlarmEventRepository {
private:
    std::shared_ptr<DatabaseManager> dbManager_;

    // 将枚举转换为字符串
    std::string eventTypeToString(AlarmEventType type) const {
        return (type == AlarmEventType::TRIGGERED) ? "TRIGGERED" : "RECOVERED";
    }

public:
    // 构造函数打开数据库并创建表
    explicit AlarmEventRepository(std::shared_ptr<DatabaseManager> dbManager)
        : dbManager_(std::move(dbManager)) {
        std::cout << "[DB] Opening database at: " << dbManager_->getDb().getFilename() << std::endl;
        try {
            std::lock_guard<std::mutex> lock(dbManager_->getMutex());
            dbManager_->getDb().exec(R"(
                CREATE TABLE IF NOT EXISTS alarm_events (
                    id              INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp       TEXT    NOT NULL,
                    rule_id         TEXT    NOT NULL,
                    template_id     TEXT    NOT NULL,
                    node_id         TEXT    NOT NULL,
                    metric_name     TEXT    NOT NULL,
                    value           REAL    NOT NULL,
                    alarm_type      TEXT    NOT NULL,
                    alarm_level     TEXT    NOT NULL,
                    event_type      TEXT    NOT NULL CHECK(event_type IN ('TRIGGERED', 'RECOVERED')),
                    details         TEXT
                );
            )");
        } catch (const std::exception& e) {
            std::cerr << "[DB] Exception on table creation (alarm_events): " << e.what() << std::endl;
            throw;
        }
    }

    // 插入一个新的告警事件
    void insertEvent(const AlarmRule& rule, AlarmEventType eventType) {
        try {
            std::lock_guard<std::mutex> lock(dbManager_->getMutex());
            
            // 获取当前时间戳
            auto now_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            char time_buf[20];
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_t));

            SQLite::Statement query(dbManager_->getDb(), R"(
                INSERT INTO alarm_events (timestamp, rule_id, template_id, node_id, metric_name, value, alarm_type, alarm_level, event_type, details) 
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?))");
            
            query.bind(1, std::string(time_buf));
            query.bind(2, rule.ruleId);
            query.bind(3, rule.templateId);
            query.bind(4, rule.nodeId);
            query.bind(5, rule.metricName);
            query.bind(6, rule.lastTriggeredValue);
            query.bind(7, rule.alarmType);
            query.bind(8, rule.alarmLevel);
            query.bind(9, eventTypeToString(eventType));
            query.bind(10, rule.contentTemplate);
            
            query.exec();
        } catch (const std::exception& e) {
            std::cerr << "[DB] Exception on insert event: " << e.what() << std::endl;
        }
    }
};