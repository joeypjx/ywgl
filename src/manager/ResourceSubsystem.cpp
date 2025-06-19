#include "ResourceSubsystem.h"
#include "database_manager.h"
#include <nlohmann/json.hpp>
#include <iostream>

ResourceSubsystem::ResourceSubsystem(std::shared_ptr<DatabaseManager> db_manager)
    : db_manager_(db_manager) {
    if (db_manager_) {
        std::cout << "ResourceSubsystem initialized with DatabaseManager." << std::endl;
    } else {
        std::cout << "ResourceSubsystem initialized without a valid DatabaseManager." << std::endl;
    }
}

ResourceSubsystem::~ResourceSubsystem() {
    std::cout << "ResourceSubsystem destroyed." << std::endl;
}

std::string ResourceSubsystem::getNodeListJson() {
    if (!db_manager_) {
        // 如果db_manager_无效，返回一个空的JSON数组字符串
        nlohmann::json error_json;
        error_json["error"] = "Database manager not initialized";
        return error_json.dump();
    }
    
    // 从DatabaseManager获取节点信息
    nlohmann::json nodes_json = db_manager_->getAllNodes();
    
    // 将JSON对象转换为字符串
    return nodes_json.dump();
}

std::string ResourceSubsystem::getAvailableNodesWithSomeMetrics() {
    if (!db_manager_) {
        nlohmann::json error_json;
        error_json["error"] = "Database manager not initialized";
        return error_json.dump();
    }
    nlohmann::json nodes_metrics = db_manager_->getNodesWithLatestMetrics();
    nlohmann::json result;
    result["nodes"] = nlohmann::json::array();
    for (const auto& node : nodes_metrics) {
        nlohmann::json node_info;
        node_info["host_ip"] = node.value("host_ip", "");
        // CPU
        if (node.contains("latest_cpu_metrics")) {
            const auto& cpu = node["latest_cpu_metrics"];
            node_info["cpu_total"] = cpu.value("core_count", 0);
            node_info["cpu_free"] = cpu.value("core_count", 0) - cpu.value("core_allocated", 0);
        } else {
            node_info["cpu_total"] = 0;
            node_info["cpu_free"] = 0;
        }
        // Memory
        if (node.contains("latest_memory_metrics")) {
            const auto& mem = node["latest_memory_metrics"];
            node_info["mem_total"] = mem.value("total", 0LL);
            node_info["mem_free"] = mem.value("free", 0LL);
        } else {
            node_info["mem_total"] = 0;
            node_info["mem_free"] = 0;
        }
        // GPU
        if (node.contains("latest_gpu_metrics") && node["latest_gpu_metrics"].contains("gpus")) {
            const auto& gpus = node["latest_gpu_metrics"]["gpus"];
            node_info["gpu_total"] = gpus.size();
            int gpu_free = 0;
            for (const auto& gpu : gpus) {
                // 假设compute_usage为0则空闲
                if (gpu.value("compute_usage", 1.0) == 0.0) {
                    gpu_free++;
                }
            }
            node_info["gpu_free"] = gpu_free;
        } else {
            node_info["gpu_total"] = 0;
            node_info["gpu_free"] = 0;
        }
        result["nodes"].push_back(node_info);
    }
    return result.dump();
} 