#include "zmq_service.h"
#include <iostream>

ZmqService::ZmqService(const std::string& endpoint)
    : endpoint_(endpoint)
    , running_(false) {
    try {
        context_ = std::make_unique<zmq::context_t>(1);
        socket_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_REP);
    } catch (const zmq::error_t& e) {
        std::cerr << "Failed to initialize ZMQ service: " << e.what() << std::endl;
        throw;
    }
}

ZmqService::~ZmqService() {
    stop();
}

void ZmqService::start() {
    if (running_) {
        return;
    }

    try {
        socket_->bind(endpoint_);
        running_ = true;
        worker_thread_ = std::thread(&ZmqService::run, this);
    } catch (const zmq::error_t& e) {
        std::cerr << "Failed to start ZMQ service: " << e.what() << std::endl;
        throw;
    }
}

void ZmqService::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    try {
        if (socket_) {
            socket_->close();
        }
        if (context_) {
            context_->close();
        }
    } catch (const zmq::error_t& e) {
        std::cerr << "Error closing ZMQ resources: " << e.what() << std::endl;
    }
}

bool ZmqService::send(const std::string& message) {
    if (!running_ || !socket_) {
        return false;
    }

    try {
        zmq::message_t zmq_message(message.size());
        memcpy(zmq_message.data(), message.data(), message.size());
        auto result = socket_->send(zmq_message, zmq::send_flags::none);
        return result.has_value();
    } catch (const zmq::error_t& e) {
        std::cerr << "Failed to send message: " << e.what() << std::endl;
        return false;
    }
}

void ZmqService::setMessageHandler(std::function<void(const std::string&)> handler) {
    message_handler_ = std::move(handler);
}

void ZmqService::run() {
    while (running_) {
        try {
            zmq::message_t message;
            auto result = socket_->recv(message, zmq::recv_flags::none);
            
            if (result.has_value()) {
                std::string received_message(static_cast<char*>(message.data()), message.size());
                handleMessage(received_message);
            }
        } catch (const zmq::error_t& e) {
            if (e.num() != ETERM) {  // 忽略终止错误
                std::cerr << "Error receiving message: " << e.what() << std::endl;
            }
            break;
        }
    }
}

void ZmqService::handleMessage(const std::string& message) {
    if (message_handler_) {
        try {
            message_handler_(message);
        } catch (const std::exception& e) {
            std::cerr << "Error in message handler: " << e.what() << std::endl;
        }
    }
} 