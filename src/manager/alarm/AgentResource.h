// AgentResource.h
#pragma once
#include "IResource.h"
#include "MetricCache.h"
#include <memory>
#include <utility>

// 代表来自Agent的某个节点的特定指标
class AgentResource : public IResource {
private:
    std::string nodeId_;
    std::string metricName_;
    std::shared_ptr<MetricCache> cache_;

public:
    AgentResource(std::string node, std::string metric, std::shared_ptr<MetricCache> c)
        : nodeId_(std::move(node)), metricName_(std::move(metric)), cache_(std::move(c)) {}

    double getValue() const override {
        return cache_->getMetric(nodeId_, metricName_);
    }

    std::string getName() const override {
        return "Metric '" + metricName_ + "' on node '" + nodeId_ + "'";
    }
};