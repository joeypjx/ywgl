// DatabaseManager.h
#pragma once

#include <SQLiteCpp/SQLiteCpp.h>
#include <mutex>
#include <string>
#include <memory>
#include <iostream>

class DatabaseManager {
private:
    // 使用 unique_ptr 来管理数据库对象的生命周期
    std::unique_ptr<SQLite::Database> db_;
    // 这个互斥锁将被所有使用者共享
    mutable std::mutex dbMutex_;

public:
    explicit DatabaseManager(const std::string& dbPath) {
        // 创建数据库连接
        db_ = std::make_unique<SQLite::Database>(dbPath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        std::cout << "[DB Manager] Database connection opened at: " << dbPath << std::endl;
    }

    // 禁止拷贝和赋值，确保全局只有一个实例（通过shared_ptr共享）
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    // 获取数据库对象的引用
    SQLite::Database& getDb() {
        return *db_;
    }

    // 获取互斥锁的引用
    std::mutex& getMutex() {
        return dbMutex_;
    }
};