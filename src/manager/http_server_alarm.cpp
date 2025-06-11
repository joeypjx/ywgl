#include "http_server.h"
#include <nlohmann/json.hpp>

// 初始化告警管理路由
void HTTPServer::initAlarmRoutes()
{
    // POST /alarm/rules - 添加一个新的告警规则
    server_.Post("/alarm/rules", [this](const httplib::Request &req, httplib::Response &res)
                 { handlePostAlarmRule(req, res); });

    // GET /alarm/rules - 获取所有告警规则
    server_.Get("/alarm/rules", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetAlarmRules(req, res); });

    // GET /alarm/events - 获取告警事件
    server_.Get("/alarm/events", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetAlarmEvents(req, res); });
}

// 处理添加告警规则的请求
void HTTPServer::handlePostAlarmRule(const httplib::Request &req, httplib::Response &res)
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
        
        if (!alarm_subsystem_) {
            sendErrorResponse(res, "Alarm subsystem not initialized");
            return;
        }
        
        // 调用 AlarmSubsystem 来处理规则的添加
        alarm_subsystem_->addAlarmTemplate(request_json);
        
        sendSuccessResponse(res, "Alarm rule added successfully");
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

// 处理获取所有告警规则的请求
void HTTPServer::handleGetAlarmRules(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        if (!alarm_subsystem_) {
            sendErrorResponse(res, "Alarm subsystem not initialized");
            return;
        }
        
        auto rules = alarm_subsystem_->getAllAlarmTemplatesAsJson();
        sendSuccessResponse(res, "alarm_rules", rules);
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

// 处理获取告警事件的请求
void HTTPServer::handleGetAlarmEvents(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        if (!alarm_subsystem_) {
            sendErrorResponse(res, "Alarm subsystem not initialized");
            return;
        }
        
        int limit = 100; // 默认限制
        if (req.has_param("limit")) {
            try {
                limit = std::stoi(req.get_param_value("limit"));
            } catch(...) {
                // 忽略无效的limit值
            }
        }
        
        auto events = alarm_subsystem_->getAllAlarmEventsAsJson(limit);
        sendSuccessResponse(res, "alarm_events", events);
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
} 