#ifndef RESOURCE_SUBSYSTEM_H
#define RESOURCE_SUBSYSTEM_H

#include <string>
#include <memory>

// 前向声明
class DatabaseManager;

/**
 * @brief ResourceSubsystem类 - 资源管理子系统
 * 
 * 负责提供资源相关的查询接口，返回JSON字符串
 */
class ResourceSubsystem {
public:
    explicit ResourceSubsystem(std::shared_ptr<DatabaseManager> db_manager);
    ~ResourceSubsystem();

    /**
     * @brief 获取所有节点的列表
     * @return std::string JSON字符串格式的节点列表
     */
    std::string getNodeListJson();
    
private:
    std::shared_ptr<DatabaseManager> db_manager_;
};

#endif // RESOURCE_SUBSYSTEM_H 