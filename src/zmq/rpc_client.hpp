#pragma once

#include <zmq.hpp>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <tuple>

using json = nlohmann::json;

class RPCClient {
public:
    explicit RPCClient(const std::string& endpoint) {
        context_ = std::make_unique<zmq::context_t>(1);
        socket_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_REQ);
        socket_->connect(endpoint);
    }

    template<typename... Args>
    json call(const std::string& method, Args&&... args) {
        json request;
        request["jsonrpc"] = "2.0";
        request["method"] = method;
        request["id"] = next_id_++;
        request["params"] = json::array();

        // 使用辅助函数来序列化参数
        serializeArgs(request["params"], std::forward<Args>(args)...);

        std::string request_str = request.dump();
        zmq::message_t request_msg(request_str.size());
        memcpy(request_msg.data(), request_str.c_str(), request_str.size());
        socket_->send(request_msg, zmq::send_flags::none);

        zmq::message_t reply;
        auto result = socket_->recv(reply);
        if (!result) {
            throw std::runtime_error("Failed to receive response");
        }

        std::string reply_str(static_cast<char*>(reply.data()), reply.size());
        json response = json::parse(reply_str);

        if (response.contains("error") && !response["error"].is_null()) {
            throw std::runtime_error("RPC error: " + response["error"]["message"].get<std::string>());
        }

        return response["result"];
    }

private:
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> socket_;
    int next_id_ = 1;

    // 辅助函数：序列化单个参数
    template<typename T>
    void serializeArg(json& params, T&& arg) {
        params.push_back(serializeValue(std::forward<T>(arg)));
    }

    // 辅助函数：序列化多个参数
    template<typename T, typename... Args>
    void serializeArgs(json& params, T&& first, Args&&... rest) {
        serializeArg(params, std::forward<T>(first));
        serializeArgs(params, std::forward<Args>(rest)...);
    }

    // 终止递归
    void serializeArgs(json& /*params*/) {}

    // 序列化值的辅助函数
    template<typename T>
    json serializeValue(T&& value) {
        return json(std::forward<T>(value));
    }
}; 