#include "http_server.h"
#include "database_manager.h"
#include <iostream>
#include <chrono>
#include <nlohmann/json.hpp>

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
        
        if (!db_manager_) {
            sendErrorResponse(res, "Database manager not initialized");
            return;
        }
        
        // 调用updateNode保存节点信息
        if (db_manager_->updateNode(data))
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
        if (!check_json_fields(data, {"hostIp", "resource"})) {
            sendErrorResponse(res, "hostIp and resource are required in request body");
            return;
        }
        
        if (!db_manager_) {
            sendErrorResponse(res, "Database manager not initialized");
            return;
        }
        
        // 构建metrics_data对象
        nlohmann::json metrics_data = {
            {"hostIp", data["hostIp"]},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"resource", data["resource"]}
        };
        
        if (db_manager_->saveNodeResourceUsage(metrics_data))
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

// 处理获取所有节点信息
void HTTPServer::handleGetAllNodes(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        if (!db_manager_) {
            sendErrorResponse(res, "Database manager not initialized");
            return;
        }
        auto nodes = db_manager_->getAllNodes();
        sendSuccessResponse(res, "nodes", nodes);
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
        if (!db_manager_) {
            sendErrorResponse(res, "Database manager not initialized");
            return;
        }
        auto metrics = db_manager_->getNodesWithLatestMetrics();
        sendSuccessResponse(res, "nodes_metrics", metrics);
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}
