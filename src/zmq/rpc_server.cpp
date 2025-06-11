#include "rpc_server.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
    // 辅助函数：从JSON中提取参数
    template<typename T>
    T deserializeArg(const json& value) {
        return value.get<T>();
    }

    // 辅助函数：调用无参数的处理函数
    json callHandlerImpl(std::function<json()> handler, const json& params) {
        if (!params.empty()) {
            throw std::runtime_error("参数数量不匹配");
        }
        return handler();
    }

    // 辅助函数：调用单参数的处理函数
    template<typename T>
    json callHandlerImpl(std::function<json(T)> handler, const json& params) {
        if (params.size() != 1) {
            throw std::runtime_error("参数数量不匹配");
        }
        return handler(deserializeArg<T>(params[0]));
    }

    // 辅助函数：调用双参数的处理函数
    template<typename T1, typename T2>
    json callHandlerImpl(std::function<json(T1, T2)> handler, const json& params) {
        if (params.size() != 2) {
            throw std::runtime_error("参数数量不匹配");
        }
        return handler(deserializeArg<T1>(params[0]), deserializeArg<T2>(params[1]));
    }
}

RPCServer::RPCServer(const std::string& endpoint) 
    : context_(1), socket_(context_, ZMQ_REP) {
    try {
        socket_.bind(endpoint);
    } catch (const zmq::error_t& e) {
        throw std::runtime_error("Failed to bind socket: " + std::string(e.what()));
    }
}

RPCServer::~RPCServer() {
    stop();
}

void RPCServer::start() {
    running_ = true;
    while (running_) {
        try {
            zmq::message_t request;
            auto result = socket_.recv(request, zmq::recv_flags::none);
            if (!result) {
                continue;
            }

            std::string request_str(static_cast<char*>(request.data()), request.size());
            json response = handleRequest(request_str);

            std::string response_str = response.dump();
            zmq::message_t reply(response_str.size());
            memcpy(reply.data(), response_str.data(), response_str.size());
            socket_.send(reply, zmq::send_flags::none);
        } catch (const std::exception& e) {
            std::cerr << "Error handling request: " << e.what() << std::endl;
            json error_response = createErrorResponse(-32000, "Internal error: " + std::string(e.what()));
            std::string error_str = error_response.dump();
            zmq::message_t reply(error_str.size());
            memcpy(reply.data(), error_str.data(), error_str.size());
            socket_.send(reply, zmq::send_flags::none);
        }
    }
}

void RPCServer::stop() {
    running_ = false;
    socket_.close();
}

json RPCServer::handleRequest(const std::string& request_str) {
    try {
        json request = json::parse(request_str);
        
        // 验证请求格式
        if (!request.contains("jsonrpc") || !request.contains("method") || !request.contains("id")) {
            return createErrorResponse(-32600, "Invalid Request");
        }

        if (request["jsonrpc"] != "2.0") {
            return createErrorResponse(-32600, "Invalid Request: jsonrpc must be '2.0'");
        }

        std::string method = request["method"];
        json params = request.value("params", json::array());
        int id = request["id"];

        // 查找并调用处理方法
        auto it = handlers_.find(method);
        if (it == handlers_.end()) {
            return createErrorResponse(-32601, "Method not found: " + method, id);
        }

        try {
            json result = it->second(params);
            return createSuccessResponse(result, id);
        } catch (const std::exception& e) {
            return createErrorResponse(-32000, "Internal error: " + std::string(e.what()), id);
        }
    } catch (const json::parse_error& e) {
        return createErrorResponse(-32700, "Parse error: " + std::string(e.what()));
    }
}

json RPCServer::createErrorResponse(int code, const std::string& message, int id) {
    json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = code;
    response["error"]["message"] = message;
    response["id"] = id;
    return response;
}

json RPCServer::createSuccessResponse(const json& result, int id) {
    json response;
    response["jsonrpc"] = "2.0";
    response["result"] = result;
    response["id"] = id;
    return response;
}

template<typename... Args>
void RPCServer::registerMethod(const std::string& method_name, 
                             std::function<json(Args...)> handler) {
    handlers_[method_name] = [handler](const json& params) -> json {
        return callHandlerImpl(handler, params);
    };
}

// 显式实例化常用的模板
template void RPCServer::registerMethod<>(const std::string&, std::function<json()>);
template void RPCServer::registerMethod<int>(const std::string&, std::function<json(int)>);
template void RPCServer::registerMethod<int, int>(const std::string&, std::function<json(int, int)>);
template void RPCServer::registerMethod<std::string>(const std::string&, std::function<json(std::string)>);
template void RPCServer::registerMethod<std::string, int>(const std::string&, std::function<json(std::string, int)>); 