#include "manager.h"
#include "http_server.h"
#include "database_manager.h"
#include "multicast_announcer.h"
#include "zmq_service.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

Manager::Manager(int port, const std::string& db_path)
    : port_(port), db_path_(db_path), running_(false) {}

Manager::~Manager() {
    if (running_) {
        stop();
    }
}

bool Manager::initialize() {
    std::cout << "[Manager] 初始化..." << std::endl;

    db_manager_ = std::make_shared<DatabaseManager>(db_path_);
    if (!db_manager_ || !db_manager_->initialize()) {
        std::cerr << "[Manager] 数据库管理器初始化失败" << std::endl;
        return false;
    }

    http_server_ = std::make_unique<HTTPServer>(db_manager_, port_);
    multicast_announcer_ = std::make_unique<MulticastAnnouncer>(port_);
    
    // 初始化ZeroMQ服务
    zmq_service_ = std::make_unique<ZmqService>("tcp://*:5555");
    zmq_service_->setMessageHandler([this](const std::string& message) -> std::string {
        return handleZmqMessage(message);
    });

    std::cout << "[Manager] 初始化成功" << std::endl;
    return true;
}

bool Manager::start() {
    if (running_) {
        std::cerr << "[Manager] 已经在运行" << std::endl;
        return false;
    }

    std::cout << "[Manager] 启动..." << std::endl;

    if (http_server_) {
        std::thread server_thread([this]() {
            if (!http_server_->start()) {
                std::cerr << "[Manager] HTTP服务器启动失败" << std::endl;
                running_ = false;
            }
        });
        server_thread.detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (multicast_announcer_) {
        multicast_announcer_->start();
    }

    // 启动ZeroMQ服务
    if (zmq_service_) {
        try {
            zmq_service_->start();
            std::cout << "[Manager] ZeroMQ服务启动成功" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Manager] ZeroMQ服务启动失败: " << e.what() << std::endl;
            return false;
        }
    }

    running_ = true;
    std::cout << "[Manager] 启动成功" << std::endl;
    return true;
}

void Manager::stop() {
    if (!running_) {
        std::cerr << "[Manager] 未在运行" << std::endl;
        return;
    }

    std::cout << "[Manager] 停止..." << std::endl;

    if (http_server_) {
        http_server_->stop();
    }
    if (multicast_announcer_) {
        multicast_announcer_->stop();
    }
    if (zmq_service_) {
        zmq_service_->stop();
    }

    running_ = false;
    std::cout << "[Manager] 已停止" << std::endl;
}

std::string Manager::handleZmqMessage(const std::string& message) {
    std::cout << "[Manager] 收到ZeroMQ消息: " << message << std::endl;
    
    // TODO: 根据消息内容处理不同的请求
    // 这里可以添加具体的消息处理逻辑
    
    return "已收到消息: " + message;
}
