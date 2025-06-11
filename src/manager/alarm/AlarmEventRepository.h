// AlarmEventRepository.h
#pragma once
#include <SQLiteCpp/SQLiteCpp.h>
#include <string>
#include <mutex>
#include <iostream>
#include "../database_manager.h"
#include "AlarmRule.h"
#include <nlohmann/json.hpp>

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
    void insertEvent(const AlarmRule& rule, AlarmEventType eventType);

    // 新增: 获取最近的告警事件
    nlohmann::json getAllEventsAsJson(int limit = 100);
};