#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "alarm/AlarmSubsystem.h"
#include "tdengine_manager.h"
// 前向声明
// class DatabaseManager;

/**
 * HTTPServer类 - HTTP服务器
 * 
 * 提供HTTP API接口
 */
class HTTPServer {
public:
    // 构造与析构
    HTTPServer(std::shared_ptr<TDengineManager> tdengine_manager, 
               std::shared_ptr<AlarmSubsystem> alarm_subsystem,
              int port = 8080);
    ~HTTPServer();

    // 启动与停止
    bool start();
    void stop();

    // 路由初始化
    void initNodeRoutes();
    void initAlarmRoutes();

    void handleResourceUpdate(const httplib::Request& req, httplib::Response& res);
    void handleGetAllNodes(const httplib::Request& req, httplib::Response& res);
    void handleHeartbeat(const httplib::Request& req, httplib::Response& res);
    void handleGetNodeMetrics(const httplib::Request& req, httplib::Response& res);
    void handleGetNodesHierarchical(const httplib::Request& req, httplib::Response& res);
    void handleGetNodeHistoricalMetrics(const httplib::Request& req, httplib::Response& res);
    
    // 告警相关路由处理
    void handlePostAlarmRule(const httplib::Request& req, httplib::Response& res);
    void handleGetAlarmRules(const httplib::Request& req, httplib::Response& res);
    void handleGetAlarmEvents(const httplib::Request& req, httplib::Response& res);

    // 统一API响应方法
    void sendSuccessResponse(httplib::Response& res, const std::string& message);
    void sendSuccessResponse(httplib::Response& res, const std::string& key, const nlohmann::json& data);
    void sendErrorResponse(httplib::Response& res, const std::string& message);
    void sendExceptionResponse(httplib::Response& res, const std::exception& e);

protected:
    httplib::Server server_;  // HTTP服务器
    std::shared_ptr<TDengineManager> tdengine_manager_;    // 数据库管理器

private:
    int port_;  // 监听端口
    bool running_;  // 是否正在运行

    std::shared_ptr<AlarmSubsystem> alarm_subsystem_;
};

#endif // HTTP_SERVER_H
