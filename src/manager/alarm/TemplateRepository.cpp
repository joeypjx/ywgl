// TemplateRepository.cpp
#include "TemplateRepository.h"
#include "GreaterThanCondition.h"
#include "LessThanCondition.h"
#include "AndCondition.h"
#include "OrCondition.h"
#include "NotCondition.h"
#include "LogAction.h"
#include "DatabaseAction.h"
#include <SQLiteCpp/SQLiteCpp.h> // 实际的SQLite库
#include <nlohmann/json.hpp>     // 实际的JSON库
#include <iostream>

using json = nlohmann::json;

TemplateRepository::TemplateRepository(std::shared_ptr<DatabaseManager> dbManager, std::shared_ptr<AlarmEventRepository> eventRepo)
    : dbManager_(dbManager), eventRepo_(eventRepo)
{
    std::cout << "[DB] Database opened at " << dbManager_->getDb().getFilename() << std::endl;
}

TemplateRepository::~TemplateRepository() = default;

void TemplateRepository::createTables()
{
    std::lock_guard<std::mutex> lock(dbManager_->getMutex());
    auto &db = dbManager_->getDb();
    SQLite::Transaction transaction(db);
    db.exec("CREATE TABLE IF NOT EXISTS alarm_templates (id TEXT PRIMARY KEY, metric_name TEXT NOT NULL, root_condition_id INTEGER NOT NULL, alarm_type TEXT NOT NULL DEFAULT '', alarm_level TEXT NOT NULL DEFAULT '', content_template TEXT NOT NULL DEFAULT '节点 {resourceName} 发生 {alarmLevel} 告警', trigger_count_threshold INTEGER NOT NULL DEFAULT 1, FOREIGN KEY (root_condition_id) REFERENCES alarm_conditions(id))");
    db.exec("CREATE TABLE IF NOT EXISTS alarm_conditions (id INTEGER PRIMARY KEY AUTOINCREMENT, condition_type TEXT NOT NULL, params_json TEXT)");
    db.exec("CREATE TABLE IF NOT EXISTS alarm_condition_composition (parent_condition_id INTEGER NOT NULL, child_condition_id INTEGER NOT NULL, child_order INTEGER NOT NULL, PRIMARY KEY (parent_condition_id, child_condition_id), FOREIGN KEY (parent_condition_id) REFERENCES alarm_conditions(id), FOREIGN KEY (child_condition_id) REFERENCES alarm_conditions(id))");
    db.exec("CREATE TABLE IF NOT EXISTS alarm_actions (id INTEGER PRIMARY KEY AUTOINCREMENT, action_type TEXT NOT NULL, params_json TEXT)");
    db.exec("CREATE TABLE IF NOT EXISTS alarm_template_actions (template_id TEXT NOT NULL, action_id INTEGER NOT NULL, PRIMARY KEY (template_id, action_id), FOREIGN KEY (template_id) REFERENCES alarm_templates(id), FOREIGN KEY (action_id) REFERENCES alarm_actions(id))");
    transaction.commit();
    std::cout << "[DB] Tables created or verified." << std::endl;
}

std::shared_ptr<IAlarmCondition> TemplateRepository::loadConditionRecursive(int conditionId)
{
    auto &db = dbManager_->getDb();
    SQLite::Statement query(db, "SELECT condition_type, params_json FROM alarm_conditions WHERE id = ?");
    query.bind(1, conditionId);

    if (!query.executeStep())
    {
        throw std::runtime_error("Condition with ID " + std::to_string(conditionId) + " not found.");
    }

    std::string type = query.getColumn(0);
    std::string params_str = query.getColumn(1).getText(); // Use getText to handle NULL

    if (type == "GreaterThan")
    {
        auto params = json::parse(params_str);
        return std::make_shared<GreaterThanCondition>(params["threshold"]);
    }
    if (type == "LessThan")
    {
        auto params = json::parse(params_str);
        return std::make_shared<LessThanCondition>(params["threshold"]);
    }
    if (type == "Not")
    {
        SQLite::Statement child_query(db, "SELECT child_condition_id FROM alarm_condition_composition WHERE parent_condition_id = ?");
        child_query.bind(1, conditionId);
        child_query.executeStep();
        int child_id = child_query.getColumn(0);
        return std::make_shared<NotCondition>(loadConditionRecursive(child_id));
    }
    if (type == "And" || type == "Or")
    {
        std::vector<std::shared_ptr<IAlarmCondition>> child_conditions;
        SQLite::Statement child_query(db, "SELECT child_condition_id FROM alarm_condition_composition WHERE parent_condition_id = ? ORDER BY child_order");
        child_query.bind(1, conditionId);
        while (child_query.executeStep())
        {
            int child_id = child_query.getColumn(0);
            child_conditions.push_back(loadConditionRecursive(child_id));
        }
        if (type == "And")
            return std::make_shared<AndCondition>(child_conditions);
        return std::make_shared<OrCondition>(child_conditions);
    }

    throw std::runtime_error("Unknown condition type: " + type);
}

std::vector<std::shared_ptr<IAlarmAction>> TemplateRepository::loadActionsForTemplate(const std::string &templateId)
{
    auto &db = dbManager_->getDb();
    std::vector<std::shared_ptr<IAlarmAction>> loaded_actions;
    SQLite::Statement query(db, "SELECT a.action_type, a.params_json FROM alarm_actions a JOIN alarm_template_actions ta ON a.id = ta.action_id WHERE ta.template_id = ?");
    query.bind(1, templateId);

    while (query.executeStep())
    {
        std::string type = query.getColumn(0);
        if (type == "Log")
        {
            loaded_actions.push_back(std::make_shared<LogAction>());
        } else if (type == "Database")
        {
            loaded_actions.push_back(std::make_shared<DatabaseAction>(eventRepo_));
        } else {
            throw std::runtime_error("Unknown action type: " + type);
        }
    }
    return loaded_actions;
}

std::vector<AlarmRuleTemplate> TemplateRepository::loadAllTemplates()
{
    std::lock_guard<std::mutex> lock(dbManager_->getMutex());
    auto &db = dbManager_->getDb();
    std::vector<AlarmRuleTemplate> templates;
    SQLite::Statement query(db, "SELECT id, metric_name, root_condition_id, alarm_type, alarm_level, content_template, trigger_count_threshold FROM alarm_templates");

    while (query.executeStep())
    {
        AlarmRuleTemplate tpl;
        tpl.templateId = query.getColumn(0).getText();
        tpl.metricName = query.getColumn(1).getText();
        int root_condition_id = query.getColumn(2);
        tpl.alarmType = query.getColumn(3).getText();
        tpl.alarmLevel = query.getColumn(4).getText();
        tpl.contentTemplate = query.getColumn(5).getText();
        tpl.triggerCountThreshold = query.getColumn(6).getInt();

        tpl.condition = loadConditionRecursive(root_condition_id);
        tpl.actions = loadActionsForTemplate(tpl.templateId);

        templates.push_back(tpl);
    }
    std::cout << "[DB] Loaded " << templates.size() << " templates from database." << std::endl;
    return templates;
}

// ---- Implementation for saving (for demo purposes) ----

int TemplateRepository::saveConditionRecursive(const std::shared_ptr<IAlarmCondition> &condition)
{
    // Note: A real implementation should check if an identical condition already exists to avoid duplicates.
    // This simplified version always inserts.

    std::string type;
    json params;

    // Use dynamic_cast to determine the concrete type
    if (auto p = std::dynamic_pointer_cast<GreaterThanCondition>(condition))
    {
        type = "GreaterThan";
        params["threshold"] = p->getThreshold(); // (Needs member access or a getter)
    }
    else if (auto p = std::dynamic_pointer_cast<LessThanCondition>(condition))
    {
        type = "LessThan";
        params["threshold"] = p->getThreshold();
    }
    else if (auto p = std::dynamic_pointer_cast<AndCondition>(condition))
    {
        type = "And";
    }
    else if (auto p = std::dynamic_pointer_cast<OrCondition>(condition))
    {
        type = "Or";
    }
    else if (auto p = std::dynamic_pointer_cast<NotCondition>(condition))
    {
        type = "Not";
    }
    else
    {
        throw std::runtime_error("Unsupported condition type for saving.");
    }

    auto &db = dbManager_->getDb();
    SQLite::Statement insert_cond(db, "INSERT INTO alarm_conditions (condition_type, params_json) VALUES (?, ?)");
    insert_cond.bind(1, type);
    insert_cond.bind(2, params.is_null() ? "null" : params.dump());
    insert_cond.exec();

    int parent_id = db.getLastInsertRowid();

    // If it's a composite, save its children
    if (auto p = std::dynamic_pointer_cast<const AndCondition>(condition))
    {
        int order = 0;
        for (const auto &child : p->getConditions())
        {
            int child_id = saveConditionRecursive(child);
            SQLite::Statement insert_comp(db, "INSERT INTO alarm_condition_composition VALUES (?, ?, ?)");
            insert_comp.bind(1, parent_id);
            insert_comp.bind(2, child_id);
            insert_comp.bind(3, order++);
            insert_comp.exec();
        }
    } // ... Add similar blocks for OrCondition and NotCondition ...

    return parent_id;
}

void TemplateRepository::saveTemplate(const AlarmRuleTemplate &tpl)
{
    // Simplified save function. A production version needs more robust error handling, transactions, and de-duplication.
    // For this to compile, GreaterThanCondition, etc., need public member access or getters. Let's assume they exist.
    try
    {
        std::lock_guard<std::mutex> lock(dbManager_->getMutex());
        auto &db = dbManager_->getDb();
        SQLite::Transaction transaction(db);

        // 1. Save Condition
        int root_condition_id = saveConditionRecursive(tpl.condition);

        // 2. Save Actions (simplified: assume one Log action with ID 1 exists)
        db.exec("INSERT OR IGNORE INTO alarm_actions (id, action_type) VALUES (1, 'Log')");

        // 3. Save Template
        SQLite::Statement insert_tpl(db, "INSERT OR REPLACE INTO alarm_templates (id, metric_name, root_condition_id, alarm_type, alarm_level, content_template, trigger_count_threshold) VALUES (?, ?, ?, ?, ?, ?, ?)");
        insert_tpl.bind(1, tpl.templateId);
        insert_tpl.bind(2, tpl.metricName);
        insert_tpl.bind(3, root_condition_id);
        insert_tpl.bind(4, tpl.alarmType);
        insert_tpl.bind(5, tpl.alarmLevel);
        insert_tpl.bind(6, tpl.contentTemplate);
        insert_tpl.bind(7, tpl.triggerCountThreshold);
        insert_tpl.exec();

        // 4. Link Template and Actions
        SQLite::Statement insert_link(db, "INSERT OR REPLACE INTO alarm_template_actions VALUES (?, ?)");
        insert_link.bind(1, tpl.templateId);
        insert_link.bind(2, 1); // Assuming action with ID 1
        insert_link.exec();

        transaction.commit();
        std::cout << "[DB] Saved template '" << tpl.templateId << "'." << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[DB] Failed to save template: " << e.what() << std::endl;
    }
}

// ---- 新增: 从JSON保存的实现 ----

int TemplateRepository::saveConditionFromJsonRecursive(const json& j_cond) {
    auto& db = dbManager_->getDb();

    // 1. 提取类型和参数
    std::string type = j_cond.at("type").get<std::string>();
    json params = j_cond.value("params", json::object());

    // 2. 插入到 'conditions' 表
    SQLite::Statement insert_cond(db, "INSERT INTO alarm_conditions (condition_type, params_json) VALUES (?, ?)");
    insert_cond.bind(1, type);
    insert_cond.bind(2, params.is_null() ? "null" : params.dump());
    insert_cond.exec();
    
    int parent_id = db.getLastInsertRowid();

    // 3. 递归处理组合条件
    if (type == "And" || type == "Or") {
        int order = 0;
        for (const auto& child_cond_json : j_cond.at("conditions")) {
            int child_id = saveConditionFromJsonRecursive(child_cond_json);
            SQLite::Statement insert_comp(db, "INSERT INTO alarm_condition_composition (parent_condition_id, child_condition_id, child_order) VALUES (?, ?, ?)");
            insert_comp.bind(1, parent_id);
            insert_comp.bind(2, child_id);
            insert_comp.bind(3, order++);
            insert_comp.exec();
        }
    } else if (type == "Not") {
        int child_id = saveConditionFromJsonRecursive(j_cond.at("condition"));
        SQLite::Statement insert_comp(db, "INSERT INTO alarm_condition_composition (parent_condition_id, child_condition_id, child_order) VALUES (?, ?, ?)");
        insert_comp.bind(1, parent_id);
        insert_comp.bind(2, child_id);
        insert_comp.bind(3, 0); // Not只有一个子节点，顺序为0
        insert_comp.exec();
    }

    return parent_id;
}

std::vector<int> TemplateRepository::saveActionsFromJson(const json& j_actions) {
    auto& db = dbManager_->getDb();
    std::vector<int> action_ids;

    for (const auto& j_action : j_actions) {
        std::string type = j_action.at("type").get<std::string>();
        json params = j_action.value("params", json::object());

        // 注意: 简单的实现总是插入新动作。生产环境可能需要检查重复。
        SQLite::Statement insert_action(db, "INSERT INTO alarm_actions (action_type, params_json) VALUES (?, ?)");
        insert_action.bind(1, type);
        insert_action.bind(2, params.is_null() ? "null" : params.dump());
        insert_action.exec();
        action_ids.push_back(db.getLastInsertRowid());
    }
    return action_ids;
}

void TemplateRepository::saveTemplate(const json& tpl_json) {
    std::lock_guard<std::mutex> lock(dbManager_->getMutex());
    SQLite::Transaction transaction(dbManager_->getDb());

    try {
        auto& db = dbManager_->getDb();
        std::string templateId = tpl_json.at("templateId").get<std::string>();
        std::string metricName = tpl_json.at("metricName").get<std::string>();
        std::string alarmType = tpl_json.value("alarmType", ""); // 使用 .value 提供默认值
        std::string alarmLevel = tpl_json.value("alarmLevel", ""); // 使用 .value 提供默认值
        std::string contentTemplate = tpl_json.value("contentTemplate", "节点 {resourceName} 发生 {alarmLevel} 告警"); // 新增
        int triggerCountThreshold = tpl_json.value("triggerCountThreshold", 1); // 新增

        // 递归保存条件并获取根ID
        int root_condition_id = saveConditionFromJsonRecursive(tpl_json.at("condition"));

        // 保存动作并获取ID列表
        std::vector<int> action_ids = saveActionsFromJson(tpl_json.at("actions"));

        // 保存模板主体 (使用 INSERT OR REPLACE 来支持更新)
        SQLite::Statement insert_tpl(db, "INSERT OR REPLACE INTO alarm_templates (id, metric_name, root_condition_id, alarm_type, alarm_level, content_template, trigger_count_threshold) VALUES (?, ?, ?, ?, ?, ?, ?)");
        insert_tpl.bind(1, templateId);
        insert_tpl.bind(2, metricName);
        insert_tpl.bind(3, root_condition_id);
        insert_tpl.bind(4, alarmType);
        insert_tpl.bind(5, alarmLevel);
        insert_tpl.bind(6, contentTemplate);
        insert_tpl.bind(7, triggerCountThreshold);
        insert_tpl.exec();

        // 关联模板与动作 (先删除旧的关联，再插入新的)
        SQLite::Statement delete_links(db, "DELETE FROM alarm_template_actions WHERE template_id = ?");
        delete_links.bind(1, templateId);
        delete_links.exec();

        SQLite::Statement insert_link(db, "INSERT INTO alarm_template_actions (template_id, action_id) VALUES (?, ?)");
        for (int action_id : action_ids) {
            insert_link.bind(1, templateId);
            insert_link.bind(2, action_id);
            insert_link.exec();
            insert_link.reset();
        }

        transaction.commit();
        std::cout << "[DB] Saved template '" << templateId << "' from JSON." << std::endl;

    } catch (const std::exception& e) {
        // 发生异常时，事务会自动回滚
        std::cerr << "[DB] Failed to save template from JSON: " << e.what() << std::endl;
        throw; // 重新抛出异常，让调用者知道操作失败
    }
}