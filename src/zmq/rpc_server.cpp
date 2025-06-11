#include "rpc_server.hpp"
#include <iostream>
#include <sstream>

RPCServer::RPCServer(const std::string& endpoint)
    : context_(1)
    , socket_(context_, ZMQ_REP)
    , endpoint_(endpoint)
    , running_(false) {
    socket_.bind(endpoint_);
}

RPCServer::~RPCServer() {
    stop();
}

void RPCServer::start() {
    if (running_) return;
    
    running_ = true;
    worker_thread_ = std::thread(&RPCServer::run, this);
}

void RPCServer::stop() {
    if (!running_) return;
    
    running_ = false;
    cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void RPCServer::run() {
    while (running_) {
        zmq::message_t request;
        try {
            auto result = socket_.recv(request, zmq::recv_flags::dontwait);
            if (result) {
                std::string request_str(static_cast<char*>(request.data()), request.size());
                Json::Value response = handleRequest(request_str);
                
                std::string response_str = response.toStyledString();
                zmq::message_t reply(response_str.size());
                memcpy(reply.data(), response_str.c_str(), response_str.size());
                socket_.send(reply, zmq::send_flags::none);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in RPC server: " << e.what() << std::endl;
        }
        
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(100), [this] { return !running_; });
    }
}

Json::Value RPCServer::handleRequest(const std::string& request_str) {
    Json::Value request;
    Json::Reader reader;
    
    if (!reader.parse(request_str, request)) {
        return createErrorResponse("Invalid JSON request");
    }
    
    if (!request.isMember("method") || !request["method"].isString()) {
        return createErrorResponse("Missing or invalid method name");
    }
    
    std::string method_name = request["method"].asString();
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = handlers_.find(method_name);
    if (it == handlers_.end()) {
        return createErrorResponse("Method not found: " + method_name);
    }
    
    try {
        Json::Value result = it->second(request["params"]);
        return createSuccessResponse(result);
    } catch (const std::exception& e) {
        return createErrorResponse(std::string("Method execution error: ") + e.what());
    }
}

Json::Value RPCServer::createErrorResponse(const std::string& error_msg) {
    Json::Value response;
    response["error"] = error_msg;
    response["success"] = false;
    return response;
}

Json::Value RPCServer::createSuccessResponse(const Json::Value& result) {
    Json::Value response;
    response["result"] = result;
    response["success"] = true;
    return response;
}

template<typename... Args>
Json::Value RPCServer::callHandler(std::function<Json::Value(Args...)> handler,
                                 const Json::Value& params) {
    if (!params.isArray()) {
        throw std::runtime_error("Parameters must be an array");
    }
    
    if (params.size() != sizeof...(Args)) {
        throw std::runtime_error("Parameter count mismatch");
    }
    
    return std::apply(handler, deserializeArgs<Args...>(params));
}

template<typename... Args>
std::tuple<Args...> deserializeArgs(const Json::Value& params) {
    return std::make_tuple(deserializeArg<Args>(params[sizeof...(Args) - 1])...);
}

template<typename T>
T deserializeArg(const Json::Value& value) {
    return value.as<T>();
} 