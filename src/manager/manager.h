#ifndef MANAGER_MANAGER_H_
#define MANAGER_MANAGER_H_

#include <string>
#include <memory>
#include <atomic>
#include <nlohmann/json.hpp>

// 前向声明
class HTTPServer;
class DatabaseManager;
class MulticastAnnouncer;

using json = nlohmann::json;

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

private:
    // 处理RPC请求的方法
    json handleGetSystemInfo();
    json handleGetResourceUsage();
    json handleGetProcessList();
    json handleGetProcessInfo(int pid);

    int port_;             // HTTP服务器端口
    std::string db_path_;  // 数据库文件路径
    std::atomic<bool> running_; // 运行标志

    std::unique_ptr<HTTPServer> http_server_;                    // HTTP服务器
    std::shared_ptr<DatabaseManager> db_manager_;                // 数据库管理器
    std::unique_ptr<MulticastAnnouncer> multicast_announcer_;    // 组播公告器
};

#endif // MANAGER_MANAGER_H_