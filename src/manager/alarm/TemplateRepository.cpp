// TemplateRepository.cpp
#include "TemplateRepository.h"
#include "GreaterThanCondition.h"
#include "LessThanCondition.h"
#include "AndCondition.h"
#include "OrCondition.h"
#include "NotCondition.h"
#include "ILogicalCondition.h"
#include "LogAction.h"
#include "DatabaseAction.h"
#include <SQLiteCpp/SQLiteCpp.h> 
#include <nlohmann/json.hpp>
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
    db.exec("CREATE TABLE IF NOT EXISTS alarm_templates ("
            "template_id TEXT PRIMARY KEY, "
            "metric_name TEXT NOT NULL, "
            "alarm_type TEXT NOT NULL, "
            "alarm_level TEXT NOT NULL, "
            "content_template TEXT NOT NULL, "
            "trigger_count_threshold INTEGER NOT NULL, "
            "root_condition_id INTEGER NOT NULL, "
            "FOREIGN KEY (root_condition_id) REFERENCES alarm_conditions(id))");
    db.exec("CREATE TABLE IF NOT EXISTS alarm_conditions ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "condition_type TEXT NOT NULL, "
            "threshold REAL)");
    db.exec("CREATE TABLE IF NOT EXISTS alarm_condition_composition ("
            "parent_condition_id INTEGER NOT NULL, "
            "child_condition_id INTEGER NOT NULL, "
            "child_order INTEGER NOT NULL, "
            "PRIMARY KEY (parent_condition_id, child_condition_id), "
            "FOREIGN KEY (parent_condition_id) REFERENCES alarm_conditions(id), "
            "FOREIGN KEY (child_condition_id) REFERENCES alarm_conditions(id))");
    db.exec("CREATE TABLE IF NOT EXISTS alarm_actions ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "action_type TEXT NOT NULL, "
            "params_json TEXT)");
    db.exec("CREATE TABLE IF NOT EXISTS alarm_template_actions ("
            "template_id TEXT NOT NULL, "
            "action_id INTEGER NOT NULL, "
            "PRIMARY KEY (template_id, action_id), "
            "FOREIGN KEY (template_id) REFERENCES alarm_templates(template_id), "
            "FOREIGN KEY (action_id) REFERENCES alarm_actions(id))");
    transaction.commit();
    std::cout << "[DB] Tables created or verified." << std::endl;
}

std::shared_ptr<IAlarmCondition> TemplateRepository::loadConditionRecursive(int conditionId)
{
    auto &db = dbManager_->getDb();
    SQLite::Statement query(db, "SELECT condition_type, threshold FROM alarm_conditions WHERE id = ?");
    query.bind(1, conditionId);

    if (!query.executeStep())
    {
        throw std::runtime_error("Condition with ID " + std::to_string(conditionId) + " not found.");
    }

    std::string type = query.getColumn(0).getText();
    double threshold = query.getColumn(1).getDouble();

    if (type == "GreaterThan")
    {
        return std::make_shared<GreaterThanCondition>(threshold);
    }
    if (type == "LessThan")
    {
        return std::make_shared<LessThanCondition>(threshold);
    }
    
    std::vector<std::shared_ptr<IAlarmCondition>> childConditions;
    SQLite::Statement childQuery(db, "SELECT child_condition_id FROM alarm_condition_composition WHERE parent_condition_id = ? ORDER BY child_order");
    childQuery.bind(1, conditionId);
    while (childQuery.executeStep())
    {
        int childId = childQuery.getColumn(0).getInt();
        childConditions.push_back(loadConditionRecursive(childId));
    }

    if (type == "And") return std::make_shared<AndCondition>(childConditions);
    if (type == "Or") return std::make_shared<OrCondition>(childConditions);
    if (type == "Not") {
        if (!childConditions.empty()) {
            return std::make_shared<NotCondition>(childConditions.front());
        }
    }
    
    throw std::runtime_error("Unknown or malformed condition type: " + type);
}

std::vector<std::shared_ptr<IAlarmAction>> TemplateRepository::loadActionsForTemplate(const std::string& templateId) {
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
    SQLite::Statement query(db, "SELECT template_id, metric_name, root_condition_id, alarm_type, alarm_level, content_template, trigger_count_threshold FROM alarm_templates");

    while (query.executeStep())
    {
        AlarmRuleTemplate tpl;
        tpl.templateId = query.getColumn(0).getText();
        tpl.metricName = query.getColumn(1).getText();
        int root_condition_id = query.getColumn(2).getInt();
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

int TemplateRepository::saveConditionRecursive(const std::shared_ptr<IAlarmCondition>& condition)
{
    std::string type;
    double threshold = 0.0;

    if (auto p = std::dynamic_pointer_cast<GreaterThanCondition>(condition)) {
        type = "GreaterThan";
        threshold = p->getThreshold();
    } else if (auto p = std::dynamic_pointer_cast<LessThanCondition>(condition)) {
        type = "LessThan";
        threshold = p->getThreshold();
    } else if (std::dynamic_pointer_cast<AndCondition>(condition)) {
        type = "And";
    } else if (std::dynamic_pointer_cast<OrCondition>(condition)) {
        type = "Or";
    } else if (std::dynamic_pointer_cast<NotCondition>(condition)) {
        type = "Not";
    } else {
        throw std::runtime_error("Unsupported condition type for saving.");
    }

    auto &db = dbManager_->getDb();
    SQLite::Statement insert_cond(db, "INSERT INTO alarm_conditions (condition_type, threshold) VALUES (?, ?)");
    insert_cond.bind(1, type);
    insert_cond.bind(2, threshold);
    insert_cond.exec();

    int parent_id = db.getLastInsertRowid();

    if (auto p = std::dynamic_pointer_cast<const ILogicalCondition>(condition)) {
        int order = 0;
        for (const auto &child : p->getConditions()) {
            int child_id = saveConditionRecursive(child);
            SQLite::Statement insert_comp(db, "INSERT INTO alarm_condition_composition (parent_condition_id, child_condition_id, child_order) VALUES (?, ?, ?)");
            insert_comp.bind(1, parent_id);
            insert_comp.bind(2, child_id);
            insert_comp.bind(3, order++);
            insert_comp.exec();
        }
    }

    return parent_id;
}

void TemplateRepository::saveTemplate(const AlarmRuleTemplate &tpl)
{
    try
    {
        std::lock_guard<std::mutex> lock(dbManager_->getMutex());
        auto &db = dbManager_->getDb();
        SQLite::Transaction transaction(db);

        int root_condition_id = saveConditionRecursive(tpl.condition);

        SQLite::Statement insert_tpl(db, "INSERT OR REPLACE INTO alarm_templates (template_id, metric_name, alarm_type, alarm_level, content_template, trigger_count_threshold, root_condition_id) VALUES (?, ?, ?, ?, ?, ?, ?)");
        insert_tpl.bind(1, tpl.templateId);
        insert_tpl.bind(2, tpl.metricName);
        insert_tpl.bind(3, tpl.alarmType);
        insert_tpl.bind(4, tpl.alarmLevel);
        insert_tpl.bind(5, tpl.contentTemplate);
        insert_tpl.bind(6, tpl.triggerCountThreshold);
        insert_tpl.bind(7, root_condition_id);
        insert_tpl.exec();
        
        // Actions would be saved and linked here...

        transaction.commit();
        std::cout << "[DB] Saved template '" << tpl.templateId << "'." << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[DB] Failed to save template: " << e.what() << std::endl;
    }
}

void TemplateRepository::saveTemplate(const json& tpl_json) {
    std::lock_guard<std::mutex> lock(dbManager_->getMutex());
    SQLite::Transaction transaction(dbManager_->getDb());

    try {
        auto& db = dbManager_->getDb();
        std::string templateId = tpl_json.at("templateId").get<std::string>();
        std::string metricName = tpl_json.at("metricName").get<std::string>();
        std::string alarmType = tpl_json.value("alarmType", "");
        std::string alarmLevel = tpl_json.value("alarmLevel", "");
        std::string contentTemplate = tpl_json.value("contentTemplate", "节点 {resourceName} 发生 {alarmLevel} 告警");
        int triggerCountThreshold = tpl_json.value("triggerCountThreshold", 1);

        int root_condition_id = saveConditionFromJsonRecursive(tpl_json.at("condition"));
        std::vector<int> action_ids = saveActionsFromJson(tpl_json.at("actions"));

        SQLite::Statement insert_tpl(db, "INSERT OR REPLACE INTO alarm_templates (template_id, metric_name, alarm_type, alarm_level, content_template, trigger_count_threshold, root_condition_id) VALUES (?, ?, ?, ?, ?, ?, ?)");
        insert_tpl.bind(1, templateId);
        insert_tpl.bind(2, metricName);
        insert_tpl.bind(3, alarmType);
        insert_tpl.bind(4, alarmLevel);
        insert_tpl.bind(5, contentTemplate);
        insert_tpl.bind(6, triggerCountThreshold);
        insert_tpl.bind(7, root_condition_id);
        insert_tpl.exec();

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
    } catch (const std::exception& e) {
        std::cerr << "[DB] Failed to save template from JSON: " << e.what() << std::endl;
        throw;
    }
}

json TemplateRepository::conditionToJsonRecursive(const std::shared_ptr<IAlarmCondition>& condition) {
    if (!condition) {
        return nullptr;
    }

    json j;
    j["type"] = condition->getType();

    if (auto gt = std::dynamic_pointer_cast<GreaterThanCondition>(condition)) {
        j["threshold"] = gt->getThreshold();
    } else if (auto lt = std::dynamic_pointer_cast<LessThanCondition>(condition)) {
        j["threshold"] = lt->getThreshold();
    }

    auto subConditions = condition->getConditions();
    if (!subConditions.empty()) {
        if (condition->getType() == "Not") {
            if (!subConditions.empty()) {
                j["condition"] = conditionToJsonRecursive(subConditions.front());
            }
        } else {
            j["conditions"] = json::array();
            for (const auto& sub : subConditions) {
                j["conditions"].push_back(conditionToJsonRecursive(sub));
            }
        }
    }

    return j;
}

nlohmann::json TemplateRepository::loadAllTemplatesAsJson() {
    std::lock_guard<std::mutex> lock(dbManager_->getMutex());
    SQLite::Database& db = dbManager_->getDb();
    
    json templates = json::array();
    
    try {
        SQLite::Statement query(db, "SELECT template_id, metric_name, alarm_type, alarm_level, content_template, trigger_count_threshold, root_condition_id FROM alarm_templates");
        
        while (query.executeStep()) {
            json tpl;
            tpl["template_id"] = query.getColumn(0).getText();
            tpl["metric_name"] = query.getColumn(1).getText();
            tpl["alarm_type"] = query.getColumn(2).getText();
            tpl["alarm_level"] = query.getColumn(3).getText();
            tpl["content_template"] = query.getColumn(4).getText();
            tpl["trigger_count_threshold"] = query.getColumn(5).getInt();
            
            int rootConditionId = query.getColumn(6).getInt();
            auto rootCondition = loadConditionRecursive(rootConditionId);
            tpl["condition"] = conditionToJsonRecursive(rootCondition);
            
            templates.push_back(tpl);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading templates as JSON: " << e.what() << std::endl;
    }
    
    return templates;
}

std::vector<int> TemplateRepository::saveActionsFromJson(const json& j_actions) {
    auto& db = dbManager_->getDb();
    std::vector<int> action_ids;

    for (const auto& j_action : j_actions) {
        std::string type = j_action.at("type").get<std::string>();
        json params = j_action.value("params", json::object());

        SQLite::Statement insert_action(db, "INSERT INTO alarm_actions (action_type, params_json) VALUES (?, ?)");
        insert_action.bind(1, type);
        insert_action.bind(2, params.dump());
        insert_action.exec();
        action_ids.push_back(db.getLastInsertRowid());
    }
    return action_ids;
}

int TemplateRepository::saveConditionFromJsonRecursive(const json& j_cond) {
    auto& db = dbManager_->getDb();

    std::string type = j_cond.at("type").get<std::string>();
    double threshold = 0.0;

    // 对于简单条件，直接从threshold字段获取值
    if (type == "GreaterThan" || type == "LessThan") {
        threshold = j_cond.at("threshold").get<double>();
    }

    SQLite::Statement insert_cond(db, "INSERT INTO alarm_conditions (condition_type, threshold) VALUES (?, ?)");
    insert_cond.bind(1, type);
    insert_cond.bind(2, threshold);
    insert_cond.exec();
    
    int parent_id = db.getLastInsertRowid();

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
        insert_comp.bind(3, 0);
        insert_comp.exec();
    }

    return parent_id;
}