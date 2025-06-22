#include "tdengine_manager.h"
#include <iostream>
#include <regex>
#include <chrono>

// 引入 TDengine WebSocket 头文件
// #include "taosws.h"

// Helper to convert NodeInfo to JSON
nlohmann::json nodeInfoToJson(const NodeInfo& info) {
    nlohmann::json j;
    j["id"] = info.id;
    j["box_id"] = info.box_id;
    j["slot_id"] = info.slot_id;
    j["cpu_id"] = info.cpu_id;
    j["srio_id"] = info.srio_id;
    j["host_ip"] = info.host_ip;
    j["hostname"] = info.hostname;
    j["service_port"] = info.service_port;
    j["box_type"] = info.box_type;
    j["board_type"] = info.board_type;
    j["cpu_type"] = info.cpu_type;
    j["os_type"] = info.os_type;
    j["resource_type"] = info.resource_type;
    j["cpu_arch"] = info.cpu_arch;
    j["gpu"] = info.gpu;
    j["status"] = info.status;
    j["created_at"] = info.created_at;
    j["updated_at"] = info.updated_at;
    return j;
}

// 构造函数：使用 WebSocket 连接到 TDengine
TDengineManager::TDengineManager(const std::string& host, const std::string& user, const std::string& password, const std::string& db, int port) {
    ws_enable_log("debug");
    
    std::string dsn = "ws://" + user + ":" + password + "@" + host + ":" + std::to_string(port) + "/" + db;
    // std::string dsn = "ws://" + host + ":" + std::to_string(port);
    
    taos_ = ws_connect(dsn.c_str());
    
    if (taos_ == nullptr) {
        std::cerr << "Failed to connect to TDengine via WebSocket DSN: " << dsn << std::endl;
        std::cerr << "ErrCode: 0x" << std::hex << ws_errno(NULL) << ", ErrMessage: " << ws_errstr(NULL) << std::endl;
        connected_ = false;
    } else {
        std::cout << "Successfully connected to TDengine via WebSocket: " << dsn << std::endl;
        connected_ = true;
        createTables(); // 连接成功后创建表
    }
}

// 析构函数：断开连接
TDengineManager::~TDengineManager() {
    stopNodeStatusMonitor();
    if (taos_) {
        ws_close(taos_);
        std::cout << "Disconnected from TDengine WebSocket." << std::endl;
    }
}

bool TDengineManager::isConnected() const {
    return connected_;
}

void TDengineManager::executeQuery(const std::string& sql) {
    if (!connected_) {
        std::cerr << "Cannot execute query: not connected to TDengine." << std::endl;
        return;
    }
    std::cout << "Executing SQL: " << sql << std::endl;

    WS_RES* res = ws_query(taos_, sql.c_str());
    if (ws_errno(res) != 0) {
        std::cerr << "TDengine WebSocket query failed: " << ws_errstr(res) << std::endl;
    }
    ws_free_result(res);
}

std::string TDengineManager::sanitizeForTableName(const std::string& input) {
    return std::regex_replace(input, std::regex("[^a-zA-Z0-9_]"), "_");
}

void TDengineManager::createTables() {
    // Note: With DSN connection, USE database is often not needed
    executeQuery("CREATE DATABASE IF NOT EXISTS node_metrics;");

    executeQuery(R"(
        CREATE STABLE IF NOT EXISTS node_metrics.server_metrics (
            ts TIMESTAMP,
            cpu_usage_percent FLOAT,
            load_avg_1m FLOAT,
            load_avg_5m FLOAT,
            load_avg_15m FLOAT,
            core_count INT,
            core_allocated INT,
            cpu_temperature FLOAT,
            cpu_power FLOAT,
            mem_total BIGINT,
            mem_used BIGINT,
            mem_free BIGINT,
            mem_usage_percent FLOAT
        ) TAGS (
            host_ip BINARY(16),
            box_id INT,
            slot_id INT
        );
    )");

    executeQuery(R"(
        CREATE STABLE IF NOT EXISTS node_metrics.gpu_metrics (
            ts TIMESTAMP,
            compute_usage FLOAT,
            mem_usage FLOAT,
            mem_used BIGINT,
            mem_total BIGINT,
            temperature FLOAT,
            power FLOAT
        ) TAGS (
            host_ip BINARY(16),
            box_id INT,
            slot_id INT,
            gpu_index INT,
            gpu_name BINARY(64)
        );
    )");

    executeQuery(R"(
        CREATE STABLE IF NOT EXISTS node_metrics.disk_metrics (
            ts TIMESTAMP,
            total BIGINT,
            used BIGINT,
            free BIGINT,
            usage_percent FLOAT
        ) TAGS (
            host_ip BINARY(16),
            box_id INT,
            slot_id INT,
            device BINARY(32),
            mount_point BINARY(256)
        );
    )");

    executeQuery(R"(
        CREATE STABLE IF NOT EXISTS node_metrics.net_metrics (
            ts TIMESTAMP,
            rx_bytes BIGINT,
            tx_bytes BIGINT,
            rx_packets BIGINT,
            tx_packets BIGINT,
            rx_errors INT,
            tx_errors INT
        ) TAGS (
            host_ip BINARY(16),
            box_id INT,
            slot_id INT,
            interface BINARY(32)
        );
    )");

    executeQuery(R"(
        CREATE STABLE IF NOT EXISTS node_metrics.container_metrics (
            ts TIMESTAMP,
            gpu_index INT,
            status BINARY(16),
            cpu_load FLOAT,
            memory_used BIGINT,
            memory_limit BIGINT,
            network_tx BIGINT,
            network_rx BIGINT
        ) TAGS (
            host_ip BINARY(16),
            box_id INT,
            slot_id INT,
            container_id BINARY(64),
            container_name BINARY(64),
            instance_id BINARY(64),
            uuid BINARY(36)
        );
    )");
}

bool TDengineManager::saveMetrics(const nlohmann::json& resource_usage) {
    if (!connected_) {
        std::cerr << "Cannot save metrics: not connected to TDengine." << std::endl;
        return false;
    }

    if (!resource_usage.contains("host_ip")) {
        std::cerr << "Metrics JSON must contain host_ip and timestamp." << std::endl;
        return false;
    }

    // 通过ip获取box_id slot_id cpu_id
    nlohmann::json node_info = getNodeInfoByHostIp(resource_usage["host_ip"].get<std::string>());
    if (node_info.is_null() || !node_info.contains("box_id") || !node_info.contains("slot_id") || !node_info.contains("cpu_id")) {
        std::cerr << "Could not find node info for host_ip: " << resource_usage["host_ip"].get<std::string>() << std::endl;
        return false;
    }
    int box_id = node_info["box_id"].get<int>();
    int slot_id = node_info["slot_id"].get<int>();
    int cpu_id = node_info["cpu_id"].get<int>();
    std::string host_ip = resource_usage["host_ip"].get<std::string>();

    const auto& resource = resource_usage.value("resource", nlohmann::json::object());
    const auto& component = resource_usage.value("component", nlohmann::json::array());

    std::cout << "box_id: " << box_id << ", slot_id: " << slot_id << ", cpu_id: " << cpu_id << ", host_ip: " << host_ip << std::endl;

    // 1. Save server metrics (CPU and Memory)
    if (resource.contains("cpu") && resource.contains("memory")) {
        const auto& cpu = resource["cpu"];
        const auto& mem = resource["memory"];
        std::string sub_table_name = "server_" + std::to_string(box_id) + "_" + std::to_string(slot_id) + "_" + std::to_string(cpu_id);

        std::string sql = "INSERT INTO " + sub_table_name +
                          " USING node_metrics.server_metrics TAGS ('" + host_ip + "', " + std::to_string(box_id) + ", " + std::to_string(slot_id) + ")" +
                          " VALUES (NOW, " +
                          std::to_string(cpu.value("usage_percent", 0.0)) + ", " +
                          std::to_string(cpu.value("load_avg_1m", 0.0)) + ", " +
                          std::to_string(cpu.value("load_avg_5m", 0.0)) + ", " +
                          std::to_string(cpu.value("load_avg_15m", 0.0)) + ", " +
                          std::to_string(cpu.value("core_count", 0)) + ", " +
                          std::to_string(cpu.value("core_allocated", 0)) + ", " +
                          std::to_string(cpu.value("temperature", 0.0)) + ", " +
                          std::to_string(cpu.value("power", 0.0)) + ", " +
                          std::to_string(mem.value("total", 0ULL)) + ", " +
                          std::to_string(mem.value("used", 0ULL)) + ", " +
                          std::to_string(mem.value("free", 0ULL)) + ", " +
                          std::to_string(mem.value("usage_percent", 0.0)) + ");";
        executeQuery(sql);
    }

    // 2. Save GPU metrics
    if (resource.contains("gpu")) {
        for (const auto& gpu : resource["gpu"]) {
            int gpu_index = gpu.value("index", -1);
            if (gpu_index == -1) continue;
            std::string gpu_name = gpu.value("name", "");
            std::string sub_table_name = "gpu_" + std::to_string(box_id) + "_" + std::to_string(slot_id) + "_" + std::to_string(gpu_index);

            std::string sql = "INSERT INTO " + sub_table_name +
                              " USING node_metrics.gpu_metrics TAGS ('" + host_ip + "', " + std::to_string(box_id) + ", " + std::to_string(slot_id) + ", " +
                              std::to_string(gpu_index) + ", '" + gpu_name + "')" +
                              " VALUES (NOW, " +
                              std::to_string(gpu.value("compute_usage", 0.0)) + ", " +
                              std::to_string(gpu.value("mem_usage", 0.0)) + ", " +
                              std::to_string(gpu.value("mem_used", 0ULL)) + ", " +
                              std::to_string(gpu.value("mem_total", 0ULL)) + ", " +
                              std::to_string(gpu.value("temperature", 0.0)) + ", " +
                              std::to_string(gpu.value("power", 0.0)) + ");";
            executeQuery(sql);
        }
    }

    // 3. Save Disk metrics
    if (resource.contains("disk")) {
        for (const auto& disk : resource["disk"]) {
            std::string device = disk.value("device", "");
            std::string mount_point = disk.value("mount_point", "");
            if (device.empty()) continue;
            std::string sub_table_name = "disk_" + std::to_string(box_id) + "_" + std::to_string(slot_id) + "_" + sanitizeForTableName(device);

            std::string sql = "INSERT INTO " + sub_table_name +
                              " USING node_metrics.disk_metrics TAGS ('" + host_ip + "', " + std::to_string(box_id) + ", " + std::to_string(slot_id) + ", '" +
                              device + "', '" + mount_point + "')" +
                              " VALUES (NOW, " +
                              std::to_string(disk.value("total", 0ULL)) + ", " +
                              std::to_string(disk.value("used", 0ULL)) + ", " +
                              std::to_string(disk.value("free", 0ULL)) + ", " +
                              std::to_string(disk.value("usage_percent", 0.0)) + ");";
            executeQuery(sql);
        }
    }

    // 4. Save Network metrics
    if (resource.contains("network")) {
        for (const auto& net : resource["network"]) {
            std::string interface = net.value("interface", "");
            if (interface.empty()) continue;
            std::string sub_table_name = "net_" + std::to_string(box_id) + "_" + std::to_string(slot_id) + "_" + sanitizeForTableName(interface);

            std::string sql = "INSERT INTO " + sub_table_name +
                              " USING node_metrics.net_metrics TAGS ('" + host_ip + "', " + std::to_string(box_id) + ", " + std::to_string(slot_id) + ", '" +
                              interface + "')" +
                              " VALUES (NOW, " +
                              std::to_string(net.value("rx_bytes", 0ULL)) + ", " +
                              std::to_string(net.value("tx_bytes", 0ULL)) + ", " +
                              std::to_string(net.value("rx_packets", 0ULL)) + ", " +
                              std::to_string(net.value("tx_packets", 0ULL)) + ", " +
                              std::to_string(net.value("rx_errors", 0)) + ", " +
                              std::to_string(net.value("tx_errors", 0)) + ");";
            executeQuery(sql);
        }
    }

    // 5. Save Container metrics
    if (component.is_array()) {
        for (const auto& container : component) {
            std::string container_id = container["config"].value("id", "");
            if (container_id.empty()) continue;
            
            std::string sub_table_name = "container_" + std::to_string(box_id) + "_" + std::to_string(slot_id) + "_" + sanitizeForTableName(container_id);
            std::string container_name = container["config"].value("name", "");
            std::string instance_id = container.value("instance_id", "");
            std::string uuid = container.value("uuid", "");
            const auto& resource = container["resource"];

            std::string sql = "INSERT INTO " + sub_table_name +
                              " USING node_metrics.container_metrics TAGS ('" + host_ip + "', " + std::to_string(box_id) + ", " + std::to_string(slot_id) + ", '" +
                              container_id + "', '" + container_name + "', '" + instance_id + "', '" + uuid + "')" +
                              " VALUES (NOW, " +
                              std::to_string(container.value("index", -1)) + ", '" +
                              container.value("state", "UNKNOWN") + "', " +
                              std::to_string(resource["cpu"].value("load", 0.0)) + ", " +
                              std::to_string(resource["memory"].value("mem_used", 0ULL)) + ", " +
                              std::to_string(resource["memory"].value("mem_limit", 0ULL)) + ", " +
                              std::to_string(resource["network"].value("tx", 0ULL)) + ", " +
                              std::to_string(resource["network"].value("rx", 0ULL)) + ");";
            executeQuery(sql);
        }
    }

    return true;
}

bool TDengineManager::updateNodeInfo(const nlohmann::json& node_info) {
    if (!node_info.contains("box_id") || !node_info.contains("slot_id") || !node_info.contains("cpu_id")) {
        std::cerr << "Node box_id, slot_id and cpu_id are required." << std::endl;
        return false;
    }

    NodeKey key{
        node_info["box_id"].get<int>(),
        node_info["slot_id"].get<int>(),
        node_info["cpu_id"].get<int>()
    };

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(node_cache_mutex_);

    auto it = node_info_cache_.find(key);
    NodeInfo* info_ptr;

    if (it != node_info_cache_.end()) {
        info_ptr = &it->second;
    } else {
        node_info_cache_[key] = NodeInfo{};
        info_ptr = &node_info_cache_[key];
        info_ptr->id = node_info_cache_.size();
        info_ptr->created_at = timestamp;
    }
    
    NodeInfo& info = *info_ptr;
    info.box_id = key.box_id;
    info.slot_id = key.slot_id;
    info.cpu_id = key.cpu_id;
    info.srio_id = node_info.value("srio_id", info.srio_id);
    info.host_ip = node_info.value("host_ip", info.host_ip);
    info.hostname = node_info.value("hostname", info.hostname);
    info.service_port = node_info.value("service_port", info.service_port);
    info.box_type = node_info.value("box_type", info.box_type);
    info.board_type = node_info.value("board_type", info.board_type);
    info.cpu_type = node_info.value("cpu_type", info.cpu_type);
    info.os_type = node_info.value("os_type", info.os_type);
    info.resource_type = node_info.value("resource_type", info.resource_type);
    info.cpu_arch = node_info.value("cpu_arch", info.cpu_arch);
    info.gpu = node_info.value("gpu", nlohmann::json::array());
    info.status = "online";
    info.updated_at = timestamp;

    if (!info.host_ip.empty()) {
        host_ip_to_key_map_[info.host_ip] = key;
    }

    return true;
}

nlohmann::json TDengineManager::getNodeInfo(int box_id, int slot_id, int cpu_id) {
    std::lock_guard<std::mutex> lock(node_cache_mutex_);
    NodeKey key{box_id, slot_id, cpu_id};
    auto it = node_info_cache_.find(key);
    if (it != node_info_cache_.end()) {
        return nodeInfoToJson(it->second);
    }
    return nlohmann::json::object();
}

nlohmann::json TDengineManager::getNodeInfoByHostIp(const std::string& host_ip) {
    std::lock_guard<std::mutex> lock(node_cache_mutex_);
    auto key_it = host_ip_to_key_map_.find(host_ip);
    if (key_it != host_ip_to_key_map_.end()) {
        auto it = node_info_cache_.find(key_it->second);
        if (it != node_info_cache_.end()) {
            return nodeInfoToJson(it->second);
        }
    }
    return nlohmann::json::object();
}

nlohmann::json TDengineManager::getAllNodesInfo() {
    std::lock_guard<std::mutex> lock(node_cache_mutex_);
    nlohmann::json result = nlohmann::json::array();
    for (const auto& pair : node_info_cache_) {
        result.push_back(nodeInfoToJson(pair.second));
    }
    return result;
}

void TDengineManager::startNodeStatusMonitor() {
    if (node_status_monitor_running_.load()) {
        return; 
    }
    node_status_monitor_running_.store(true);
    node_status_monitor_thread_ = std::make_unique<std::thread>(&TDengineManager::nodeStatusMonitorLoop, this);
    std::cout << "Node status monitor thread started." << std::endl;
}

void TDengineManager::stopNodeStatusMonitor() {
    if (node_status_monitor_running_.load()) {
        node_status_monitor_running_.store(false);
        if (node_status_monitor_thread_ && node_status_monitor_thread_->joinable()) {
            node_status_monitor_thread_->join();
        }
    }
}

void TDengineManager::nodeStatusMonitorLoop() {
    while (node_status_monitor_running_.load()) {
        try {
            auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            
            std::lock_guard<std::mutex> lock(node_cache_mutex_);
            for (auto& pair : node_info_cache_) {
                NodeInfo& info = pair.second;
                if (info.status == "online" && (now_epoch - info.updated_at) > 5) {
                    std::cout << "Node with host_ip " << info.host_ip << " is inactive. Setting status to offline." << std::endl;
                    info.status = "offline";
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in TDengineManager node status monitor: " << e.what() << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1)); 
    }
    std::cout << "TDengineManager node status monitor loop finished." << std::endl;
}

nlohmann::json TDengineManager::queryTDengine(const std::string& sql) {
    nlohmann::json result = nlohmann::json::array();
    if (!connected_) {
        std::cerr << "Cannot execute query: not connected to TDengine." << std::endl;
        return result;
    }

    WS_RES* res = ws_query(taos_, sql.c_str());
    if (ws_errno(res) != 0) {
        std::cerr << "TDengine WebSocket query failed for SQL: " << sql << std::endl;
        std::cerr << "Error: " << ws_errstr(res) << std::endl;
        ws_free_result(res);
        return result;
    }

    int num_fields = ws_field_count(res);
    const WS_FIELD* fields = ws_fetch_fields(res);

    WS_ROW row;
    while ((row = ws_fetch_row(res))) {
        nlohmann::json row_json;
        for (int i = 0; i < num_fields; ++i) {
            const char* value = (const char*)row[i];
            if (value) {
                switch (fields[i].type) {
                    case TSDB_DATA_TYPE_BOOL:
                        row_json[fields[i].name] = *(bool*)value;
                        break;
                    case TSDB_DATA_TYPE_TINYINT:
                        row_json[fields[i].name] = *(int8_t*)value;
                        break;
                    case TSDB_DATA_TYPE_SMALLINT:
                        row_json[fields[i].name] = *(int16_t*)value;
                        break;
                    case TSDB_DATA_TYPE_INT:
                        row_json[fields[i].name] = *(int32_t*)value;
                        break;
                    case TSDB_DATA_TYPE_BIGINT:
                        row_json[fields[i].name] = *(int64_t*)value;
                        break;
                    case TSDB_DATA_TYPE_FLOAT:
                        row_json[fields[i].name] = *(float*)value;
                        break;
                    case TSDB_DATA_TYPE_DOUBLE:
                        row_json[fields[i].name] = *(double*)value;
                        break;
                    case TSDB_DATA_TYPE_BINARY:
                    case TSDB_DATA_TYPE_NCHAR:
                        row_json[fields[i].name] = std::string(value, fields[i].bytes);
                        break;
                    case TSDB_DATA_TYPE_TIMESTAMP:
                         row_json[fields[i].name] = *(int64_t*)value;
                        break;
                    default:
                        row_json[fields[i].name] = value;
                        break;
                }
            } else {
                 row_json[fields[i].name] = nullptr;
            }
        }
        result.push_back(row_json);
    }

    ws_free_result(res);
    return result;
}

nlohmann::json TDengineManager::getNodesWithLatestMetrics() {
    nlohmann::json nodes = getAllNodesInfo();

    for (auto& node : nodes) {
        if (!node.contains("host_ip") || !node.contains("box_id") || !node.contains("slot_id") || !node.contains("cpu_id")) {
            continue;
        }

        std::string host_ip = node["host_ip"].get<std::string>();
        int box_id = node["box_id"].get<int>();
        int slot_id = node["slot_id"].get<int>();
        int cpu_id = node["cpu_id"].get<int>();

        // 1. Get latest server metrics
        std::string server_table_name = "server_" + std::to_string(box_id) + "_" + std::to_string(slot_id) + "_" + std::to_string(cpu_id);
        nlohmann::json server_metrics = queryTDengine("SELECT LAST_ROW(*) FROM node_metrics." + server_table_name);
        node["latest_server_metrics"] = server_metrics.empty() ? nlohmann::json::object() : server_metrics[0];

        // 2. Get latest GPU metrics
        std::string gpu_query_sql = "SELECT LAST_ROW(*) FROM node_metrics.gpu_metrics WHERE "
                                  "host_ip = '" + host_ip + "' AND box_id = " + std::to_string(box_id) + " AND slot_id = " + std::to_string(slot_id) +
                                  " GROUP BY gpu_index";
        node["latest_gpu_metrics"] = queryTDengine(gpu_query_sql);
        
        // 3. Get latest Disk metrics
        std::string disk_query_sql = "SELECT LAST_ROW(*) FROM node_metrics.disk_metrics WHERE "
                                   "host_ip = '" + host_ip + "' AND box_id = " + std::to_string(box_id) + " AND slot_id = " + std::to_string(slot_id) +
                                   " GROUP BY device, mount_point";
        node["latest_disk_metrics"] = queryTDengine(disk_query_sql);

        // 4. Get latest Network metrics
        std::string net_query_sql = "SELECT LAST_ROW(*) FROM node_metrics.net_metrics WHERE "
                                  "host_ip = '" + host_ip + "' AND box_id = " + std::to_string(box_id) + " AND slot_id = " + std::to_string(slot_id) +
                                  " GROUP BY interface";
        node["latest_network_metrics"] = queryTDengine(net_query_sql);

        // 5. Get latest Container metrics
        std::string container_query_sql = "SELECT LAST_ROW(*) FROM node_metrics.container_metrics WHERE "
                                        "host_ip = '" + host_ip + "' AND box_id = " + std::to_string(box_id) + " AND slot_id = " + std::to_string(slot_id) +
                                        " GROUP BY container_id";
        node["latest_container_metrics"] = queryTDengine(container_query_sql);
    }

    return nodes;
} 