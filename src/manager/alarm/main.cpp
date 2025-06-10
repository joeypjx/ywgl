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
#include "DatabaseAction.h"
#include "AndCondition.h"
#include "OrCondition.h"
#include "NotCondition.h"

// 模拟Agent数据上报的函数
void simulate_agent_data(std::shared_ptr<MetricCache> cache, std::atomic<bool>& stop_signal) {
    std::cout << "[Simulator] Started. Simulating agent data..." << std::endl;
    std::vector<std::string> node_pool = {"node-01", "node-02", "web-server-alpha"};

    std::default_random_engine generator(std::random_device{}());
    std::uniform_real_distribution<double> normal_dist(10.0, 40.0);
    std::uniform_real_distribution<double> high_dist(90.0, 99.0);

    int seconds_passed = 0;
    while (!stop_signal) {
        // node-01: high cpu
        cache->updateNodeMetrics("node-01", json{{"cpu_usage_percent", high_dist(generator)}, {"memory_usage_percent", normal_dist(generator)}});
        
        // node-02: normal cpu
        cache->updateNodeMetrics("node-02", json{{"cpu_usage_percent", normal_dist(generator)}, {"memory_usage_percent", normal_dist(generator)}});
        
        // web-server-alpha: normal cpu
        cache->updateNodeMetrics("web-server-alpha", json{{"cpu_usage_percent", normal_dist(generator)}, {"memory_usage_percent", normal_dist(generator)}});

        // 在25秒后，模拟一个新节点上线
        if (seconds_passed > 25 && seconds_passed <= 27) {
             std::cout << "\n[Simulator] A new node 'db-server-beta' just came online!\n" << std::endl;
             cache->updateNodeMetrics("db-server-beta", json{{"cpu_usage_percent", normal_dist(generator)}, {"memory_usage_percent", normal_dist(generator)}});
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        seconds_passed += 2;
    }
    std::cout << "[Simulator] Stopped." << std::endl;
}


int main() {
    // 1. 创建核心共享组件
    auto cache = std::make_shared<MetricCache>();
    auto manager = std::make_shared<AlarmManager>();
    auto provisioner = std::make_shared<RuleProvisioner>(manager, cache);
    auto repository = std::make_shared<AlarmEventRepository>("alarm_events.db");

    // 2. 定义告警模板
    AlarmRuleTemplate highCpuTemplate;
    highCpuTemplate.templateId = "tpl-high-cpu";
    highCpuTemplate.metricName = "cpu_usage_percent";
    highCpuTemplate.condition = std::make_shared<GreaterThanCondition>(90.0);
    highCpuTemplate.actions.push_back(std::make_shared<LogAction>());
    highCpuTemplate.actions.push_back(std::make_shared<DatabaseAction>(repository, AlarmEventType::TRIGGERED));

    AlarmRuleTemplate highMemoryTemplate;
    highMemoryTemplate.templateId = "tpl-high-memory";
    highMemoryTemplate.metricName = "memory_usage_percent";
    auto greater_than_80 = std::make_shared<GreaterThanCondition>(80.0);
    auto less_than_95 = std::make_shared<LessThanCondition>(95.0);

    highMemoryTemplate.condition = std::make_shared<AndCondition>(std::vector<std::shared_ptr<IAlarmCondition>>{
        greater_than_80, 
        less_than_95
    }); 

    highMemoryTemplate.actions.push_back(std::make_shared<LogAction>());
    highMemoryTemplate.actions.push_back(std::make_shared<DatabaseAction>(repository, AlarmEventType::TRIGGERED));
    highMemoryTemplate.recoveryActions.push_back(std::make_shared<DatabaseAction>(repository, AlarmEventType::RECOVERED));

    // 3. 将模板添加到供应器
    provisioner->addTemplate(highCpuTemplate);
    provisioner->addTemplate(highMemoryTemplate);

    // 4. 启动所有后台服务
    manager->start();
    provisioner->start();
    
    // 5. 启动模拟器
    std::atomic<bool> stop_simulator(false);
    std::thread simulator_thread(simulate_agent_data, cache, std::ref(stop_simulator));

    // 6. 让程序运行一段时间
    std::cout << "\n--- System is running. Monitoring for 60 seconds. ---" << std::endl;
    std::cout << "--- A new node will appear after 25 seconds. ---\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(60));

    // 7. 优雅地停止所有服务
    std::cout << "\n--- Shutting down system... ---\n" << std::endl;
    stop_simulator = true;
    simulator_thread.join();
    provisioner->stop();
    manager->stop();

    std::cout << "\n--- System shut down gracefully. ---" << std::endl;

    return 0;
}