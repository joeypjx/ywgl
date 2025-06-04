#include "database_manager.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <chrono>
#include <string>
#include <nlohmann/json.hpp>
#include <thread>

// 只保留 node 表和所有 metrics 表的创建
bool DatabaseManager::initializeNodeTables() {
    if (!db_) {
        std::cerr << "Database connection not initialized in initializeChassisAndSlots." << std::endl;
        return false;
    }
    try {
        db_->exec("PRAGMA foreign_keys = ON;");

        // 创建 node 表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS node (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                box_id INTEGER NOT NULL,
                slot_id INTEGER NOT NULL,
                cpu_id INTEGER NOT NULL,
                srio_id INTEGER NOT NULL,
                host_ip TEXT NOT NULL,
                hostname TEXT NOT NULL,
                service_port INTEGER NOT NULL,
                box_type TEXT NOT NULL,
                board_type TEXT NOT NULL,
                cpu_type TEXT NOT NULL,
                os_type TEXT NOT NULL,
                resource_type TEXT NOT NULL,
                cpu_arch TEXT NOT NULL,
                gpu TEXT,
                status TEXT NOT NULL,
                created_at TIMESTAMP NOT NULL,
                updated_at TIMESTAMP NOT NULL,
                UNIQUE(box_id, slot_id, cpu_id)
            );
        )");
        
        // 创建node_cpu_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS node_cpu_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                host_ip TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                usage_percent REAL NOT NULL,
                load_avg_1m REAL NOT NULL,
                load_avg_5m REAL NOT NULL,
                load_avg_15m REAL NOT NULL,
                core_count INTEGER NOT NULL
            );
        )");

        // 创建node_memory_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS node_memory_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                host_ip TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                total BIGINT NOT NULL,
                used BIGINT NOT NULL,
                free BIGINT NOT NULL,
                usage_percent REAL NOT NULL
            );
        )");

        // 创建node_disk_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS node_disk_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                host_ip TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                disk_count INTEGER NOT NULL
            );
        )");

        // 创建node_disk_usage表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS node_disk_usage (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                slot_disk_metrics_id INTEGER NOT NULL,
                device TEXT NOT NULL,
                mount_point TEXT NOT NULL,
                total BIGINT NOT NULL,
                used BIGINT NOT NULL,
                free BIGINT NOT NULL,
                usage_percent REAL NOT NULL,
                FOREIGN KEY (slot_disk_metrics_id) REFERENCES node_disk_metrics(id)
            );
        )");

        // 创建node_network_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS node_network_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                host_ip TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                network_count INTEGER NOT NULL
            );
        )");

        // 创建node_network_usage表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS node_network_usage (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                slot_network_metrics_id INTEGER NOT NULL,
                interface TEXT NOT NULL,
                rx_bytes BIGINT NOT NULL,
                tx_bytes BIGINT NOT NULL,
                rx_packets BIGINT NOT NULL,
                tx_packets BIGINT NOT NULL,
                rx_errors INTEGER NOT NULL,
                tx_errors INTEGER NOT NULL,
                FOREIGN KEY (slot_network_metrics_id) REFERENCES node_network_metrics(id)
            );
        )");

        // 创建node_gpu_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS node_gpu_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                host_ip TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                gpu_count INTEGER NOT NULL
            );
        )");

        // 创建node_gpu_usage表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS node_gpu_usage (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                slot_gpu_metrics_id INTEGER NOT NULL,
                gpu_index INTEGER NOT NULL,
                name TEXT NOT NULL,
                compute_usage REAL NOT NULL,
                mem_usage REAL NOT NULL,
                mem_used BIGINT NOT NULL,
                mem_total BIGINT NOT NULL,
                temperature REAL NOT NULL,
                voltage REAL NOT NULL,
                current REAL NOT NULL,
                power REAL NOT NULL,
                FOREIGN KEY (slot_gpu_metrics_id) REFERENCES node_gpu_metrics(id)
            );
        )");

        // 创建node_docker_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS node_docker_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                host_ip TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                container_count INTEGER NOT NULL,
                running_count INTEGER NOT NULL,
                paused_count INTEGER NOT NULL,
                stopped_count INTEGER NOT NULL
            );
        )");

        // 创建node_docker_containers表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS node_docker_containers (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                slot_docker_metric_id INTEGER NOT NULL,
                container_id TEXT NOT NULL,
                name TEXT NOT NULL,
                image TEXT NOT NULL,
                status TEXT NOT NULL,
                cpu_percent REAL NOT NULL,
                memory_usage BIGINT NOT NULL,
                FOREIGN KEY (slot_docker_metric_id) REFERENCES node_docker_metrics(id)
            );
        )");

        // 创建索引以提高查询性能
        db_->exec("CREATE INDEX IF NOT EXISTS idx_node_cpu_metrics_host_ip ON node_cpu_metrics(host_ip)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_node_cpu_metrics_timestamp ON node_cpu_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_node_memory_metrics_host_ip ON node_memory_metrics(host_ip)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_node_memory_metrics_timestamp ON node_memory_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_node_disk_metrics_host_ip ON node_disk_metrics(host_ip)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_node_disk_metrics_timestamp ON node_disk_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_node_network_metrics_host_ip ON node_network_metrics(host_ip)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_node_network_metrics_timestamp ON node_network_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_node_docker_metrics_host_ip ON node_docker_metrics(host_ip)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_node_docker_metrics_timestamp ON node_docker_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_node_gpu_metrics_host_ip ON node_gpu_metrics(host_ip)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_node_gpu_metrics_timestamp ON node_gpu_metrics(timestamp)");

        std::cout << "Node metrics tables initialized successfully." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing node tables: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::updateNodeStatusOnly(const std::string& host_ip, const std::string& new_status) {
    if (!db_) {
        std::cerr << "Database connection not initialized in updateNodeStatusOnly." << std::endl;
        return false;
    }
    try {
        SQLite::Statement update(*db_, R"(
            UPDATE node 
            SET status = ?
            WHERE host_ip = ?
        )");
        update.bind(1, new_status);
        update.bind(2, host_ip);
        int rows_affected = update.exec();
        return rows_affected > 0;
    } catch (const std::exception& e) {
        std::cerr << "Error setting node status for host " << host_ip << ": " << e.what() << std::endl;
        return false;
    }
}

void DatabaseManager::startNodeStatusMonitorThread() {
    if (node_status_monitor_running_.load()) {
        return; 
    }
    node_status_monitor_running_.store(true);
    node_status_monitor_thread_.reset(new std::thread(&DatabaseManager::nodeStatusMonitorLoop, this));
    std::cout << "Node status monitor thread started." << std::endl;
}

void DatabaseManager::nodeStatusMonitorLoop() {
    if (!db_) {
        std::cerr << "Database connection is null for node status monitor loop." << std::endl;
        node_status_monitor_running_.store(false);
        return;
    }

    while (node_status_monitor_running_.load()) {
        try {
            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            int64_t now_epoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            
            SQLite::Statement query(*db_, "SELECT host_ip, updated_at, status FROM node");
            
            while (query.executeStep()) {
                std::string host_ip = query.getColumn(0).getString();
                int64_t last_updated_at = query.getColumn(1).getInt64();
                std::string current_status = query.getColumn(2).getString();

                if (current_status != "empty" && current_status != "offline") {
                    if ((now_epoch - last_updated_at) > 5) { 
                        std::cout << "Node with host_ip " << host_ip 
                                  << " is inactive. Setting status to offline via DatabaseManager." << std::endl;
                        this->updateNodeStatusOnly(host_ip, "offline"); 
                    }
                }
            }
        } catch (const SQLite::Exception& e) {
            std::cerr << "SQLite error in DatabaseManager node status monitor: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5)); 
        } catch (const std::exception& e) {
            std::cerr << "Error in DatabaseManager node status monitor: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1)); 
    }
    std::cout << "DatabaseManager node status monitor loop finished." << std::endl;
}

// 更新节点信息
bool DatabaseManager::updateNode(const nlohmann::json& node_info) {
    if (!db_) {
        std::cerr << "Database connection not initialized in saveNode." << std::endl;
        return false;
    }
    
    // 检查必要字段是否存在
    if (!node_info.contains("box_id") || !node_info.contains("slot_id") || !node_info.contains("cpu_id")) {
        std::cerr << "Node box_id, slot_id and cpu_id are required." << std::endl;
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    int box_id = node_info["box_id"].get<int>();
    int slot_id = node_info["slot_id"].get<int>();
    int cpu_id = node_info["cpu_id"].get<int>();
    int srio_id = node_info.value("srio_id", 0);
    std::string host_ip = node_info.value("host_ip", "");
    std::string hostname = node_info.value("hostname", "");
    int service_port = node_info.value("service_port", 0);
    std::string box_type = node_info.value("box_type", "");
    std::string board_type = node_info.value("board_type", "");
    std::string cpu_type = node_info.value("cpu_type", "");
    std::string os_type = node_info.value("os_type", "");
    std::string resource_type = node_info.value("resource_type", "");
    std::string cpu_arch = node_info.value("cpu_arch", "");
    
    // 处理GPU信息，将JSON数组转换为字符串存储
    std::string gpu_json = "[]";
    if (node_info.contains("gpu") && node_info["gpu"].is_array()) {
        gpu_json = node_info["gpu"].dump();
    }

    try {
        SQLite::Statement query(*db_, "SELECT id FROM node WHERE box_id = ? AND slot_id = ? AND cpu_id = ?");
        query.bind(1, box_id);
        query.bind(2, slot_id);
        query.bind(3, cpu_id);

        if (query.executeStep()) { // Node exists, update it
            SQLite::Statement update(*db_, R"(
                UPDATE node 
                SET srio_id = ?, host_ip = ?, hostname = ?, service_port = ?, 
                    box_type = ?, board_type = ?, cpu_type = ?, os_type = ?, 
                    resource_type = ?, cpu_arch = ?, gpu = ?, status = ?, updated_at = ?
                WHERE box_id = ? AND slot_id = ? AND cpu_id = ?
            )");
            update.bind(1, srio_id);
            update.bind(2, host_ip);
            update.bind(3, hostname);
            update.bind(4, service_port);
            update.bind(5, box_type);
            update.bind(6, board_type);
            update.bind(7, cpu_type);
            update.bind(8, os_type);
            update.bind(9, resource_type);
            update.bind(10, cpu_arch);
            update.bind(11, gpu_json);
            update.bind(12, "online");
            update.bind(13, timestamp);
            update.bind(14, box_id);
            update.bind(15, slot_id);
            update.bind(16, cpu_id);
            update.exec();
        } else { // Node does not exist, insert it
            SQLite::Statement insert(*db_, R"(
                INSERT INTO node (box_id, slot_id, cpu_id, srio_id, host_ip, hostname, service_port, 
                                box_type, board_type, cpu_type, os_type, resource_type, cpu_arch, 
                                gpu, status, created_at, updated_at)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            )");
            insert.bind(1, box_id);
            insert.bind(2, slot_id);
            insert.bind(3, cpu_id);
            insert.bind(4, srio_id);
            insert.bind(5, host_ip);
            insert.bind(6, hostname);
            insert.bind(7, service_port);
            insert.bind(8, box_type);
            insert.bind(9, board_type);
            insert.bind(10, cpu_type);
            insert.bind(11, os_type);
            insert.bind(12, resource_type);
            insert.bind(13, cpu_arch);
            insert.bind(14, gpu_json);
            insert.bind(15, "online");
            insert.bind(16, timestamp);
            insert.bind(17, timestamp);
            insert.exec();
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving node: " << e.what() << std::endl;
        return false;
    }
}

// 根据box_id, slot_id和cpu_id获取节点信息
nlohmann::json DatabaseManager::getNode(int box_id, int slot_id, int cpu_id) {
    if (!db_) {
        std::cerr << "Database connection not initialized in getNode." << std::endl;
        return nlohmann::json::object();
    }
    
    try {
        SQLite::Statement query(*db_, R"(
            SELECT id, box_id, slot_id, cpu_id, srio_id, host_ip, hostname, service_port, 
                   box_type, board_type, cpu_type, os_type, resource_type, cpu_arch, 
                   gpu, status, created_at, updated_at 
            FROM node 
            WHERE box_id = ? AND slot_id = ? AND cpu_id = ?
        )");
        query.bind(1, box_id);
        query.bind(2, slot_id);
        query.bind(3, cpu_id);
        
        if (query.executeStep()) {
            nlohmann::json node;
            node["id"] = query.getColumn(0).getInt();
            node["box_id"] = query.getColumn(1).getInt();
            node["slot_id"] = query.getColumn(2).getInt();
            node["cpu_id"] = query.getColumn(3).getInt();
            node["srio_id"] = query.getColumn(4).getInt();
            node["host_ip"] = query.getColumn(5).getString();
            node["hostname"] = query.getColumn(6).getString();
            node["service_port"] = query.getColumn(7).getInt();
            node["box_type"] = query.getColumn(8).getString();
            node["board_type"] = query.getColumn(9).getString();
            node["cpu_type"] = query.getColumn(10).getString();
            node["os_type"] = query.getColumn(11).getString();
            node["resource_type"] = query.getColumn(12).getString();
            node["cpu_arch"] = query.getColumn(13).getString();
            
            // 解析GPU JSON字符串为JSON数组
            std::string gpu_json = query.getColumn(14).getString();
            try {
                node["gpu"] = nlohmann::json::parse(gpu_json);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing GPU JSON: " << e.what() << std::endl;
                node["gpu"] = nlohmann::json::array();
            }
            
            node["status"] = query.getColumn(15).getString();
            node["created_at"] = query.getColumn(16).getInt64();
            node["updated_at"] = query.getColumn(17).getInt64();
            
            return node;
        }
        return nlohmann::json::object(); // 返回空对象表示未找到
    } catch (const std::exception& e) {
        std::cerr << "Error getting node: " << e.what() << std::endl;
        return nlohmann::json::object();
    }
}

// 根据host_ip获取节点信息
nlohmann::json DatabaseManager::getNodeByhost_ip(const std::string& host_ip) {
    if (!db_) {
        std::cerr << "Database connection not initialized in getNodeByhost_ip." << std::endl;
        return nlohmann::json::object();
    }
    
    try {
        SQLite::Statement query(*db_, R"(
            SELECT id, box_id, slot_id, cpu_id, srio_id, host_ip, hostname, service_port, 
                   box_type, board_type, cpu_type, os_type, resource_type, cpu_arch, 
                   gpu, status, created_at, updated_at 
            FROM node 
            WHERE host_ip = ?
        )");
        query.bind(1, host_ip);
        
        if (query.executeStep()) {
            nlohmann::json node;
            node["id"] = query.getColumn(0).getInt();
            node["box_id"] = query.getColumn(1).getInt();
            node["slot_id"] = query.getColumn(2).getInt();
            node["cpu_id"] = query.getColumn(3).getInt();
            node["srio_id"] = query.getColumn(4).getInt();
            node["host_ip"] = query.getColumn(5).getString();
            node["hostname"] = query.getColumn(6).getString();
            node["service_port"] = query.getColumn(7).getInt();
            node["box_type"] = query.getColumn(8).getString();
            node["board_type"] = query.getColumn(9).getString();
            node["cpu_type"] = query.getColumn(10).getString();
            node["os_type"] = query.getColumn(11).getString();
            node["resource_type"] = query.getColumn(12).getString();
            node["cpu_arch"] = query.getColumn(13).getString();
            
            // 解析GPU JSON字符串为JSON数组
            std::string gpu_json = query.getColumn(14).getString();
            try {
                node["gpu"] = nlohmann::json::parse(gpu_json);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing GPU JSON: " << e.what() << std::endl;
                node["gpu"] = nlohmann::json::array();
            }
            
            node["status"] = query.getColumn(15).getString();
            node["created_at"] = query.getColumn(16).getInt64();
            node["updated_at"] = query.getColumn(17).getInt64();
            
            return node;
        }
        return nlohmann::json::object(); // 返回空对象表示未找到
    } catch (const std::exception& e) {
        std::cerr << "Error getting node by host_ip: " << e.what() << std::endl;
        return nlohmann::json::object();
    }
}

// 获取所有节点信息
nlohmann::json DatabaseManager::getAllNodes() {
    if (!db_) {
        std::cerr << "Database connection not initialized in getAllNodes." << std::endl;
        return nlohmann::json::array();
    }
    
    try {
        nlohmann::json result = nlohmann::json::array();
        
        SQLite::Statement query(*db_, R"(
            SELECT id, box_id, slot_id, cpu_id, srio_id, host_ip, hostname, service_port, 
                   box_type, board_type, cpu_type, os_type, resource_type, cpu_arch, 
                   gpu, status, created_at, updated_at 
            FROM node 
            ORDER BY box_id, slot_id, cpu_id
        )");
        
        while (query.executeStep()) {
            nlohmann::json node;
            node["id"] = query.getColumn(0).getInt();
            node["box_id"] = query.getColumn(1).getInt();
            node["slot_id"] = query.getColumn(2).getInt();
            node["cpu_id"] = query.getColumn(3).getInt();
            node["srio_id"] = query.getColumn(4).getInt();
            node["host_ip"] = query.getColumn(5).getString();
            node["hostname"] = query.getColumn(6).getString();
            node["service_port"] = query.getColumn(7).getInt();
            node["box_type"] = query.getColumn(8).getString();
            node["board_type"] = query.getColumn(9).getString();
            node["cpu_type"] = query.getColumn(10).getString();
            node["os_type"] = query.getColumn(11).getString();
            node["resource_type"] = query.getColumn(12).getString();
            node["cpu_arch"] = query.getColumn(13).getString();
            node["status"] = query.getColumn(14).getString();
            
            // 解析GPU JSON字符串为JSON数组
            std::string gpu_json = query.getColumn(14).getString();
            try {
                node["gpu"] = nlohmann::json::parse(gpu_json);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing GPU JSON: " << e.what() << std::endl;
                node["gpu"] = nlohmann::json::array();
            }
            
            node["status"] = query.getColumn(15).getString();
            node["created_at"] = query.getColumn(16).getInt64();
            node["updated_at"] = query.getColumn(17).getInt64();
            
            result.push_back(node);
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Error getting all nodes: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取所有node信息及其最新的metrics
nlohmann::json DatabaseManager::getNodesWithLatestMetrics() {
    if (!db_) {
        std::cerr << "Database connection not initialized in getNodesWithLatestMetrics." << std::endl;
        return nlohmann::json::array();
    }
    
    try {
        nlohmann::json result = nlohmann::json::array();
        
        // 首先获取所有node的基本信息
        SQLite::Statement query(*db_, R"(
            SELECT id, box_id, slot_id, cpu_id, srio_id, host_ip, hostname, service_port, 
                   box_type, board_type, cpu_type, os_type, resource_type, cpu_arch, 
                   gpu, status, created_at, updated_at 
            FROM node
            ORDER BY box_id, slot_id, cpu_id
        )");
        
        while (query.executeStep()) {            
            nlohmann::json node;
            node["id"] = query.getColumn(0).getInt();
            node["box_id"] = query.getColumn(1).getInt();
            node["slot_id"] = query.getColumn(2).getInt();
            node["cpu_id"] = query.getColumn(3).getInt();
            node["srio_id"] = query.getColumn(4).getInt();
            node["host_ip"] = query.getColumn(5).getString();
            node["hostname"] = query.getColumn(6).getString();
            node["service_port"] = query.getColumn(7).getInt();
            node["box_type"] = query.getColumn(8).getString();
            node["board_type"] = query.getColumn(9).getString();
            node["cpu_type"] = query.getColumn(10).getString();
            node["os_type"] = query.getColumn(11).getString();
            node["resource_type"] = query.getColumn(12).getString();
            node["cpu_arch"] = query.getColumn(13).getString();
            // 解析GPU JSON字符串为JSON数组
            std::string gpu_json = query.getColumn(14).getString();
            try {
                node["gpu"] = nlohmann::json::parse(gpu_json);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing GPU JSON: " << e.what() << std::endl;
                node["gpu"] = nlohmann::json::array();
            }
            node["status"] = query.getColumn(15).getString();
            node["created_at"] = query.getColumn(16).getInt64();
            node["updated_at"] = query.getColumn(17).getInt64();
            
            std::string host_ip = node["host_ip"];
            // 获取最新的CPU metrics
            SQLite::Statement cpu_query(*db_, R"(
                SELECT timestamp, usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count 
                FROM node_cpu_metrics 
                WHERE host_ip = ? 
                ORDER BY timestamp DESC LIMIT 1
            )");
            cpu_query.bind(1, host_ip);
            
            if (cpu_query.executeStep()) {
                nlohmann::json cpu_metrics;
                cpu_metrics["timestamp"] = cpu_query.getColumn(0).getInt64();
                cpu_metrics["usage_percent"] = cpu_query.getColumn(1).getDouble();
                cpu_metrics["load_avg_1m"] = cpu_query.getColumn(2).getDouble();
                cpu_metrics["load_avg_5m"] = cpu_query.getColumn(3).getDouble();
                cpu_metrics["load_avg_15m"] = cpu_query.getColumn(4).getDouble();
                cpu_metrics["core_count"] = cpu_query.getColumn(5).getInt();
                node["latest_cpu_metrics"] = cpu_metrics;
            } else {
                node["latest_cpu_metrics"] = nlohmann::json::object();
            }
            
            // 获取最新的Memory metrics
            SQLite::Statement mem_query(*db_, R"(
                SELECT timestamp, total, used, free, usage_percent 
                FROM node_memory_metrics 
                WHERE host_ip = ? 
                ORDER BY timestamp DESC LIMIT 1
            )");
            mem_query.bind(1, host_ip);
            
            if (mem_query.executeStep()) {
                nlohmann::json memory_metrics;
                memory_metrics["timestamp"] = mem_query.getColumn(0).getInt64();
                memory_metrics["total"] = mem_query.getColumn(1).getInt64();
                memory_metrics["used"] = mem_query.getColumn(2).getInt64();
                memory_metrics["free"] = mem_query.getColumn(3).getInt64();
                memory_metrics["usage_percent"] = mem_query.getColumn(4).getDouble();
                node["latest_memory_metrics"] = memory_metrics;
            } else {
                node["latest_memory_metrics"] = nlohmann::json::object();
            }
            
            // 获取最新的Disk metrics
            SQLite::Statement disk_query(*db_, R"(
                SELECT id, timestamp, disk_count 
                FROM node_disk_metrics 
                WHERE host_ip = ? 
                ORDER BY timestamp DESC LIMIT 1
            )");
            disk_query.bind(1, host_ip);
            
            if (disk_query.executeStep()) {
                nlohmann::json disk_metrics;
                long long slot_disk_metrics_id = disk_query.getColumn(0).getInt64();
                disk_metrics["timestamp"] = disk_query.getColumn(1).getInt64();
                disk_metrics["disk_count"] = disk_query.getColumn(2).getInt();
                
                // 获取磁盘详细信息
                SQLite::Statement disk_usage_query(*db_, R"(
                    SELECT device, mount_point, total, used, free, usage_percent 
                    FROM node_disk_usage 
                    WHERE slot_disk_metrics_id = ?
                )");
                disk_usage_query.bind(1, static_cast<int64_t>(slot_disk_metrics_id));
                
                nlohmann::json disks = nlohmann::json::array();
                while (disk_usage_query.executeStep()) {
                    nlohmann::json disk;
                    disk["device"] = disk_usage_query.getColumn(0).getString();
                    disk["mount_point"] = disk_usage_query.getColumn(1).getString();
                    disk["total"] = disk_usage_query.getColumn(2).getInt64();
                    disk["used"] = disk_usage_query.getColumn(3).getInt64();
                    disk["free"] = disk_usage_query.getColumn(4).getInt64();
                    disk["usage_percent"] = disk_usage_query.getColumn(5).getDouble();
                    disks.push_back(disk);
                }
                disk_metrics["disks"] = disks;
                node["latest_disk_metrics"] = disk_metrics;
            } else {
                node["latest_disk_metrics"] = nlohmann::json::object();
            }
            
            // 获取最新的Network metrics
            SQLite::Statement net_query(*db_, R"(
                SELECT id, timestamp, network_count 
                FROM node_network_metrics 
                WHERE host_ip = ? 
                ORDER BY timestamp DESC LIMIT 1
            )");
            net_query.bind(1, host_ip);
            
            if (net_query.executeStep()) {
                nlohmann::json network_metrics;
                long long slot_network_metrics_id = net_query.getColumn(0).getInt64();
                network_metrics["timestamp"] = net_query.getColumn(1).getInt64();
                network_metrics["network_count"] = net_query.getColumn(2).getInt();
                
                // 获取网络接口详细信息
                SQLite::Statement net_usage_query(*db_, R"(
                    SELECT interface, rx_bytes, tx_bytes, rx_packets, tx_packets, rx_errors, tx_errors 
                    FROM node_network_usage 
                    WHERE slot_network_metrics_id = ?
                )");
                net_usage_query.bind(1, static_cast<int64_t>(slot_network_metrics_id));
                
                nlohmann::json networks = nlohmann::json::array();
                while (net_usage_query.executeStep()) {
                    nlohmann::json network;
                    network["interface"] = net_usage_query.getColumn(0).getString();
                    network["rx_bytes"] = net_usage_query.getColumn(1).getInt64();
                    network["tx_bytes"] = net_usage_query.getColumn(2).getInt64();
                    network["rx_packets"] = net_usage_query.getColumn(3).getInt64();
                    network["tx_packets"] = net_usage_query.getColumn(4).getInt64();
                    network["rx_errors"] = net_usage_query.getColumn(5).getInt();
                    network["tx_errors"] = net_usage_query.getColumn(6).getInt();
                    networks.push_back(network);
                }
                network_metrics["networks"] = networks;
                node["latest_network_metrics"] = network_metrics;
            } else {
                node["latest_network_metrics"] = nlohmann::json::object();
            }
            
            // 获取最新的Docker metrics
            SQLite::Statement docker_query(*db_, R"(
                SELECT id, timestamp, container_count, running_count, paused_count, stopped_count 
                FROM node_docker_metrics 
                WHERE host_ip = ? 
                ORDER BY timestamp DESC LIMIT 1
            )");
            docker_query.bind(1, host_ip);
            
            if (docker_query.executeStep()) {
                nlohmann::json docker_metrics;
                long long slot_docker_metric_id = docker_query.getColumn(0).getInt64();
                docker_metrics["timestamp"] = docker_query.getColumn(1).getInt64();
                docker_metrics["container_count"] = docker_query.getColumn(2).getInt();
                docker_metrics["running_count"] = docker_query.getColumn(3).getInt();
                docker_metrics["paused_count"] = docker_query.getColumn(4).getInt();
                docker_metrics["stopped_count"] = docker_query.getColumn(5).getInt();
                
                // 获取容器详细信息
                SQLite::Statement container_query(*db_, R"(
                    SELECT container_id, name, image, status, cpu_percent, memory_usage 
                    FROM node_docker_containers 
                    WHERE slot_docker_metric_id = ?
                )");
                container_query.bind(1, static_cast<int64_t>(slot_docker_metric_id));
                
                nlohmann::json containers = nlohmann::json::array();
                while (container_query.executeStep()) {
                    nlohmann::json container;
                    container["id"] = container_query.getColumn(0).getString();
                    container["name"] = container_query.getColumn(1).getString();
                    container["image"] = container_query.getColumn(2).getString();
                    container["status"] = container_query.getColumn(3).getString();
                    container["cpu_percent"] = container_query.getColumn(4).getDouble();
                    container["memory_usage"] = container_query.getColumn(5).getInt64();
                    containers.push_back(container);
                }
                docker_metrics["containers"] = containers;
                node["latest_docker_metrics"] = docker_metrics;
            } else {
                node["latest_docker_metrics"] = nlohmann::json::object();
            }
            
            // 获取最新的GPU metrics
            SQLite::Statement gpu_query(*db_, R"(
                SELECT id, timestamp, gpu_count 
                FROM node_gpu_metrics 
                WHERE host_ip = ? 
                ORDER BY timestamp DESC LIMIT 1
            )");
            gpu_query.bind(1, host_ip);
            
            if (gpu_query.executeStep()) {
                nlohmann::json gpu_metrics;
                long long slot_gpu_metrics_id = gpu_query.getColumn(0).getInt64();
                gpu_metrics["timestamp"] = gpu_query.getColumn(1).getInt64();
                gpu_metrics["gpu_count"] = gpu_query.getColumn(2).getInt();
                
                // 获取GPU详细信息
                SQLite::Statement gpu_usage_query(*db_, R"(
                    SELECT gpu_index, name, compute_usage, mem_usage, mem_used, mem_total, temperature, voltage, current, power 
                    FROM node_gpu_usage 
                    WHERE slot_gpu_metrics_id = ?
                )");
                gpu_usage_query.bind(1, static_cast<int64_t>(slot_gpu_metrics_id));
                
                nlohmann::json gpus = nlohmann::json::array();
                while (gpu_usage_query.executeStep()) {
                    nlohmann::json gpu;
                    gpu["index"] = gpu_usage_query.getColumn(0).getInt();
                    gpu["name"] = gpu_usage_query.getColumn(1).getString();
                    gpu["compute_usage"] = gpu_usage_query.getColumn(2).getDouble();
                    gpu["mem_usage"] = gpu_usage_query.getColumn(3).getDouble();
                    gpu["mem_used"] = gpu_usage_query.getColumn(4).getInt64();
                    gpu["mem_total"] = gpu_usage_query.getColumn(5).getInt64();
                    gpu["temperature"] = gpu_usage_query.getColumn(6).getDouble();
                    gpu["voltage"] = gpu_usage_query.getColumn(7).getDouble();
                    gpu["current"] = gpu_usage_query.getColumn(8).getDouble();
                    gpu["power"] = gpu_usage_query.getColumn(9).getDouble();
                    gpus.push_back(gpu);
                }
                gpu_metrics["gpus"] = gpus;
                node["latest_gpu_metrics"] = gpu_metrics;
            } else {
                node["latest_gpu_metrics"] = nlohmann::json::object();
            }
            
            result.push_back(node);
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Error getting nodes with latest metrics: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// Slot Metrics Methods Implementation

// 保存slot CPU指标
bool DatabaseManager::saveNodeCpuMetrics(const std::string& host_ip,
                                         long long timestamp, const nlohmann::json& cpu_data) {
    try {
        // 检查必要字段
        if (!cpu_data.contains("usage_percent") || !cpu_data.contains("load_avg_1m") ||
            !cpu_data.contains("load_avg_5m") || !cpu_data.contains("load_avg_15m") ||
            !cpu_data.contains("core_count")) {
            return false;
        }

        // 插入CPU指标
        SQLite::Statement insert(*db_,
            "INSERT INTO node_cpu_metrics (host_ip, timestamp, usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)");
        insert.bind(1, host_ip);
        insert.bind(2, static_cast<int64_t>(timestamp));
        insert.bind(3, cpu_data["usage_percent"].get<double>());
        insert.bind(4, cpu_data["load_avg_1m"].get<double>());
        insert.bind(5, cpu_data["load_avg_5m"].get<double>());
        insert.bind(6, cpu_data["load_avg_15m"].get<double>());
        insert.bind(7, cpu_data["core_count"].get<int>());
        insert.exec();

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save node CPU metrics error: " << e.what() << std::endl;
        return false;
    }
}

// 保存slot内存指标
bool DatabaseManager::saveNodeMemoryMetrics(const std::string& host_ip,
                                            long long timestamp, const nlohmann::json& memory_data) {
    try {
        // 检查必要字段
        if (!memory_data.contains("total") || !memory_data.contains("used") ||
            !memory_data.contains("free") || !memory_data.contains("usage_percent")) {
            return false;
        }

        // 插入内存指标
        SQLite::Statement insert(*db_,
            "INSERT INTO node_memory_metrics (host_ip, timestamp, total, used, free, usage_percent) "
            "VALUES (?, ?, ?, ?, ?, ?)");
        insert.bind(1, host_ip);
        insert.bind(2, static_cast<int64_t>(timestamp));
        insert.bind(3, static_cast<int64_t>(memory_data["total"].get<unsigned long long>()));
        insert.bind(4, static_cast<int64_t>(memory_data["used"].get<unsigned long long>()));
        insert.bind(5, static_cast<int64_t>(memory_data["free"].get<unsigned long long>()));
        insert.bind(6, memory_data["usage_percent"].get<double>());
        insert.exec();

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save node memory metrics error: " << e.what() << std::endl;
        return false;
    }
}

// 保存slot磁盘指标
bool DatabaseManager::saveNodeDiskMetrics(const std::string& host_ip,
                                          long long timestamp, const nlohmann::json& disk_data) {
    try {
        if (!disk_data.is_array()) {
            return false;
        }

        int disk_count = disk_data.size();

        // 1. 插入slot_disk_metrics汇总信息
        SQLite::Statement insert_metrics(*db_,
            "INSERT INTO node_disk_metrics (host_ip, timestamp, disk_count) VALUES (?, ?, ?)");
        insert_metrics.bind(1, host_ip);
        insert_metrics.bind(2, static_cast<int64_t>(timestamp));
        insert_metrics.bind(3, disk_count);
        insert_metrics.exec();

        long long slot_disk_metrics_id = db_->getLastInsertRowid();

        // 2. 插入每个磁盘详细信息到slot_disk_usage
        for (const auto& disk : disk_data) {
            if (!disk.contains("device") || !disk.contains("mount_point") ||
                !disk.contains("total") || !disk.contains("used") ||
                !disk.contains("free") || !disk.contains("usage_percent")) {
                continue;
            }

            SQLite::Statement insert_usage(*db_,
                "INSERT INTO node_disk_usage (slot_disk_metrics_id, device, mount_point, total, used, free, usage_percent) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)");
            insert_usage.bind(1, static_cast<int64_t>(slot_disk_metrics_id));
            insert_usage.bind(2, disk["device"].get<std::string>());
            insert_usage.bind(3, disk["mount_point"].get<std::string>());
            insert_usage.bind(4, static_cast<int64_t>(disk["total"].get<unsigned long long>()));
            insert_usage.bind(5, static_cast<int64_t>(disk["used"].get<unsigned long long>()));
            insert_usage.bind(6, static_cast<int64_t>(disk["free"].get<unsigned long long>()));
            insert_usage.bind(7, disk["usage_percent"].get<double>());
            insert_usage.exec();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save node disk metrics error: " << e.what() << std::endl;
        return false;
    }
}

// 保存slot网络指标
bool DatabaseManager::saveNodeNetworkMetrics(const std::string& host_ip,
                                             long long timestamp, const nlohmann::json& network_data) {
    try {
        if (!network_data.is_array()) {
            return false;
        }

        int network_count = network_data.size();

        // 1. 插入slot_network_metrics汇总信息
        SQLite::Statement insert_metrics(*db_,
            "INSERT INTO node_network_metrics (host_ip, timestamp, network_count) VALUES (?, ?, ?)");
        insert_metrics.bind(1, host_ip);
        insert_metrics.bind(2, static_cast<int64_t>(timestamp));
        insert_metrics.bind(3, network_count);
        insert_metrics.exec();

        long long slot_network_metrics_id = db_->getLastInsertRowid();

        // 2. 插入每个网卡详细信息到slot_network_usage
        for (const auto& network : network_data) {
            if (!network.contains("interface") || !network.contains("rx_bytes") ||
                !network.contains("tx_bytes") || !network.contains("rx_packets") ||
                !network.contains("tx_packets") || !network.contains("rx_errors") ||
                !network.contains("tx_errors")) {
                continue;
            }

            SQLite::Statement insert_usage(*db_,
                "INSERT INTO node_network_usage (slot_network_metrics_id, interface, rx_bytes, tx_bytes, rx_packets, tx_packets, rx_errors, tx_errors) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
            insert_usage.bind(1, static_cast<int64_t>(slot_network_metrics_id));
            insert_usage.bind(2, network["interface"].get<std::string>());
            insert_usage.bind(3, static_cast<int64_t>(network["rx_bytes"].get<unsigned long long>()));
            insert_usage.bind(4, static_cast<int64_t>(network["tx_bytes"].get<unsigned long long>()));
            insert_usage.bind(5, static_cast<int64_t>(network["rx_packets"].get<unsigned long long>()));
            insert_usage.bind(6, static_cast<int64_t>(network["tx_packets"].get<unsigned long long>()));
            insert_usage.bind(7, network["rx_errors"].get<int>());
            insert_usage.bind(8, network["tx_errors"].get<int>());
            insert_usage.exec();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save node network metrics error: " << e.what() << std::endl;
        return false;
    }
}

// 保存slot GPU指标
bool DatabaseManager::saveNodeGpuMetrics(const std::string& host_ip,
                                         long long timestamp, const nlohmann::json& gpu_data) {
    try {
        if (!gpu_data.is_array()) {
            return false;
        }

        int gpu_count = gpu_data.size();

        // 1. 插入slot_gpu_metrics汇总信息
        SQLite::Statement insert_metrics(*db_,
            "INSERT INTO node_gpu_metrics (host_ip, timestamp, gpu_count) VALUES (?, ?, ?)");
        insert_metrics.bind(1, host_ip);
        insert_metrics.bind(2, static_cast<int64_t>(timestamp));
        insert_metrics.bind(3, gpu_count);
        insert_metrics.exec();

        long long slot_gpu_metrics_id = db_->getLastInsertRowid();

        // 2. 插入每个GPU详细信息到slot_gpu_usage
        for (const auto& gpu : gpu_data) {
            if (!gpu.contains("index") || !gpu.contains("name") ||
                !gpu.contains("compute_usage") || !gpu.contains("mem_usage") ||
                !gpu.contains("mem_used") || !gpu.contains("mem_total") ||
                !gpu.contains("temperature") || !gpu.contains("voltage") ||
                !gpu.contains("current") || !gpu.contains("power")) {
                continue;
            }

            SQLite::Statement insert_usage(*db_,
                "INSERT INTO node_gpu_usage (slot_gpu_metrics_id, gpu_index, name, compute_usage, mem_usage, "
                "mem_used, mem_total, temperature, voltage, current, power) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            insert_usage.bind(1, static_cast<int64_t>(slot_gpu_metrics_id));
            insert_usage.bind(2, gpu["index"].get<int>());
            insert_usage.bind(3, gpu["name"].get<std::string>());
            insert_usage.bind(4, gpu["compute_usage"].get<double>());
            insert_usage.bind(5, gpu["mem_usage"].get<double>());
            insert_usage.bind(6, static_cast<int64_t>(gpu["mem_used"].get<unsigned long long>()));
            insert_usage.bind(7, static_cast<int64_t>(gpu["mem_total"].get<unsigned long long>()));
            insert_usage.bind(8, gpu["temperature"].get<double>());
            insert_usage.bind(9, gpu["voltage"].get<double>());
            insert_usage.bind(10, gpu["current"].get<double>());
            insert_usage.bind(11, gpu["power"].get<double>());
            insert_usage.exec();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save node gpu metrics error: " << e.what() << std::endl;
        return false;
    }
}

// 保存slot Docker指标
bool DatabaseManager::saveNodeDockerMetrics(const std::string& host_ip,
                                            long long timestamp, const nlohmann::json& docker_data) {
    try {
        // 检查必要字段
        if (!docker_data.contains("container_count") || !docker_data.contains("running_count") ||
            !docker_data.contains("paused_count") || !docker_data.contains("stopped_count") ||
            !docker_data.contains("containers")) {
            return false;
        }

        // 插入Docker指标
        SQLite::Statement insert(*db_,
            "INSERT INTO node_docker_metrics (host_ip, timestamp, container_count, running_count, paused_count, stopped_count) "
            "VALUES (?, ?, ?, ?, ?, ?)");
        insert.bind(1, host_ip);
        insert.bind(2, static_cast<int64_t>(timestamp));
        insert.bind(3, docker_data["container_count"].get<int>());
        insert.bind(4, docker_data["running_count"].get<int>());
        insert.bind(5, docker_data["paused_count"].get<int>());
        insert.bind(6, docker_data["stopped_count"].get<int>());
        insert.exec();

        // 获取插入的Docker指标ID
        long long slot_docker_metric_id = db_->getLastInsertRowid();

        // 遍历所有容器
        for (const auto& container : docker_data["containers"]) {
            // 检查必要字段
            if (!container.contains("id") || !container.contains("name") ||
                !container.contains("image") || !container.contains("status") ||
                !container.contains("cpu_percent") || !container.contains("memory_usage")) {
                continue;
            }

            // 插入容器信息
            SQLite::Statement insert_container(*db_,
                "INSERT INTO node_docker_containers (slot_docker_metric_id, container_id, name, image, status, cpu_percent, memory_usage) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)");
            insert_container.bind(1, static_cast<int64_t>(slot_docker_metric_id));
            insert_container.bind(2, container["id"].get<std::string>());
            insert_container.bind(3, container["name"].get<std::string>());
            insert_container.bind(4, container["image"].get<std::string>());
            insert_container.bind(5, container["status"].get<std::string>());
            insert_container.bind(6, container["cpu_percent"].get<double>());
            insert_container.bind(7, static_cast<int64_t>(container["memory_usage"].get<unsigned long long>()));
            insert_container.exec();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save node Docker metrics error: " << e.what() << std::endl;
        return false;
    }
}

// 获取slot CPU指标
nlohmann::json DatabaseManager::getNodeCpuMetrics(const std::string& host_ip, int limit) {
    try {
        nlohmann::json result = nlohmann::json::array();

        // 查询CPU指标
        SQLite::Statement query(*db_,
            "SELECT timestamp, usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count "
            "FROM node_cpu_metrics WHERE host_ip = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, host_ip);
        query.bind(2, limit);

        while (query.executeStep()) {
            nlohmann::json metric;
            metric["timestamp"] = query.getColumn(0).getInt64();
            metric["usage_percent"] = query.getColumn(1).getDouble();
            metric["load_avg_1m"] = query.getColumn(2).getDouble();
            metric["load_avg_5m"] = query.getColumn(3).getDouble();
            metric["load_avg_15m"] = query.getColumn(4).getDouble();
            metric["core_count"] = query.getColumn(5).getInt();

            result.push_back(metric);
        }

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get node CPU metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取slot内存指标
nlohmann::json DatabaseManager::getNodeMemoryMetrics(const std::string& host_ip, int limit) {
    try {
        nlohmann::json result = nlohmann::json::array();

        // 查询内存指标
        SQLite::Statement query(*db_,
            "SELECT timestamp, total, used, free, usage_percent "
            "FROM node_memory_metrics WHERE host_ip = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, host_ip);
        query.bind(2, limit);

        while (query.executeStep()) {
            nlohmann::json metric;
            metric["timestamp"] = query.getColumn(0).getInt64();
            metric["total"] = query.getColumn(1).getInt64();
            metric["used"] = query.getColumn(2).getInt64();
            metric["free"] = query.getColumn(3).getInt64();
            metric["usage_percent"] = query.getColumn(4).getDouble();

            result.push_back(metric);
        }

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get node memory metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取slot磁盘指标
nlohmann::json DatabaseManager::getNodeDiskMetrics(const std::string& host_ip, int limit) {
    try {
        nlohmann::json result = nlohmann::json::array();

        // 查询slot_disk_metrics
        SQLite::Statement query(*db_,
            "SELECT id, timestamp, disk_count FROM node_disk_metrics WHERE host_ip = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, host_ip);
        query.bind(2, limit);

        while (query.executeStep()) {
            nlohmann::json metric;
            long long slot_disk_metrics_id = query.getColumn(0).getInt64();
            metric["timestamp"] = query.getColumn(1).getInt64();
            metric["disk_count"] = query.getColumn(2).getInt();

            // 查询该时间点所有磁盘详细信息
            SQLite::Statement usage_query(*db_,
                "SELECT device, mount_point, total, used, free, usage_percent FROM node_disk_usage WHERE slot_disk_metrics_id = ?");
            usage_query.bind(1, static_cast<int64_t>(slot_disk_metrics_id));

            nlohmann::json disks = nlohmann::json::array();
            while (usage_query.executeStep()) {
                nlohmann::json disk;
                disk["device"] = usage_query.getColumn(0).getString();
                disk["mount_point"] = usage_query.getColumn(1).getString();
                disk["total"] = usage_query.getColumn(2).getInt64();
                disk["used"] = usage_query.getColumn(3).getInt64();
                disk["free"] = usage_query.getColumn(4).getInt64();
                disk["usage_percent"] = usage_query.getColumn(5).getDouble();
                disks.push_back(disk);
            }
            metric["disks"] = disks;
            result.push_back(metric);
        }

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get node disk metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取slot网络指标
nlohmann::json DatabaseManager::getNodeNetworkMetrics(const std::string& host_ip, int limit) {
    try {
        nlohmann::json result = nlohmann::json::array();

        // 查询slot_network_metrics
        SQLite::Statement query(*db_,
            "SELECT id, timestamp, network_count FROM node_network_metrics WHERE host_ip = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, host_ip);
        query.bind(2, limit);

        while (query.executeStep()) {
            nlohmann::json metric;
            long long slot_network_metrics_id = query.getColumn(0).getInt64();
            metric["timestamp"] = query.getColumn(1).getInt64();
            metric["network_count"] = query.getColumn(2).getInt();

            // 查询该时间点所有网卡详细信息
            SQLite::Statement usage_query(*db_,
                "SELECT interface, rx_bytes, tx_bytes, rx_packets, tx_packets, rx_errors, tx_errors FROM node_network_usage WHERE slot_network_metrics_id = ?");
            usage_query.bind(1, static_cast<int64_t>(slot_network_metrics_id));

            nlohmann::json networks = nlohmann::json::array();
            while (usage_query.executeStep()) {
                nlohmann::json network;
                network["interface"] = usage_query.getColumn(0).getString();
                network["rx_bytes"] = usage_query.getColumn(1).getInt64();
                network["tx_bytes"] = usage_query.getColumn(2).getInt64();
                network["rx_packets"] = usage_query.getColumn(3).getInt64();
                network["tx_packets"] = usage_query.getColumn(4).getInt64();
                network["rx_errors"] = usage_query.getColumn(5).getInt();
                network["tx_errors"] = usage_query.getColumn(6).getInt();
                networks.push_back(network);
            }
            metric["networks"] = networks;
            result.push_back(metric);
        }

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get node network metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取slot GPU指标
nlohmann::json DatabaseManager::getNodeGpuMetrics(const std::string& host_ip, int limit) {
    try {
        nlohmann::json result = nlohmann::json::array();

        // 查询slot_gpu_metrics
        SQLite::Statement query(*db_,
            "SELECT id, timestamp, gpu_count FROM node_gpu_metrics WHERE host_ip = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, host_ip);
        query.bind(2, limit);

        while (query.executeStep()) {
            nlohmann::json metric;
            long long slot_gpu_metrics_id = query.getColumn(0).getInt64();
            metric["timestamp"] = query.getColumn(1).getInt64();
            metric["gpu_count"] = query.getColumn(2).getInt();

            // 查询该时间点所有GPU详细信息
            SQLite::Statement usage_query(*db_,
                "SELECT gpu_index, name, compute_usage, mem_usage, mem_used, mem_total, temperature, voltage, current, power "
                "FROM node_gpu_usage WHERE slot_gpu_metrics_id = ?");
            usage_query.bind(1, static_cast<int64_t>(slot_gpu_metrics_id));

            nlohmann::json gpus = nlohmann::json::array();
            while (usage_query.executeStep()) {
                nlohmann::json gpu;
                gpu["index"] = usage_query.getColumn(0).getInt();
                gpu["name"] = usage_query.getColumn(1).getString();
                gpu["compute_usage"] = usage_query.getColumn(2).getDouble();
                gpu["mem_usage"] = usage_query.getColumn(3).getDouble();
                gpu["mem_used"] = usage_query.getColumn(4).getInt64();
                gpu["mem_total"] = usage_query.getColumn(5).getInt64();
                gpu["temperature"] = usage_query.getColumn(6).getDouble();
                gpu["voltage"] = usage_query.getColumn(7).getDouble();
                gpu["current"] = usage_query.getColumn(8).getDouble();
                gpu["power"] = usage_query.getColumn(9).getDouble();
                gpus.push_back(gpu);
            }
            metric["gpus"] = gpus;
            result.push_back(metric);
        }

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get node gpu metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取slot Docker指标
nlohmann::json DatabaseManager::getNodeDockerMetrics(const std::string& host_ip, int limit) {
    try {
        nlohmann::json result = nlohmann::json::array();

        // 查询Docker指标
        SQLite::Statement query(*db_,
            "SELECT id, timestamp, container_count, running_count, paused_count, stopped_count "
            "FROM node_docker_metrics WHERE host_ip = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, host_ip);
        query.bind(2, limit);

        while (query.executeStep()) {
            nlohmann::json metric;
            long long slot_docker_metric_id = query.getColumn(0).getInt64();

            metric["timestamp"] = query.getColumn(1).getInt64();
            metric["container_count"] = query.getColumn(2).getInt();
            metric["running_count"] = query.getColumn(3).getInt();
            metric["paused_count"] = query.getColumn(4).getInt();
            metric["stopped_count"] = query.getColumn(5).getInt();

            // 查询容器信息
            SQLite::Statement container_query(*db_,
                "SELECT container_id, name, image, status, cpu_percent, memory_usage "
                "FROM node_docker_containers WHERE slot_docker_metric_id = ?");
            container_query.bind(1, static_cast<int64_t>(slot_docker_metric_id));

            nlohmann::json containers = nlohmann::json::array();

            while (container_query.executeStep()) {
                nlohmann::json container;
                container["id"] = container_query.getColumn(0).getString();
                container["name"] = container_query.getColumn(1).getString();
                container["image"] = container_query.getColumn(2).getString();
                container["status"] = container_query.getColumn(3).getString();
                container["cpu_percent"] = container_query.getColumn(4).getDouble();
                container["memory_usage"] = container_query.getColumn(5).getInt64();

                containers.push_back(container);
            }

            metric["containers"] = containers;
            result.push_back(metric);
        }

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get node Docker metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 保存node资源使用情况
bool DatabaseManager::saveNodeResourceUsage(const nlohmann::json& resource_usage) {
    // 检查必要字段
    if (!resource_usage.contains("host_ip") || 
        !resource_usage.contains("timestamp") || !resource_usage.contains("resource")) {
        return false;
    }
    
    std::string host_ip = resource_usage["host_ip"];
    long long timestamp = resource_usage["timestamp"];
    const auto& resource = resource_usage["resource"];
    
    // 保存各类资源数据
    if (resource.contains("cpu")) {
        saveNodeCpuMetrics(host_ip, timestamp, resource["cpu"]);
    }
    if (resource.contains("memory")) {
        saveNodeMemoryMetrics(host_ip, timestamp, resource["memory"]);
    }
    if (resource.contains("disk")) {
        saveNodeDiskMetrics(host_ip, timestamp, resource["disk"]);
    }
    if (resource.contains("network")) {
        saveNodeNetworkMetrics(host_ip, timestamp, resource["network"]);
    }
    if (resource.contains("docker")) {
        saveNodeDockerMetrics(host_ip, timestamp, resource["docker"]);
    }
    if (resource.contains("gpu")) {
        saveNodeGpuMetrics(host_ip, timestamp, resource["gpu"]);
    }
    return true;
}