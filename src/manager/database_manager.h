#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>
#include <unordered_map>
#include <vector>
#include <optional>
#include <mutex>
#include <atomic>

// 前向声明
namespace SQLite {
    class Database;
}

/**
 * DatabaseManager类 - 数据库管理器
 * 
 * 负责管理SQLite数据库的连接和操作
 */
class DatabaseManager {
public:
    // 构造与析构
    explicit DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    // 数据库初始化
    bool initialize();
    bool initializeNodeTables();

    // Node Status Monitor
    void startNodeStatusMonitorThread();
    bool updateNodeStatusOnly(const std::string& hostIp, const std::string& new_status);

    // Node Management
    bool updateNode(const nlohmann::json& node_info);
    nlohmann::json getNode(int box_id, int slot_id, int cpu_id);
    nlohmann::json getNodeByHostIp(const std::string& hostIp);
    nlohmann::json getAllNodes();
    nlohmann::json getNodesWithLatestMetrics();

    // Node Metrics Methods
    bool saveNodeCpuMetrics(const std::string& hostIp, long long timestamp, const nlohmann::json& cpu_data);
    bool saveNodeMemoryMetrics(const std::string& hostIp, long long timestamp, const nlohmann::json& memory_data);
    bool saveNodeDiskMetrics(const std::string& hostIp, long long timestamp, const nlohmann::json& disk_data);
    bool saveNodeNetworkMetrics(const std::string& hostIp, long long timestamp, const nlohmann::json& network_data);
    bool saveNodeDockerMetrics(const std::string& hostIp, long long timestamp, const nlohmann::json& docker_data);
    bool saveNodeGpuMetrics(const std::string& hostIp, long long timestamp, const nlohmann::json& gpu_data);

    // Node Metrics Query Methods
    nlohmann::json getNodeCpuMetrics(const std::string& hostIp, int limit = 100);
    nlohmann::json getNodeMemoryMetrics(const std::string& hostIp, int limit = 100);
    nlohmann::json getNodeDiskMetrics(const std::string& hostIp, int limit = 100);
    nlohmann::json getNodeNetworkMetrics(const std::string& hostIp, int limit = 100);
    nlohmann::json getNodeDockerMetrics(const std::string& hostIp, int limit = 100);
    nlohmann::json getNodeGpuMetrics(const std::string& hostIp, int limit = 100);
    
    bool saveNodeResourceUsage(const nlohmann::json& resource_usage);

private:
    std::string db_path_;                     // 数据库文件路径
    std::unique_ptr<SQLite::Database> db_;    // 数据库连接

    // Node Status Monitor
    std::unique_ptr<std::thread> node_status_monitor_thread_;
    std::atomic<bool> node_status_monitor_running_{false}; // Initialize to false
    void nodeStatusMonitorLoop(); // Method to be run by node_status_monitor_thread_
};

#endif // DATABASE_MANAGER_H
