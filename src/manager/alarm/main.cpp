// main.cpp
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <nlohmann/json.hpp>
#include <vector>
using json = nlohmann::json;

#include "MetricCache.h"
#include "AlarmManager.h"
#include "RuleProvisioner.h"
#include "AlarmRuleTemplate.h"
#include "GreaterThanCondition.h"
#include "LessThanCondition.h"
#include "LogAction.h"
#include "AlarmEventRepository.h"
#include "TemplateRepository.h"
#include "DatabaseAction.h"
#include "AndCondition.h"
#include "OrCondition.h"
#include "NotCondition.h"

// 模拟Agent数据上报的函数
void simulate_agent_data(std::shared_ptr<MetricCache> cache, std::atomic<bool> &stop_signal)
{
    std::cout << "[Simulator] Started. Simulating agent data..." << std::endl;
    std::vector<std::string> node_pool = {"node-01", "node-02", "web-server-alpha"};

    std::default_random_engine generator(std::random_device{}());
    std::uniform_real_distribution<double> normal_dist(10.0, 40.0);
    std::uniform_real_distribution<double> high_dist(90.0, 99.0);

    int seconds_passed = 0;
    while (!stop_signal)
    {
        // node-01: high cpu
        cache->updateNodeMetrics("node-01", json{
            {"cpu_usage_percent", high_dist(generator)},
            {"memory_usage_percent", normal_dist(generator)},
            {"disk", json::array({
                {{"path", "/dev/sda1"}, {"usage_percent", high_dist(generator)}},
                {{"path", "/dev/sdb1"}, {"usage_percent", high_dist(generator)}}
            })}
        });
        // node-02: normal cpu
        cache->updateNodeMetrics("node-02", json{{"cpu_usage_percent", normal_dist(generator)}, {"memory_usage_percent", normal_dist(generator)}});

        // web-server-alpha: normal cpu
        cache->updateNodeMetrics("web-server-alpha", json{{"cpu_usage_percent", normal_dist(generator)}, {"memory_usage_percent", normal_dist(generator)}});

        // 在25秒后，模拟一个新节点上线
        if (seconds_passed > 25 && seconds_passed <= 27)
        {
            std::cout << "\n[Simulator] A new node 'db-server-beta' just came online!\n"
                      << std::endl;
            cache->updateNodeMetrics("db-server-beta", json{{"cpu_usage_percent", normal_dist(generator)}, {"memory_usage_percent", normal_dist(generator)}});
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        seconds_passed += 2;
    }
    std::cout << "[Simulator] Stopped." << std::endl;
}

int main()
{
    // 1. 创建核心共享组件
    auto dbManager = std::make_shared<DatabaseManager>("alarm_events.db");
    auto cache = std::make_shared<MetricCache>();
    auto manager = std::make_shared<AlarmManager>();
    auto provisioner = std::make_shared<RuleProvisioner>(manager, cache);
    auto repository = std::make_shared<AlarmEventRepository>(dbManager);

    auto templateRepository = std::make_shared<TemplateRepository>(dbManager, repository);
    templateRepository->createTables();

    // 创建一个JSON对象来表示告警模板
    json high_cpu_tpl = {
        {"templateId", "高CPU使用率-严重"},
        {"metricName", "cpu_usage_percent"},
        {"alarmType", "system"},
        {"alarmLevel", "critical"},
        {"condition", {{"type", "GreaterThan"}, {"params", {{"threshold", 90.0}}}}},
        {"triggerCountThreshold", 3},
        {"contentTemplate", "{state} 节点 {nodeId} 发生 {alarmLevel} 告警，指标 {metricName} 值为 {value}"},
        {"actions", {{{"type", "Log"}}, {{"type", "Database"}}}}};

    json high_disk_tpl = {
        {"templateId", "中磁盘使用率-警告"},
        {"metricName", "disk[path=/dev/sda1].usage_percent"},
        {"alarmType", "system"},
        {"alarmLevel", "warning"},
        {"triggerCountThreshold", 3},
        {"condition", {{"type", "And"}, {"conditions", {
            {{"type", "GreaterThan"}, {"params", {{"threshold", 80.0}}}},
            {{"type", "LessThan"}, {"params", {{"threshold", 95.0}}}}
        }}}},
        {"contentTemplate", "{state} 节点 {nodeId} 发生 {alarmLevel} 告警，指标 {metricName} 值为 {value}"},
        {"actions", {{{"type", "Log"}}, {{"type", "Database"}}}}};

    try
    {
        // 调用新函数来保存模板
        templateRepository->saveTemplate(high_cpu_tpl);
        templateRepository->saveTemplate(high_disk_tpl);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to run demo save: " << e.what() << std::endl;
    }
    
    for (const auto& tpl : templateRepository->loadAllTemplates()) {
        provisioner->addTemplate(tpl);
    }

    // 4. 启动所有后台服务
    manager->start();
    provisioner->start();

    // 5. 启动模拟器
    std::atomic<bool> stop_simulator(false);
    std::thread simulator_thread(simulate_agent_data, cache, std::ref(stop_simulator));

    // 6. 让程序运行一段时间
    std::cout << "\n--- System is running. Monitoring for 60 seconds. ---" << std::endl;
    std::cout << "--- A new node will appear after 25 seconds. ---\n"
              << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(60));

    // 7. 优雅地停止所有服务
    std::cout << "\n--- Shutting down system... ---\n"
              << std::endl;
    stop_simulator = true;
    simulator_thread.join();
    provisioner->stop();
    manager->stop();

    std::cout << "\n--- System shut down gracefully. ---" << std::endl;

    return 0;
}