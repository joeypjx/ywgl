#include "manager.h"
#include "http_server.h"
#include "database_manager.h"
#include "multicast_announcer.h"
#include "alarm/AlarmSubsystem.h"
#include "ResourceSubsystem.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

// 静态成员变量定义
std::shared_ptr<ResourceSubsystem> Manager::resource_subsystem_instance_ = nullptr;

Manager::Manager(int port, const std::string& db_path)
    : port_(port), db_path_(db_path), running_(false) {}

Manager::~Manager() {
    if (running_) {
        stop();
    }
}

bool Manager::initialize() {
    std::cout << "[Manager] 初始化..." << std::endl;

    tdengine_manager_ = std::make_shared<TDengineManager>("192.168.31.20", "test", "HZ715Net", "node_metrics");
    if (!tdengine_manager_ ) {
        std::cerr << "[Manager] TDengine管理器初始化失败" << std::endl;
        return false;
    }

    nlohmann::json resource_usage = nlohmann::json::parse(R"(
        {
            "host_ip": "192.168.10.29",
            "box_id": 1,
            "slot_id": 1,
            "timestamp": 1719168000,
            "resource": {
                "cpu": {
                    "usage_percent": 0.5,
                    "load_avg_1m": 0.1,
                    "load_avg_5m": 0.2,
                    "load_avg_15m": 0.3,
                    "core_count": 8,
                    "core_allocated": 4,
                    "temperature": 45.5,
                    "power": 18.0
                },
                "memory": {
                    "total": 16106127360,
                    "used": 8053063680,
                    "free": 8053063680,
                    "usage_percent": 50.0
                },
                "network": [
                    {
                        "interface": "eth0",
                        "rx_bytes": 1024,
                        "tx_bytes": 2048,
                        "rx_packets": 10,
                        "tx_packets": 20,
                        "rx_errors": 0,
                        "tx_errors": 0
                    }
                ],
                "disk": [
                    {
                        "device": "/dev/sda1",
                        "mount_point": "/",
                        "total": 1099511627776,
                        "used": 549755813888,
                        "free": 549755813888,
                        "usage_percent": 50.0
                    }
                ],
                "gpu": [
                    {
                        "index": 0,
                        "name": "NVIDIA A100",
                        "compute_usage": 30.5,
                        "mem_usage": 25.0,
                        "mem_used": 8589934592,
                        "mem_total": 34359738368,
                        "temperature": 65.0,
                        "power": 120.0
                    }
                ]
            },
            "component": [
                {
                    "instance_id": "instance-001",
                    "uuid": "uuid-001",
                    "index": 0,
                    "config": {
                        "name": "container-1",
                        "id": "container-id-001"
                    },
                    "state": "RUNNING",
                    "resource": {
                        "cpu": {
                            "load": 25.5
                        },
                        "memory": {
                            "mem_used": 2147483648,
                            "mem_limit": 4294967296
                        },
                        "network": {
                            "tx": 1048576,
                            "rx": 524288
                        }
                    }
                }
            ]
        }
    )");
    tdengine_manager_->saveMetrics(resource_usage);

    db_manager_ = std::make_shared<DatabaseManager>(db_path_);
    if (!db_manager_ || !db_manager_->initialize()) {
        std::cerr << "[Manager] 数据库管理器初始化失败" << std::endl;
        return false;
    }

    // 初始化ResourceSubsystem
    resource_subsystem_ = std::make_shared<ResourceSubsystem>(tdengine_manager_);
    resource_subsystem_instance_ = resource_subsystem_; // 保存到静态变量

    alarm_subsystem_ = std::make_shared<AlarmSubsystem>(db_manager_);
    alarm_subsystem_->initialize();

    http_server_ = std::make_unique<HTTPServer>(tdengine_manager_, alarm_subsystem_, port_);
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
        // std::thread server_thread([this]() {
            if (!http_server_->start()) {
                std::cerr << "[Manager] HTTP服务器启动失败" << std::endl;
                running_ = false;
            }
        // });
        // server_thread.detach();
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

// 静态方法实现
std::shared_ptr<ResourceSubsystem> Manager::getResourceSubsystem() {
    return resource_subsystem_instance_;
}
