#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

class ConfigManager {
public:
    static nlohmann::json load(const std::string& config_path) {
        std::ifstream in(config_path);
        if (!in.is_open()) {
            std::cerr << "[ConfigManager] 无法打开配置文件: " << config_path << std::endl;
            return {};
        }
        try {
            nlohmann::json j;
            in >> j;
            return j;
        } catch (const std::exception& e) {
            std::cerr << "[ConfigManager] 解析配置文件失败: " << e.what() << std::endl;
            return {};
        }
    }

    static std::string getString(const nlohmann::json& j, const std::string& key, const std::string& def = "") {
        if (j.contains(key) && j[key].is_string()) return j[key].get<std::string>();
        return def;
    }

    static int getInt(const nlohmann::json& j, const std::string& key, int def = 0) {
        if (j.contains(key) && j[key].is_number_integer()) return j[key].get<int>();
        return def;
    }
};

#endif // CONFIG_MANAGER_H 