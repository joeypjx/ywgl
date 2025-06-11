#include "AlarmEventRepository.h"
#include <chrono>

namespace {
// 辅助函数，用于替换字符串中的所有占位符
std::string& replace_all(std::string& str, const std::string& old_value, const std::string& new_value) {
    size_t pos = 0;
    while ((pos = str.find(old_value, pos)) != std::string::npos) {
        str.replace(pos, old_value.length(), new_value);
        pos += new_value.length();
    }
    return str;
}
}

// 插入一个新的告警事件
void AlarmEventRepository::insertEvent(const AlarmRule& rule, AlarmEventType eventType) {
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
        
        std::string message = rule.contentTemplate;
        // 替换所有占位符
        replace_all(message, "{ruleId}", rule.ruleId);
        replace_all(message, "{metricName}", rule.metricName);
        replace_all(message, "{alarmType}", rule.alarmType);
        replace_all(message, "{alarmLevel}", rule.alarmLevel);
        replace_all(message, "{resourceName}", rule.resource->getName());
        replace_all(message, "{value}", std::to_string(rule.lastTriggeredValue));
        replace_all(message, "{condition}", rule.condition->getDescription());
        replace_all(message, "{state}", eventTypeToString(eventType));
        replace_all(message, "{nodeId}", rule.nodeId);

        query.bind(10, message);
        
        query.exec();
    } catch (const std::exception& e) {
        std::cerr << "[DB] Exception on insert event: " << e.what() << std::endl;
    }
}

// 获取最近的告警事件
nlohmann::json AlarmEventRepository::getAllEventsAsJson(int limit) {
    std::lock_guard<std::mutex> lock(dbManager_->getMutex());
    SQLite::Database& db = dbManager_->getDb();
    
    nlohmann::json events = nlohmann::json::array();
    
    try {
        SQLite::Statement query(db, "SELECT id, timestamp, rule_id, template_id, node_id, metric_name, value, alarm_type, alarm_level, event_type, details FROM alarm_events ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, limit);
        
        while (query.executeStep()) {
            nlohmann::json event;
            event["id"] = query.getColumn(0).getInt();
            event["timestamp"] = query.getColumn(1).getText();
            event["rule_id"] = query.getColumn(2).getText();
            event["template_id"] = query.getColumn(3).getText();
            event["node_id"] = query.getColumn(4).getText();
            event["metric_name"] = query.getColumn(5).getText();
            event["value"] = query.getColumn(6).getDouble();
            event["alarm_type"] = query.getColumn(7).getText();
            event["alarm_level"] = query.getColumn(8).getText();
            event["event_type"] = query.getColumn(9).getText();
            event["details"] = query.getColumn(10).getText();
            events.push_back(event);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading alarm events as JSON: " << e.what() << std::endl;
    }
    
    return events;
} 