#ifndef AGENT_CONTROL_MANAGER_H
#define AGENT_CONTROL_MANAGER_H

#include <string>
#include <memory>
#include <nlohmann/json.hpp>

class DatabaseManager;

class AgentControlManager {
public:
    AgentControlManager(std::shared_ptr<DatabaseManager> db_manager);
    nlohmann::json controlAgent(const std::string& board_id, const nlohmann::json& request_json);
private:
    std::shared_ptr<DatabaseManager> db_manager_;
};

#endif // AGENT_CONTROL_MANAGER_H 