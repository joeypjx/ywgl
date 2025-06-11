#include "rpc_client.hpp"
#include <iostream>
#include <sstream>

RPCClient::RPCClient(const std::string& endpoint)
    : context_(1)
    , socket_(context_, ZMQ_REQ)
    , endpoint_(endpoint) {
    socket_.connect(endpoint_);
}

RPCClient::~RPCClient() {
    socket_.close();
}

Json::Value RPCClient::sendRequest(const Json::Value& request) {
    std::string request_str = request.toStyledString();
    zmq::message_t message(request_str.size());
    memcpy(message.data(), request_str.c_str(), request_str.size());
    
    socket_.send(message, zmq::send_flags::none);
    return receiveResponse();
}

Json::Value RPCClient::receiveResponse() {
    zmq::message_t reply;
    socket_.recv(reply);
    
    std::string reply_str(static_cast<char*>(reply.data()), reply.size());
    Json::Value response;
    Json::Reader reader;
    
    if (!reader.parse(reply_str, response)) {
        throw std::runtime_error("Failed to parse response");
    }
    
    if (!response.isMember("success")) {
        throw std::runtime_error("Invalid response format");
    }
    
    if (!response["success"].asBool()) {
        throw std::runtime_error(response["error"].asString());
    }
    
    return response["result"];
} 