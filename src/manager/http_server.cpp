#include "http_server.h"
#include "database_manager.h"
#include <iostream>
#include <utility>

HTTPServer::HTTPServer(std::shared_ptr<DatabaseManager> db_manager,
                       int port)
    : db_manager_(std::move(db_manager)),
      port_(port),
      running_(false)
{
}

HTTPServer::~HTTPServer()
{
    try {
        if (running_)
        {
            stop();
        }
    } catch (const std::exception& e) {
        std::cerr << "[HTTPServer] 析构时异常: " << e.what() << std::endl;
    }
}

bool HTTPServer::start()
{
    try {
        std::cout << "[HTTPServer] 启动，端口: " << port_ << std::endl;

        // 路由初始化
        initNodeRoutes();

        // 启动服务器
        running_ = true;
        server_.set_default_headers({
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type"}
        });
        server_.listen("0.0.0.0", port_);

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[HTTPServer] 启动异常: " << e.what() << std::endl;
        running_ = false;
        return false;
    }
}

void HTTPServer::stop()
{
    if (!running_) return;
    try {
        std::cout << "[HTTPServer] 停止" << std::endl;
        server_.stop();
        running_ = false;
    } catch (const std::exception& e) {
        std::cerr << "[HTTPServer] 停止异常: " << e.what() << std::endl;
    }
}

// 响应辅助方法
void HTTPServer::sendSuccessResponse(httplib::Response& res, const std::string& message) {
    nlohmann::json response = {
        {"api_version", 1},
        {"status", "success"},
        {"data", {{"message", message}}}
    };
    res.set_content(response.dump(), "application/json");
}

void HTTPServer::sendSuccessResponse(httplib::Response& res, const std::string& key, const nlohmann::json& data) {
    nlohmann::json response = {
        {"api_version", 1},
        {"status", "success"},
        {"data", {{key, data}}}
    };
    res.set_content(response.dump(), "application/json");
}

void HTTPServer::sendErrorResponse(httplib::Response& res, const std::string& message) {
    nlohmann::json response = {
        {"api_version", 1},
        {"status", "error"},
        {"data", {{"message", message}}}
    };
    res.set_content(response.dump(), "application/json");
}

void HTTPServer::sendExceptionResponse(httplib::Response& res, const std::exception& e) {
    sendErrorResponse(res, e.what());
} 