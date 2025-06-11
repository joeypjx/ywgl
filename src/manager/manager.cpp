#include "manager.h"
#include "http_server.h"
#include "database_manager.h"
#include "multicast_announcer.h"
#include "alarm/AlarmSubsystem.h"
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

    alarm_subsystem_ = std::make_shared<AlarmSubsystem>(db_manager_);
    alarm_subsystem_->initialize();

    http_server_ = std::make_unique<HTTPServer>(db_manager_, alarm_subsystem_, port_);
    multicast_announcer_ = std::make_unique<MulticastAnnouncer>(port_);

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

    if (alarm_subsystem_) {
        alarm_subsystem_->start();
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

    if (alarm_subsystem_) {
        alarm_subsystem_->stop();
    }

    if (http_server_) {
        http_server_->stop();
    }
    if (multicast_announcer_) {
        multicast_announcer_->stop();
    }

    running_ = false;
    std::cout << "[Manager] 已停止" << std::endl;
}
