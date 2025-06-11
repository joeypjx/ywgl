#ifndef ALARM_SUBSYSTEM_H
#define ALARM_SUBSYSTEM_H

#include <memory>
#include <atomic>
#include <thread>
#include <string>
#include <nlohmann/json.hpp>

// 前向声明，避免在头文件中引入过多依赖
class MetricCache;
class AlarmManager;
class RuleProvisioner;
class TemplateRepository;
class DatabaseManager;
class AlarmEventRepository;

class AlarmSubsystem {
public:
    AlarmSubsystem(std::shared_ptr<DatabaseManager> dbManager);
    ~AlarmSubsystem();

    void initialize();
    void start();
    void stop();

    // 允许外部更新指标，例如从HTTP请求中
    void updateNodeMetrics(const std::string& nodeId, const nlohmann::json& metrics);

    // 新增：允许外部通过API添加告警模板
    void addAlarmTemplate(const nlohmann::json& ruleJson);

    // 新增: 获取所有告警规则
    nlohmann::json getAllAlarmTemplatesAsJson();
    
    // 新增: 获取所有告警事件
    nlohmann::json getAllAlarmEventsAsJson(int limit = 100);

private:
    void runDataSimulator();

    std::shared_ptr<DatabaseManager> dbManager_;
    std::shared_ptr<MetricCache> cache_;
    std::shared_ptr<AlarmManager> manager_;
    std::shared_ptr<RuleProvisioner> provisioner_;
    std::shared_ptr<AlarmEventRepository> eventRepo_;
    std::shared_ptr<TemplateRepository> templateRepo_;

    std::atomic<bool> stop_signal_{false};
    std::thread simulator_thread_;
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> alarm_thread_;
};

#endif // ALARM_SUBSYSTEM_H 