#pragma once

#include <string>
#include <memory>
#include <atomic>

// 前向声明
class HTTPServer;
class DatabaseManager;
class MulticastAnnouncer;
class AlarmSubsystem;
class ResourceSubsystem;

/**
 * @brief 系统中心管理器，协调各模块
 */
class Manager
{
public:
    Manager(int port = 8080, const std::string &db_path = "resource_monitor.db");
    ~Manager();

    // 初始化
    bool initialize();
    // 启动
    bool start();
    // 停止
    void stop();

    // 静态方法：获取ResourceSubsystem实例
    static std::shared_ptr<ResourceSubsystem> getResourceSubsystem();

private:
    int port_;             // HTTP服务器端口
    std::string db_path_;  // 数据库文件路径
    std::atomic<bool> running_; // 运行标志

    std::shared_ptr<DatabaseManager> db_manager_;                // 数据库管理器
    std::unique_ptr<HTTPServer> http_server_;                    // HTTP服务器
    std::unique_ptr<MulticastAnnouncer> multicast_announcer_;    // 组播公告器
    std::shared_ptr<AlarmSubsystem> alarm_subsystem_;            // 告警子系统
    std::shared_ptr<ResourceSubsystem> resource_subsystem_;      // 资源子系统

    // 静态成员变量：保存ResourceSubsystem实例
    static std::shared_ptr<ResourceSubsystem> resource_subsystem_instance_;
};