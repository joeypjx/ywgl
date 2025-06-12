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
#include <regex> // 用于解析自定义路径
using json = nlohmann::json;

// 节点的指标快照
using MetricSnapshot = json;

namespace {
// 辅助结构体，用于存储解析后的路径信息
struct MetricPath {
    bool isComplex = false;
    std::string arrayKey;
    std::string matchKey;
    std::string matchValue;
    std::string targetKey;
};

// 解析自定义的指标路径，例如 "disk[path=/dev/sda1].usage_percent" 或 "cpu.usage_percent"
MetricPath parseMetricPath(const std::string& path) {
    // 尝试匹配复杂路径格式：disk[path=/dev/sda1].usage_percent
    static const std::regex complexRe(R"((\w+)\[(\w+)=([^\]]+)\]\.(\w+))");
    std::smatch complexMatch;
    if (std::regex_match(path, complexMatch, complexRe) && complexMatch.size() == 5) {
        return {true, complexMatch[1], complexMatch[2], complexMatch[3], complexMatch[4]};
    }

    // 尝试匹配简单路径格式：cpu.usage_percent
    static const std::regex simpleRe(R"((\w+)\.(\w+))");
    std::smatch simpleMatch;
    if (std::regex_match(path, simpleMatch, simpleRe) && simpleMatch.size() == 3) {
        return {false, simpleMatch[1], "", "", simpleMatch[2]};
    }

    return {false};
}
}

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
        if (nodeIt == cache_.end()) {
            return 0.0;
        }

        const auto& metrics = nodeIt->second.metrics;

        // 1. 尝试解析自定义路径
        MetricPath path = parseMetricPath(metricName);
        if (path.isComplex) {
            if (metrics.contains(path.arrayKey) && metrics[path.arrayKey].is_array()) {
                for (const auto& item : metrics[path.arrayKey]) {
                    if (item.contains(path.matchKey) && item[path.matchKey].get<std::string>() == path.matchValue) {
                        if (item.contains(path.targetKey)) {
                            return item[path.targetKey].get<double>();
                        }
                    }
                }
            }
            return 0.0; // 未找到匹配项
        } else if (!path.arrayKey.empty()) {
            // 处理简单路径格式：cpu.usage_percent
            if (metrics.contains(path.arrayKey) && metrics[path.arrayKey].contains(path.targetKey)) {
                return metrics[path.arrayKey][path.targetKey].get<double>();
            }
            return 0.0;
        }

        // 2. 如果不是自定义路径，回退到JSON Pointer
        try {
            json::json_pointer ptr(metricName);
            if (metrics.contains(ptr)) {
                return metrics.at(ptr).get<double>();
            }
        } catch (json::parse_error&) {
            // 3. 如果也不是JSON Pointer，回退到普通key
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