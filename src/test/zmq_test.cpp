#include "../manager/zmq_service.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <zmq.hpp>

// 服务端处理函数
std::string handleRequest(const std::string& message) {
    std::cout << "Server received: " << message << std::endl;
    return "Server response: " + message;
}

// 客户端函数
void runClient() {
    try {
        zmq::context_t context(1);
        zmq::socket_t socket(context, ZMQ_REQ);
        socket.connect("tcp://localhost:5555");

        // 发送测试消息
        std::string test_message = "Hello from client!";
        std::cout << "Client sending: " << test_message << std::endl;

        zmq::message_t request(test_message.size());
        memcpy(request.data(), test_message.data(), test_message.size());
        socket.send(request, zmq::send_flags::none);

        // 接收响应
        zmq::message_t reply;
        auto result = socket.recv(reply, zmq::recv_flags::none);
        
        if (result.has_value()) {
            std::string response(static_cast<char*>(reply.data()), reply.size());
            std::cout << "Client received: " << response << std::endl;
        }
    } catch (const zmq::error_t& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
    }
}

int main() {
    try {
        // 创建并启动服务
        ZmqService server("tcp://*:5555");
        server.setMessageHandler(handleRequest);
        server.start();

        std::cout << "Server started..." << std::endl;

        // 等待服务启动
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 运行客户端
        runClient();

        // 等待一段时间后停止服务
        std::this_thread::sleep_for(std::chrono::seconds(1));
        server.stop();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}