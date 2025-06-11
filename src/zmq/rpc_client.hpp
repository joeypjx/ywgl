#pragma once

#include <zmq.hpp>
#include <string>
#include <functional>
#include <memory>
#include <json/json.h>

class RPCClient {
public:
    RPCClient(const std::string& endpoint);
    ~RPCClient();

    // 调用远程方法
    template<typename... Args>
    Json::Value call(const std::string& method_name, Args&&... args) {
        Json::Value request;
        request["method"] = method_name;
        request["params"] = Json::Value(Json::arrayValue);
        
        // 将参数序列化为JSON数组
        int index = 0;
        ((request["params"][index++] = serializeArg(std::forward<Args>(args))), ...);

        return sendRequest(request);
    }

private:
    Json::Value sendRequest(const Json::Value& request);
    Json::Value receiveResponse();
    
    template<typename T>
    Json::Value serializeArg(const T& arg) {
        return Json::Value(arg);
    }

    zmq::context_t context_;
    zmq::socket_t socket_;
    std::string endpoint_;
}; 