#include "ResourceSubsystem.h"
#include "database_manager.h"
#include <nlohmann/json.hpp>
#include <iostream>

ResourceSubsystem::ResourceSubsystem(std::shared_ptr<DatabaseManager> db_manager)
    : db_manager_(db_manager) {
    if (db_manager_) {
        std::cout << "ResourceSubsystem initialized with DatabaseManager." << std::endl;
    } else {
        std::cout << "ResourceSubsystem initialized without a valid DatabaseManager." << std::endl;
    }
}

ResourceSubsystem::~ResourceSubsystem() {
    std::cout << "ResourceSubsystem destroyed." << std::endl;
}

std::string ResourceSubsystem::getNodeListJson() {
    if (!db_manager_) {
        // 如果db_manager_无效，返回一个空的JSON数组字符串
        nlohmann::json error_json;
        error_json["error"] = "Database manager not initialized";
        return error_json.dump();
    }
    
    // 从DatabaseManager获取节点信息
    nlohmann::json nodes_json = db_manager_->getAllNodes();
    
    // 将JSON对象转换为字符串
    return nodes_json.dump();
} 