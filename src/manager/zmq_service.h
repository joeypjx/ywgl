#ifndef ZMQ_SERVICE_H
#define ZMQ_SERVICE_H

#include <zmq.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>

class ZmqService {
public:
    ZmqService(const std::string& endpoint = "tcp://*:5555");
    ~ZmqService();

    // 启动服务
    void start();
    // 停止服务
    void stop();
    // 发送消息
    bool send(const std::string& message);
    // 设置消息处理回调
    void setMessageHandler(std::function<void(const std::string&)> handler);

private:
    void run();
    void handleMessage(const std::string& message);

    std::string endpoint_;
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> socket_;
    std::thread worker_thread_;
    std::atomic<bool> running_;
    std::function<void(const std::string&)> message_handler_;
};

#endif // ZMQ_SERVICE_H 