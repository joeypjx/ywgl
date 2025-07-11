#include "http_server.h"
#include "database_manager.h"
#include <iostream>
#include <chrono>
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>

// 工具函数：检查json字段
static bool check_json_fields(const nlohmann::json& j, const std::vector<std::string>& fields) {
    for (const auto& f : fields) {
        if (!j.contains(f)) return false;
    }
    return true;
}

// 初始化节点管理路由
void HTTPServer::initNodeRoutes()
{
    // 节点心跳API
    server_.Post("/heartbeat", [this](const httplib::Request &req, httplib::Response &res)
                { handleHeartbeat(req, res); });
                 
    // POST /resource - 更新资源使用情况数据
    server_.Post("/resource", [this](const httplib::Request &req, httplib::Response &res)
                 { handleResourceUpdate(req, res); });
                 
    // GET /node - 获取所有节点信息
    server_.Get("/node", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetAllNodes(req, res); });

    // GET /node/metrics - 获取节点指标
    server_.Get("/node/metrics", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetNodeMetrics(req, res); });

    // GET /node/hierarchical - 获取层级结构的节点信息
    server_.Get("/node/hierarchical", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetNodesHierarchical(req, res); });

    // GET /node/historical-metrics - 获取节点历史指标数据
    server_.Get("/node/historical-metrics", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetNodeHistoricalMetrics(req, res); });
}

// 处理节点心跳请求
void HTTPServer::handleHeartbeat(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        nlohmann::json request_json;
        try {
            request_json = nlohmann::json::parse(req.body);
        } catch (const std::exception& e) {
            sendErrorResponse(res, "Invalid JSON: " + std::string(e.what()));
            return;
        }
        
        if (!check_json_fields(request_json, {"api_version", "data"})) {
            sendErrorResponse(res, "Missing api_version or data field in request");
            return;
        }
        
        auto& data = request_json["data"];
        
        // 检查必要字段
        if (!check_json_fields(data, {"box_id", "slot_id", "cpu_id"})) {
            sendErrorResponse(res, "box_id, slot_id and cpu_id are required in data");
            return;
        }
        
        if (!tdengine_manager_) {
            sendErrorResponse(res, "TDengine manager not initialized");
            return;
        }
        
        // 调用updateNode保存节点信息
        if (tdengine_manager_->updateNodeInfo(data))
        {
            sendSuccessResponse(res, "Node information updated successfully");
        }
        else
        {
            sendErrorResponse(res, "Failed to update node information");
        }
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

// 处理资源更新请求
void HTTPServer::handleResourceUpdate(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        nlohmann::json request_json;
        try {
            request_json = nlohmann::json::parse(req.body);
        } catch (const std::exception& e) {
            sendErrorResponse(res, "Invalid JSON: " + std::string(e.what()));
            return;
        }
        
        if (!check_json_fields(request_json, {"api_version", "data"})) {
            sendErrorResponse(res, "Missing api_version or data field in request");
            return;
        }
        
        auto& data = request_json["data"];
        
        // 检查必要字段
        if (!check_json_fields(data, {"host_ip", "resource"})) {
            sendErrorResponse(res, "host_ip and resource are required in request body");
            return;
        }
        
        if (!tdengine_manager_) {
            sendErrorResponse(res, "TDengine manager not initialized");
            return;
        }
        
        // 构建metrics_data对象
        nlohmann::json metrics_data = {
            {"host_ip", data["host_ip"]},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"resource", data["resource"]},
            {"component", data["component"]}
        };
        
        alarm_subsystem_->updateNodeMetrics(data["host_ip"], metrics_data);

        ModuleDataAccess moduleDataAccess;
        // data["component"] is array
        for (const auto& component : data["component"]) {
            // mem usage is mem used / mem limit
            //component["resource"]["memory"]["mem_used"] is json object, need to get the value. and limit may be 0 or null
            double mem_usage = 0;
            if (component["resource"]["memory"]["mem_limit"].get<double>() != 0) {
                mem_usage = component["resource"]["memory"]["mem_used"].get<double>() / component["resource"]["memory"]["mem_limit"].get<double>();
            }
            std::cout << "compupdateComponentStateonent: instance_id: " << component["instance_id"] << " index: " << component["index"] << " state: " << component["state"] << " cpu_load: " << component["resource"]["cpu"]["load"] << " mem_usage: " << mem_usage << " network_tx: " << component["resource"]["network"]["tx"] << " network_rx: " << component["resource"]["network"]["rx"] << std::endl;
            moduleDataAccess.updateComponentState(component["instance_id"], component["index"], component["state"], component["resource"]["cpu"]["load"], mem_usage, 0, component["resource"]["network"]["tx"], component["resource"]["network"]["rx"]);
        }

        if (tdengine_manager_->saveMetrics(metrics_data))
        {
            sendSuccessResponse(res, "Resource data updated successfully");
        }
        else
        {
            sendErrorResponse(res, "Failed to update resource data");
        }
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

// 处理获取节点信息（支持可选的host_ip参数）
void HTTPServer::handleGetAllNodes(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        if (!tdengine_manager_) {
            sendErrorResponse(res, "TDengine manager not initialized");
            return;
        }

        // 检查是否有 host_ip 参数
        if (req.has_param("host_ip")) {
            std::string host_ip = req.get_param_value("host_ip");
            
            // 获取指定 IP 的节点信息
            auto node = tdengine_manager_->getNodeInfoByHostIp(host_ip);
            
            if (node.is_null() || node.empty()) {
                sendErrorResponse(res, "Node not found for host_ip: " + host_ip);
                return;
            }
            
            sendSuccessResponse(res, "node", node);
        } else {
            // 获取所有节点信息
            auto nodes = tdengine_manager_->getAllNodesInfo();
            sendSuccessResponse(res, "nodes", nodes);
        }
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

// 处理获取所有节点及其最新metrics
void HTTPServer::handleGetNodeMetrics(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        if (!tdengine_manager_) {
            sendErrorResponse(res, "TDengine manager not initialized");
            return;
        }
        auto metrics = tdengine_manager_->getNodesWithLatestMetrics();
        sendSuccessResponse(res, "nodes_metrics", metrics);
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

// 处理获取层级结构的节点信息
void HTTPServer::handleGetNodesHierarchical(const httplib::Request &req, httplib::Response &res)
{
    // try
    // {
    //     if (!tdengine_manager_) {
    //         sendErrorResponse(res, "TDengine manager not initialized");
    //         return;
    //     }
    //     auto nodes = tdengine_manager_->getNodesHierarchical();
    //     sendSuccessResponse(res, "nodes_hierarchical", nodes);
    // }
    // catch (const std::exception &e)
    // {
    //     sendExceptionResponse(res, e);
    // }
}

// 处理获取节点历史指标数据
void HTTPServer::handleGetNodeHistoricalMetrics(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        if (!tdengine_manager_) {
            sendErrorResponse(res, "TDengine manager not initialized");
            return;
        }

        // 获取查询参数
        std::string host_ip;
        std::string time_range;
        std::string metrics_param;

        // 检查必需的参数
        if (req.has_param("host_ip")) {
            host_ip = req.get_param_value("host_ip");
        } else {
            sendErrorResponse(res, "Missing required parameter: host_ip");
            return;
        }

        if (req.has_param("time_range")) {
            time_range = req.get_param_value("time_range");
        } else {
            sendErrorResponse(res, "Missing required parameter: time_range");
            return;
        }

        if (req.has_param("metrics")) {
            metrics_param = req.get_param_value("metrics");
        } else {
            sendErrorResponse(res, "Missing required parameter: metrics");
            return;
        }

        // 解析 metrics 参数（逗号分隔的字符串）
        std::vector<std::string> metrics;
        std::string metric;
        std::stringstream ss(metrics_param);
        
        while (std::getline(ss, metric, ',')) {
            // 去除首尾空格
            metric.erase(0, metric.find_first_not_of(" \t"));
            metric.erase(metric.find_last_not_of(" \t") + 1);
            if (!metric.empty()) {
                metrics.push_back(metric);
            }
        }

        if (metrics.empty()) {
            sendErrorResponse(res, "No valid metrics specified");
            return;
        }

        // 验证指标类型是否有效
        std::vector<std::string> valid_metrics = {"cpu", "memory", "gpu", "disk", "network", "container"};
        for (const auto& m : metrics) {
            if (std::find(valid_metrics.begin(), valid_metrics.end(), m) == valid_metrics.end()) {
                sendErrorResponse(res, "Invalid metric type: " + m + ". Valid types are: cpu, memory, gpu, disk, network, container");
                return;
            }
        }

        // 调用 TDengineManager 获取历史数据
        auto historical_data = tdengine_manager_->getNodeHistoricalMetrics(host_ip, time_range, metrics);
        
        // 检查是否有错误
        if (historical_data.contains("error")) {
            sendErrorResponse(res, historical_data["error"].get<std::string>());
            return;
        }

        sendSuccessResponse(res, "historical_metrics", historical_data);
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}
