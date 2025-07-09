#include "ResourceSubsystem.h"
#include "database_manager.h"
#include "tdengine_manager.h"
#include <nlohmann/json.hpp>
#include <iostream>



ResourceSubsystem::ResourceSubsystem(std::shared_ptr<TDengineManager> tdengine_manager)
    : tdengine_manager_(tdengine_manager) {
    if (tdengine_manager_) {
        std::cout << "ResourceSubsystem initialized with TDengineManager." << std::endl;
    } else {
        std::cout << "ResourceSubsystem initialized without a valid TDengineManager." << std::endl;
    }
}

ResourceSubsystem::~ResourceSubsystem() {
    std::cout << "ResourceSubsystem destroyed." << std::endl;
}

std::string ResourceSubsystem::getAvailableNodes() {
    if (!tdengine_manager_) {
        nlohmann::json error_json;
        error_json["error"] = "TDengine manager not initialized";
        return error_json.dump();
    }

    nlohmann::json nodes_metrics = tdengine_manager_->getNodesWithLatestMetrics();

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