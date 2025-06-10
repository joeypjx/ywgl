// AlarmEventRepository.h
#pragma once
#include <SQLiteCpp/SQLiteCpp.h>
#include <string>
#include <mutex>
#include <iostream>

// 告警事件类型
enum class AlarmEventType {
    TRIGGERED,
    RECOVERED
};

// 线程安全的数据库操作类
class AlarmEventRepository {
private:
    SQLite::Database db_;
    mutable std::mutex dbMutex_;

    // 将枚举转换为字符串
    std::string eventTypeToString(AlarmEventType type) const {
        return (type == AlarmEventType::TRIGGERED) ? "TRIGGERED" : "RECOVERED";
    }

public:
    // 构造函数打开数据库并创建表
    explicit AlarmEventRepository(const std::string& dbPath)
        : db_(dbPath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
        std::cout << "[DB] Opening database at: " << dbPath << std::endl;
        try {
            std::lock_guard<std::mutex> lock(dbMutex_);
            db_.exec(R"(
                CREATE TABLE IF NOT EXISTS alarm_events (
                    id            INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp     TEXT    NOT NULL,
                    rule_id       TEXT    NOT NULL,
                    resource_name TEXT    NOT NULL,
                    event_type    TEXT    NOT NULL CHECK(event_type IN ('TRIGGERED', 'RECOVERED')),
                    details       TEXT
                );
            )");
        } catch (const std::exception& e) {
            std::cerr << "[DB] Exception on table creation: " << e.what() << std::endl;
            throw;
        }
    }

    // 插入一个新的告警事件
    void insertEvent(const std::string& ruleId, const std::string& resourceName, AlarmEventType eventType, const std::string& details) {
        try {
            std::lock_guard<std::mutex> lock(dbMutex_);
            
            // 获取当前时间戳 (ISO 8601 format)
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);

            SQLite::Statement query(db_, "INSERT INTO alarm_events (timestamp, rule_id, resource_name, event_type, details) VALUES (?, ?, ?, ?, ?)");
            query.bind(1, std::to_string(in_time_t));
            query.bind(2, ruleId);
            query.bind(3, resourceName);
            query.bind(4, eventTypeToString(eventType));
            query.bind(5, details);
            
            query.exec();
        } catch (const std::exception& e) {
            std::cerr << "[DB] Exception on insert: " << e.what() << std::endl;
        }
    }
};