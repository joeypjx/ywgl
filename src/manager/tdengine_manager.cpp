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
            uuid BINARY(36),
            component_index INT
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
                              container_id + "', '" + container_name + "', '" + instance_id + "', '" + uuid + "', " +
                              std::to_string(container.value("index", -1)) + ")" +
                              " VALUES (NOW, '" +
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
    int* lengths = ws_fetch_lengths(res);

    WS_ROW row;
    while ((row = ws_fetch_row_r(res, lengths))) {
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
                    case TSDB_DATA_TYPE_NCHAR: {
                        row_json[fields[i].name] = std::string(value, lengths[i]);
                        break;
                    }
                    case TSDB_DATA_TYPE_TIMESTAMP:
                         // TDengine时间戳是纳秒级的，转换为毫秒级
                         row_json[fields[i].name] = *(int64_t*)value / 1000000;
                        break;
                    default: {
                        row_json[fields[i].name] = std::string(value, lengths[i]);
                        break;
                    }
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
    /*
    nodes:
    [
            {
                "board_type": "GPU",
                "box_id": 1,
                "box_type": "计算I型",
                "cpu_arch": "aarch64",
                "cpu_id": 1,
                "cpu_type": " Phytium,D2000/8",
                "created_at": 1750038100,
                "gpu": [
                    {
                        "index": 0,
                        "name": " Iluvatar MR-V50A"
                    }
                ],
                "host_ip": "192.168.10.58",
                "hostname": "localhost.localdomain",
                "id": 1,
                "latest_cpu_metrics": {
                    "core_allocated": 0,
                    "core_count": 8,
                    "current": 2.0,
                    "load_avg_15m": 0.69,
                    "load_avg_1m": 1.12,
                    "load_avg_5m": 1.12,
                    "power": 23.8,
                    "temperature": 35.0,
                    "timestamp": 1750122089668,
                    "usage_percent": 12.83806343906511,
                    "voltage": 11.9
                },
                "latest_disk_metrics": {
                    "disk_count": 2,
                    "disks": [
                        {
                            "device": "/dev/mapper/klas-root",
                            "free": 6929584128,
                            "mount_point": "/",
                            "total": 107321753600,
                            "usage_percent": 93.54316911944309,
                            "used": 100392169472
                        },
                        {
                            "device": "/dev/mapper/klas-data",
                            "free": 34764398592,
                            "mount_point": "/data",
                            "total": 255149084672,
                            "usage_percent": 86.37486838853825,
                            "used": 220384686080
                        }
                    ],
                    "timestamp": 1750122089668
                },
                "latest_docker_metrics": {
                    "component": [],
                    "container_count": 0,
                    "paused_count": 0,
                    "running_count": 0,
                    "stopped_count": 0,
                    "timestamp": 1750122089668
                },
                "latest_gpu_metrics": {
                    "gpu_count": 1,
                    "gpus": [
                        {
                            "compute_usage": 0.0,
                            "current": 0.0,
                            "index": 0,
                            "mem_total": 17179869184,
                            "mem_usage": 1.0,
                            "mem_used": 119537664,
                            "name": " Iluvatar MR-V50A",
                            "power": 0.0,
                            "temperature": 0.0,
                            "voltage": 0.0
                        }
                    ],
                    "timestamp": 1750122089668
                },
                "latest_memory_metrics": {
                    "free": 12675907584,
                    "timestamp": 1750122089668,
                    "total": 15647768576,
                    "usage_percent": 18.992235075345736,
                    "used": 2971860992
                },
                "latest_network_metrics": {
                    "network_count": 5,
                    "networks": [
                        {
                            "interface": "bond0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0
                        },
                        {
                            "interface": "docker0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "tx_bytes": 716,
                            "tx_errors": 0,
                            "tx_packets": 8
                        },
                        {
                            "interface": "virbr0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0
                        },
                        {
                            "interface": "docker_gwbridge",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "tx_bytes": 446,
                            "tx_errors": 0,
                            "tx_packets": 5
                        },
                        {
                            "interface": "enaphyt4i0",
                            "rx_bytes": 3947210,
                            "rx_errors": 0,
                            "rx_packets": 40992,
                            "tx_bytes": 12865772,
                            "tx_errors": 0,
                            "tx_packets": 19010
                        }
                    ],
                    "timestamp": 1750122089668
                },
                "os_type": "Kylin Linux Advanced Server V10",
                "resource_type": "GPU I",
                "service_port": 23980,
                "slot_id": 1,
                "srio_id": 5,
                "status": "online",
                "updated_at": 1750122094
            }
        ]

    */
    nlohmann::json nodes = getAllNodesInfo();

    for (auto& node : nodes) {
        if (!node.contains("host_ip") || !node.contains("box_id") || !node.contains("slot_id") || !node.contains("cpu_id")) {
            continue;
        }

        std::string host_ip = node["host_ip"].get<std::string>();
        int box_id = node["box_id"].get<int>();
        int slot_id = node["slot_id"].get<int>();
        int cpu_id = node["cpu_id"].get<int>();

        // 1. Get latest server metrics and split into CPU and Memory metrics
        std::string server_table_name = "server_" + std::to_string(box_id) + "_" + std::to_string(slot_id) + "_" + std::to_string(cpu_id);
        nlohmann::json server_metrics = queryTDengine("SELECT LAST_ROW(*) FROM node_metrics." + server_table_name);
        
        if (!server_metrics.empty()) {
            const auto& server_data = server_metrics[0];
            
            // CPU metrics
            nlohmann::json cpu_metrics;
            cpu_metrics["core_allocated"] = server_data.value("last_row(core_allocated)", 0);
            cpu_metrics["core_count"] = server_data.value("last_row(core_count)", 0);
            cpu_metrics["current"] = 0.0; // 需要从其他地方获取
            cpu_metrics["load_avg_15m"] = server_data.value("last_row(load_avg_15m)", 0.0);
            cpu_metrics["load_avg_1m"] = server_data.value("last_row(load_avg_1m)", 0.0);
            cpu_metrics["load_avg_5m"] = server_data.value("last_row(load_avg_5m)", 0.0);
            cpu_metrics["power"] = server_data.value("last_row(cpu_power)", 0.0);
            cpu_metrics["temperature"] = server_data.value("last_row(cpu_temperature)", 0.0);
            cpu_metrics["timestamp"] = server_data.value("last_row(ts)", 0);
            cpu_metrics["usage_percent"] = server_data.value("last_row(cpu_usage_percent)", 0.0);
            cpu_metrics["voltage"] = 0.0; // 需要从其他地方获取
            node["latest_cpu_metrics"] = cpu_metrics;
            
            // Memory metrics
            nlohmann::json memory_metrics;
            memory_metrics["free"] = server_data.value("last_row(mem_free)", 0ULL);
            memory_metrics["timestamp"] = server_data.value("last_row(ts)", 0);
            memory_metrics["total"] = server_data.value("last_row(mem_total)", 0ULL);
            memory_metrics["usage_percent"] = server_data.value("last_row(mem_usage_percent)", 0.0);
            memory_metrics["used"] = server_data.value("last_row(mem_used)", 0ULL);
            node["latest_memory_metrics"] = memory_metrics;
        } else {
            node["latest_cpu_metrics"] = nlohmann::json::object();
            node["latest_memory_metrics"] = nlohmann::json::object();
        }

        // 2. Get latest GPU metrics
        // 需要查询所有GPU子表，因为每个GPU都有独立的子表
        nlohmann::json gpu_metrics_result = queryTDengine("SHOW TABLES LIKE 'gpu_" + std::to_string(box_id) + "_" + std::to_string(slot_id) + "_%'");
        
        nlohmann::json gpu_metrics;
        gpu_metrics["gpu_count"] = 0;
        gpu_metrics["gpus"] = nlohmann::json::array();
        
        // 从表名列表中提取GPU索引并查询每个GPU的最新数据
        for (const auto& table_info : gpu_metrics_result) {
            std::string table_name = table_info.value("table_name", "");
            if (table_name.empty()) continue;
            
            // 从表名中提取GPU索引: gpu_box_slot_index
            size_t last_underscore = table_name.find_last_of('_');
            if (last_underscore == std::string::npos) continue;
            
            std::string gpu_index_str = table_name.substr(last_underscore + 1);
            int gpu_index = std::stoi(gpu_index_str);
            
            // 查询这个GPU的最新数据
            nlohmann::json gpu_data = queryTDengine("SELECT LAST_ROW(*) FROM " + table_name);
            if (!gpu_data.empty()) {
                const auto& data = gpu_data[0];
                nlohmann::json gpu_info;
                gpu_info["compute_usage"] = data.value("last_row(compute_usage)", 0.0);
                gpu_info["current"] = 0.0; // 需要从其他地方获取
                gpu_info["index"] = gpu_index;
                gpu_info["mem_total"] = data.value("last_row(mem_total)", 0ULL);
                gpu_info["mem_usage"] = data.value("last_row(mem_usage)", 0.0);
                gpu_info["mem_used"] = data.value("last_row(mem_used)", 0ULL);
                gpu_info["name"] = data.value("gpu_name", ""); // TAGS字段不需要last_row前缀
                gpu_info["power"] = data.value("last_row(power)", 0.0);
                gpu_info["temperature"] = data.value("last_row(temperature)", 0.0);
                gpu_info["voltage"] = 0.0; // 需要从其他地方获取
                gpu_metrics["gpus"].push_back(gpu_info);
                gpu_metrics["gpu_count"] = gpu_metrics["gpu_count"].get<int>() + 1;
            }
        }
        
        gpu_metrics["timestamp"] = gpu_metrics["gpus"].empty() ? 0 : gpu_metrics["gpus"][0].value("timestamp", 0);
        node["latest_gpu_metrics"] = gpu_metrics;
        
        // 3. Get latest Disk metrics
        // 需要查询所有磁盘子表，因为每个磁盘都有独立的子表
        nlohmann::json disk_metrics_result = queryTDengine("SHOW TABLES LIKE 'disk_" + std::to_string(box_id) + "_" + std::to_string(slot_id) + "_%'");
        
        nlohmann::json disk_metrics;
        disk_metrics["disk_count"] = 0;
        disk_metrics["disks"] = nlohmann::json::array();
        
        // 从表名列表中查询每个磁盘的最新数据
        for (const auto& table_info : disk_metrics_result) {
            std::string table_name = table_info.value("table_name", "");
            if (table_name.empty()) continue;
            
            // 查询这个磁盘的最新数据
            nlohmann::json disk_data = queryTDengine("SELECT LAST_ROW(*) FROM " + table_name);
            if (!disk_data.empty()) {
                const auto& data = disk_data[0];
                nlohmann::json disk_info;
                disk_info["device"] = data.value("device", ""); // TAGS字段不需要last_row前缀
                disk_info["free"] = data.value("last_row(free)", 0ULL);
                disk_info["mount_point"] = data.value("mount_point", ""); // TAGS字段不需要last_row前缀
                disk_info["total"] = data.value("last_row(total)", 0ULL);
                disk_info["usage_percent"] = data.value("last_row(usage_percent)", 0.0);
                disk_info["used"] = data.value("last_row(used)", 0ULL);
                disk_metrics["disks"].push_back(disk_info);
                disk_metrics["disk_count"] = disk_metrics["disk_count"].get<int>() + 1;
            }
        }
        
        disk_metrics["timestamp"] = disk_metrics["disks"].empty() ? 0 : disk_metrics["disks"][0].value("timestamp", 0);
        node["latest_disk_metrics"] = disk_metrics;

        // 4. Get latest Network metrics
        // 需要查询所有网络接口子表，因为每个网络接口都有独立的子表
        nlohmann::json net_metrics_result = queryTDengine("SHOW TABLES LIKE 'net_" + std::to_string(box_id) + "_" + std::to_string(slot_id) + "_%'");
        
        nlohmann::json network_metrics;
        network_metrics["network_count"] = 0;
        network_metrics["networks"] = nlohmann::json::array();
        
        // 从表名列表中查询每个网络接口的最新数据
        for (const auto& table_info : net_metrics_result) {
            std::string table_name = table_info.value("table_name", "");
            if (table_name.empty()) continue;
            
            // 查询这个网络接口的最新数据
            nlohmann::json net_data = queryTDengine("SELECT LAST_ROW(*) FROM " + table_name);
            if (!net_data.empty()) {
                const auto& data = net_data[0];
                nlohmann::json net_info;
                net_info["interface"] = data.value("interface", ""); // TAGS字段不需要last_row前缀
                net_info["rx_bytes"] = data.value("last_row(rx_bytes)", 0ULL);
                net_info["rx_errors"] = data.value("last_row(rx_errors)", 0);
                net_info["rx_packets"] = data.value("last_row(rx_packets)", 0ULL);
                net_info["tx_bytes"] = data.value("last_row(tx_bytes)", 0ULL);
                net_info["tx_errors"] = data.value("last_row(tx_errors)", 0);
                net_info["tx_packets"] = data.value("last_row(tx_packets)", 0ULL);
                network_metrics["networks"].push_back(net_info);
                network_metrics["network_count"] = network_metrics["network_count"].get<int>() + 1;
            }
        }
        
        network_metrics["timestamp"] = network_metrics["networks"].empty() ? 0 : network_metrics["networks"][0].value("timestamp", 0);
        node["latest_network_metrics"] = network_metrics;

        // 5. Get latest Container metrics
        // 需要查询所有容器子表，因为每个容器都有独立的子表
        nlohmann::json container_metrics_result = queryTDengine("SHOW TABLES LIKE 'container_" + std::to_string(box_id) + "_" + std::to_string(slot_id) + "_%'");
        
        nlohmann::json docker_metrics;
        docker_metrics["component"] = nlohmann::json::array(); // 需要从其他地方获取完整的容器信息
        docker_metrics["container_count"] = 0;
        docker_metrics["paused_count"] = 0; // 需要统计状态
        docker_metrics["running_count"] = 0; // 需要统计状态
        docker_metrics["stopped_count"] = 0; // 需要统计状态
        
        // 从表名列表中查询每个容器的最新数据并统计状态
        for (const auto& table_info : container_metrics_result) {
            std::string table_name = table_info.value("table_name", "");
            if (table_name.empty()) continue;
            
            // 查询这个容器的最新数据
            nlohmann::json container_data = queryTDengine("SELECT LAST_ROW(*) FROM " + table_name);
            if (!container_data.empty()) {
                const auto& data = container_data[0];
                std::string status = data.value("last_row(status)", "");
                if (status == "running") {
                    docker_metrics["running_count"] = docker_metrics["running_count"].get<int>() + 1;
                } else if (status == "paused") {
                    docker_metrics["paused_count"] = docker_metrics["paused_count"].get<int>() + 1;
                } else if (status == "stopped") {
                    docker_metrics["stopped_count"] = docker_metrics["stopped_count"].get<int>() + 1;
                }
                docker_metrics["container_count"] = docker_metrics["container_count"].get<int>() + 1;
            }
        }
        
        docker_metrics["timestamp"] = docker_metrics["container_count"].get<int>() == 0 ? 0 : 0; // 需要从实际数据中获取时间戳
        node["latest_docker_metrics"] = docker_metrics;
    }

    return nodes;
} 