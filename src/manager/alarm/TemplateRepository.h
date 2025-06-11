// TemplateRepository.h
#pragma once
#include "AlarmRuleTemplate.h"
#include <string>
#include <vector>
#include <memory>
#include "../database_manager.h"
#include "nlohmann/json.hpp"
#include "AlarmEventRepository.h"

using json = nlohmann::json;

class TemplateRepository {
private:
    std::shared_ptr<DatabaseManager> dbManager_;
    std::shared_ptr<AlarmEventRepository> eventRepo_;

    // 内部递归辅助函数
    std::shared_ptr<IAlarmCondition> loadConditionRecursive(int conditionId);
    std::vector<std::shared_ptr<IAlarmAction>> loadActionsForTemplate(const std::string& templateId);

public:
    explicit TemplateRepository(std::shared_ptr<DatabaseManager> dbManager, std::shared_ptr<AlarmEventRepository> eventRepo);
    ~TemplateRepository();

    // 初始化数据库，创建表
    void createTables();

    // 从数据库加载所有告警模板
    std::vector<AlarmRuleTemplate> loadAllTemplates();
    nlohmann::json loadAllTemplatesAsJson();
    
    
    // (可选) 为了演示，提供一个保存模板的函数
    void saveTemplate(const AlarmRuleTemplate& tpl);
    void saveTemplate(const json& tpl_json);

private:
    // 递归保存条件的辅助函数 (from C++ objects)
    int saveConditionRecursive(const std::shared_ptr<IAlarmCondition>& condition);

    // 新增: 递归保存条件的辅助函数 (from JSON)
    // 返回条件在数据库中的ID,用于建立条件之间的父子关系
    int saveConditionFromJsonRecursive(const json& j_cond);
    // 新增: 保存动作的辅助函数 (from JSON)
    std::vector<int> saveActionsFromJson(const json& j_actions);

    // 新增: 递归加载条件并转换为JSON的辅助函数
    json conditionToJsonRecursive(const std::shared_ptr<IAlarmCondition>& condition);
};