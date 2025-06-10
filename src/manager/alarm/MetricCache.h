// MetricCache.h
#pragma once
#include <string>
#include <map>
#include <set>
#include <mutex>
#include <chrono>
#include <utility>
#include <nlohmann/json.hpp>
#include <iostream>
using json = nlohmann::json;

// 节点的指标快照
using MetricSnapshot = json;

// 线程安全的中心化缓存，存储所有节点的最新指标
class MetricCache {
private:
    struct NodeData {
        MetricSnapshot metrics;
        std::chrono::time_point<std::chrono::steady_clock> lastUpdated;
    };

    std::map<std::string, NodeData> cache_;
    mutable std::mutex mutex_;

public:
    // 由数据接收端调用，更新节点的指标
    void updateNodeMetrics(const std::string& nodeId, MetricSnapshot metrics) {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[nodeId] = {std::move(metrics), std::chrono::steady_clock::now()};
    }

    // 由资源对象调用，获取特定节点的特定指标
    double getMetric(const std::string& nodeId, const std::string& metricName) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto nodeIt = cache_.find(nodeId);
        if (nodeIt != cache_.end()) {
            const auto& metrics = nodeIt->second.metrics;
            // 打印metrics
            if (metrics.contains(metricName)) {
                return metrics.at(metricName).get<double>();
            }
        }
        return 0.0;
    }

    // 由规则供应器调用，获取所有活跃的节点ID
    std::set<std::string> getActiveNodeIds(std::chrono::seconds timeout = std::chrono::minutes(5)) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::set<std::string> activeNodes;
        auto now = std::chrono::steady_clock::now();
        for (const auto& pair : cache_) {
            if ((now - pair.second.lastUpdated) < timeout) {
                activeNodes.insert(pair.first);
            }
        }
        return activeNodes;
    }
};