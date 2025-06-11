// IResource.h
#pragma once
#include <string>

// 资源接口，定义了一个可被监控的对象
class IResource {
public:
    virtual ~IResource() = default;
    
    // 获取资源的当前值
    virtual double getValue() const = 0;
    
    // 获取资源的名称
    virtual std::string getName() const = 0;

    // 获取资源的指标名称
    virtual std::string getMetricName() const = 0;

    // 获取资源的节点ID
    virtual std::string getNodeId() const = 0;
};