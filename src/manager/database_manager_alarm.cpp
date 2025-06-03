#include "database_manager.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>
#include <optional>
#include <mutex>

// 全局缓存和互斥锁定义
std::unordered_map<std::string, std::vector<AlarmRule>> g_cached_rules;
std::mutex g_alarm_rule_mutex;

// 告警规则表创建SQL
static const char* CREATE_ALARM_RULES_TABLE_SQL = R"(
CREATE TABLE IF NOT EXISTS alarm_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    alarm_name TEXT NOT NULL,
    alarm_type INTEGER NOT NULL,
    alarm_level INTEGER NOT NULL,
    metric_key TEXT NOT NULL,
    comparison_operator TEXT NOT NULL,
    threshold_value TEXT NOT NULL,
    secondary_threshold_value TEXT,
    trigger_count INTEGER NOT NULL DEFAULT 1,
    is_enabled INTEGER NOT NULL DEFAULT 1,
    target_identifier TEXT,
    description TEXT,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT DEFAULT CURRENT_TIMESTAMP
);
)";

bool DatabaseManager::createAlarmRulesTable() {
    try {
        SQLite::Statement query(*db_, CREATE_ALARM_RULES_TABLE_SQL);
        query.exec();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create alarm_rules table: " << e.what() << std::endl;
        return false;
    }
}

// 新增：插入告警规则
nlohmann::json DatabaseManager::addAlarmRule(const nlohmann::json& rule) {
    try {
        SQLite::Statement insert(*db_,
            "INSERT INTO alarm_rules (alarm_name, alarm_type, alarm_level, metric_key, comparison_operator, threshold_value, secondary_threshold_value, trigger_count, is_enabled, target_identifier, description) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        insert.bind(1, rule["alarm_name"].get<std::string>());
        insert.bind(2, rule["alarm_type"].get<int>());
        insert.bind(3, rule["alarm_level"].get<int>());
        insert.bind(4, rule["metric_key"].get<std::string>());
        insert.bind(5, rule["comparison_operator"].get<std::string>());
        insert.bind(6, rule["threshold_value"].get<std::string>());
        if (rule.contains("secondary_threshold_value"))
            insert.bind(7, rule["secondary_threshold_value"].get<std::string>());
        else
            insert.bind(7);
        insert.bind(8, rule.value("trigger_count", 1));
        insert.bind(9, rule.value("is_enabled", 1));
        if (rule.contains("target_identifier"))
            insert.bind(10, rule["target_identifier"].get<std::string>());
        else
            insert.bind(10);
        if (rule.contains("description"))
            insert.bind(11, rule["description"].get<std::string>());
        else
            insert.bind(11);
        insert.exec();
        return { {"status", "success"}, {"id", db_->getLastInsertRowid()} };
    } catch (const std::exception& e) {
        return { {"status", "error"}, {"message", e.what()} };
    }
}

// 新增：删除告警规则
nlohmann::json DatabaseManager::deleteAlarmRule(int id) {
    try {
        SQLite::Statement del(*db_, "DELETE FROM alarm_rules WHERE id = ?");
        del.bind(1, id);
        int changes = del.exec();
        return { {"status", "success"}, {"deleted", changes} };
    } catch (const std::exception& e) {
        return { {"status", "error"}, {"message", e.what()} };
    }
}

// 新增：更新告警规则
nlohmann::json DatabaseManager::updateAlarmRule(int id, const nlohmann::json& rule) {
    try {
        SQLite::Statement update(*db_,
            "UPDATE alarm_rules SET alarm_name=?, alarm_type=?, alarm_level=?, metric_key=?, comparison_operator=?, threshold_value=?, secondary_threshold_value=?, trigger_count=?, is_enabled=?, target_identifier=?, description=?, updated_at=CURRENT_TIMESTAMP WHERE id=?");
        update.bind(1, rule["alarm_name"].get<std::string>());
        update.bind(2, rule["alarm_type"].get<int>());
        update.bind(3, rule["alarm_level"].get<int>());
        update.bind(4, rule["metric_key"].get<std::string>());
        update.bind(5, rule["comparison_operator"].get<std::string>());
        update.bind(6, rule["threshold_value"].get<std::string>());
        if (rule.contains("secondary_threshold_value"))
            update.bind(7, rule["secondary_threshold_value"].get<std::string>());
        else
            update.bind(7);
        update.bind(8, rule.value("trigger_count", 1));
        update.bind(9, rule.value("is_enabled", 1));
        if (rule.contains("target_identifier"))
            update.bind(10, rule["target_identifier"].get<std::string>());
        else
            update.bind(10);
        if (rule.contains("description"))
            update.bind(11, rule["description"].get<std::string>());
        else
            update.bind(11);
        update.bind(12, id);
        int changes = update.exec();
        return { {"status", "success"}, {"updated", changes} };
    } catch (const std::exception& e) {
        return { {"status", "error"}, {"message", e.what()} };
    }
}

// 新增：查询单条告警规则
nlohmann::json DatabaseManager::getAlarmRule(int id) {
    try {
        SQLite::Statement query(*db_, "SELECT * FROM alarm_rules WHERE id = ?");
        query.bind(1, id);
        if (query.executeStep()) {
            nlohmann::json rule;
            rule["id"] = query.getColumn("id").getInt();
            rule["alarm_name"] = query.getColumn("alarm_name").getString();
            rule["alarm_type"] = query.getColumn("alarm_type").getInt();
            rule["alarm_level"] = query.getColumn("alarm_level").getInt();
            rule["metric_key"] = query.getColumn("metric_key").getString();
            rule["comparison_operator"] = query.getColumn("comparison_operator").getString();
            rule["threshold_value"] = query.getColumn("threshold_value").getString();
            rule["secondary_threshold_value"] = query.getColumn("secondary_threshold_value").isNull() ? "" : query.getColumn("secondary_threshold_value").getString();
            rule["trigger_count"] = query.getColumn("trigger_count").getInt();
            rule["is_enabled"] = query.getColumn("is_enabled").getInt();
            rule["target_identifier"] = query.getColumn("target_identifier").isNull() ? "" : query.getColumn("target_identifier").getString();
            rule["description"] = query.getColumn("description").isNull() ? "" : query.getColumn("description").getString();
            rule["created_at"] = query.getColumn("created_at").getString();
            rule["updated_at"] = query.getColumn("updated_at").getString();
            return { {"status", "success"}, {"rule", rule} };
        } else {
            return { {"status", "error"}, {"message", "Not found"} };
        }
    } catch (const std::exception& e) {
        return { {"status", "error"}, {"message", e.what()} };
    }
}

// 新增：查询所有告警规则
nlohmann::json DatabaseManager::getAlarmRules() {
    try {
        nlohmann::json rules = nlohmann::json::array();
        SQLite::Statement query(*db_, "SELECT * FROM alarm_rules ORDER BY id DESC");
        while (query.executeStep()) {
            nlohmann::json rule;
            rule["id"] = query.getColumn("id").getInt();
            rule["alarm_name"] = query.getColumn("alarm_name").getString();
            rule["alarm_type"] = query.getColumn("alarm_type").getInt();
            rule["alarm_level"] = query.getColumn("alarm_level").getInt();
            rule["metric_key"] = query.getColumn("metric_key").getString();
            rule["comparison_operator"] = query.getColumn("comparison_operator").getString();
            rule["threshold_value"] = query.getColumn("threshold_value").getString();
            rule["secondary_threshold_value"] = query.getColumn("secondary_threshold_value").isNull() ? "" : query.getColumn("secondary_threshold_value").getString();
            rule["trigger_count"] = query.getColumn("trigger_count").getInt();
            rule["is_enabled"] = query.getColumn("is_enabled").getInt();
            rule["target_identifier"] = query.getColumn("target_identifier").isNull() ? "" : query.getColumn("target_identifier").getString();
            rule["description"] = query.getColumn("description").isNull() ? "" : query.getColumn("description").getString();
            rule["created_at"] = query.getColumn("created_at").getString();
            rule["updated_at"] = query.getColumn("updated_at").getString();
            rules.push_back(rule);
        }
        return { {"status", "success"}, {"rules", rules} };
    } catch (const std::exception& e) {
        return { {"status", "error"}, {"message", e.what()} };
    }
}

// 从数据库加载所有 is_enabled=1 的规则到缓存
void loadEnabledAlarmRulesToCache(SQLite::Database* db) {
    SQLite::Statement query(*db, "SELECT * FROM alarm_rules WHERE is_enabled=1");
    while (query.executeStep()) {
        AlarmRule rule;
        rule.id = query.getColumn("id").getInt();
        rule.alarm_name = query.getColumn("alarm_name").getString();
        rule.alarm_type = query.getColumn("alarm_type").getInt();
        rule.alarm_level = query.getColumn("alarm_level").getInt();
        rule.metric_key = query.getColumn("metric_key").getString();
        rule.comparison_operator = query.getColumn("comparison_operator").getString();
        rule.threshold_value = query.getColumn("threshold_value").getString();
        rule.secondary_threshold_value = query.getColumn("secondary_threshold_value").isNull() ? "" : query.getColumn("secondary_threshold_value").getString();
        rule.trigger_count = query.getColumn("trigger_count").getInt();
        rule.target_identifier = query.getColumn("target_identifier").isNull() ? "" : query.getColumn("target_identifier").getString();
        rule.description = query.getColumn("description").isNull() ? "" : query.getColumn("description").getString();
        rule.created_at = query.getColumn("created_at").getString();
        rule.updated_at = query.getColumn("updated_at").getString();
        g_cached_rules[rule.metric_key].push_back(rule);
    }
}

// 外部调用：刷新缓存（如配置API/管理UI变更后调用）
void refreshAlarmRuleCache(SQLite::Database* db) {
    loadEnabledAlarmRulesToCache(db);
} 