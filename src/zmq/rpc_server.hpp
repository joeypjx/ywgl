#pragma once

#include <zmq.hpp>
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <json/json.h>

class RPCServer {
public:
    RPCServer(const std::string& endpoint);
    ~RPCServer();

    // 注册远程方法
    template<typename... Args>
    void registerMethod(const std::string& method_name, 
                       std::function<Json::Value(Args...)> handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_[method_name] = [handler](const Json::Value& params) {
            return callHandler(handler, params);
        };
    }

    // 启动服务器
    void start();
    // 停止服务器
    void stop();

private:
    void run();
    Json::Value handleRequest(const std::string& request);
    Json::Value createErrorResponse(const std::string& error_msg);
    Json::Value createSuccessResponse(const Json::Value& result);

    template<typename... Args>
    static Json::Value callHandler(std::function<Json::Value(Args...)> handler,
                                 const Json::Value& params);

    zmq::context_t context_;
    zmq::socket_t socket_;
    std::string endpoint_;
    std::unordered_map<std::string, std::function<Json::Value(const Json::Value&)>> handlers_;
    std::thread worker_thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool running_;
}; 