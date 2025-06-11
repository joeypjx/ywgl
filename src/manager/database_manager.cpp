#include "database_manager.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>

DatabaseManager::DatabaseManager(const std::string &db_path)
    : db_path_(db_path),
      db_(nullptr),
      node_status_monitor_running_(false)
{
    // 构造函数，初始化数据库路径
}

DatabaseManager::~DatabaseManager()
{

    // 停止节点状态监控线程
    if (node_status_monitor_running_.load())
    {
        node_status_monitor_running_.store(false);
        if (node_status_monitor_thread_ && node_status_monitor_thread_->joinable())
        {
            node_status_monitor_thread_->join();
        }
    }
    // 数据库连接会自动关闭
}

bool DatabaseManager::initialize()
{
    try
    {
        // 创建或打开数据库
        db_ = std::make_unique<SQLite::Database>(db_path_, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

        // 启用外键约束
        db_->exec("PRAGMA foreign_keys = ON");

        // 初始化各类数据库表
        if (!initializeNodeTables())
        {
            std::cerr << "[DatabaseManager] Node tables initialization error" << std::endl;
            return false;
        }

        // 启动监控线程
        startNodeStatusMonitorThread();

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[DatabaseManager] Database initialization error: " << e.what() << std::endl;
        return false;
    }
}

SQLite::Database& DatabaseManager::getDb()
{
    return *db_;
}

std::mutex& DatabaseManager::getMutex()
{
    return db_mutex_;
}