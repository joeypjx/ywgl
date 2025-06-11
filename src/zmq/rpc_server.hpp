#pragma once

#include <zmq.hpp>
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class RPCServer {
public:
    explicit RPCServer(const std::string& endpoint);
    ~RPCServer();

    // 启动和停止服务器
    void start();
    void stop();

    // 注册RPC方法
    template<typename... Args>
    void registerMethod(const std::string& method_name, 
                       std::function<json(Args...)> handler);

private:
    // 处理请求
    json handleRequest(const std::string& request_str);

    // 创建响应
    json createErrorResponse(int code, const std::string& message, int id = 0);
    json createSuccessResponse(const json& result, int id);

    // ZeroMQ相关
    zmq::context_t context_;
    zmq::socket_t socket_;
    bool running_ = false;

    // 方法处理器
    std::unordered_map<std::string, std::function<json(const json&)>> handlers_;
}; 